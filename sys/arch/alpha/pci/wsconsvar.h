/*	$NetBSD: wsconsvar.h,v 1.1 1995/11/23 02:38:39 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
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

struct device;
struct ansicons;
struct ansicons_functions;

int	wscattach_output __P((struct device *, int, struct ansicons *,
	    struct ansicons_functions *, void *, int, int, int, int));
void	wscattach_input __P((struct device *, void *, int (*)(void *),
	    void (*)(void *, int)));
void	wscons_kbdinput __P((char *));
void	wsc_console __P((struct ansicons *, struct ansicons_functions *,
	    void *, int, int, int, int));
