/*	$NetBSD: sysctl.h,v 1.1 1996/12/17 19:26:51 gwr Exp $	*/

/*
 * CTL_MACHDEP definitions.  (Common to all m68k ports.)
 * This should be included by each m68k port's cpu.h so
 * /usr/sbin/sysctl can be shared on all of them.
 */
#ifndef CTL_MACHDEP_NAMES

#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_MAXID		2	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}

#endif	/* CTL_MACHDEP_NAMES */
