/*
 * Copyright (c) 1994-1996,1998-1999 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Sudo: insults.h,v 1.45 1999/12/06 06:47:13 millert Exp $
 */

#ifndef _SUDO_INSULTS_H
#define _SUDO_INSULTS_H

#if defined(HAL_INSULTS) || defined(GOONS_INSULTS) || defined(CLASSIC_INSULTS) || defined(CSOPS_INSULTS)

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

#endif /* HAL_INSULTS || GOONS_INSULTS || CLASSIC_INSULTS || CSOPS_INSULTS */

#endif /* _SUDO_INSULTS_H */
