/*	$OpenBSD: sensors.h,v 1.4 2004/02/10 19:53:34 grange Exp $	*/

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
	SENSOR_TEMP,			/* temperature */
	SENSOR_FANRPM,			/* fan revolution speed */
	SENSOR_VOLTS_DC,		/* voltage */
	SENSOR_VOLTS_AC,		/* voltage (alternating-current) */
	SENSOR_OHMS,			/* resistance */
	SENSOR_WATTS,			/* power */
	SENSOR_AMPS,			/* current */
	SENSOR_WATTHOUR,		/* power capacity */
	SENSOR_AMPHOUR,			/* power capacity */
	SENSOR_INDICATOR,		/* boolean indicator */
	SENSOR_INTEGER			/* generic interger value */
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
	int flags;			/* sensor flags */
#define SENSOR_FINVALID		0x0001	/* sensor is invalid */
};

SLIST_HEAD(sensors_head, sensor);

#ifdef _KERNEL
extern int nsensors;
extern struct sensors_head sensors;

#define SENSOR_ADD(s) do { \
	(s)->num = nsensors++;				\
	SLIST_INSERT_HEAD(&sensors, (s), list);		\
} while (0)
#endif	/* _KERNEL */

#endif	/* !_SYS_SENSORS_H_ */
