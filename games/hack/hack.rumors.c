/*	$OpenBSD: hack.rumors.c,v 1.4 2003/03/16 21:22:36 camield Exp $	*/

/*
 * Copyright (c) 1985, Stichting Centrum voor Wiskunde en Informatica,
 * Amsterdam
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the Stichting Centrum voor Wiskunde en
 * Informatica, nor the names of its contributors may be used to endorse or
 * promote products derived from this software without specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982 Jay Fenlason <hack@gnu.org>
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
 * 3. The name of the author may not be used to endorse or promote products
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
static char rcsid[] = "$OpenBSD: hack.rumors.c,v 1.4 2003/03/16 21:22:36 camield Exp $";
#endif /* not lint */

#include	<stdio.h>
#include	"hack.h"		/* for RUMORFILE and BSD (index) */
#define	CHARSZ	8			/* number of bits in a char */
extern long *alloc();
int n_rumors = 0;
int n_used_rumors = -1;
char *usedbits;

init_rumors(rumf) register FILE *rumf; {
register int i;
	n_used_rumors = 0;
	while(skipline(rumf)) n_rumors++;
	rewind(rumf);
	i = n_rumors/CHARSZ;
	usedbits = (char *) alloc((unsigned)(i+1));
	for( ; i>=0; i--) usedbits[i] = 0;
}

skipline(rumf) register FILE *rumf; {
char line[COLNO];
	while(1) {
		if(!fgets(line, sizeof(line), rumf)) return(0);
		if(strchr(line, '\n')) return(1);
	}
}

outline(rumf) register FILE *rumf; {
char line[COLNO];
register char *ep;
	if(!fgets(line, sizeof(line), rumf)) return;
	if((ep = strchr(line, '\n')) != 0) *ep = 0;
	pline("This cookie has a scrap of paper inside! It reads: ");
	pline(line);
}

outrumor(){
register int rn,i;
register FILE *rumf;
	if(n_rumors <= n_used_rumors ||
	  (rumf = fopen(RUMORFILE, "r")) == (FILE *) 0) return;
	if(n_used_rumors < 0) init_rumors(rumf);
	if(!n_rumors) goto none;
	rn = rn2(n_rumors - n_used_rumors);
	i = 0;
	while(rn || used(i)) {
		(void) skipline(rumf);
		if(!used(i)) rn--;
		i++;
	}
	usedbits[i/CHARSZ] |= (1 << (i % CHARSZ));
	n_used_rumors++;
	outline(rumf);
none:
	(void) fclose(rumf);
}

used(i) register int i; {
	return(usedbits[i/CHARSZ] & (1 << (i % CHARSZ)));
}
