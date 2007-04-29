/*	$OpenBSD: cpu.h,v 1.2 2007/04/29 17:53:37 miod Exp $	*/
/*	$NetBSD: cpu.h,v 1.1 2006/09/01 21:26:18 uwe Exp $	*/

#ifndef	_LANDISK_CPU_H_
#define	_LANDISK_CPU_H_

void machine_reset(void);

#include <sh/cpu.h>

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_KBDRESET		2	/* keyboard reset */
#define	CPU_LED_BLINK		3	/* keyboard reset */
#define	CPU_MAXID		4	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES {						\
	{ 0, 0 },							\
	{ "console_device",	CTLTYPE_STRUCT },			\
	{ "kbdreset",		CTLTYPE_INT },				\
	{ "led_blink",		CTLTYPE_INT }				\
}

#endif	/* _LANDISK_CPU_H_ */
