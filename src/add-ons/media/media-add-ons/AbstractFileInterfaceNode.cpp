// AbstractFileInterfaceNode.cpp
//
// Andrew Bachmann, 2002
//
// The AbstractFileInterfaceNode class implements
// the common functionality between MediaReader
// and MediaWriter.

#include <MediaDefs.h>
#include <MediaNode.h>
#include <MediaAddOn.h>
#include <FileInterface.h>
#include <Controllable.h>
#include <MediaEventLooper.h>
#include <File.h>
#include <Errors.h>
#include <Entry.h>
#include <BufferGroup.h>
#include <TimeSource.h>
#include <Buffer.h>
#include <ParameterWeb.h>
#include <limits.h>

#include "AbstractFileInterfaceNode.h"

#include <stdio.h>
#include <string.h>

// -------------------------------------------------------- //
// ctor/dtor
// -------------------------------------------------------- //

AbstractFileInterfaceNode::~AbstractFileInterfaceNode(void)
{
	fprintf(stderr,"AbstractFileInterfaceNode::~AbstractFileInterfaceNode\n");
	// Stop the BMediaEventLooper thread
	Quit();
	if (fCurrentFile != 0) {
		delete fCurrentFile;
	}
}

AbstractFileInterfaceNode::AbstractFileInterfaceNode(
				size_t defaultChunkSize = 8192,
				float defaultBitRate = 800000,
				const flavor_info * info = 0,
				BMessage * config = 0,
				BMediaAddOn * addOn = 0)
	: BMediaNode("AbstractFileInterfaceNode"),
	  BFileInterface(),
	  BControllable(),
	  BMediaEventLooper()
{
	fprintf(stderr,"AbstractFileInterfaceNode::AbstractFileInterfaceNode\n");
	// keep our creator around for AddOn calls later
	fAddOn = addOn;
	// null some fields
	fCurrentFile = 0;
	f_current_mime_type[0] = '\0';
	// initialize the parameters
	if (defaultChunkSize <= 0) {
		fInitCheckStatus = B_BAD_VALUE;
		return;
	}
	fDefaultChunkSizeParam = defaultChunkSize;
	fDefaultChunkSizeParamChangeTime = 0;
	if (defaultBitRate <= 0) {
		fInitCheckStatus = B_BAD_VALUE;
		return;
	}
	fDefaultBitRateParam = defaultBitRate;
	fDefaultBitRateParamChangeTime = 0;
	// initialize parameter compute fields
	fLeastRecentlyUpdatedParameter = DEFAULT_BIT_RATE_PARAM;
	fLastUpdatedParameter = DEFAULT_BUFFER_PERIOD_PARAM;	
	// From the chunk size and bit rate we compute the buffer period.
	int64 value = int64(8000000/1024*fDefaultChunkSizeParam/fDefaultBitRateParam);
	if ((value <= 0) || (value > INT_MAX)) {
		fInitCheckStatus = B_BAD_VALUE;
		return;
	}
	fDefaultBufferPeriodParam = int32(value);
	fDefaultBufferPeriodParamChangeTime = 0;
	
	fInitCheckStatus = B_OK;
}

status_t AbstractFileInterfaceNode::InitCheck(void) const
{
	fprintf(stderr,"AbstractFileInterfaceNode::InitCheck\n");
	return fInitCheckStatus;
}

status_t AbstractFileInterfaceNode::GetConfigurationFor(
				BMessage * into_message)
{
	fprintf(stderr,"AbstractFileInterfaceNode::GetConfigurationFor\n");
	return B_OK;
}

// -------------------------------------------------------- //
// implementation of BMediaNode
// -------------------------------------------------------- //

BMediaAddOn * AbstractFileInterfaceNode::AddOn(
				int32 * internal_id) const
{
	fprintf(stderr,"AbstractFileInterfaceNode::AddOn\n");
	// BeBook says this only gets called if we were in an add-on.
	if (fAddOn != 0) {
		// If we get a null pointer then we just won't write.
		if (internal_id != 0) {
			internal_id = 0;
		}
	}
	return fAddOn;
}

void AbstractFileInterfaceNode::Start(
				bigtime_t performance_time)
{
	fprintf(stderr,"AbstractFileInterfaceNode::Start(pt=%i)\n",performance_time);
	BMediaEventLooper::Start(performance_time);
}

void AbstractFileInterfaceNode::Stop(
				bigtime_t performance_time,
				bool immediate)
{
	fprintf(stderr,"AbstractFileInterfaceNode::Stop(pt=%i,%s)\n",performance_time,(immediate?"now":"then"));
	BMediaEventLooper::Stop(performance_time,immediate);
}

void AbstractFileInterfaceNode::Seek(
				bigtime_t media_time,
				bigtime_t performance_time)
{
	fprintf(stderr,"AbstractFileInterfaceNode::Seek(mt=%i,pt=%i)\n",media_time,performance_time);
	BMediaEventLooper::Seek(media_time,performance_time);
}

