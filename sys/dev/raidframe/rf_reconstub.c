/*	$OpenBSD: rf_reconstub.c,v 1.1 1999/01/11 14:29:47 niklas Exp $	*/
/*	$NetBSD: rf_reconstub.c,v 1.1 1998/11/13 04:20:34 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/**************************************************************************
 *
 * rf_reconstub.c -- stub routines used when you don't want reconstruction
 * in some particular instantiation of the raidframe
 *
 * this file also contains stubs for some reconstruction-related
 * routines that we don't want compiled into the kernel.
 *
 * The OSF/1 kernel configuration includes an option "raidframe_recon".  If
 * enabled, most of this file is ifdef'd out.
 *
 **************************************************************************/

/* :  
 * Log: rf_reconstub.c,v 
 * Revision 1.9  1996/05/24 01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.8  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.7  1996/04/03  23:25:33  jimz
 * make inclusion of raidframe_recon.h #ifdef KERNEL
 *
 * Revision 1.6  1995/12/06  15:06:54  root
 * added copyright info
 *
 */

#ifdef KERNEL
#include <raidframe_recon.h>
#endif /* KERNEL */
#include <sys/errno.h>

#if RAIDFRAME_RECON == 0

int rf_ConfigureReconstruction() { return(0); }
int rf_ConfigureReconEvent()     { return(0); }
int rf_ConfigurePSStatus()       { return(0); }
int rf_ConfigureNWayXor()        { return(0); }
int rf_ConfigureCopyback()       { return(0); }
int rf_ShutdownCopyback()        { return(0); }
int rf_ShutdownReconstruction()  { return(0); }
int rf_ShutdownReconEvent()      { return(0); }
int rf_ShutdownPSStatus()        { return(0); }
int rf_ShutdownNWayXor()         { return(0); }

int rf_ForceOrBlockRecon()       { return(0); }
int rf_UnblockRecon()            { return(0); }
int rf_ReconstructFailedDisk()   { return(ENOTTY); }
int rf_CheckRUReconstructed()    { return(0); }

void rf_start_cpu_monitor()  {}
void rf_stop_cpu_monitor()   {}
void rf_print_cpu_util()     {}

#endif /* RAIDFRAME_RECON == 0 */
