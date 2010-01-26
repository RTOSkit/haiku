/*
 * Copyright 2008, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2005-2009, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */


#include <low_resource_manager.h>

#include <new>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <KernelExport.h>

#include <condition_variable.h>
#include <elf.h>
#include <lock.h>
#include <sem.h>
#include <util/AutoLock.h>
#include <util/DoublyLinkedList.h>
#include <vm/vm_page.h>
#include <vm/vm_priv.h>


//#define TRACE_LOW_RESOURCE_MANAGER
#ifdef TRACE_LOW_RESOURCE_MANAGER
#	define TRACE(x)	dprintf x
#else
#	define TRACE(x) ;
#endif


struct low_resource_handler
		: public DoublyLinkedListLinkImpl<low_resource_handler> {
	low_resource_func	function;
	void*				data;
	uint32				resources;
	int32				priority;
};

typedef DoublyLinkedList<low_resource_handler> HandlerList;


static const bigtime_t kLowResourceInterval = 3000000;	// 3 secs
static const bigtime_t kWarnResourceInterval = 500000;	// 0.5 secs

// page limits
static const size_t kNotePagesLimit = VM_PAGE_RESERVE_USER * 4;
static const size_t kWarnPagesLimit = VM_PAGE_RESERVE_USER;
static const size_t kCriticalPagesLimit = VM_PAGE_RESERVE_SYSTEM;

// memory limits
static const off_t kMinNoteMemoryLimit		= VM_MEMORY_RESERVE_USER * 4;
static const off_t kMinWarnMemoryLimit		= VM_MEMORY_RESERVE_USER;
static const off_t kMinCriticalMemoryLimit	= VM_MEMORY_RESERVE_SYSTEM;
static off_t sNoteMemoryLimit;
static off_t sWarnMemoryLimit;
static off_t sCriticalMemoryLimit;


static int32 sLowPagesState = B_NO_LOW_RESOURCE;
static int32 sLowMemoryState = B_NO_LOW_RESOURCE;
static int32 sLowSemaphoresState = B_NO_LOW_RESOURCE;
static int32 sLowSpaceState = B_NO_LOW_RESOURCE;
static uint32 sLowResources = 0;	// resources that are not B_NO_LOW_RESOURCE
static bigtime_t sLastMeasurement;

static recursive_lock sLowResourceLock
	= RECURSIVE_LOCK_INITIALIZER("low resource");
static sem_id sLowResourceWaitSem;
static HandlerList sLowResourceHandlers;

static ConditionVariable sLowResourceWaiterCondition;


static int32
low_resource_state_no_update(uint32 resources)
{
	int32 state = B_NO_LOW_RESOURCE;

	if ((resources & B_KERNEL_RESOURCE_PAGES) != 0)
		state = max_c(state, sLowPagesState);
	if ((resources & B_KERNEL_RESOURCE_MEMORY) != 0)
		state = max_c(state, sLowMemoryState);
	if ((resources & B_KERNEL_RESOURCE_SEMAPHORES) != 0)
		state = max_c(state, sLowSemaphoresState);
	if ((resources & B_KERNEL_RESOURCE_ADDRESS_SPACE) != 0)
		state = max_c(state, sLowSpaceState);

	return state;
}


/*!	Calls low resource handlers for the given resources.
	sLowResourceLock must be held.
*/
static void
call_handlers(uint32 lowResources)
{
	if (sLowResourceHandlers.IsEmpty())
		return;

	// Add a marker, so we can drop the lock while calling the handlers and
	// still iterate safely.
	low_resource_handler marker;
	sLowResourceHandlers.Insert(&marker, false);

	while (low_resource_handler* handler
			= sLowResourceHandlers.GetNext(&marker)) {
		// swap with handler
		sLowResourceHandlers.Swap(&marker, handler);
		marker.priority = handler->priority;

		int32 resources = handler->resources & lowResources;
		if (resources != 0) {
			recursive_lock_unlock(&sLowResourceLock);
			handler->function(handler->data, resources,
				low_resource_state_no_update(resources));
			recursive_lock_lock(&sLowResourceLock);
		}
	}

	// remove marker
	sLowResourceHandlers.Remove(&marker);
}