void AbstractFileInterfaceNode::SetRunMode(
				run_mode mode)
{
	fprintf(stderr,"AbstractFileInterfaceNode::SetRunMode(%i)\n",mode);
	BMediaEventLooper::SetRunMode(mode);
}

void AbstractFileInterfaceNode::TimeWarp(
				bigtime_t at_real_time,
				bigtime_t to_performance_time)
{
	fprintf(stderr,"AbstractFileInterfaceNode::TimeWarp(rt=%i,pt=%i)\n",at_real_time,to_performance_time);
	BMediaEventLooper::TimeWarp(at_real_time,to_performance_time);
}

void AbstractFileInterfaceNode::Preroll(void)
{
	fprintf(stderr,"AbstractFileInterfaceNode::Preroll\n");
	// XXX:Performance opportunity
	BMediaNode::Preroll();
}

void AbstractFileInterfaceNode::SetTimeSource(
				BTimeSource * time_source)
{
	fprintf(stderr,"AbstractFileInterfaceNode::SetTimeSource\n");
	BMediaNode::SetTimeSource(time_source);
}

status_t AbstractFileInterfaceNode::HandleMessage(
				int32 message,
				const void * data,
				size_t size)
{
	fprintf(stderr,"AbstractFileInterfaceNode::HandleMessage\n");
	status_t status = B_OK;
	switch (message) {
		// no special messages for now
		default:
			status = BFileInterface::HandleMessage(message,data,size);
			if (status == B_OK) {
				break;
			}
			status = BMediaNode::HandleMessage(message,data,size);
			if (status == B_OK) {
				break;
			}
			BMediaNode::HandleBadMessage(message,data,size);
			status = B_ERROR;
			break;
	}
	return status;
}

status_t AbstractFileInterfaceNode::RequestCompleted(
				const media_request_info & info)
{
	fprintf(stderr,"AbstractFileInterfaceNode::RequestCompleted\n");
	return BMediaNode::RequestCompleted(info);
}

status_t AbstractFileInterfaceNode::DeleteHook(
				BMediaNode * node)
{
	fprintf(stderr,"AbstractFileInterfaceNode::DeleteHook\n");
	return BMediaEventLooper::DeleteHook(node);
}

void AbstractFileInterfaceNode::NodeRegistered(void)
{
	fprintf(stderr,"AbstractFileInterfaceNode::NodeRegistered\n");
	
	// set up our parameter web
	SetParameterWeb(MakeParameterWeb());
	
	// start the BMediaEventLooper thread
	SetPriority(B_REAL_TIME_PRIORITY);
	Run();
}

status_t AbstractFileInterfaceNode::GetNodeAttributes(
				media_node_attribute * outAttributes,
				size_t inMaxCount)
{
	fprintf(stderr,"AbstractFileInterfaceNode::GetNodeAttributes\n");
	return BMediaNode::GetNodeAttributes(outAttributes,inMaxCount);
}

status_t AbstractFileInterfaceNode::AddTimer(
					bigtime_t at_performance_time,
					int32 cookie)
{
	fprintf(stderr,"AbstractFileInterfaceNode::AddTimer\n");
	return BMediaEventLooper::AddTimer(at_performance_time,cookie);
}

// protected:

BParameterWeb * AbstractFileInterfaceNode::MakeParameterWeb(void)
{
	fprintf(stderr,"AbstractFileInterfaceNode::MakeParameterWeb\n");
	
	BParameterWeb * web = new BParameterWeb();
	BParameterGroup * mainGroup = web->MakeGroup("AbstractFileInterfaceNode Parameters");

	// these three are related:
	// DEFAULT_CHUNK_SIZE_PARAM = DEFAULT_BIT_RATE_PARAM / 1024 * DEFAULT_BUFFER_PERIOD_PARAM * 1000
	BParameterGroup * chunkSizeGroup = mainGroup->MakeGroup("Chunk Size Group");
	BContinuousParameter * chunkSizeParameter
	   = chunkSizeGroup->MakeContinuousParameter(
	     DEFAULT_CHUNK_SIZE_PARAM, B_MEDIA_MULTISTREAM,
		 "Chunk Size", B_GAIN, "bytes", 1024, 32*1024, 512);
	chunkSizeParameter->SetResponse(BContinuousParameter::B_LINEAR,1,0);
	chunkSizeParameter->SetValue(&fDefaultChunkSizeParam,sizeof(fDefaultChunkSizeParam),0);
	
	BParameterGroup * bitRateGroup = mainGroup->MakeGroup("Bit Rate Group");
	BContinuousParameter * bitRateParameter
	   = bitRateGroup->MakeContinuousParameter(
	     DEFAULT_BIT_RATE_PARAM, B_MEDIA_MULTISTREAM,
	     "Bit Rate", B_GAIN, "kbits/sec", 1, 320000, 1);
	bitRateParameter->SetResponse(BContinuousParameter::B_LINEAR,.001,0);
	bitRateParameter->SetValue(&fDefaultBitRateParam,sizeof(fDefaultBitRateParam),0);
	
	BParameterGroup * bufferPeriodGroup = mainGroup->MakeGroup("Buffer Period Group");
	BContinuousParameter * bufferPeriodParameter
	   = bufferPeriodGroup->MakeContinuousParameter(
	     DEFAULT_BUFFER_PERIOD_PARAM, B_MEDIA_MULTISTREAM,
	     "Buffer Period", B_GAIN, "ms", 1, 10000, 1);
	bufferPeriodParameter->SetResponse(BContinuousParameter::B_LINEAR,1,0);
	bufferPeriodParameter->SetValue(&fDefaultBufferPeriodParam,sizeof(fDefaultBufferPeriodParam),0);
	
	return web;
}

