//-----------------------------------------------------------------------
//  This software is part of the OpenBeOS distribution and is covered 
//  by the OpenBeOS license.
//
//  Copyright (c) 2003-2004 Waldemar Kornewald, Waldemar.Kornewald@web.de
//-----------------------------------------------------------------------

#ifndef _PPP_CONTROL__H
#define _PPP_CONTROL__H

#include <Drivers.h>
#include <driver_settings.h>
#include <PPPDefs.h>


// various constants
#define PPP_HANDLER_NAME_LENGTH_LIMIT		63
	// if the name is longer than this value it will be truncated to fit the structure

// starting values and other values for control ops
#define PPP_RESERVE_OPS_COUNT				0xFFFF
#define PPP_OPS_START						B_DEVICE_OP_CODES_END + 1
#define PPP_INTERFACE_OPS_START				PPP_OPS_START + PPP_RESERVE_OPS_COUNT
#define PPP_DEVICE_OPS_START				PPP_OPS_START + 2 * PPP_RESERVE_OPS_COUNT
#define PPP_PROTOCOL_OPS_START				PPP_OPS_START + 3 * PPP_RESERVE_OPS_COUNT
#define PPP_OPTION_HANDLER_OPS_START		PPP_OPS_START + 5 * PPP_RESERVE_OPS_COUNT
#define PPP_LCP_EXTENSION_OPS_START			PPP_OPS_START + 6 * PPP_RESERVE_OPS_COUNT
#define PPP_COMMON_OPS_START				PPP_OPS_START + 10 * PPP_RESERVE_OPS_COUNT
#define PPP_USER_OPS_START					PPP_OPS_START + 32 * PPP_RESERVE_OPS_COUNT


//! These values should be used for ppp_control_info::op.
enum ppp_control_ops {
	// -----------------------------------------------------
	// PPPManager (the PPP interface module)
	PPPC_CREATE_INTERFACE = PPP_OPS_START,
	PPPC_CREATE_INTERFACE_WITH_NAME,
	PPPC_DELETE_INTERFACE,
	PPPC_BRING_INTERFACE_UP,
	PPPC_BRING_INTERFACE_DOWN,
	PPPC_CONTROL_INTERFACE,
	PPPC_GET_INTERFACES,
	PPPC_COUNT_INTERFACES,
	PPPC_FIND_INTERFACE_WITH_SETTINGS,
	// -----------------------------------------------------
	
	// -----------------------------------------------------
	// KPPPInterface
	PPPC_GET_INTERFACE_INFO = PPP_INTERFACE_OPS_START,
	PPPC_SET_MRU,
	PPPC_SET_DIAL_ON_DEMAND,
	PPPC_SET_AUTO_REDIAL,
	PPPC_HAS_INTERFACE_SETTINGS,
	
	// handler access
	PPPC_CONTROL_DEVICE,
	PPPC_CONTROL_PROTOCOL,
	PPPC_CONTROL_OPTION_HANDLER,
	PPPC_CONTROL_LCP_EXTENSION,
	PPPC_CONTROL_CHILD,
	// -----------------------------------------------------
	
	// -----------------------------------------------------
	// KPPPDevice
	PPPC_GET_DEVICE_INFO = PPP_DEVICE_OPS_START,
	// -----------------------------------------------------
	
	// -----------------------------------------------------
	// KPPPProtocol
	PPPC_GET_PROTOCOL_INFO = PPP_PROTOCOL_OPS_START,
	// -----------------------------------------------------
	
	// -----------------------------------------------------
	// Common/mixed ops
	PPPC_ENABLE,
	PPPC_GET_SIMPLE_HANDLER_INFO,
		// KPPPOptionHandler and KPPPLCPExtension
	
	// these two control ops use the ppp_report_request structure
	PPPC_ENABLE_REPORTS,
	PPPC_DISABLE_REPORTS,
		// flags are not used for this control op
	// -----------------------------------------------------
	
	PPP_CONTROL_OPS_END = B_DEVICE_OP_CODES_END + 0xFFFF
};


//!	Basic structure used for creating and searching PPP interfaces.
typedef struct ppp_interface_description_info {
	union {
		const driver_settings *settings;
			//!< Interface settings.
		const char *name;
			//!< Name of interface description file.
	} u;
		//!< Different values for describing an interface.
	const driver_settings *profile;
		//!< 
	ppp_interface_id interface;
		//!< the found/created interface
} ppp_interface_description_info;


