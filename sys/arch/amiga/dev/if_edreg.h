/*	$NetBSD: if_edreg.h,v 1.4 1995/05/08 02:40:54 chopps Exp $	*/

/*
 * Vendor types
 */
#define	ED_VENDOR_HYDRA		0x03	/* Hydra Systems */

/*
 * Compile-time config flags
 */
/*
 * This sets the default for enabling/disablng the tranceiver.
 */
#define ED_FLAGS_DISABLE_TRANCEIVER	0x0001

/*
 * This forces the board to be used in 8/16-bit mode even if it autoconfigs
 * differently.
 */
#define ED_FLAGS_FORCE_8BIT_MODE	0x0002
#define ED_FLAGS_FORCE_16BIT_MODE	0x0004

/*
 * This disables the use of double transmit buffers.
 */
#define ED_FLAGS_NO_MULTI_BUFFERING	0x0008

/*
 * This forces all operations with the NIC memory to use Programmed I/O (i.e.
 * not via shared memory).
 */
#define ED_FLAGS_FORCE_PIO		0x0010

/*
 *		Definitions for Hydra Systems boards
 */
#define	HYDRA_ADDRPROM	0xffc0
#define	HYDRA_NIC_BASE	0xffe1
/*
 *		Definitions for ASDG LANRover boards
 */
#define ASDG_ADDRPROM	0xff
#define ASDG_NIC_BASE	0x1