// -------------------------------------------------------- //
// implementation of BFileInterface
// -------------------------------------------------------- //

status_t AbstractFileInterfaceNode::GetNextFileFormat(
				int32 * cookie,
				media_file_format * out_format)
{
	fprintf(stderr,"AbstractFileInterfaceNode::GetNextFileFormat\n");
	// let's not crash even if they are stupid
	if (out_format == 0) {
		// no place to write!
		fprintf(stderr,"<- B_BAD_VALUE\n");
		return B_BAD_VALUE;
	}
	if (cookie != 0) {
		// it's valid but they already got our 1 file format
		if (*cookie != 0) {
			fprintf(stderr,"<- B_ERROR\n");
			return B_ERROR;
		}
		// so next time they won't get the same format again
		*cookie = 1;
	}
	GetFileFormat(out_format);
	return B_OK;
}

void AbstractFileInterfaceNode::DisposeFileFormatCookie(
				int32 cookie)
{
	fprintf(stderr,"AbstractFileInterfaceNode::DisposeFileFormatCookie\n");
	// nothing to do since our cookies are just integers
}

status_t AbstractFileInterfaceNode::GetDuration(
				bigtime_t * out_time)
{
	fprintf(stderr,"AbstractFileInterfaceNode::GetDuration\n");
	if (out_time == 0) {
		fprintf(stderr,"<- B_BAD_VALUE\n");
		return B_BAD_VALUE;
	}
	if (fCurrentFile == 0) {
		fprintf(stderr,"<- B_NO_INIT\n");
		return B_NO_INIT;
	}	
	return fCurrentFile->GetSize(out_time);
}

status_t AbstractFileInterfaceNode::SniffRef(
				const entry_ref & file,
				char * out_mime_type,	/* 256 bytes */
				float * out_quality)
{
	fprintf(stderr,"AbstractFileInterfaceNode::SniffRef\n");
	return StaticSniffRef(file,out_mime_type,out_quality);
}

status_t AbstractFileInterfaceNode::SetRef(
				const entry_ref & file,
				uint32 openMode,
				bool create,
				bigtime_t * out_time)
{
	fprintf(stderr,"AbstractFileInterfaceNode::SetRef\n");
	if (out_time == 0) {
		fprintf(stderr,"<- B_BAD_VALUE\n");
		return B_BAD_VALUE; // no crashes today thanks
	}
	status_t status;
	f_current_ref = file;
	if (fCurrentFile == 0) {
		fCurrentFile = new BFile(&f_current_ref,(openMode|(create?B_CREATE_FILE:0)));
		status = fCurrentFile->InitCheck();
	} else {
		status = fCurrentFile->SetTo(&f_current_ref,(openMode|(create?B_CREATE_FILE:0)));
	}
	if (status != B_OK) {
		fprintf(stderr,"<- failed BFile initialization\n");
		return status;
	}
	// cache the mime type for later
	fCurrentFile->ReadAttr("BEOS:TYPE",0,0,f_current_mime_type,B_MIME_TYPE_LENGTH);
	// compute the duration and return any error
	return GetDuration(out_time);
}

status_t AbstractFileInterfaceNode::GetRef(
				entry_ref * out_ref,
				char * out_mime_type)
{
	fprintf(stderr,"AbstractFileInterfaceNode::GetRef\n");
	if ((out_ref == 0) || (out_mime_type == 0)) {
		fprintf(stderr,"<- B_BAD_VALUE\n");
		return B_BAD_VALUE; // no crashes today thanks
	}
	if (fCurrentFile == 0) {
		fprintf(stderr,"<- B_NO_INIT\n");
		return B_NO_INIT; // the input_ref isn't valid yet either
	}
	*out_ref = f_current_ref;
	// they hopefully allocated enough space (no way to check :-/ )
	strcpy(out_mime_type,f_current_mime_type);
	return B_OK;
}

// provided for BAbstractFileInterfaceNodeAddOn

