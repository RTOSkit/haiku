//----------------------------------------------------------------------
//  This software is part of the OpenBeOS distribution and is covered 
//  by the OpenBeOS license.
//---------------------------------------------------------------------

#ifndef _DISK_DEVICE_H
#define _DISK_DEVICE_H

class BDiskDevice : public BPartition {
public:
	char* DeviceType() const;
	bool IsRemovable() const;
	bool HasMedia() const;

	status_t Eject(bool update = false);

	status_t Update(bool *updated = NULL);
	void Unset();
	
	bool IsModified() const;
	int32 CommitModifications(bool synchronously = true,
		BMessenger progressMessenger = BMessenger(),
		BMessage *template = NULL);
private:
	friend class BDiskDeviceList;
	friend class BDiskDeviceRoster;

	char		fDeviceType[B_FILE_NAME_LENGTH];
	char		fPath[B_FILE_NAME_LENGTH];

	bool		fIsRemovable;
	status_t	fMediaStatus;
}

#endif	// _DISK_DEVICE_H
