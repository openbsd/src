/*	$OpenBSD: insults.h,v 1.6 1998/09/15 02:42:44 millert Exp $	*/

/*
 *  CU sudo version 1.5.6
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please send bugs, changes, problems to sudo-bugs@courtesan.com
 *
 *  $From: insults.h,v 1.32 1998/09/07 02:59:36 millert Exp $
 */

#ifndef _SUDO_INSULTS_H
#define _SUDO_INSULTS_H

#ifdef USE_INSULTS

#if !defined(HAL_INSULTS) && !defined(GOONS_INSULTS) && !defined(CLASSIC_INSULTS)
#  define CLASSIC_INSULTS
#  define CSOPS_INSULTS
#endif

/*
 * Use one or more set of insults as defined in options.h.
 */

char *insults[] = {

# ifdef HAL_INSULTS
#  include "ins_2001.h"
# endif

# ifdef GOONS_INSULTS
#  include "ins_goons.h"
# endif

# ifdef CLASSIC_INSULTS
#  include "ins_classic.h"
# endif

# ifdef CSOPS_INSULTS
#  include "ins_csops.h"
# endif

    (char *) 0

};

/*
 * How may I insult you?  Let me count the ways...
 */
#define NOFINSULTS (sizeof(insults) / sizeof(insults[0]) - 1)

/*
 * return a pseudo-random insult.
 */
#define INSULT		(insults[time(NULL) % NOFINSULTS])

#endif /* USE_INSULTS */

#endif /* _SUDO_INSULTS_H */