status_t AbstractFileInterfaceNode::StaticSniffRef(
				const entry_ref & file,
				char * out_mime_type,	/* 256 bytes */
				float * out_quality)
{
	fprintf(stderr,"AbstractFileInterfaceNode::StaticSniffRef\n");
	if ((out_mime_type == 0) || (out_quality == 0)) {
		fprintf(stderr,"<- B_BAD_VALUE\n");
		return B_BAD_VALUE; // we refuse to crash because you were stupid
	}	
	BNode node(&file);
	status_t initCheck = node.InitCheck();
	if (initCheck != B_OK) {
		fprintf(stderr,"<- failed BNode::InitCheck()\n");
		return initCheck;
	}
	// they hopefully allocated enough room
	node.ReadAttr("BEOS:TYPE",0,0,out_mime_type,B_MIME_TYPE_LENGTH);
	*out_quality = 1.0; // we handle all files perfectly!  we are so amazing!
	return B_OK;
}

// -------------------------------------------------------- //
// implementation for BControllable
// -------------------------------------------------------- //

const int32 AbstractFileInterfaceNode::DEFAULT_CHUNK_SIZE_PARAM = 1;
const int32 AbstractFileInterfaceNode::DEFAULT_BIT_RATE_PARAM = 2;
const int32 AbstractFileInterfaceNode::DEFAULT_BUFFER_PERIOD_PARAM = 3;

status_t AbstractFileInterfaceNode::GetParameterValue(
				int32 id,
				bigtime_t * last_change,
				void * value,
				size_t * ioSize)
{
	fprintf(stderr,"AbstractFileInterfaceNode::GetParameterValue\n");
	if ((last_change == 0) || (value == 0) || (ioSize == 0)) {
		return B_BAD_VALUE; // no crashing
	}	
	switch (id) {
		case DEFAULT_CHUNK_SIZE_PARAM:
			if (*ioSize < sizeof(size_t)) {
				return B_ERROR; // not enough room
			}
			*last_change = fDefaultChunkSizeParamChangeTime;
			*((size_t*)value) = fDefaultChunkSizeParam;
			*ioSize = sizeof(size_t);
			break;
			
		case DEFAULT_BIT_RATE_PARAM:
			if (*ioSize < sizeof(float)) {
				return B_ERROR; // not enough room
			}
			*last_change = fDefaultBitRateParamChangeTime;
			*((float*)value) = fDefaultBitRateParam;
			*ioSize = sizeof(float);
			break;
		
		case DEFAULT_BUFFER_PERIOD_PARAM:
			if (*ioSize < sizeof(int32)) {
				return B_ERROR; // not enough room
			}
			*last_change = fDefaultBufferPeriodParamChangeTime;
			*((int32*)value) = fDefaultBufferPeriodParam;
			*ioSize = sizeof(int32);
			break;
				
		default:
			fprintf(stderr,"AbstractFileInterfaceNode::GetParameterValue unknown id (%i)\n",id);
			return B_ERROR;
	}
	return B_OK;			
}
				
void AbstractFileInterfaceNode::SetParameterValue(
				int32 id,
				bigtime_t when,
				const void * value,
				size_t size)
{
	fprintf(stderr,"AbstractFileInterfaceNode::SetParameterValue(id=%i,when=%ld,size=%i)\n",id,int32(when),int32(size));
	switch (id) {
		case DEFAULT_CHUNK_SIZE_PARAM:
		case DEFAULT_BIT_RATE_PARAM:
		case DEFAULT_BUFFER_PERIOD_PARAM:
			{
				media_timed_event event(when, BTimedEventQueue::B_PARAMETER,
										NULL, BTimedEventQueue::B_NO_CLEANUP,
										size, id, (char*) value, size);
				EventQueue()->AddEvent(event);
			}
			break;
			
		default:
			fprintf(stderr,"AbstractFileInterfaceNode::SetParameterValue unknown id (%i)\n",id);
			break;
	}
}			

// the default implementation should call the add-on main()
status_t AbstractFileInterfaceNode::StartControlPanel(
				BMessenger * out_messenger)
{
	BControllable::StartControlPanel(out_messenger);
}

// -------------------------------------------------------- //
// implementation for BMediaEventLooper
// -------------------------------------------------------- //

void AbstractFileInterfaceNode::HandleEvent(
				const media_timed_event *event,
				bigtime_t lateness,
				bool realTimeEvent = false)
{
	fprintf(stderr,"AbstractFileInterfaceNode::HandleEvent\n");
	switch (event->type) {
		case BTimedEventQueue::B_START:
			HandleStart(event,lateness,realTimeEvent);
			break;
		case BTimedEventQueue::B_SEEK:
			HandleSeek(event,lateness,realTimeEvent);
			break;
		case BTimedEventQueue::B_WARP:
			HandleWarp(event,lateness,realTimeEvent);
			break;
		case BTimedEventQueue::B_STOP:
			HandleStop(event,lateness,realTimeEvent);
			break;
		case BTimedEventQueue::B_HANDLE_BUFFER:
			if (RunState() == BMediaEventLooper::B_STARTED) {
				HandleBuffer(event,lateness,realTimeEvent);
			}
			break;
		case BTimedEventQueue::B_DATA_STATUS:
			HandleDataStatus(event,lateness,realTimeEvent);
			break;
		case BTimedEventQueue::B_PARAMETER:
			HandleParameter(event,lateness,realTimeEvent);
			break;
		default:
			fprintf(stderr,"  unknown event type: %i\n",event->type);
			break;
	}
}

