/* $OpenBSD: acpibat.h,v 1.2 2005/12/14 03:35:04 marco Exp $ */
/*
 * Copyright (c) 2005 Marco Peereboom <marco@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __DEV_ACPI_ACPIBAT_H__
#define __DEV_ACPI_ACPIBAT_H__

/*
 * _BIF (Battery InFormation)
 * Arguments: none
 * Results  : package _BIF (Battery InFormation)
 * Package {
 * 	// ASCIIZ is ASCII character string terminated with a 0x00.
 * 	Power Unit			//DWORD
 * 	Design Capacity			//DWORD
 * 	Last Full Charge Capacity	//DWORD
 * 	Battery Technology		//DWORD
 * 	Design Voltage			//DWORD
 * 	Design Capacity of Warning	//DWORD
 * 	Design Capacity of Low		//DWORD
 * 	Battery Capacity Granularity 1	//DWORD
 * 	Battery Capacity Granularity 2	//DWORD
 * 	Model Number			//ASCIIZ
 * 	Serial Number			//ASCIIZ
 * 	Battery Type			//ASCIIZ
 * 	OEM Information			//ASCIIZ
 * }
 */
struct acpibat_bif {
	u_int32_t	bif_power_unit;
#define BIF_POWER_MW		0x00
#define BIF_POWER_MA		0x01
	u_int32_t	bif_capacity;
#define BIF_UNKNOWN		0xffffffff
	u_int32_t	bif_last_capacity;
	u_int32_t	bif_technology;
#define BIF_TECH_PRIMARY	0x00
#define BIF_TECH_SECONDARY	0x01
	u_int32_t	bif_voltage;
	u_int32_t	bif_warning;
	u_int32_t	bif_low;
	u_int32_t	bif_cap_granu1;
	u_int32_t	bif_cap_granu2;
	char		bif_data[];	/* 4 strings */
};

/*
 * _OSC Definition for Control Method Battery
 * Arguments: none
 * Results  : DWORD flags
 */
#define CMB_OSC_UUID		"f18fc78b-0f15-4978-b793-53f833a1d35b"
#define   CMB_OSC_GRANULARITY	0x01
#define   CMB_OSC_WAKE_ON_LOW	0x02

/*
 * _BST (Battery STatus)
 * Arguments: none
 * Results  : package _BST (Battery STatus)
 * Package {
 * 	Battery State			//DWORD
 * 	Battery Present Rate		//DWORD
 * 	Battery Remaining Capacity	//DWORD
 * 	Battery Present Voltage		//DWORD
 * }
 * 
 * Per the spec section 10.2.2.3
 * Remaining Battery Percentage[%] = (Battery Remaining Capacity [=0 ~ 100] /
 *     Last Full Charged Capacity[=100]) * 100
 * 
 * Remaining Battery Life [h] = Battery Remaining Capacity [mAh/mWh] /
 *     Battery Present Rate [=0xFFFFFFFF] = unknown
 */
struct acpibat_bst {
	u_int32_t	bst_state;
#define BST_DISCHARGE		0x01
#define BST_CHARGE		0x02
#define BST_CRITICAL		0x04
	u_int32_t	bst_rate;
#define BST_UNKNOWN		0xffffffff
	u_int32_t	bst_capacity;
	u_int32_t	bst_voltage;
};

/*
 * _BTP (Battery Trip Point)
 * Arguments: DWORD level
 * Results  : none
 */
#define BTP_CLEAR_TRIP_POINT	0x00

/*
 * _BTM (Battery TiMe)
 * Arguments: DWORD rate of discharge
 * Results  : DWORD time in seconds or error/unknown
 */
#define BTM_CURRENT_RATE	0x00

#define BTM_RATE_TOO_LARGE	0x00
#define BTM_CRITICAL		0x00
#define BTM_UNKNOWN		0xffffffff

/*
 * _BMD (Battery Maintenance Data)
 * Arguments: none
 * Results  : package _BMD (Battery Maintenance Data)
 * Package {
 * 	Status Flags		//DWORD
 * 	Capability Flags	//DWORD
 * 	Recalibrate Count	//DWORD
 * 	Quick Recalibrate Time	//DWORD
 * 	Slow Recalibrate Time	//DWORD
 * }
 */
struct acpibat_bmd {
	u_int32_t	bmd_status;
#define BMD_AML_CALIBRATE_CYCLE	0x01
#define BMD_CHARGING_DISABLED	0x02
#define BMD_DISCHARGE_WHILE_AC	0x04
#define BMD_RECALIBRATE_BAT	0x08
#define BMD_GOTO_STANDBY_SPEED	0x10
	u_int32_t	bmd_capability;
#define BMD_CB_AML_CALIBRATION	0x01
#define BMD_CB_DISABLE_CHARGER	0x02
#define BMD_CB_DISCH_WHILE_AC	0x04
#define BMD_CB_AFFECT_ALL_BATT	0x08
#define BMD_CB_FULL_CHRG_FIRST	0x10
	u_int32_t	bmd_recalibrate_count;
#define BMD_ONLY_CALIB_IF_ST3	0x00	/* only recal when status bit 3 set */
	u_int32_t	bmd_quick_recalibrate_time;
#define BMD_UNKNOWN		0xffffffff
	u_int32_t	bmd_slow_recalibrate_time;
};

/*
 * _BMC (Battery Maintenance Control)
 * Arguments: DWORD flags
 * Results  : none
 */
#define BMC_AML_CALIBRATE	0x01
#define BMC_DISABLE_CHARGING	0x02
#define BMC_ALLOW_AC_DISCHARGE	0x04

#endif /* __DEV_ACPI_ACPIBAT_H__ */
