/*	$OpenBSD: extern.h,v 1.2 1998/09/02 06:36:07 pjanzen Exp $	*/
/*	$NetBSD: extern.h,v 1.3 1997/10/11 01:55:27 lukem Exp $	*/

/*
 * Copyright (c) 1997 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <string.h>

/* crc.c */
void crc_start __P((void));
unsigned long crc __P((const char *, int));

/* done.c */
int score __P((void));
void done __P((int));
void die __P((int));

/* init.c */
void init __P((void));
char   *decr __P((int, int, int, int, int));
void linkdata __P((void));
void trapdel __P((int));
void startup __P((void));

/* io.c */
void getin __P((char **, char **));
int yes __P((int, int, int));
int yesm __P((int, int, int));
int next __P((void));
void rdata __P((void));
int rnum __P((void));
void rdesc __P((int));
void rtrav __P((void));
#ifdef DEBUG
void twrite __P((int));
#endif
void rvoc __P((void));
void rlocs __P((void));
void rdflt __P((void));
void rliq __P((void));
void rhints __P((void));
void rspeak __P((int));
void mspeak __P((int));
struct text;
void speak __P((const struct text *));
void pspeak __P((int, int));

/* main.c */
int main __P((int, char **));

/* save.c */
int save __P((const char *));
int restore __P((const char *));

/* subr.c */
int toting __P((int));
int here __P((int));
int at  __P((int));
int liq2 __P((int));
int liq __P((void));
int liqloc __P((int));
int bitset __P((int, int));
int forced __P((int));
int dark __P((int));
int pct __P((int));
int fdwarf __P((void));
int march __P((void));
int mback __P((void));
int specials __P((void));
int trbridge __P((void));
void badmove __P((void));
void bug __P((int));
void checkhints __P((void));
int trsay __P((void));
int trtake __P((void));
int dropper __P((void));
int trdrop __P((void));
int tropen __P((void));
int trkill __P((void));
int trtoss __P((void));
int trfeed __P((void));
int trfill __P((void));
void closing __P((void));
void caveclose __P((void));

/* vocab.c */
void dstroy __P((int));
void juggle __P((int));
void move __P((int, int));
int put __P((int, int, int));
void carry __P((int, int));
void drop __P((int, int));
int vocab __P((const char *, int, int));
void prht __P((void));

/* These three used to be functions in vocab.c */
#define copystr(src, dest)	strcpy((dest), (src))
#define weq(str1, str2)		(!strncmp((str1), (str2), 5))
#define length(str)			(strlen((str)) + 1)

/* wizard.c */
void datime __P((time_t *, time_t *));
void poof __P((void));
int Start __P((void));
int wizard __P((void));
void ciao __P((void));
int ran __P((int));