/* override to clean up custom events you have added to your queue */
void AbstractFileInterfaceNode::CleanUpEvent(
				const media_timed_event *event)
{
	BMediaEventLooper::CleanUpEvent(event);
}
		
/* called from Offline mode to determine the current time of the node */
/* update your internal information whenever it changes */
bigtime_t AbstractFileInterfaceNode::OfflineTime()
{
	fprintf(stderr,"AbstractFileInterfaceNode::OfflineTime\n");
	return BMediaEventLooper::OfflineTime();
//	if (inputFile == 0) {
//		return 0;
//	} else {
//		return inputFile->Position();
//	}
}

/* override only if you know what you are doing! */
/* otherwise much badness could occur */
/* the actual control loop function: */
/* 	waits for messages, Pops events off the queue and calls DispatchEvent */
void AbstractFileInterfaceNode::ControlLoop() {
	BMediaEventLooper::ControlLoop();
}

// protected:

status_t AbstractFileInterfaceNode::HandleStart(
						const media_timed_event *event,
						bigtime_t lateness,
						bool realTimeEvent = false)
{
	fprintf(stderr,"MediaReader::HandleStart()\n");
	if (RunState() != B_STARTED) {
		HandleBuffer(event,lateness,realTimeEvent);
//		media_timed_event firstBufferEvent(event->event_time, BTimedEventQueue::B_HANDLE_BUFFER);
//		//this->HandleEvent(&firstBufferEvent, 0, false);
//		EventQueue()->AddEvent(firstBufferEvent);
	}
}

status_t AbstractFileInterfaceNode::HandleSeek(
						const media_timed_event *event,
						bigtime_t lateness,
						bool realTimeEvent = false)
{
	// XXX: argghh.. seek events seem broken when received from
	//      the usual BMediaEventLooper::Seek() dispatcher :-(
	//      They lost where we are supposed to seek to!?!
	fprintf(stderr,"AbstractFileInterfaceNode::HandleSeek(t=%i,d=%i,bd=%ld)\n",event->event_time,event->data,event->bigdata);
	if (fCurrentFile != 0) {
		fCurrentFile->Seek(event->bigdata,SEEK_SET);
	}
}
						
status_t AbstractFileInterfaceNode::HandleWarp(
						const media_timed_event *event,
						bigtime_t lateness,
						bool realTimeEvent = false)
{
	fprintf(stderr,"AbstractFileInterfaceNode::HandleWarp\n");
}

status_t AbstractFileInterfaceNode::HandleStop(
						const media_timed_event *event,
						bigtime_t lateness,
						bool realTimeEvent = false)
{
	fprintf(stderr,"AbstractFileInterfaceNode::HandleStop\n");
	// flush the queue so downstreamers don't get any more
	EventQueue()->FlushEvents(0, BTimedEventQueue::B_ALWAYS, true, BTimedEventQueue::B_HANDLE_BUFFER);
}

