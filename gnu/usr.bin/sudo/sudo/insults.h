/*	$OpenBSD: insults.h,v 1.9 1999/03/29 20:29:04 millert Exp $	*/

/*
 *  CU sudo version 1.5.9
 *  Copyright (c) 1994,1996,1998,1999 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *  $Sudo: insults.h,v 1.39 1999/03/29 04:05:09 millert Exp $
 */

#ifndef _SUDO_INSULTS_H
#define _SUDO_INSULTS_H

#ifdef USE_INSULTS

/*
 * Use one or more set of insults as determined by configure
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
