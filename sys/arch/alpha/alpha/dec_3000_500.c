/*	$NetBSD: dec_3000_500.c,v 1.1 1995/08/03 00:57:26 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
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
dec_3000_500_modelname()
{

	switch (hwrpb->rpb_variation & SV_ST_MASK) {
	case SV_ST_SANDPIPER:
systype_sandpiper:
		return "DEC 3000/400 (\"Sandpiper\")";

	case SV_ST_FLAMINGO:
systype_flamingo:
		return "DEC 3000/500 (\"Flamingo\")";

	case SV_ST_HOTPINK:
		return "DEC 3000/500X (\"Hot Pink\")";

	case SV_ST_FLAMINGOPLUS:
	case SV_ST_ULTRA:
		return "DEC 3000/800 (\"Flamingo+\")";

	case SV_ST_SANDPLUS:
		return "DEC 3000/600 (\"Sandpiper+\")";

	case SV_ST_SANDPIPER45:
		return "DEC 3000/700 (\"Sandpiper45\")";

	case SV_ST_FLAMINGO45:
		return "DEC 3000/900 (\"Flamingo45\")";

	case SV_ST_RESERVED: /* this is how things used to be done */
		if (hwrpb->rpb_variation & SV_GRAPHICS)
			goto systype_flamingo;
		else
			goto systype_sandpiper;

	default:
		printf("unknown system variation %lx\n",
		    hwrpb->rpb_variation & SV_ST_MASK);
		return NULL;
	}
}

void
dec_3000_500_consinit(constype)
	char *constype;
{

}

dev_t
dec_3000_500_bootdev(booted_dev)
	char *booted_dev;
{

	panic("gack.");
}
