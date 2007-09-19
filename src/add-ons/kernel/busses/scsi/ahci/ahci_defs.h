/*
 * Copyright 2007, Marcus Overhagen. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _AHCI_DEFS_H
#define _AHCI_DEFS_H

#include <bus/PCI.h>
#include <bus/SCSI.h>

#define AHCI_DEVICE_MODULE_NAME "busses/scsi/ahci/device_v1"
#define AHCI_SIM_MODULE_NAME "busses/scsi/ahci/sim/v1"

enum {
	CAP_S64A		= (1 << 31),	// Supports 64-bit Addressing
	CAP_SNCQ		= (1 << 30),	// Supports Native Command Queuing
	CAP_SSNTF		= (1 << 29),	// Supports SNotification Register
	CAP_SMPS		= (1 << 28),	// Supports Mechanical Presence Switch
	CAP_SSS			= (1 << 27),	// Supports Staggered Spin-up
	CAP_SALP		= (1 << 26),	// Supports Aggressive Link Power Management
	CAP_SAL			= (1 << 25),	// Supports Activity LED
	CAP_SCLO		= (1 << 24),	// Supports Command List Override
	CAP_ISS_MASK 	= 0xf, 			// Interface Speed Support
	CAP_ISS_SHIFT	= 20,
	CAP_SNZO 		= (1 << 19),	// Supports Non-Zero DMA Offsets
	CAP_SAM 		= (1 << 18),	// Supports AHCI mode only
	CAP_SPM 		= (1 << 17),	// Supports Port Multiplier
	CAP_FBSS 		= (1 << 16),	// FIS-based Switching Supported
	CAP_PMD 		= (1 << 15),	// PIO Multiple DRQ Block
	CAP_SSC 		= (1 << 14),	// Slumber State Capable
	CAP_PSC 		= (1 << 13),	// Partial State Capable
	CAP_NCS_MASK 	= 0x1f,			// Number of Command Slots (zero-based number)
	CAP_NCS_SHIFT	= 8,
	CAP_CCCS 		= (1 << 7),		// Command Completion Coalescing Supported
	CAP_EMS 		= (1 << 6),		// Enclosure Management Supported
	CAP_SXS 		= (1 << 5), 	// Supports External SATA
	CAP_NP_MASK		= 0x1f,			// Number of Ports (zero-based number)
	CAP_NP_SHIFT	= 0,
};


enum {
	GHC_AE			= (1 << 31),	// AHCI Enable
	GHC_MRSM		= (1 << 2),		// MSI Revert to Single Message
	GHC_IE			= (1 << 1),		// Interrupt Enable
	GHC_HR			= (1 << 0),		// HBA Reset
};


enum {
	INT_CPD			= (1 << 31),	// Cold Port Detect Status/Enable
	INT_TFE			= (1 << 30),	// Task File Error Status/Enable
	INT_HBF			= (1 << 29),	// Host Bus Fatal Error Status/Enable
	INT_HBD			= (1 << 28),	// Host Bus Data Error Status/Enable
	INT_IF			= (1 << 27),	// Interface Fatal Error Status/Enable
	INT_INF			= (1 << 26),	// Interface Non-fatal Error Status/Enable
	INT_OF			= (1 << 24),	// Overflow Status/Enable
	INT_IPM			= (1 << 23),	// Incorrect Port Multiplier Status/Enable
	INT_PRC			= (1 << 22),	// PhyRdy Change Status/Enable
	INT_DMP			= (1 << 7),		// Device Mechanical Presence Status/Enable
	INT_PC			= (1 << 6),		// Port Change Interrupt Status/Enable
	INT_DP			= (1 << 5),		// Descriptor Processed Interrupt/Enable
	INT_UF			= (1 << 4),		// Unknown FIS Interrupt/Enable
	INT_SDB			= (1 << 3),		// Set Device Bits Interrupt/Enable
	INT_DS			= (1 << 2),		// DMA Setup FIS Interrupt/Enable
	INT_PS			= (1 << 1),		// PIO Setup FIS Interrupt/Enable
	INT_DHR			= (1 << 0),		// Device to Host Register FIS Interrupt/Enable
};


enum {
	AHCI_CLB_SIZE	= 1024,
	AHCI_FIS_SIZE	= 256,
};


typedef struct {
	uint32		clb;			// Command List Base Address (alignment 1024 byte)
	uint32		clbu;			// Command List Base Address Upper 32-Bits
	uint32		fb;				// FIS Base Address (alignment 256 byte)
	uint32		fbu;			// FIS Base Address Upper 32-Bits
	uint32		is;				// Interrupt Status
	uint32		ie;				// Interrupt Enable
	uint32		cmd;			// Command and Status
	uint32 		res1;			// Reserved
	uint32		tfd;			// Task File Data
	uint32		sig;			// Signature
	uint32		ssts;			// Serial ATA Status (SCR0: SStatus)
	uint32		sctl;			// Serial ATA Control (SCR2: SControl)
	uint32		serr;			// Serial ATA Error (SCR1: SError)
	uint32		sact;			// Serial ATA Active (SCR3: SActive)
	uint32		ci;				// Command Issue
	uint32		sntf;			// SNotification
	uint32		res2;			// Reserved for FIS-based Switching Definition
	uint32		res[11];		// Reserved
	uint32		vendor[2];		// Vendor Specific
} ahci_port;


typedef struct {
	uint32		cap;			// Host Capabilities
	uint32		ghc;			// Global Host Control
	uint32		is;				// Interrupt Status
	uint32		pi;				// Ports Implemented
	uint32		vs;				// Version
	uint32		ccc_ctl;		// Command Completion Coalescing Control
	uint32		ccc_ports;		// Command Completion Coalsecing Ports
	uint32		em_loc;			// Enclosure Management Location
	uint32		em_ctl;			// Enclosure Management Control
	uint32		res[31];		// Reserved
	uint32		vendor[24];		// Vendor Specific registers
	ahci_port	port[32];
} ahci_hba;


typedef struct {
	uint8		dsfis[0x1c];	// DMA Setup FIS
	uint8		res1[0x04];
	uint8		psfis[0x14];	// PIO Setup FIS
	uint8		res2[0x0c];
	uint8		rfis[0x20];		// D2H Register FIS
	uint8		res3[0x04];
	uint8		sdbfis[0x08];	// Set Device Bits FIS
	uint8		ufis[0x40];		// Unknown FIS
	uint8		res4[0x60];
} fis;


typedef struct {
  union {
   struct {
	uint16		prdtl;			// physical region description table length;		
	uint16		pmp : 4;		// Port Multiplier Port
	uint16		: 1;
	uint16		c : 1;			// Clear Busy upon R_OK
	uint16		b : 1;			// Build In Self Test
	uint16		r : 1;			// Reset 
	uint16		p : 1;			// Prefetchable
	uint16		w : 1;			// Write
	uint16		a :	1;// ATAPI
	uint16		cfl : 5;		// command FIS length
   };
    uint32		prdtl_flags_cfl;
  };
	uint32		ctba;			// command table desciptor base address (alignment 128 byte)
	uint32		ctbau;			// command table desciptor base address upper
	uint8		res1[0x10];
} command_list_entry;

#define COMMAND_LIST_ENTRY_COUNT 32

typedef struct {
	uint32		dba;			// Data Base Address (2-byte aligned)
	uint32		dbau;			// Data Base Address Upper
	uint32		res;
	uint32		dbc;			// Bytecount (0-based, even, max 4MB)
	#define DBC_I	0x80000000	/* Interrupt on completition */
} prd;


typedef struct {
	uint8		cfis[0x40];		// command FIS
	uint8		acmd[0x20];		// ATAPI command
	uint8		res[0x20];		// reserved
} command_table;


#define PRD_TABLE_ENTRY_COUNT 168



extern scsi_sim_interface gAHCISimInterface;
extern device_manager_info *gDeviceManager;
extern pci_device_module_info *gPCI;
extern scsi_for_sim_interface *gSCSI;

#define LO32(val) ((uint32)(val))
#define HI32(val) (((uint64)(val)) >> 32)

#ifdef __cplusplus

template <class T>
int count_bits_set(T value)
{
	int count = 0;
	for (T mask = 1; mask; mask <<= 1)
		if (value & mask)
			count++;
	return count;
}

#endif	/* __cplusplus */


#endif	/* _AHCI_DEFS_H */
