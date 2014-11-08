/* $OpenBSD: acpidev.h,v 1.35 2014/11/08 07:45:10 mlarkin Exp $ */
/*
 * Copyright (c) 2005 Marco Peereboom <marco@openbsd.org>
 * Copyright (c) 2005 Thorsten Lockert <tholo@sigmasoft.com>
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

#ifndef __DEV_ACPI_ACPIDEV_H__
#define __DEV_ACPI_ACPIDEV_H__

#include <sys/sensors.h>
#include <sys/rwlock.h>
#include <dev/acpi/acpireg.h>

#define DEVNAME(s)  ((s)->sc_dev.dv_xname)

#define ACPIDEV_NOPOLL		0
#define ACPIDEV_POLL		1

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
	char		bif_model[20];
	char		bif_serial[20];
	char		bif_type[20];
	char		bif_oem[20];
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

/* AC device */
/*
 * _PSR (Power Source)
 * Arguments: none
 * Results  : DWORD status
 */
#define PSR_OFFLINE		0x00
#define PSR_ONLINE		0x01

/*
 * _PCL (Power Consumer List)
 * Arguments: none
 * Results  : LIST of Power Class pointers
 */

/* hpet device */
#define	HPET_REG_SIZE		1024

#define	HPET_CAPABILITIES	0x000
#define	HPET_CONFIGURATION	0x010
#define	HPET_INTERRUPT_STATUS	0x020
#define	HPET_MAIN_COUNTER	0x0F0
#define	HPET_TIMER0_CONFIG	0x100
#define	HPET_TIMER0_COMPARE	0x108
#define	HPET_TIMER0_INTERRUPT	0x110
#define	HPET_TIMER1_CONFIG	0x200
#define	HPET_TIMER1_COMPARE	0x208
#define	HPET_TIMER1_INTERRUPT	0x310
#define	HPET_TIMER2_CONFIG	0x400
#define	HPET_TIMER2_COMPARE	0x408
#define	HPET_TIMER2_INTERRUPT	0x510

#define STA_PRESENT   (1L << 0)
#define STA_ENABLED   (1L << 1)
#define STA_SHOW_UI   (1L << 2)
#define STA_DEV_OK    (1L << 3)
#define STA_BATTERY   (1L << 4)

/*
 * _PSS (Performance Supported States)
 * Arguments: none
 * Results  : package _PSS (Performance Supported States)
 * Package {
 *	CoreFreq		//DWORD
 *	Power			//DWORD
 *	TransitionLatency	//DWORD
 *	BusMasterLatency	//DWORD
 *	Control			//DWORD
 * 	Status			//DWORD
 * }
 */
struct acpicpu_pss {
	u_int32_t	pss_core_freq;
	u_int32_t	pss_power;
	u_int32_t	pss_trans_latency;
	u_int32_t	pss_bus_latency;
	u_int32_t	pss_ctrl;
	u_int32_t	pss_status;
};

int acpicpu_fetch_pss(struct acpicpu_pss **);
void acpicpu_set_notify(void (*)(struct acpicpu_pss *, int));
/*
 * XXX this is returned in a buffer and is not a "natural" type.
 *
 * GRD (Generic Register Descriptor )
 *
 */
struct acpi_grd {
	u_int8_t	grd_descriptor;
	u_int16_t	grd_length;
	struct acpi_gas	grd_gas;
} __packed;

/*
 * _PCT (Performance Control )
 * Arguments: none
 * Results  : package _PCT (Performance Control)
 * Package {
 *	Perf_Ctrl_register	//Register
 *	Perf_Status_register	//Register
 * }
 */
struct acpicpu_pct {
	struct acpi_grd	pct_ctrl;
	struct acpi_grd	pct_status;
};

/* softc for fake apm devices */
struct acpiac_softc {
	struct device		sc_dev;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	int			sc_ac_stat;

	struct ksensor		sc_sens[1];
	struct ksensordev	sc_sensdev;
};

struct acpibat_softc {
	struct device		sc_dev;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	struct acpibat_bif	sc_bif;
	struct acpibat_bst	sc_bst;
	volatile int		sc_bat_present;

	struct ksensor		sc_sens[9];
	struct ksensordev	sc_sensdev;
};

TAILQ_HEAD(aml_nodelisth, aml_nodelist);

struct acpidock_softc {
	struct device		sc_dev;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	struct aml_nodelisth	sc_deps_h;
	struct aml_nodelist	*sc_deps;

	struct ksensor		sc_sens;
	struct ksensordev	sc_sensdev;

	int			sc_docked;
	int			sc_sta;

#define ACPIDOCK_STATUS_UNKNOWN		-1
#define ACPIDOCK_STATUS_UNDOCKED	0
#define ACPIDOCK_STATUS_DOCKED		1
};

#define ACPIDOCK_EVENT_INSERT	0
#define ACPIDOCK_EVENT_DEVCHECK 1
#define	ACPIDOCK_EVENT_EJECT	3

#define ACPIEC_MAX_EVENTS	256

struct acpiec_event {
	struct aml_node *event;
};

struct acpiec_softc {
	struct device		sc_dev;

	int			sc_ecbusy;

	/* command/status register */
	bus_space_tag_t		sc_cmd_bt;
	bus_space_handle_t	sc_cmd_bh;

	/* data register */
	bus_space_tag_t		sc_data_bt;
	bus_space_handle_t	sc_data_bh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;
	u_int32_t		sc_gpe;
	struct acpiec_event	sc_events[ACPIEC_MAX_EVENTS];
	int			sc_gotsci;
	int			sc_glk;
};

void		acpibtn_disable_psw(void);
void		acpibtn_enable_psw(void);
int		acpibtn_checklidopen(void);
#endif /* __DEV_ACPI_ACPIDEV_H__ */
