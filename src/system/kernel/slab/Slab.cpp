/*
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *      Hugo Santos, hugosantos@gmail.com
 */

#include <Slab.h>

#include <string.h>

#include <KernelExport.h>
#include <util/AutoLock.h>
#include <util/DoublyLinkedList.h>
#include <util/OpenHashTable.h>

#include <vm_low_memory.h>

#include <algorithm> // swap
#include <new>


// TODO all of the small allocations we perform here will fallback
//      to the internal allocator which in the future will use this
//      same code. We'll have to resolve all of the dependencies
//      then, for now, it is still not required.
// TODO kMagazineCapacity should be dynamically tuned per cache.


#define TRACE_SLAB

#ifdef TRACE_SLAB
#define TRACE_CACHE(cache, format, args...) \
	dprintf("Cache[%p, %s] " format "\n", cache, cache->name , ##args)
#else
#define TRACE_CACHE(cache, format, bananas...) do { } while (0)
#endif


extern "C" status_t slab_init();


static const int kMagazineCapacity = 32;
static const size_t kCacheColorPeriod = 8;

struct object_link {
	struct object_link *next;
};

struct slab : DoublyLinkedListLinkImpl<slab> {
	void *pages;
	size_t count, size;
	size_t offset;
	object_link *free;
};

typedef DoublyLinkedList<slab> SlabList;

struct object_cache : DoublyLinkedListLinkImpl<object_cache> {
	char name[32];
	benaphore lock;
	size_t object_size;
	size_t cache_color_cycle;
	SlabList empty, partial, full;
	size_t empty_count, pressure;

	size_t slab_size;
	size_t usage, maximum;
	uint32 flags;

	void *cookie;
	object_cache_constructor constructor;
	object_cache_destructor destructor;
	object_cache_reclaimer reclaimer;

	base_depot depot;

	virtual slab *CreateSlab(uint32 flags) = 0;
	virtual void ReturnSlab(slab *slab) = 0;
	virtual slab *ObjectSlab(void *object) const = 0;

	slab *InitSlab(slab *slab, void *pages, size_t byteCount);
	void UninitSlab(slab *slab);

	virtual status_t PrepareObject(slab *source, void *object) { return B_OK; }
	virtual void UnprepareObject(slab *source, void *object) {}

	virtual ~object_cache() {}
};

typedef DoublyLinkedList<object_cache> ObjectCacheList;

struct SmallObjectCache : object_cache {
	slab *CreateSlab(uint32 flags);
	void ReturnSlab(slab *slab);
	slab *ObjectSlab(void *object) const;
};


struct HashedObjectCache : object_cache {
	struct Link : HashTableLink<Link> {
		const void *buffer;
		slab *parent;
	};

	struct Definition {
		typedef HashedObjectCache	ParentType;
		typedef const void *		KeyType;
		typedef Link				ValueType;

		Definition(HashedObjectCache *_parent) : parent(_parent) {}

		size_t HashKey(const void *key) const
		{
			return (((const uint8 *)key) - ((const uint8 *)0))
				>> parent->lower_boundary;
		}

		size_t Hash(Link *value) const { return HashKey(value->buffer); }
		bool Compare(const void *key, Link *value) const
			{ return value->buffer == key; }
		HashTableLink<Link> *GetLink(Link *value) const { return value; }

		HashedObjectCache *parent;
	};

	typedef OpenHashTable<Definition> HashTable;

	HashedObjectCache()
		: hash_table(this) {}

	slab *CreateSlab(uint32 flags);
	void ReturnSlab(slab *slab);
	slab *ObjectSlab(void *object) const;
	status_t PrepareObject(slab *source, void *object);
	void UnprepareObject(slab *source, void *object);

	HashTable hash_table;
	size_t lower_boundary;
};


struct depot_magazine {
	struct depot_magazine *next;
	uint16_t current_round, round_count;
	void *rounds[0];
};


static object_cache *sSlabCache, *sLinkCache;
static ObjectCacheList sObjectCaches;
static benaphore sObjectCacheListLock;


static depot_magazine *alloc_magazine();
static void free_magazine(depot_magazine *magazine);

template<typename Type> static Type *
_pop(Type* &head)
{
	Type *oldHead = head;
	head = head->next;
	return oldHead;
}


template<typename Type> static inline void
_push(Type* &head, Type *object)
{
	object->next = head;
	head = object;
}


static inline void *
link_to_object(object_link *link, size_t objectSize)
{
	return ((uint8_t *)link) - (objectSize - sizeof(object_link));
}


static inline object_link *
object_to_link(void *object, size_t objectSize)
{
	return (object_link *)(((uint8_t *)object)
		+ (objectSize - sizeof(object_link)));
}


static inline int
__fls0(size_t value)
{
	if (value == 0)
		return -1;

	int bit;
	for (bit = 0; value != 1; bit++)
		value >>= 1;
	return bit;
}


static status_t
allocate_pages(object_cache *cache, void **pages, uint32 flags)
{
	TRACE_CACHE(cache, "allocate pages (%lu, 0x0%lx)", cache->slab_size, flags);

	area_id areaId = create_area(cache->name, pages, B_ANY_KERNEL_ADDRESS,
		cache->slab_size, B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);
	if (areaId < 0)
		return areaId;

	cache->usage += cache->slab_size;

	TRACE_CACHE(cache, "  ... = { %ld, %p }", areaId, *pages);

	return B_OK;
}


static void
free_pages(object_cache *cache, void *pages)
{
	area_id id = area_for(pages);

	TRACE_CACHE(cache, "delete pages %p (%ld)", pages, id);

	if (id < B_OK)
		panic("object cache: freeing unknown area");

	delete_area(id);

	cache->usage -= cache->slab_size;
}


static void
object_cache_low_memory(void *_self, int32 level)
{
	if (level == B_NO_LOW_MEMORY)
		return;

	object_cache *cache = (object_cache *)_self;

	BenaphoreLocker _(cache->lock);

	size_t minimumAllowed;

	// TODO: call reclaim

	switch (level) {
	case B_LOW_MEMORY_NOTE:
		minimumAllowed = cache->pressure / 2 + 1;
		break;

	case B_LOW_MEMORY_WARNING:
		cache->pressure /= 2;
		minimumAllowed = 0;
		break;

	default:
		cache->pressure = 0;
		minimumAllowed = 0;
		break;
	}

	if (cache->empty_count <= minimumAllowed)
		return;

	TRACE_CACHE(cache, "cache: memory pressure, will release down to %lu.",
		minimumAllowed);

	while (cache->empty_count > minimumAllowed) {
		cache->ReturnSlab(cache->empty.RemoveHead());
		cache->empty_count--;
	}
}


static status_t
object_cache_init(object_cache *cache, const char *name, size_t objectSize,
	size_t alignment, size_t maximum, uint32 flags, void *cookie,
	object_cache_constructor constructor, object_cache_destructor destructor,
	object_cache_reclaimer reclaimer)
{
	status_t status = benaphore_init(&cache->lock, name);
	if (status < B_OK)
		return status;

	strlcpy(cache->name, name, sizeof(cache->name));

	if (objectSize < sizeof(object_link))
		objectSize = sizeof(object_link);

	if (alignment > 0 && (objectSize & (alignment - 1)))
		cache->object_size = objectSize + alignment
			- (objectSize & (alignment - 1));
	else
		cache->object_size = objectSize;

	TRACE_CACHE(cache, "init %lu, %lu -> %lu", objectSize, alignment,
		cache->object_size);

	cache->cache_color_cycle = 0;
	cache->empty_count = 0;
	cache->pressure = 0;

	cache->usage = 0;
	cache->maximum = maximum;

	cache->flags = flags;

	if (!(flags & CACHE_NO_DEPOT)) {
		// TODO init depot
	}

	cache->cookie = cookie;
	cache->constructor = constructor;
	cache->destructor = destructor;
	cache->reclaimer = reclaimer;

	register_low_memory_handler(object_cache_low_memory, cache, 0);

	BenaphoreLocker _(sObjectCacheListLock);
	sObjectCaches.Add(cache);

	return B_OK;
}


static SmallObjectCache *
create_small_object_cache(const char *name, size_t object_size,
	size_t alignment, size_t maximum, uint32 flags, void *cookie,
	object_cache_constructor constructor, object_cache_destructor destructor,
	object_cache_reclaimer reclaimer)
{
	SmallObjectCache *cache = new (std::nothrow) SmallObjectCache();
	if (cache == NULL)
		return NULL;

	if (object_cache_init(cache, name, object_size, alignment, maximum, flags,
		cookie, constructor, destructor, reclaimer) < B_OK) {
		delete cache;
		return NULL;
	}

	cache->slab_size = B_PAGE_SIZE;

	return cache;
}


static HashedObjectCache *
create_hashed_object_cache(const char *name, size_t object_size,
	size_t alignment, size_t maximum, uint32 flags, void *cookie,
	object_cache_constructor constructor, object_cache_destructor destructor,
	object_cache_reclaimer reclaimer)
{
	HashedObjectCache *cache = new (std::nothrow) HashedObjectCache();
	if (cache == NULL)
		return NULL;

	if (object_cache_init(cache, name, object_size, alignment, maximum, flags,
		cookie, constructor, destructor, reclaimer) < B_OK) {
		delete cache;
		return NULL;
	}

	cache->slab_size = 16 * B_PAGE_SIZE;
	cache->lower_boundary = __fls0(cache->object_size);

	return cache;
}


object_cache *
create_object_cache(const char *name, size_t object_size, size_t alignment,
	void *cookie, object_cache_constructor constructor,
	object_cache_destructor destructor)
{
	return create_object_cache_etc(name, object_size, alignment, 0, 0, cookie,
		constructor, destructor, NULL);
}


object_cache *
create_object_cache_etc(const char *name, size_t object_size, size_t alignment,
	size_t maximum, uint32 flags, void *cookie,
	object_cache_constructor constructor, object_cache_destructor destructor,
	object_cache_reclaimer reclaimer)
{
	if (object_size == 0)
		return NULL;
	else if (object_size <= 256)
		return create_small_object_cache(name, object_size, alignment,
			maximum, flags, cookie, constructor, destructor, reclaimer);

	return create_hashed_object_cache(name, object_size, alignment,
		maximum, flags, cookie, constructor, destructor, reclaimer);
}


void
delete_object_cache(object_cache *cache)
{
	{
		BenaphoreLocker _(sObjectCacheListLock);
		sObjectCaches.Remove(cache);
	}

	benaphore_lock(&cache->lock);

	unregister_low_memory_handler(object_cache_low_memory, cache);

	if (!cache->full.IsEmpty())
		panic("cache destroy: still has full slabs");

	if (!cache->partial.IsEmpty())
		panic("cache destroy: still has partial slabs");

	while (!cache->empty.IsEmpty())
		cache->ReturnSlab(cache->empty.RemoveHead());

	cache->empty_count = 0;

	benaphore_destroy(&cache->lock);
	delete cache;
}


void *
object_cache_alloc(object_cache *cache, uint32 flags)
{
	BenaphoreLocker _(cache->lock);

	slab *source = NULL;

	if (cache->partial.IsEmpty()) {
		if (cache->empty.IsEmpty()) {
			source = cache->CreateSlab(flags);
			if (source == NULL)
				return NULL;

			cache->pressure++;
		} else {
			cache->empty_count--;
			source = cache->empty.RemoveHead();
		}

		cache->partial.Add(source);
	} else {
		source = cache->partial.Head();
	}

	object_link *link = _pop(source->free);
	source->count--;

	TRACE_CACHE(cache, "allocate %p from %p, %lu remaining.", link, source,
		source->count);

	if (source->count == 0) {
		cache->partial.Remove(source);
		cache->full.Add(source);
	}

	return link_to_object(link, cache->object_size);
}


static void
object_cache_return_to_slab(object_cache *cache, slab *source, void *object)
{
	if (source == NULL)
		panic("object_cache: free'd object has no slab");

	object_link *link = object_to_link(object, cache->object_size);

	TRACE_CACHE(cache, "returning %p to %p, %lu used (%lu empty slabs).",
		link, source, source->size - source->count, cache->empty_count);

	_push(source->free, link);
	source->count++;
	if (source->count == source->size) {
		cache->partial.Remove(source);

		if (cache->empty_count < cache->pressure) {
			cache->empty_count++;
			cache->empty.Add(source);
		} else {
			cache->ReturnSlab(source);
		}
	} else if (source->count == 1) {
		cache->full.Remove(source);
		cache->partial.Add(source);
	}
}


void
object_cache_free(object_cache *cache, void *object)
{
	BenaphoreLocker _(cache->lock);
	object_cache_return_to_slab(cache, cache->ObjectSlab(object), object);
}


slab *
object_cache::InitSlab(slab *slab, void *pages, size_t byteCount)
{
	TRACE_CACHE(this, "construct (%p, %p, %lu)", slab, pages, byteCount);

	slab->pages = pages;
	slab->count = slab->size = byteCount / object_size;
	slab->free = NULL;

	size_t spareBytes = byteCount - (slab->size * object_size);
	slab->offset = cache_color_cycle;

	if (slab->offset > spareBytes)
		cache_color_cycle = slab->offset = 0;
	else
		cache_color_cycle += kCacheColorPeriod;

	TRACE_CACHE(this, "  %lu objects, %lu spare bytes, offset %lu",
		slab->size, spareBytes, slab->offset);

	uint8_t *data = ((uint8_t *)pages) + slab->offset;

	for (size_t i = 0; i < slab->size; i++) {
		bool failedOnFirst = false;

		status_t status = PrepareObject(slab, data);
		if (status < B_OK)
			failedOnFirst = true;
		else if (constructor)
			status = constructor(cookie, data);

		if (status < B_OK) {
			if (!failedOnFirst)
				UnprepareObject(slab, data);

			data = ((uint8_t *)pages) + slab->offset;
			for (size_t j = 0; j < i; j++) {
				if (destructor)
					destructor(cookie, data);
				UnprepareObject(slab, data);
				data += object_size;
			}

			return NULL;
		}

		_push(slab->free, object_to_link(data, object_size));
		data += object_size;
	}

	return slab;
}


void
object_cache::UninitSlab(slab *slab)
{
	TRACE_CACHE(this, "destruct %p", slab);

	if (slab->count != slab->size)
		panic("cache: destroying a slab which isn't empty.");

	uint8_t *data = ((uint8_t *)slab->pages) + slab->offset;

	for (size_t i = 0; i < slab->size; i++) {
		if (destructor)
			destructor(cookie, data);
		UnprepareObject(slab, data);
		data += object_size;
	}
}


static inline slab *
slab_in_pages(const void *pages, size_t slab_size)
{
	return (slab *)(((uint8_t *)pages) + slab_size - sizeof(slab));
}


static inline const void *
lower_boundary(void *object, size_t byteCount)
{
	const uint8_t *null = (uint8_t *)NULL;
	return null + ((((uint8_t *)object) - null) & ~(byteCount - 1));
}


static inline bool
check_cache_quota(object_cache *cache)
{
	if (cache->maximum == 0)
		return true;

	return (cache->usage + cache->slab_size) <= cache->maximum;
}


slab *
SmallObjectCache::CreateSlab(uint32 flags)
{
	if (!check_cache_quota(this))
		return NULL;

	void *pages;

	if (allocate_pages(this, &pages, flags) < B_OK)
		return NULL;

	return InitSlab(slab_in_pages(pages, slab_size), pages,
		slab_size - sizeof(slab));
}


void
SmallObjectCache::ReturnSlab(slab *slab)
{
	UninitSlab(slab);
	free_pages(this, slab->pages);
}


slab *
SmallObjectCache::ObjectSlab(void *object) const
{
	return slab_in_pages(lower_boundary(object, object_size), slab_size);
}


static slab *
allocate_slab(uint32 flags)
{
	return (slab *)object_cache_alloc(sSlabCache, flags);
}


static void
free_slab(slab *slab)
{
	object_cache_free(sSlabCache, slab);
}


static HashedObjectCache::Link *
allocate_link(uint32 flags)
{
	return (HashedObjectCache::Link *)object_cache_alloc(sLinkCache, flags);
}


static void
free_link(HashedObjectCache::Link *link)
{
	object_cache_free(sLinkCache, link);
}


slab *
HashedObjectCache::CreateSlab(uint32 flags)
{
	if (!check_cache_quota(this))
		return NULL;

	slab *slab = allocate_slab(flags);
	if (slab == NULL)
		return NULL;

	void *pages;
	if (allocate_pages(this, &pages, flags) < B_OK) {
		free_slab(slab);
		return NULL;
	}

	if (InitSlab(slab, pages, slab_size) == NULL) {
		free_pages(this, pages);
		free_slab(slab);
		return NULL;
	}

	return slab;
}


void
HashedObjectCache::ReturnSlab(slab *slab)
{
	UninitSlab(slab);
	free_pages(this, slab->pages);
}


slab *
HashedObjectCache::ObjectSlab(void *object) const
{
	Link *link = hash_table.Lookup(object);
	if (link == NULL)
		panic("object cache: requested object missing from hash table");
	return link->parent;
}


status_t
HashedObjectCache::PrepareObject(slab *source, void *object)
{
	Link *link = allocate_link(CACHE_DONT_SLEEP);
	if (link == NULL)
		return B_NO_MEMORY;

	link->buffer = object;
	link->parent = source;

	hash_table.Insert(link);
	return B_OK;
}


void
HashedObjectCache::UnprepareObject(slab *source, void *object)
{
	Link *link = hash_table.Lookup(object);
	if (link == NULL)
		panic("object cache: requested object missing from hash table");
	if (link->parent != source)
		panic("object cache: slab mismatch");

	hash_table.Remove(link);
	free_link(link);
}


static inline bool
is_magazine_empty(depot_magazine *magazine)
{
	return magazine->current_round == 0;
}


static inline bool
is_magazine_full(depot_magazine *magazine)
{
	return magazine->current_round == magazine->round_count;
}


static inline void *
pop_magazine(depot_magazine *magazine)
{
	return magazine->rounds[--magazine->current_round];
}


static inline bool
push_magazine(depot_magazine *magazine, void *object)
{
	if (is_magazine_full(magazine))
		return false;
	magazine->rounds[magazine->current_round++] = object;
	return true;
}


static bool
exchange_with_full(base_depot *depot, depot_magazine* &magazine)
{
	BenaphoreLocker _(depot->lock);

	if (depot->full == NULL)
		return false;

	depot->full_count--;
	depot->empty_count++;

	_push(depot->empty, magazine);
	magazine = _pop(depot->full);
	return true;
}


static bool
exchange_with_empty(base_depot *depot, depot_magazine* &magazine)
{
	BenaphoreLocker _(depot->lock);

	if (depot->empty == NULL) {
		depot->empty = alloc_magazine();
		if (depot->empty == NULL)
			return false;
	} else {
		depot->empty_count--;
	}

	if (magazine) {
		_push(depot->full, magazine);
		depot->full_count++;
	}

	magazine = _pop(depot->empty);
	return true;
}


static depot_magazine *
alloc_magazine()
{
	depot_magazine *magazine = (depot_magazine *)malloc(sizeof(depot_magazine)
		+ kMagazineCapacity * sizeof(void *));
	if (magazine) {
		magazine->next = NULL;
		magazine->current_round = 0;
		magazine->round_count = kMagazineCapacity;
	}

	return magazine;
}


static void
free_magazine(depot_magazine *magazine)
{
	free(magazine);
}


static void
empty_magazine(base_depot *depot, depot_magazine *magazine)
{
	for (uint16_t i = 0; i < magazine->current_round; i++)
		depot->return_object(depot, magazine->rounds[i]);
	free_magazine(magazine);
}


status_t
base_depot_init(base_depot *depot,
	void (*return_object)(base_depot *depot, void *object))
{
	depot->full = NULL;
	depot->empty = NULL;
	depot->full_count = depot->empty_count = 0;

	status_t status = benaphore_init(&depot->lock, "depot");
	if (status < B_OK)
		return status;

	depot->stores = new (std::nothrow) depot_cpu_store[smp_get_num_cpus()];
	if (depot->stores == NULL) {
		benaphore_destroy(&depot->lock);
		return B_NO_MEMORY;
	}

	for (int i = 0; i < smp_get_num_cpus(); i++) {
		benaphore_init(&depot->stores[i].lock, "cpu store");
		depot->stores[i].loaded = depot->stores[i].previous = NULL;
	}

	depot->return_object = return_object;

	return B_OK;
}


void
base_depot_destroy(base_depot *depot)
{
	base_depot_make_empty(depot);

	for (int i = 0; i < smp_get_num_cpus(); i++) {
		benaphore_destroy(&depot->stores[i].lock);
	}

	delete [] depot->stores;

	benaphore_destroy(&depot->lock);
}


void *
base_depot_obtain_from_store(base_depot *depot, depot_cpu_store *store)
{
	BenaphoreLocker _(store->lock);

	// To better understand both the Alloc() and Free() logic refer to
	// Bonwick's ``Magazines and Vmem'' [in 2001 USENIX proceedings]

	// In a nutshell, we try to get an object from the loaded magazine
	// if it's not empty, or from the previous magazine if it's full
	// and finally from the Slab if the magazine depot has no full magazines.

	if (store->loaded == NULL)
		return NULL;

	while (true) {
		if (!is_magazine_empty(store->loaded))
			return pop_magazine(store->loaded);

		if (store->previous && (is_magazine_full(store->previous)
			|| exchange_with_full(depot, store->previous)))
			std::swap(store->previous, store->loaded);
		else
			return NULL;
	}
}


int
base_depot_return_to_store(base_depot *depot, depot_cpu_store *store,
	void *object)
{
	BenaphoreLocker _(store->lock);

	// We try to add the object to the loaded magazine if we have one
	// and it's not full, or to the previous one if it is empty. If
	// the magazine depot doesn't provide us with a new empty magazine
	// we return the object directly to the slab.

	while (true) {
		if (store->loaded && push_magazine(store->loaded, object))
			return 1;

		if ((store->previous && is_magazine_empty(store->previous))
			|| exchange_with_empty(depot, store->previous))
			std::swap(store->loaded, store->previous);
		else
			return 0;
	}
}


void
base_depot_make_empty(base_depot *depot)
{
	// TODO locking

	for (int i = 0; i < smp_get_num_cpus(); i++) {
		if (depot->stores[i].loaded)
			empty_magazine(depot, depot->stores[i].loaded);
		if (depot->stores[i].previous)
			empty_magazine(depot, depot->stores[i].previous);
		depot->stores[i].loaded = depot->stores[i].previous = NULL;
	}

	while (depot->full)
		empty_magazine(depot, _pop(depot->full));

	while (depot->empty)
		empty_magazine(depot, _pop(depot->empty));
}


static int
dump_slabs(int argc, char *argv[])
{
	kprintf("%10s %32s %8s %8s %6s\n", "address", "name", "objsize", "usage",
		"empty");

	ObjectCacheList::Iterator it = sObjectCaches.GetIterator();

	while (it.HasNext()) {
		object_cache *cache = it.Next();

		kprintf("%p %32s %8lu %8lu %6lu\n", cache, cache->name,
			cache->object_size, cache->usage, cache->empty_count);
	}

	return 0;
}


status_t
slab_init()
{
	status_t status = benaphore_init(&sObjectCacheListLock, "object cache list");
	if (status < B_OK)
		panic("slab_init: failed to create object cache list lock");

	new (&sObjectCaches) ObjectCacheList();

	sSlabCache = create_object_cache("slab cache", sizeof(slab), 4, NULL, NULL,
		NULL);
	if (sSlabCache == NULL)
		panic("slab_init: failed to create slab cache");

	sLinkCache = create_object_cache("link cache",
		sizeof(HashedObjectCache::Link), 4, NULL, NULL, NULL);
	if (sLinkCache == NULL)
		panic("slab_init: failed to create link cache");

	add_debugger_command("slabs", dump_slabs, "list all object caches");

	return B_OK;
}

