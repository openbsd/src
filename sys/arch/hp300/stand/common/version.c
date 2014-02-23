/*	$OpenBSD: version.c,v 1.16 2014/02/23 19:22:40 miod Exp $	*/

/*
 * Record major changes in the boot code here, and increment the version
 * number.
 */

/*
 * 2.0	INITIAL REVISION
 * 2.1	APCI support, config changes from NetBSD.
 * 2.2	Grand reorganization.
 * 2.3	Added CD9660 boot support.
 * 2.4	Added/fixed 425e support.
 * 2.5	Added SYS_CDBOOT.
 * 2.6	Fixed RTC reading for 2001.
 * 2.7	Minor syncs with the kernel (recognize more models and use the same
 *	logic to pick the console on 425e).
 * 2.8	TurboVRX frame buffer support.
 * 2.9	SGC frame buffers support, bug fixes and code cleanup.
 * 2.10	Blind SGC support on models 362 and 382, turned out to be useless.
 * 2.11	sti@dio frame buffer support (for models 362 and 382), and various
 *	cleanups.
 * 2.12	Switch to MI loadfile code.
 * 2.13	Allow kernels with uppercase characters in their names to be loaded
 *	from the default boot device without an explicit device or a leading
 *	`/'.
 * 2.14 Build with the ELF toolchain.
 * 2.15 Remove SLOWSCSI from scsi code, and increase target selection timeout.
 * 2.16	Loadfile support for .openbsd.randomdata section.
 * 2.17	/etc/random.seed support in uboot (SYS_UBOOT).
 */

const char version[] = "2.17";
