/*	$OpenBSD: db_hangman.c,v 1.6 1996/06/17 12:07:00 mickey Exp $	*/

/*
 * Copyright (c) 1996 Theo de Raadt, Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#include <dev/cons.h>
#include "rnd.h"
#if NRND
#include <dev/rndvar.h>
#endif

#define	TOLOWER(c)	(('A'<=(c)&&(c)<='Z')?(c)-'a'+'A':(c))
#define	ISALPHA(c)	(('a'<=(c)&&(c)<='z')||('A'<=(c)&&(c)<='Z'))

static	__inline size_t db_random __P((size_t));
static	__inline char *db_randomsym __P((size_t *));
static	void db_hang __P((int, char *, char *));
static	int db_hangon __P((void));

static __inline size_t
db_random( mod )
	register size_t	mod;
{
#if NRND
	size_t	ret;
	get_random_bytes(&ret, sizeof(ret) );
	return ret % mod;
#else
	u_int random __P((void));
	return (size_t)(random() % mod);
#endif
}

static int	skill;

static __inline char *
db_randomsym(lenp)
	size_t	*lenp;
{
	register char	*p, *q;
		/* choose random symtab */
	register db_symtab_t	stab = db_istab(db_random(db_nsymtabs));

		/* choose random symbol from the table */
	q = db_qualify(X_db_isym(stab, db_random(X_db_nsyms(stab))),stab->name);

		/* don't show symtab name if there are less than 3 of 'em */
	if (db_nsymtabs < 3)
		while(*q++ != ':');

		/* strlen(q) && ignoring underscores and colons */
	for ((*lenp) = 0, p = q; *p; p++)
		if (ISALPHA(*p))
			(*lenp)++;

	return q;
}


static void
db_hang(tries, word, abc)
	int	tries;
	register char	*word;
	register char	*abc;
{
	register char	*p;

	cnputc(' ');
	for (p = word; *p; p++)
		if (ISALPHA(*p))
			cnputc(abc[TOLOWER(*p) - 'a']);
		else
			cnputc(*p);

	cnputc(' ');
	cnputc('(');
	cnputc('0' + tries);
	cnputc(' ');

	for (p = abc; *p; p++)
		if (*p == '_')
			cnputc('a' + (p - abc));

	cnputc(')');
	cnputc('\r');
}


static int
db_hangon(void)
{
	static size_t	len;
	static size_t	tries;
	static char	*word = NULL;
	static char	abc[26+1];	/* for '\0' */

	if (word == NULL) {
		register char	*p;

		for (p = abc; p < &abc[sizeof(abc)-1]; p++)
			*p = '-';
		*p = '\0';

		tries = 2 * (1 + skill / 3);
		word = db_randomsym(&len);
	}

	{
		register char	c, c1;

		db_hang(tries, word, abc);
		c1 = cngetc();

		c = TOLOWER(c1);
		if (ISALPHA(c) && abc[c - 'a'] == '-') {
			register char	*p;
			register size_t	n;

				/* strchr(word,c) */
			for (n = 0, p = word; *p ; p++)
				if (*p == c)
					n++;

			if (n) {
				abc[c - 'a'] = c1;
				len -= n;
			} else {
				abc[c - 'a'] = '_';
				tries--;
			}
		}
	}

	if (tries && len)
		return db_hangon();

	if (!tries && skill > 2)
	{
		register char	*p = word;
		for (; *p; p++)
			if (ISALPHA(*p))
				abc[TOLOWER(*p) - 'a'] = *p;
	}
	db_hang(tries, word, abc);
	cnputc('\n');
	word = NULL;

	return !tries;
}

void
db_hangman( addr, haddr, count, modif)
	db_expr_t addr;
	int	haddr;
	db_expr_t count;
	char	*modif;
{
	if (modif[0] == 's' && '0' <= modif[1] && modif[1] <= '9')
		skill = modif[1] - '0';
	else
		skill = 5;

	while (db_hangon());
}
