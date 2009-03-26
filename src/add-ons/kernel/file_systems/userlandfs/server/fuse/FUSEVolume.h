/*
 * Copyright 2009, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */
#ifndef USERLAND_FS_FUSE_VOLUME_H
#define USERLAND_FS_FUSE_VOLUME_H

#include <AutoLocker.h>
#include <RWLockManager.h>

#include "Locker.h"

#include "fuse_fs.h"
#include "FUSEEntry.h"

#include "../Volume.h"


namespace UserlandFS {

class FUSEFileSystem;


class FUSEVolume : public Volume {
public:
								FUSEVolume(FUSEFileSystem* fileSystem,
									dev_t id);
	virtual						~FUSEVolume();

			status_t			Init();

			void				SetFS(fuse_fs* fs)	{ fFS = fs; }

	// FS
	virtual	status_t			Mount(const char* device, uint32 flags,
									const char* parameters, ino_t* rootID);
	virtual	status_t			Unmount();
	virtual	status_t			Sync();
	virtual	status_t			ReadFSInfo(fs_info* info);
	virtual	status_t			WriteFSInfo(const struct fs_info* info,
									uint32 mask);

	// vnodes
	virtual	status_t			Lookup(void* dir, const char* entryName,
									ino_t* vnid);
	virtual	status_t			GetVNodeName(void* node, char* buffer,
									size_t bufferSize);
	virtual	status_t			ReadVNode(ino_t vnid, bool reenter,
									void** node, int* type, uint32* flags,
									FSVNodeCapabilities* _capabilities);
	virtual	status_t			WriteVNode(void* node, bool reenter);
	virtual	status_t			RemoveVNode(void* node, bool reenter);

	// nodes
	virtual	status_t			SetFlags(void* node, void* cookie,
									int flags);

	virtual	status_t			FSync(void* node);

	virtual	status_t			ReadSymlink(void* node, char* buffer,
									size_t bufferSize, size_t* bytesRead);
	virtual	status_t			CreateSymlink(void* dir, const char* name,
									const char* target, int mode);

	virtual	status_t			Link(void* dir, const char* name,
									void* node);
	virtual	status_t			Unlink(void* dir, const char* name);
	virtual	status_t			Rename(void* oldDir, const char* oldName,
									void* newDir, const char* newName);

	virtual	status_t			Access(void* node, int mode);
	virtual	status_t			ReadStat(void* node, struct stat* st);
	virtual	status_t			WriteStat(void* node, const struct stat* st,
									uint32 mask);

	// files
	virtual	status_t			Create(void* dir, const char* name,
									int openMode, int mode, void** cookie,
									ino_t* vnid);
	virtual	status_t			Open(void* node, int openMode,
									void** cookie);
	virtual	status_t			Close(void* node, void* cookie);
	virtual	status_t			FreeCookie(void* node, void* cookie);
	virtual	status_t			Read(void* node, void* cookie, off_t pos,
									void* buffer, size_t bufferSize,
									size_t* bytesRead);
	virtual	status_t			Write(void* node, void* cookie,
									off_t pos, const void* buffer,
									size_t bufferSize, size_t* bytesWritten);

	// directories
	virtual	status_t			CreateDir(void* dir, const char* name,
									int mode);
	virtual	status_t			RemoveDir(void* dir, const char* name);
	virtual	status_t			OpenDir(void* node, void** cookie);
	virtual	status_t			CloseDir(void* node, void* cookie);
	virtual	status_t			FreeDirCookie(void* node, void* cookie);
	virtual	status_t			ReadDir(void* node, void* cookie,
									void* buffer, size_t bufferSize,
									uint32 count, uint32* countRead);
	virtual	status_t			RewindDir(void* node, void* cookie);

	// attribute directories
	virtual	status_t			OpenAttrDir(void* node, void** cookie);
	virtual	status_t			CloseAttrDir(void* node, void* cookie);
	virtual	status_t			FreeAttrDirCookie(void* node,
									void* cookie);
	virtual	status_t			ReadAttrDir(void* node, void* cookie,
									void* buffer, size_t bufferSize,
									uint32 count, uint32* countRead);
	virtual	status_t			RewindAttrDir(void* node, void* cookie);

private:
	struct DirEntryCache;
	struct DirCookie;
	struct FileCookie;
	struct AttrDirCookie;
	struct ReadDirBuffer;
	struct RWLockableReadLocking;
	struct RWLockableWriteLocking;
	struct RWLockableReadLocker;
	struct RWLockableWriteLocker;
	struct NodeLocker;
	struct NodeReadLocker;
	struct NodeWriteLocker;

	friend struct RWLockableReadLocking;
	friend struct RWLockableWriteLocking;
	friend struct NodeLocker;

private:
	inline	FUSEFileSystem*		_FileSystem() const;

			ino_t				_GenerateNodeID();

			status_t			_GetNode(FUSENode* dir, const char* entryName,
									FUSENode** _node);
			status_t			_InternalGetNode(FUSENode* dir,
									const char* entryName, FUSENode** _node,
									AutoLocker<Locker>& locker);
			void				_PutNode(FUSENode* node);
			void				_PutNodes(FUSENode* const* nodes, int32 count);

			void				_RemoveEntry(FUSEEntry* entry);
			status_t			_RemoveEntry(FUSENode* dir, const char* name);

			status_t			_LockNodeChain(FUSENode* node, bool parent,
									bool writeLock);
			void				_UnlockNodeChain(FUSENode* node, bool parent,
									bool writeLock);

			status_t			_BuildPath(FUSENode* dir, const char* entryName,
									char* path, size_t& pathLen);
			status_t			_BuildPath(FUSENode* node, char* path,
									size_t& pathLen);

	static	int					_AddReadDirEntry(void* buffer, const char* name,
									const struct stat* st, off_t offset);
	static	int					_AddReadDirEntryGetDir(fuse_dirh_t handle,
									const char* name, int type, ino_t nodeID);
			int					_AddReadDirEntry(ReadDirBuffer* buffer,
									const char* name, int type, ino_t nodeID,
									off_t offset);

private:
			RWLockManager		fLockManager;
			Locker				fLock;
			fuse_fs*			fFS;
			FUSEEntryTable		fEntries;
			FUSENodeTable		fNodes;
			FUSENode*			fRootNode;
			ino_t				fNextNodeID;
			bool				fUseNodeIDs;	// TODO: Actually read the
												// option!
			char				fName[B_OS_NAME_LENGTH];
};

}	// namespace UserlandFS

using UserlandFS::FUSEVolume;

#endif	// USERLAND_FS_FUSE_VOLUME_H
