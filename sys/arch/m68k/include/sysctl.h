/*	$OpenBSD: sysctl.h,v 1.4 2001/07/04 08:36:27 niklas Exp $	*/
/*	$NetBSD: sysctl.h,v 1.1 1996/12/17 19:26:51 gwr Exp $	*/

/*
 * CTL_MACHDEP definitions.  (Common to all m68k ports.)
 * This should be included by each m68k port's cpu.h so
 * /usr/sbin/sysctl can be shared on all of them.
 */
#ifndef CTL_MACHDEP_NAMES

#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_ROOT_DEVICE		2	/* string: root device name */
#define	CPU_BOOTED_KERNEL	3	/* string: booted kernel name */
#define	CPU_MAXID		4	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
	{ "root_device", CTLTYPE_STRING }, \
	{ "booted_kernel", CTLTYPE_STRING }, \
}

#endif	/* CTL_MACHDEP_NAMES */