static void
compute_state(void)
{
	sLastMeasurement = system_time();

	sLowResources = B_ALL_KERNEL_RESOURCES;

	// free pages state
	uint32 freePages = vm_page_num_free_pages();

	if (freePages < kCriticalPagesLimit) {
		sLowPagesState = B_LOW_RESOURCE_CRITICAL;
	} else if (freePages < kWarnPagesLimit) {
		sLowPagesState = B_LOW_RESOURCE_WARNING;
	} else if (freePages < kNotePagesLimit) {
		sLowPagesState = B_LOW_RESOURCE_NOTE;
	} else {
		sLowPagesState = B_NO_LOW_RESOURCE;
		sLowResources &= ~B_KERNEL_RESOURCE_PAGES;
	}

	// free memory state
	off_t freeMemory = vm_available_not_needed_memory();

	if (freeMemory < sCriticalMemoryLimit) {
		sLowMemoryState = B_LOW_RESOURCE_CRITICAL;
	} else if (freeMemory < sWarnMemoryLimit) {
		sLowMemoryState = B_LOW_RESOURCE_WARNING;
	} else if (freeMemory < sNoteMemoryLimit) {
		sLowMemoryState = B_LOW_RESOURCE_NOTE;
	} else {
		sLowMemoryState = B_NO_LOW_RESOURCE;
		sLowResources &= ~B_KERNEL_RESOURCE_MEMORY;
	}

	// free semaphores state
	uint32 maxSems = sem_max_sems();
	uint32 freeSems = maxSems - sem_used_sems();

	if (freeSems < maxSems >> 16) {
		sLowSemaphoresState = B_LOW_RESOURCE_CRITICAL;
	} else if (freeSems < maxSems >> 8) {
		sLowSemaphoresState = B_LOW_RESOURCE_WARNING;
	} else if (freeSems < maxSems >> 4) {
		sLowSemaphoresState = B_LOW_RESOURCE_NOTE;
	} else {
		sLowSemaphoresState = B_NO_LOW_RESOURCE;
		sLowResources &= ~B_KERNEL_RESOURCE_SEMAPHORES;
	}

	// free kernel address space state
	// TODO: this should take fragmentation into account
	size_t maxSpace = KERNEL_SIZE;
	size_t freeSpace = vm_kernel_address_space_left();

	if (freeSpace < maxSpace >> 16)
		sLowSpaceState = B_LOW_RESOURCE_CRITICAL;
	if (freeSpace < maxSpace >> 8)
		sLowSpaceState = B_LOW_RESOURCE_WARNING;
	if (freeSpace < maxSpace >> 4)
		sLowSpaceState = B_LOW_RESOURCE_NOTE;
	else {
		sLowSpaceState = B_NO_LOW_RESOURCE;
		sLowResources &= ~B_KERNEL_RESOURCE_ADDRESS_SPACE;
	}
}


static status_t
low_resource_manager(void*)
{
	bigtime_t timeout = kLowResourceInterval;
	while (true) {
		int32 state = low_resource_state_no_update(B_ALL_KERNEL_RESOURCES);
		if (state != B_LOW_RESOURCE_CRITICAL) {
			acquire_sem_etc(sLowResourceWaitSem, 1, B_RELATIVE_TIMEOUT,
				timeout);
		}

		RecursiveLocker _(&sLowResourceLock);

		compute_state();
		state = low_resource_state_no_update(B_ALL_KERNEL_RESOURCES);

		TRACE(("low_resource_manager: state = %ld, %ld free pages, %lld free "
			"memory, %lu free semaphores\n", state, vm_page_num_free_pages(),
			vm_available_not_needed_memory(),
			sem_max_sems() - sem_used_sems()));

		if (state < B_LOW_RESOURCE_NOTE)
			continue;

		call_handlers(sLowResources);

		if (state == B_LOW_RESOURCE_WARNING)
			timeout = kWarnResourceInterval;
		else
			timeout = kLowResourceInterval;

		sLowResourceWaiterCondition.NotifyAll();
	}
	return 0;
}


static const char*
state_to_string(uint32 state)
{
	switch (state) {
		case B_LOW_RESOURCE_CRITICAL:
			return "critical";
		case B_LOW_RESOURCE_WARNING:
			return "warning";
		case B_LOW_RESOURCE_NOTE:
			return "note";

		default:
			return "normal";
	}
}


static int
dump_handlers(int argc, char** argv)
{
	kprintf("current state: %c%c%c%c\n",
		(sLowResources & B_KERNEL_RESOURCE_PAGES) != 0 ? 'p' : '-',
		(sLowResources & B_KERNEL_RESOURCE_MEMORY) != 0 ? 'm' : '-',
		(sLowResources & B_KERNEL_RESOURCE_SEMAPHORES) != 0 ? 's' : '-',
		(sLowResources & B_KERNEL_RESOURCE_ADDRESS_SPACE) != 0 ? 'a' : '-');
	kprintf("  pages:  %s\n", state_to_string(sLowPagesState));
	kprintf("  memory: %s\n", state_to_string(sLowMemoryState));
	kprintf("  sems:   %s\n", state_to_string(sLowSemaphoresState));
	kprintf("  aspace: %s\n\n", state_to_string(sLowSpaceState));

	HandlerList::Iterator iterator = sLowResourceHandlers.GetIterator();
	kprintf("function    data         resources  prio  function-name\n");

	while (iterator.HasNext()) {
		low_resource_handler *handler = iterator.Next();

		const char* symbol = NULL;
		elf_debug_lookup_symbol_address((addr_t)handler->function, NULL,
			&symbol, NULL, NULL);

		char resources[16];
		snprintf(resources, sizeof(resources), "%c %c %c",
			handler->resources & B_KERNEL_RESOURCE_PAGES ? 'p' : ' ',
			handler->resources & B_KERNEL_RESOURCE_MEMORY ? 'm' : ' ',
			handler->resources & B_KERNEL_RESOURCE_SEMAPHORES ? 's' : ' ');

		kprintf("%p  %p   %s      %4ld  %s\n", handler->function, handler->data,
			resources, handler->priority, symbol);
	}

	return 0;
}


