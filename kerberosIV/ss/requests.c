/*	$Id: requests.c,v 1.2 1996/09/16 03:13:00 tholo Exp $	*/

/*-
 * Copyright 1987, 1988, 1989 by the Student Information Processing Board
 *	of the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

/*
 * Various minor routines...
 */

#include <stdio.h>
#include "ss_internal.h"

#define	DECLARE(name)	void name(argc,argv,sci_idx,arg)int argc,sci_idx;const char *const *argv;void *arg;

/*
 * ss_self_identify -- assigned by default to the "." request
 */
DECLARE(ss_self_identify)
{
     register ss_data *info = ss_info(sci_idx);
     printf("%s version %s\n", info->subsystem_name,
	    info->subsystem_version);
}

/*
 * ss_subsystem_name -- print name of subsystem
 */
DECLARE(ss_subsystem_name)
{
     printf("%s\n", ss_info(sci_idx)->subsystem_name);
}

/*
 * ss_subsystem_version -- print version of subsystem
 */
DECLARE(ss_subsystem_version)
{
     printf("%s\n", ss_info(sci_idx)->subsystem_version);
}

/*
 * ss_unimplemented -- routine not implemented (should be
 * set up as (dont_list,dont_summarize))
 */
DECLARE(ss_unimplemented)
{
     ss_perror(sci_idx, SS_ET_UNIMPLEMENTED, "");
}