status_t AbstractFileInterfaceNode::HandleParameter(
				const media_timed_event *event,
				bigtime_t lateness,
				bool realTimeEvent = false)
{
	fprintf(stderr,"AbstractFileInterfaceNode::HandleParameter");
	status_t status = B_OK;
	
	bool chunkSizeUpdated = false, bitRateUpdated = false, bufferPeriodUpdated = false;
	
	size_t dataSize = size_t(event->data);
	int32 param = int32(event->bigdata);
	
	switch (param) {
		case DEFAULT_CHUNK_SIZE_PARAM:
			fprintf(stderr,"(DEFAULT_CHUNK_SIZE_PARAM,size=%i",dataSize);
			if (dataSize < sizeof(size_t)) {
				fprintf(stderr,")\n");
				fprintf(stderr,"<- B_BAD_VALUE\n",param);
				status = B_BAD_VALUE;
			} else {
				size_t newDefaultChunkSize = *((size_t*)event->user_data);
				fprintf(stderr,",%i)\n",newDefaultChunkSize);
				// ignore non positive chunk sizes
				// XXX: we may decide later that a 0 chunk size means ship the whole file in one chunk (!)
				if ((newDefaultChunkSize > 0) && (newDefaultChunkSize != fDefaultChunkSizeParam)) {
					fprintf(stderr,"  got a new chunk size, old chunk size was %i\n",fDefaultChunkSizeParam);
					fDefaultChunkSizeParam = newDefaultChunkSize;
					fDefaultChunkSizeParamChangeTime = TimeSource()->Now();
					chunkSizeUpdated = true;
					if (fLeastRecentlyUpdatedParameter == DEFAULT_CHUNK_SIZE_PARAM) {
						// Okay we were the least recently updated parameter,
						// but we just got an update so we are no longer that.
						// Let's figure out who the new least recently updated
						// parameter is.  We are going to prefer to compute the
						// bit rate since you usually don't want to muck with
						// the buffer period.  However, if you just set the bitrate
						// then we are stuck with making the buffer period the new
						// parameter to be computed.
						if (fLastUpdatedParameter == DEFAULT_BIT_RATE_PARAM) {
							fLeastRecentlyUpdatedParameter = DEFAULT_BUFFER_PERIOD_PARAM;
						} else {
							fLeastRecentlyUpdatedParameter = DEFAULT_BIT_RATE_PARAM;
						}
					}
					// we just got an update, so we are the new lastUpdatedParameter
					fLastUpdatedParameter = DEFAULT_CHUNK_SIZE_PARAM;
					// now we have to compute the new value for the leastRecentlyUpdatedParameter
					// we use the chunk size change time to preserve "simultaneity" information
					if (fLeastRecentlyUpdatedParameter == DEFAULT_BUFFER_PERIOD_PARAM) {
						int64 value = int64(8000000/1024*fDefaultChunkSizeParam/fDefaultBitRateParam);
						if (value > INT_MAX) {
							// clamp to INT_MAX
							fDefaultBufferPeriodParam = INT_MAX;
							// recompute chunk size
							fDefaultChunkSizeParam = size_t(1024/8000000*fDefaultBitRateParam*fDefaultBufferPeriodParam);
						} else {
							fDefaultBufferPeriodParam = MAX(1,value);
						}							
						fDefaultBufferPeriodParamChangeTime = fDefaultChunkSizeParamChangeTime;
						bufferPeriodUpdated = true;
					} else { // must have been bit rate
						fDefaultBitRateParam = MAX(0.001,8000000/1024*fDefaultChunkSizeParam/fDefaultBufferPeriodParam);
						fDefaultBitRateParamChangeTime = fDefaultChunkSizeParamChangeTime;
						bitRateUpdated = true;
					}
				}
			}
			break;		
		case DEFAULT_BIT_RATE_PARAM:
			fprintf(stderr,"(DEFAULT_BIT_RATE_PARAM,size=%i",dataSize);
			if (dataSize < sizeof(float)) {
				fprintf(stderr,")\n");
				fprintf(stderr,"<- B_BAD_VALUE\n",param);
				status = B_BAD_VALUE;
			} else {
				float newDefaultBitRate = *((float*)event->user_data);
				fprintf(stderr,",%f)\n",newDefaultBitRate);
				// ignore non positive bitrates
				if ((newDefaultBitRate > 0) && (newDefaultBitRate != fDefaultBitRateParam)) {
					fprintf(stderr,"  got a new bit rate, old bit rate was %i\n",fDefaultBitRateParam);
					fDefaultBitRateParam = newDefaultBitRate;
					fDefaultBitRateParamChangeTime = TimeSource()->Now();
					bitRateUpdated = true;
					if (fLeastRecentlyUpdatedParameter == DEFAULT_BIT_RATE_PARAM) {
						// Okay we were the least recently updated parameter,
						// but we just got an update so we are no longer that.
						// Let's figure out who the new least recently updated
						// parameter is.  We are going to prefer to compute the
						// chunk size since you usually don't want to muck with
						// the buffer period.  However, if you just set the chunk size
						// then we are stuck with making the buffer period the new
						// parameter to be computed.
						if (fLastUpdatedParameter == DEFAULT_CHUNK_SIZE_PARAM) {
							fLeastRecentlyUpdatedParameter = DEFAULT_BUFFER_PERIOD_PARAM;
						} else {
							fLeastRecentlyUpdatedParameter = DEFAULT_CHUNK_SIZE_PARAM;
						}
					}
					// we just got an update, so we are the new lastUpdatedParameter
					fLastUpdatedParameter = DEFAULT_BIT_RATE_PARAM;
					// now we have to compute the new value for the leastRecentlyUpdatedParameter
					// we use the bit rate change time to preserve "simultaneity" information
					if (fLeastRecentlyUpdatedParameter == DEFAULT_BUFFER_PERIOD_PARAM) {
						int64 value = int64(8000000/1024*fDefaultChunkSizeParam/fDefaultBitRateParam);
						if (value > INT_MAX) {
							// clamp to INT_MAX
							fDefaultBufferPeriodParam = INT_MAX;
							// recompute bit rate
							fDefaultBitRateParam = MAX(0.001,8000000/1024*fDefaultChunkSizeParam/fDefaultBufferPeriodParam);
						} else {
							fDefaultBufferPeriodParam = MAX(1,int32(value));
						}
						fDefaultBufferPeriodParamChangeTime = fDefaultBitRateParamChangeTime;
						bufferPeriodUpdated = true;
					} else { // must have been chunk size
						int64 value = int64(1024/8000000*fDefaultBitRateParam*fDefaultBufferPeriodParam);
						if (value > INT_MAX) {
							// clamp to INT_MAX
							fDefaultChunkSizeParam = INT_MAX;
							// recompute bit rate
							fDefaultBitRateParam = MAX(0.001,8000000/1024*fDefaultChunkSizeParam/fDefaultBufferPeriodParam);
						} else {
							fDefaultChunkSizeParam = MAX(1,int32(value));
						}
						fDefaultChunkSizeParamChangeTime = fDefaultBitRateParamChangeTime;
						chunkSizeUpdated = true;
					}
				}
			}
			break;		
		case DEFAULT_BUFFER_PERIOD_PARAM:
			fprintf(stderr,"(DEFAULT_BUFFER_PERIOD_PARAM,size=%i",dataSize);
			if (dataSize < sizeof(int32)) {
				fprintf(stderr,")\n");
				fprintf(stderr,"<- B_BAD_VALUE\n",param);
				status = B_BAD_VALUE;
			} else {
				int32 newBufferPeriod = *((int32*)event->user_data);
				fprintf(stderr,",%i)\n",newBufferPeriod);
				// ignore non positive buffer period
				if ((newBufferPeriod > 0) && (newBufferPeriod != fDefaultBufferPeriodParam)) {
					fprintf(stderr,"  got a new buffer period, old buffer period was %i\n",fDefaultBufferPeriodParam);
					fDefaultBufferPeriodParam = newBufferPeriod;
					fDefaultBufferPeriodParamChangeTime = TimeSource()->Now();
					bufferPeriodUpdated = true;
					if (fLastUpdatedParameter == DEFAULT_BUFFER_PERIOD_PARAM) {
						// prefer to update bit rate, unless you just set it
						if (fLastUpdatedParameter == DEFAULT_BIT_RATE_PARAM) {
							fLeastRecentlyUpdatedParameter = DEFAULT_CHUNK_SIZE_PARAM;
						} else {
							fLeastRecentlyUpdatedParameter = DEFAULT_BIT_RATE_PARAM;
						}
					}
					// we just got an update, so we are the new lastUpdatedParameter
					fLastUpdatedParameter = DEFAULT_BUFFER_PERIOD_PARAM;
					// now we have to compute the new value for the leastRecentlyUpdatedParameter
					// we use the buffer period change time to preserve "simultaneity" information
					if (fLeastRecentlyUpdatedParameter == DEFAULT_BIT_RATE_PARAM) {
						fDefaultBitRateParam = MAX(0.001,8000000/1024*fDefaultChunkSizeParam/fDefaultBufferPeriodParam);
						fDefaultBitRateParamChangeTime = fDefaultBufferPeriodParamChangeTime;
						bitRateUpdated = true;
					} else { // must have been chunk size
						int64 value = int64(1024/8000000*fDefaultBitRateParam*fDefaultBufferPeriodParam);
						if (value > INT_MAX) {
							// clamp to INT_MAX
							fDefaultChunkSizeParam = INT_MAX;
							// recompute buffer period
							fDefaultBufferPeriodParam = size_t(8000000/1024*fDefaultChunkSizeParam/fDefaultBitRateParam);
						} else {
							fDefaultChunkSizeParam = MAX(1,int32(value));
						}
						fDefaultChunkSizeParamChangeTime = fDefaultBufferPeriodParamChangeTime;
						chunkSizeUpdated = true;
					}					
				}
			}
			break;	
		default:
			fprintf(stderr,"AbstractFileInterfaceNode::HandleParameter called with unknown param id (%i)\n",param);
			status = B_ERROR;
	}
	// send updates out for all the parameters that changed
	// in every case this should be two updates. (if I have not made an error :-) )
	if (chunkSizeUpdated) {
		fprintf(stderr,"  chunk size parameter updated\n");
		BroadcastNewParameterValue(fDefaultChunkSizeParamChangeTime,
								   DEFAULT_CHUNK_SIZE_PARAM,
								   &fDefaultChunkSizeParam,
								   sizeof(fDefaultChunkSizeParam));
	}
	if (bitRateUpdated) {
		fprintf(stderr,"  bit rate parameter updated\n");
		BroadcastNewParameterValue(fDefaultBitRateParamChangeTime,
								   DEFAULT_BIT_RATE_PARAM,
								   &fDefaultBitRateParam,
								   sizeof(fDefaultBitRateParam));
	}
	if (bufferPeriodUpdated) {
		fprintf(stderr,"  buffer period parameter updated\n");
		BroadcastNewParameterValue(fDefaultBufferPeriodParamChangeTime,
								   DEFAULT_BUFFER_PERIOD_PARAM,
								   &fDefaultBufferPeriodParam,
								   sizeof(fDefaultBufferPeriodParam));
	}
	return status;
}

