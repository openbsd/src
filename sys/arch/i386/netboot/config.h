/*	$NetBSD: config.h,v 1.3 1994/10/27 04:21:10 cgd Exp $	*/

/* netboot
 *
 * source in this file came from
 */

#if !defined(__config_h_)
#define __config_h_

/*
   configuration items shared between .c and .s files
 */

#define WORK_AREA_SIZE 0x10000L

#define T(x) printf(" " #x ":\n")

/* turn a near address into an integer
   representing a linear physical addr */
#define LA(x) ((u_long)(x))

#endif
