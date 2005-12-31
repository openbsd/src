/*	$OpenBSD: version.c,v 1.6 2005/12/31 17:59:47 miod Exp $	*/

/*
 * Record major changes in the boot code here, and increment the version
 * number.
 */

/*
 * 2.0			INITIAL REVISION
 *
 * 2.1			APCI support, config changes from NetBSD.
 *
 * 2.2			Grand reorganization.
 *
 * 2.3			Added CD9660 boot support.
 *
 * 2.4			Added/fixed 425e support.
 *
 * 2.5			Added SYS_CDBOOT.
 *
 * 2.6			Fixed RTC reading for 2001.
 *
 * 2.7			Minor syncs with the kernel (recognize more models
 *			and use the same logic to pick the console on 425e).
 */

const char version[] = "2.7";
