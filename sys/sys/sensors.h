/*	$OpenBSD: sensors.h,v 1.14 2006/01/28 09:53:37 dlg Exp $	*/

/*
 * Copyright (c) 2003, 2004 Alexander Yurchenko <grange@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_SENSORS_H_
#define _SYS_SENSORS_H_

/* Sensor types */
enum sensor_type {
	SENSOR_TEMP,			/* temperature (muK) */
	SENSOR_FANRPM,			/* fan revolution speed */
	SENSOR_VOLTS_DC,		/* voltage (muV DC) */
	SENSOR_VOLTS_AC,		/* voltage (muV AC) */
	SENSOR_OHMS,			/* resistance */
	SENSOR_WATTS,			/* power */
	SENSOR_AMPS,			/* current (muA) */
	SENSOR_WATTHOUR,		/* power capacity */
	SENSOR_AMPHOUR,			/* power capacity */
	SENSOR_INDICATOR,		/* boolean indicator */
	SENSOR_INTEGER,			/* generic integer value */
	SENSOR_PERCENT,			/* percent */
	SENSOR_LUX,			/* illuminance (mulx) */
	SENSOR_DRIVE			/* disk */
};

#define SENSOR_DRIVE_EMPTY	1
#define SENSOR_DRIVE_READY	2
#define SENSOR_DRIVE_POWERUP	3
#define SENSOR_DRIVE_ONLINE	4
#define SENSOR_DRIVE_IDLE	5
#define SENSOR_DRIVE_ACTIVE	6
#define SENSOR_DRIVE_REBUILD	7
#define SENSOR_DRIVE_POWERDOWN	8
#define SENSOR_DRIVE_FAIL	9
#define SENSOR_DRIVE_PFAIL	10

/* Sensor states */
enum sensor_status {
	SENSOR_S_UNSPEC,		/* status is unspecified */
	SENSOR_S_OK,			/* status is ok */
	SENSOR_S_WARN,			/* status is warning */
	SENSOR_S_CRIT,			/* status is critical */
	SENSOR_S_UNKNOWN		/* status is unknown */
};

/* Sensor data */
struct sensor {
	SLIST_ENTRY(sensor) list;
	int num;			/* sensor number */
	char device[16];		/* device name */
	enum sensor_type type;		/* sensor type */
	char desc[32];			/* sensor description */
	int64_t value;			/* current value */
	u_int rfact;			/* resistor factor */
	enum sensor_status status;	/* sensor status */
	int flags;			/* sensor flags */
#define SENSOR_FINVALID		0x0001	/* sensor is invalid */
#define SENSOR_FUNKNOWN		0x0002	/* sensor value is unknown */
};

SLIST_HEAD(sensors_head, sensor);

#ifdef _KERNEL

void		sensor_add(struct sensor *);
void		sensor_del(struct sensor *);
struct sensor	*sensor_get(int);

int		sensor_task_register(void *, void (*)(void *), int);
void		sensor_task_unregister(void *);

#endif	/* _KERNEL */

#endif	/* !_SYS_SENSORS_H_ */
