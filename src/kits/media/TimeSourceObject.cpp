/***********************************************************************
 * Copyright (c) 2002 Marcus Overhagen. All Rights Reserved.
 * This file may be used under the terms of the OpenBeOS License.
 *
 * The object returned by BMediaRoster's
 * BTimeSource * MakeTimeSourceFor(const media_node & for_node);
 *
 ***********************************************************************/

#include <OS.h>
#include <stdio.h>
#include <MediaRoster.h>
#include "TimeSourceObject.h"
#include "TimeSourceObjectManager.h"

TimeSourceObject::TimeSourceObject(media_node_id id) :
	BMediaNode("some timesource object", id, B_TIME_SOURCE),
	BTimeSource(id)
{
	printf("TimeSourceObject::TimeSourceObject enter, id = %ld\n", id);
	delete_port(fControlPort);
	fControlPort = -999666; // must match value in BMediaNode::~BMediaNode()
	ASSERT(id == fNodeID);
	printf("TimeSourceObject::TimeSourceObject leave, node id %ld\n", fNodeID);
}

/* virtual */ status_t
TimeSourceObject::SnoozeUntil(
				bigtime_t performance_time,
				bigtime_t with_latency,
				bool retry_signals)
{
	bigtime_t time = performance_time - with_latency;
	status_t err;
	do {
		err = snooze_until(time, B_SYSTEM_TIMEBASE);
	} while (err == B_INTERRUPTED && retry_signals);
	return err;
}

/* virtual */ status_t
TimeSourceObject::TimeSourceOp(
				const time_source_op_info & op,
				void * _reserved)
{
	return B_OK;
}

/* virtual */ BMediaAddOn* 
TimeSourceObject::AddOn(int32 *internal_id) const
{
	if (internal_id)
		*internal_id = 0;
	return NULL;
}

/* virtual */ status_t
TimeSourceObject::DeleteHook(BMediaNode * node)
{
	printf("TimeSourceObject::DeleteHook enter\n");
	_TimeSourceObjectManager->ObjectDeleted(this);
	BTimeSource::DeleteHook(node);
	printf("TimeSourceObject::DeleteHook leave\n");
}


SystemTimeSourceObject::SystemTimeSourceObject(media_node_id id)
 : 	BMediaNode("System Time Source", id, B_TIME_SOURCE),
	TimeSourceObject(id)
{
	printf("SystemTimeSourceObject::SystemTimeSourceObject enter, id = %ld\n", id);

	printf("SystemTimeSourceObject::SystemTimeSourceObject leave, node id %ld\n", ID());
}

/* virtual */ status_t
SystemTimeSourceObject::SnoozeUntil(
				bigtime_t performance_time,
				bigtime_t with_latency = 0,
				bool retry_signals = false)
{
	return TimeSourceObject::SnoozeUntil(performance_time, with_latency, retry_signals);
}

/* virtual */ status_t
SystemTimeSourceObject::DeleteHook(BMediaNode * node)
{
	FATAL("SystemTimeSourceObject::DeleteHook called\n");
	return B_ERROR;
}
