// ****************************************************************************
//
//		C3g.H
//
//		Include file for interfacing with the 3G generic driver class
//		Set editor tabs to 3 for your viewing pleasure.
//
// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------
//
//   Copyright Echo Digital Audio Corporation (c) 1998 - 2004
//   All rights reserved
//   www.echoaudio.com
//   
//   This file is part of Echo Digital Audio's generic driver library.
//   
//   Echo Digital Audio's generic driver library is free software; 
//   you can redistribute it and/or modify it under the terms of 
//   the GNU General Public License as published by the Free Software Foundation.
//   
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//   
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 59 Temple Place - Suite 330, Boston, 
//   MA  02111-1307, USA.
//
// ****************************************************************************

//	Prevent problems with multiple includes
#ifndef _3GOBJECT_
#define _3GOBJECT_

#include "CEchoGalsMTC.h"
#include "C3gDco.h"

//
//	Class used for interfacing with the 3G audio card.
//
class C3g : public CEchoGalsMTC
{
public:
	//
	//	Construction/destruction
	//
	C3g( PCOsSupport pOsSupport );
	virtual ~C3g();

	//
	// Setup & initialization methods
	//
	virtual ECHOSTATUS InitHw();

	//
	//	Return the capabilities of this card; card type, card name,
	//	# analog inputs, # analog outputs, # digital channels,
	//	# MIDI ports and supported clocks.
	//	See ECHOGALS_CAPS definition above.
	//
	virtual ECHOSTATUS GetCapabilities
	(
		PECHOGALS_CAPS	pCapabilities
	);
	
	void Get3gBoxType(DWORD *pOriginalBoxType,DWORD *pCurrentBoxType);
	char *Get3gBoxName();

	//
	// Get the audio latency for a single pipe
	//
	virtual void GetAudioLatency(ECHO_AUDIO_LATENCY *pLatency);

	//
	//	Audio Interface methods
	//
	virtual ECHOSTATUS QueryAudioSampleRate
	(
		DWORD		dwSampleRate
	);

	//
	// Get a bitmask of all the clocks the hardware is currently detecting
	//
	virtual ECHOSTATUS GetInputClockDetect(DWORD &dwClockDetectBits);
	
	//
	// Phantom power on/off for Gina3G
	//
	virtual void GetPhantomPower(BOOL *pfPhantom);
	virtual void SetPhantomPower(BOOL fPhantom);

	//
	//	Overload new & delete so memory for this object is allocated from 
	//	non-paged memory.
	//
	PVOID operator new( size_t Size );
	VOID  operator delete( PVOID pVoid ); 

protected:
	//
	//	Get access to the appropriate DSP comm object
	//
	PC3gDco GetDspCommObject()
		{ return( (PC3gDco) m_pDspCommObject ); }
		
	BOOL	 m_fPhantomPower;

};		// class CMia


typedef C3g * PC3g;

#endif

// *** C3g.H ***