//! Used to get all interface ids from the PPP interface manager.
typedef struct ppp_get_interfaces_info {
	ppp_interface_id *interfaces;
		//!< The interface ids will be written to this pointer's target.
	int32 count;
		//!< \a Interface has \a count entries.
	ppp_interface_filter filter;
		//!< Only interfaces that match this filter will be returned
	int32 resultCount;
		//!< The number of saved interfaces.
} ppp_get_interfaces_info;


//! With this structure you can refer to some handler/interface.
typedef struct ppp_control_info {
	uint32 index;
		//!< Index/id of interface/protocol/etc.
	uint32 op;
		//!< The Control()/ioctl() opcode. This can be any value from ppp_control_ops.
	void *data;
		//!< Additional data may be specified.
	size_t length;
		//!< The length should always be set.
} ppp_control_info;


// -----------------------------------------------------------
// structures for storing information about interface/handlers
// use the xxx_info_t structures when allocating memory (they
// reserve memory for future implementations)
// -----------------------------------------------------------
#define _PPP_INFO_T_SIZE_								256

typedef struct ppp_interface_info {
	char name[PPP_HANDLER_NAME_LENGTH_LIMIT + 1];
	int32 if_unit;
		// negative if not registered
	
	ppp_mode mode;
	ppp_state state;
	ppp_phase phase;
	ppp_authentication_status localAuthenticationStatus, peerAuthenticationStatus;
	ppp_pfc_state localPFCState, peerPFCState;
	uint8 pfcOptions;
	
	uint32 protocolsCount, optionHandlersCount, LCPExtensionsCount, childrenCount;
	uint32 MRU, interfaceMTU;
	
	uint32 dialRetry, dialRetriesLimit;
	uint32 dialRetryDelay, redialDelay;
	uint32 idleSince, disconnectAfterIdleSince;
	
	bool doesDialOnDemand, doesAutoRedial, hasDevice, isMultilink, hasParent;
} ppp_interface_info;
typedef struct ppp_interface_info_t {
	ppp_interface_info info;
	uint8 _reserved_[_PPP_INFO_T_SIZE_ - sizeof(ppp_interface_info)];
} ppp_interface_info_t;


// devices are special handlers, so they have their own structure
typedef struct ppp_device_info {
	char name[PPP_HANDLER_NAME_LENGTH_LIMIT + 1];
	
	uint32 MTU;
	uint32 inputTransferRate, outputTransferRate, outputBytesCount;
	bool isUp;
} ppp_device_info;
typedef struct ppp_device_info_t {
	ppp_device_info info;
	uint8 _reserved_[_PPP_INFO_T_SIZE_ - sizeof(ppp_device_info)];
} ppp_device_info_t;


typedef struct ppp_protocol_info {
	char name[PPP_HANDLER_NAME_LENGTH_LIMIT + 1];
	char type[PPP_HANDLER_NAME_LENGTH_LIMIT + 1];
	
	ppp_phase activationPhase;
	int32 addressFamily, flags;
	ppp_side side;
	ppp_level level;
	uint32 overhead;
	
	ppp_phase connectionPhase;
		// there are four possible states:
		// PPP_ESTABLISHED_PHASE	-		IsUp() == true
		// PPP_DOWN_PHASE			-		IsDown() == true
		// PPP_ESTABLISHMENT_PHASE	-		IsGoingUp() == true
		// PPP_TERMINATION_PHASE	-		IsGoingDown() == true
	
	uint16 protocolNumber;
	bool isEnabled;
	bool isUpRequested;
} ppp_protocol_info;
typedef struct ppp_protocol_info_t {
	ppp_protocol_info info;
	uint8 _reserved_[_PPP_INFO_T_SIZE_ - sizeof(ppp_protocol_info)];
} ppp_protocol_info_t;


typedef struct ppp_simple_handler_info {
	char name[PPP_HANDLER_NAME_LENGTH_LIMIT + 1];
	
	bool isEnabled;
	
	uint8 code;
		// only KPPPLCPExtension
} ppp_simple_handler_info;
typedef struct ppp_simple_handler_info_t {
	ppp_simple_handler_info info;
	uint8 _reserved_[_PPP_INFO_T_SIZE_ - sizeof(ppp_simple_handler_info)];
} ppp_simple_handler_info_t;


#endif