// -------------------------------------------------------- //
// AbstractFileInterfaceNode specific functions
// -------------------------------------------------------- //

// public:

void AbstractFileInterfaceNode::GetFlavor(flavor_info * info, int32 id)
{
	fprintf(stderr,"AbstractFileInterfaceNode::GetFlavor\n");
	if (info == 0) {
		return;
	}
	info->name = "AbstractFileInterfaceNode";
	info->info = "A AbstractFileInterfaceNode node handles a file.";
	info->kinds = B_FILE_INTERFACE | B_CONTROLLABLE;
	info->flavor_flags = B_FLAVOR_IS_LOCAL;
	info->possible_count = INT_MAX;
	info->in_format_count = 0; // no inputs
	info->in_formats = 0;
	info->out_format_count = 0; // no outputs
	info->out_formats = 0;
	info->internal_id = id;
	return;
}

void AbstractFileInterfaceNode::GetFormat(media_format * outFormat)
{
	fprintf(stderr,"AbstractFileInterfaceNode::GetFormat\n");
	if (outFormat == 0) {
		return;
	}
	outFormat->type = B_MEDIA_MULTISTREAM;
	outFormat->require_flags = B_MEDIA_MAUI_UNDEFINED_FLAGS;
	outFormat->deny_flags = B_MEDIA_MAUI_UNDEFINED_FLAGS;	
	outFormat->u.multistream = media_multistream_format::wildcard;
}

