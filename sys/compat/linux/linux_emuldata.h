/*	$OpenBSD: linux_emuldata.h,v 1.1 2003/06/21 00:42:58 tedu Exp $	*/
/*	$NetBSD: linux_emuldata.h,v 1.4 2002/02/15 16:48:02 christos Exp $	*/

#ifndef _LINUX_EMULDATA_H
#define _LINUX_EMULDATA_H

/*
 * This is auxillary data the linux compat code
 * needs to do its work.  A pointer to it is
 * stored in the emuldata field of the proc
 * structure.
 */
struct linux_emuldata {
	caddr_t	p_break;	/* Cached per-process break value */	
};
#endif /* !_LINUX_EMULDATA_H */
