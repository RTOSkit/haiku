// ****************************************************************************
//
//		CDspCommObjectVmixer.H
//
//		DSP comm object with vmixer support
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

#ifndef	_DSPCOMMOBJECT_VMIXER_H_
#define	_DSPCOMMOBJECT_VMIXER_H_

#include "CDspCommObject.h"

class CDspCommObjectVmixer : public CDspCommObject
{
public:
	//
	//	Construction/destruction
	//
	CDspCommObjectVmixer( PDWORD pdwRegBase, PCOsSupport pOsSupport );
	virtual ~CDspCommObjectVmixer();

	virtual ECHOSTATUS GetAudioMeters
	(
		PECHOGALS_METERS	pMeters
	);

	virtual ECHOSTATUS SetPipeOutGain
	( 
		WORD wPipeOut, 
		WORD wBusOut,
		INT32 iGain,
		BOOL fImmediate = TRUE
	);
	
	virtual ECHOSTATUS GetPipeOutGain
	( 
		WORD wPipeOut, 
		WORD wBusOut,
		INT32 &iGain
	);
	
	virtual ECHOSTATUS UpdateVmixerLevel();
	
	virtual ECHOSTATUS SetBusOutGain(WORD wBusOut,INT32 iGain);

};		// class CDspCommObjectVmixer

typedef CDspCommObjectVmixer * PCDspCommObjectVmixer;

#endif // _DSPCOMMOBJECT_VMIXER_H_

// **** CDspCommObjectVmixer.h ****