void AbstractFileInterfaceNode::GetFileFormat(media_file_format * outFileFormat)
{
	fprintf(stderr,"AbstractFileInterfaceNode::GetFileFormat\n");
	if (outFileFormat == 0) {
		return;
	}
	outFileFormat->capabilities =
			  media_file_format::B_PERFECTLY_SEEKABLE
			| media_file_format::B_IMPERFECTLY_SEEKABLE
			| media_file_format::B_KNOWS_ANYTHING;
	/* I don't know what to initialize this to. (or if I should) */
	// format.id =
	outFileFormat->family = B_ANY_FORMAT_FAMILY;
	outFileFormat->version = 100;
	// see media_file_format in <MediaDefs.h> for limits
	strncpy(outFileFormat->mime_type,"",63);
	outFileFormat->mime_type[63]='\0';
	strncpy(outFileFormat->pretty_name,"any media file format",63);
	outFileFormat->pretty_name[63]='\0';
	strncpy(outFileFormat->short_name,"any",31);
	outFileFormat->short_name[31]='\0';
	strncpy(outFileFormat->file_extension,"",7);
	outFileFormat->file_extension[7]='\0';	
}

// protected:

// Here we make some guesses based on the file's mime type.
// We don't have enough information to add any other requirements.
// This function doesn't complain if you have already decided you want
// the stream to be considered a different one. (so you can say that you
// want to read that mpeg file as avi if you are so nutty.)
//
status_t AbstractFileInterfaceNode::AddRequirements(media_format * format)
{
	if (strcmp("video/x-msvideo",f_current_mime_type) == 0) {
		if (format->u.multistream.format == media_multistream_format::wildcard.format) {
			format->u.multistream.format = media_multistream_format::B_AVI;
		}
	} else
	if (strcmp("video/mpeg",f_current_mime_type) == 0) {
		if (format->u.multistream.format == media_multistream_format::wildcard.format) {
			format->u.multistream.format = media_multistream_format::B_MPEG1;
		}
	} else
	if (strcmp("video/quicktime",f_current_mime_type) == 0) {
		if (format->u.multistream.format == media_multistream_format::wildcard.format) {
			format->u.multistream.format = media_multistream_format::B_QUICKTIME;
		}
	} else
	if (strcmp("audio/x-mpeg",f_current_mime_type) == 0) {
		if (format->u.multistream.format == media_multistream_format::wildcard.format) {
			format->u.multistream.format = media_multistream_format::B_MPEG1;
		}
	}
	return B_OK;
}

// We need some sort of bit rate and chunk size, so if the other guy
// didn't care, we'll use our own defaults.
status_t AbstractFileInterfaceNode::ResolveWildcards(media_format * format)
{
	fprintf(stderr,"AbstractFileInterfaceNode::ResolveWildcards\n");
// There isn't an unknown format. hmph.
//	if (format->u.multistream.format == media_multistream_format::wildcard.format) {
//		format->u.multistream.format = media_multistream_format::B_UNKNOWN;
//	}
	if (format->u.multistream.max_bit_rate == media_multistream_format::wildcard.max_bit_rate) {
		format->u.multistream.max_bit_rate = fDefaultBitRateParam;
	}
	if (format->u.multistream.max_chunk_size == media_multistream_format::wildcard.max_chunk_size) {
		format->u.multistream.max_chunk_size = fDefaultChunkSizeParam;
	}
	if (format->u.multistream.avg_bit_rate == media_multistream_format::wildcard.avg_bit_rate) {
		format->u.multistream.avg_bit_rate = fDefaultBitRateParam;
	}
	if (format->u.multistream.avg_chunk_size == media_multistream_format::wildcard.avg_chunk_size) {
		format->u.multistream.avg_chunk_size = fDefaultChunkSizeParam;
	}
	return B_OK;
}

// -------------------------------------------------------- //
// stuffing
// -------------------------------------------------------- //

status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_0(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_1(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_2(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_3(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_4(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_5(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_6(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_7(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_8(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_9(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_10(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_11(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_12(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_13(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_14(void *) {}
status_t AbstractFileInterfaceNode::_Reserved_AbstractFileInterfaceNode_15(void *) {}