//	#pragma mark - private kernel API


/*!	Notifies the low resource manager that a resource is lacking. If \a flags
	and \a timeout specify a timeout, the function will wait until the low
	resource manager has finished its next iteration of calling low resource
	handlers, or until the timeout occurs (whichever happens first).
*/
void
low_resource(uint32 resource, uint64 requirements, uint32 flags, uint32 timeout)
{
	// TODO: take requirements into account

	switch (resource) {
		case B_KERNEL_RESOURCE_PAGES:
			vm_schedule_page_scanner(requirements);
			break;

		case B_KERNEL_RESOURCE_MEMORY:
		case B_KERNEL_RESOURCE_SEMAPHORES:
		case B_KERNEL_RESOURCE_ADDRESS_SPACE:
			break;
	}

	release_sem(sLowResourceWaitSem);

	if ((flags & B_RELATIVE_TIMEOUT) == 0 || timeout > 0)
		sLowResourceWaiterCondition.Wait(flags, timeout);
}


int32
low_resource_state(uint32 resources)
{
	recursive_lock_lock(&sLowResourceLock);

	if (system_time() - sLastMeasurement > 500000)
		compute_state();

	int32 state = low_resource_state_no_update(resources);

	recursive_lock_unlock(&sLowResourceLock);

	return state;
}


status_t
low_resource_manager_init(void)
{
	new(&sLowResourceHandlers) HandlerList;
		// static initializers do not work in the kernel,
		// so we have to do it here, manually

	sLowResourceWaiterCondition.Init(NULL, "low resource waiters");

	// compute the free memory limits
	off_t totalMemory = (off_t)vm_page_num_pages() * B_PAGE_SIZE;
	sNoteMemoryLimit = totalMemory / 16;
	if (sNoteMemoryLimit < kMinNoteMemoryLimit) {
		sNoteMemoryLimit = kMinNoteMemoryLimit;
		sWarnMemoryLimit = kMinWarnMemoryLimit;
		sCriticalMemoryLimit = kMinCriticalMemoryLimit;
	} else {
		sWarnMemoryLimit = totalMemory / 64;
		sCriticalMemoryLimit = totalMemory / 256;
	}

	return B_OK;
}


status_t
low_resource_manager_init_post_thread(void)
{
	sLowResourceWaitSem = create_sem(0, "low resource wait");
	if (sLowResourceWaitSem < B_OK)
		return sLowResourceWaitSem;

	thread_id thread = spawn_kernel_thread(&low_resource_manager,
		"low resource manager", B_LOW_PRIORITY, NULL);
	send_signal_etc(thread, SIGCONT, B_DO_NOT_RESCHEDULE);

	add_debugger_command("low_resource", &dump_handlers,
		"Dump list of low resource handlers");
	return B_OK;
}


status_t
unregister_low_resource_handler(low_resource_func function, void* data)
{
	TRACE(("unregister_low_resource_handler(function = %p, data = %p)\n",
		function, data));

	RecursiveLocker locker(&sLowResourceLock);
	HandlerList::Iterator iterator = sLowResourceHandlers.GetIterator();

	while (iterator.HasNext()) {
		low_resource_handler* handler = iterator.Next();

		if (handler->function == function && handler->data == data) {
			sLowResourceHandlers.Remove(handler);
			free(handler);
			return B_OK;
		}
	}

	return B_ENTRY_NOT_FOUND;
}


/*! Registers a low resource handler. The higher the \a priority, the earlier
	the handler will be called in low resource situations.
*/
status_t
register_low_resource_handler(low_resource_func function, void* data,
	uint32 resources, int32 priority)
{
	TRACE(("register_low_resource_handler(function = %p, data = %p)\n",
		function, data));

	low_resource_handler *newHandler = (low_resource_handler*)malloc(
		sizeof(low_resource_handler));
	if (newHandler == NULL)
		return B_NO_MEMORY;

	newHandler->function = function;
	newHandler->data = data;
	newHandler->resources = resources;
	newHandler->priority = priority;

	RecursiveLocker locker(&sLowResourceLock);

	// sort it in after priority (higher priority comes first)

	HandlerList::ReverseIterator iterator
		= sLowResourceHandlers.GetReverseIterator();
	low_resource_handler* last = NULL;
	while (iterator.HasNext()) {
		low_resource_handler *handler = iterator.Next();

		if (handler->priority >= priority) {
			sLowResourceHandlers.Insert(last, newHandler);
			return B_OK;
		}
		last = handler;
	}

	sLowResourceHandlers.Add(newHandler, false);
	return B_OK;
}
