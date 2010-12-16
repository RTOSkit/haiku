/*
 * Copyright 2004-2010, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _USB_HID_PAGE_BATTERY_SYSTEM_H
#define _USB_HID_PAGE_BATTERY_SYSTEM_H


/* Reference:
 *		HID Usage Page 0x85: BATTERY SYSTEM
 *		Universal Serial Bus Usage Tables for HID Power Devices Ver. 1.0
 *		http://www.usb.org/developers/devclass_docs/pdcv10.pdf
 */

// Usage IDs
enum {
	B_HID_UID_BAT_SMB_BATTERY_MODE = 0x01,
	B_HID_UID_BAT_SMB_BATTERY_STATUS,
	B_HID_UID_BAT_SMB_ALARM_WARNING,
	B_HID_UID_BAT_SMB_CHARGER_MODE,
	B_HID_UID_BAT_SMB_CHARGER_STATUS,
	B_HID_UID_BAT_SMB_CHARGER_SPEC_INFO,
	B_HID_UID_BAT_SMB_SELECTOR_STATE,
	B_HID_UID_BAT_SMB_SELECTOR_PRESETS,
	B_HID_UID_BAT_SMB_SELECTOR_INFO,
	
	B_HID_UID_BAT_OPTIONAL_MFG_FUNCTION1 = 0x10,
	B_HID_UID_BAT_OPTIONAL_MFG_FUNCTION2,
	B_HID_UID_BAT_OPTIONAL_MFG_FUNCTION3,
	B_HID_UID_BAT_OPTIONAL_MFG_FUNCTION4,
	B_HID_UID_BAT_OPTIONAL_MFG_FUNCTION5,
	B_HID_UID_BAT_CONNECTION_TO_SM_BUS,
	B_HID_UID_BAT_OUTPUT_CONNECTION,
	B_HID_UID_BAT_CHARGER_CONNECTION,
	B_HID_UID_BAT_BATTERY_INSERTION,
	B_HID_UID_BAT_USE_NEXT,
	B_HID_UID_BAT_OK_TO_USE,
	B_HID_UID_BAT_BATTERY_SUPPORTED,
	B_HID_UID_BAT_SELECTOR_REVISION,
	B_HID_UID_BAT_CHARGING_INDICATOR,
	
	B_HID_UID_BAT_MANUFACTURER_ACCESS = 0x28,
	B_HID_UID_BAT_REMAINING_CAPACITY_LIMIT,
	B_HID_UID_BAT_REMAINING_TIME_LIMIT,
	B_HID_UID_BAT_AT_RATE,
	B_HID_UID_BAT_CAPACITY_MODE,
	B_HID_UID_BAT_BROADCAST_TO_CHARGER,
	B_HID_UID_BAT_PRIMARY_BATTERY,
	B_HID_UID_BAT_CHARGE_CONTROLLER,
	
	B_HID_UID_BAT_TERMINATE_CHARGE = 0x40,
	B_HID_UID_BAT_TERMINATE_DISCHARGE,
	B_HID_UID_BAT_BELOW_REMAINING_CAPACITY_LIMIT,
	B_HID_UID_BAT_REMAINING_TIME_LIMIT_EXPIRED,
	B_HID_UID_BAT_CHARGING,
	B_HID_UID_BAT_DISCHARGING,
	B_HID_UID_BAT_FULLY_CHARGED,
	B_HID_UID_BAT_FULLY_DISCHARGED,
	B_HID_UID_BAT_CONDITIONAL_FLAG,
	B_HID_UID_BAT_AT_RATE_OK,
	B_HID_UID_BAT_SMB_ERROR_CODE,
	B_HID_UID_BAT_NEED_REPLACEMENT,
	
	B_HID_UID_BAT_AT_RATE_TIME_TO_FULL = 0x60,
	B_HID_UID_BAT_AT_RATE_TIME_TO_EMPTY,
	B_HID_UID_BAT_AVERAGE_CURRENT,
	B_HID_UID_BAT_MAX_ERROR,
	B_HID_UID_BAT_RELATIVE_STATE_OF_CHARGE,
	B_HID_UID_BAT_ABSOLUTE_STATE_OF_CHARGE,
	B_HID_UID_BAT_REMAINING_CAPACITY,
	B_HID_UID_BAT_FULL_CHARGE_CAPACITY,
	B_HID_UID_BAT_RUN_TIME_TO_EMPTY,
	B_HID_UID_BAT_AVERAGE_TIME_TO_EMPTY,
	B_HID_UID_BAT_AVERAGE_TIME_TO_FULL,
	B_HID_UID_BAT_CYCLE_COUNT,
	
	B_HID_UID_BAT_BATT_PACK_MODEL_LEVEL = 0x80,
	B_HID_UID_BAT_INTERNAL_CHARGE_CONTROLLER,
	B_HID_UID_BAT_PRIMARY_BATTERY_SUPPORT,
	B_HID_UID_BAT_DESIGN_CAPACITY,
	B_HID_UID_BAT_SPECIFICATION_INFO,
	B_HID_UID_BAT_MANUFACTURER_DATE,
	B_HID_UID_BAT_SERIAL_NUMBER,
	B_HID_UID_BAT_IMANUFACTURER_NAME,
	B_HID_UID_BAT_IDEVICE_NAME,
	B_HID_UID_BAT_IDEVICE_CHEMISTRY,
	B_HID_UID_BAT_MANUFACTURER_DATA,
	B_HID_UID_BAT_RECHARGABLE,
	B_HID_UID_BAT_WARNING_CAPACITY_LIMIT,
	B_HID_UID_BAT_CAPACITY_GRANULARITY_1,
	B_HID_UID_BAT_CAPACITY_GRANULARITY_2,
	B_HID_UID_BAT_IOEM_INFORMATION,
	
	B_HID_UID_BAT_INHIBIT_CHARGE = 0xc0,
	B_HID_UID_BAT_ENABLE_POLLING,
	B_HID_UID_BAT_RESET_TO_ZERO,
	
	B_HID_UID_BAT_AC_PRESENT = 0xd0,
	B_HID_UID_BAT_BATTERY_PRESENT,
	B_HID_UID_BAT_POWER_FAIL,
	B_HID_UID_BAT_ALARM_INHIBITED,
	B_HID_UID_BAT_THERMISTOR_UNDER_RANGE,
	B_HID_UID_BAT_THERMISTOR_HOT,
	B_HID_UID_BAT_THERMISTOR_COLD,
	B_HID_UID_BAT_THERMISTOR_OVER_RANGE,
	B_HID_UID_BAT_VOLTAGE_OUT_OF_RANGE,
	B_HID_UID_BAT_CURRENT_OUT_OF_RANGE,
	B_HID_UID_BAT_CURRENT_NOT_REGULATED,
	B_HID_UID_BAT_VOLTAGE_NOT_REGULATED,
	B_HID_UID_BAT_MASTER_MODE,
	
	B_HID_UID_BAT_CHARGER_SELECTOR_SUPPORT = 0xf0,
	B_HID_UID_BAT_CHARGER_SPEC,
	B_HID_UID_BAT_LEVEL_2,
	B_HID_UID_BAT_LEVEL_3
};


#endif // _USB_HID_PAGE_BATTERY_SYSTEM_H
