/*	$NetBSD: dec_3000_300.c,v 1.2 1995/08/03 01:12:21 cgd Exp $	*/

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

#include <sys/param.h>
#include <machine/rpb.h>

char *
dec_3000_300_modelname()
{

	switch (hwrpb->rpb_variation & SV_ST_MASK) {
	case SV_ST_PELICAN:
		return "DEC 3000/300 (\"Pelican\")";

	case SV_ST_PELICA:
		return "DEC 3000/300L (\"Pelica\")";

	case SV_ST_PELICANPLUS:
		return "DEC 3000/300X (\"Pelican+\")";

	case SV_ST_PELICAPLUS:
		return "DEC 3000/300LX (\"Pelica+\")";

	default:
		printf("unknown system variation %lx\n",
		    hwrpb->rpb_variation & SV_ST_MASK);
		return NULL;
	}
}

void
dec_3000_300_consinit(constype)
	char *constype;
{

}

dev_t
dec_3000_300_bootdev(booted_dev)
	char *booted_dev;
{

	panic("gack.");
}
