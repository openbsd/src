/*	$OpenBSD: tputs.c,v 1.3 1996/08/07 03:23:07 tholo Exp $	*/

/*
 * Copyright (c) 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by SigmaSoft, Th.  Lockert.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: tputs.c,v 1.3 1996/08/07 03:23:07 tholo Exp $";
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "term.h"

#undef ospeed
extern short ospeed;

static short tmspc10[] = {
    0, 2000, 1333, 909, 743, 666, 500, 333, 166, 83, 55, 41, 20, 10, 5
};

int
tputs(cp, count, outc)
     const char *cp;
     int count;
     int (*outc) __P((int));
{
    register int mspc10;
    float i = 0.0;
    char pc;

    if (cp == 0)
	return ERR;

    /*
     * The guts of the string.
     */
    while (*cp) {
	if (*cp != '$')
	    (*outc)(*cp++);
	else {
	    if (*++cp != '<') {
		(*outc)('$');
		(*outc)(*cp);
	    }
	    else {
		cp++;
		if ((!isdigit(*cp) && *cp != '.') || strchr(cp, '>') == NULL) {
		    (*outc)('$');
		    (*outc)('<');
		    continue;
		}
		while (isdigit(*cp)) {
		    i *= 10;
		    i += *cp++ - '0';
		}
		if (*cp == '.') {
		    if (isdigit(*++cp))
			i += (float)(*cp - '0') / 10.0;
		    while (isdigit(*cp))
			cp++;
		}
		if (*cp == '*') {
		    i *= count;
		    cp++;
		}
		if (*cp == '/')
		    cp++;
	    }
	    if (*cp)
		cp++;
	}
    }

    /*
     * If no delay needed, or output speed is
     * not comprehensible, then don't try to delay.
     */
    if (i > 0.0 ||
	(padding_baud_rate != 0 && cur_term->baudrate < padding_baud_rate))
	return OK;
    if (ospeed <= 0 || ospeed >= (sizeof tmspc10 / sizeof tmspc10[0]))
	return OK;

    /*
     * Round up by a half a character frame,
     * and then do the delay.
     * Too bad there are no user program accessible programmed delays.
     * Transmitting pad characters slows many
     * terminals down and also loads the system.
     */
    mspc10 = tmspc10[ospeed];
    i += mspc10 / 2.0;
    pc = (pad_char && *pad_char) ? *pad_char : '\0';
    for (i /= mspc10; i > 0; i -= 1.0)
	(*outc)(pc);
    return OK;
}
