/*	$OpenBSD: db_hangman.c,v 1.19 2001/11/06 19:53:18 miod Exp $	*/

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
 *
 */

#include <sys/param.h>

#include <uvm/uvm_extern.h>

#include <machine/db_machdep.h>

#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#include <ddb/db_output.h>

#include <dev/cons.h>
#include <dev/rndvar.h>

#define	TOLOWER(c)	(('A'<=(c)&&(c)<='Z')?(c)-'A'+'a':(c))
#define	ISALPHA(c)	(('a'<=(c)&&(c)<='z')||('A'<=(c)&&(c)<='Z'))

void	 db_hang __P((int, char *, char *));

static int	skill;
u_long		db_plays, db_guesses;

static __inline size_t
db_random(size_t mod)
{
	return arc4random() % mod;
}

struct db_hang_forall_arg {
	int cnt;
	db_sym_t sym;
};

/*
 * Horrible abuse of the forall function, but we're not in a hurry.
 */
static void db_hang_forall __P((db_symtab_t *, db_sym_t, char *, char *, int,
			void *));

static void
db_hang_forall(stab, sym, name, suff, pre, varg)
	db_symtab_t *stab;
	db_sym_t sym;
	char *name;
	char *suff;
	int pre;
	void *varg;
{
	struct db_hang_forall_arg *arg = (struct db_hang_forall_arg *)varg;

	if (--arg->cnt == 0)
		arg->sym = sym;
}

static __inline char *
db_randomsym(size_t *lenp)
{
	extern db_symtab_t db_symtabs[];
	db_symtab_t *stab;
	int nsymtabs, nsyms;
	char	*p, *q;
	struct db_hang_forall_arg dfa;

	for (nsymtabs = 0; db_symtabs[nsymtabs].name != NULL; nsymtabs++)
		;

	if (nsymtabs == 0)
		return (NULL);

	stab = &db_symtabs[db_random(nsymtabs)];

	dfa.cnt = 1000000;
	X_db_forall(stab, db_hang_forall, &dfa);
	if (dfa.cnt <= 0)
		return (NULL);
	nsyms = 1000000 - dfa.cnt;

	if (nsyms == 0)
		return (NULL);

	dfa.cnt = db_random(nsyms);
	X_db_forall(stab, db_hang_forall, &dfa);

	q = db_qualify(dfa.sym, stab->name);

	/* don't show symtab name if there are less than 3 of 'em */
	if (nsymtabs < 3)
		while(*q++ != ':');

	/* strlen(q) && ignoring underscores and colons */
	for ((*lenp) = 0, p = q; *p; p++)
		if (ISALPHA(*p))
			(*lenp)++;

	return (q);
}

static const char hangpic[]=
	"\n88888 \r\n"
	"9 7 6 \r\n"
	"97  5 \r\n"
	"9  423\r\n"
	"9   2 \r\n"
	"9  1 0\r\n"
	"9\r\n"
	"9  ";
static const char substchar[]="\\/|\\/O|/-|";

void
db_hang(tries, word, abc)
	int	tries;
	register char	*word;
	register char	*abc;
{
	register const char	*p;

	for(p = hangpic; *p; p++)
		cnputc((*p >= '0' && *p <= '9') ? ((tries <= (*p) - '0') ?
		    substchar[(*p) - '0'] : ' ') : *p);

	for (p = word; *p; p++)
		cnputc(ISALPHA(*p) && abc[TOLOWER(*p) - 'a'] == '-'? '-' : *p);

	db_printf(" (");

	for (p = abc; *p; p++)
		if (*p == '_')
			cnputc('a' + (p - abc));

	db_printf(")\r");
}


static __inline int
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

		tries = skill + 1;
		word = db_randomsym(&len);
		if (word == NULL)
			return (0);

		db_plays++;
	}

	{
		register char	c;

		db_hang(tries, word, abc);
		c = cngetc();
		c = TOLOWER(c);

		if (ISALPHA(c) && abc[c - 'a'] == '-') {
			register char	*p;
			register size_t	n;

				/* strchr(word,c) */
			for (n = 0, p = word; *p ; p++)
				if (TOLOWER(*p) == c)
					n++;

			if (n) {
				abc[c - 'a'] = c;
				len -= n;
			} else {
				abc[c - 'a'] = '_';
				tries--;
			}
		}
	}

	if (tries && len)
		return (1);

	if (!tries && skill > 2) {
		register char	*p = word;
		for (; *p; p++)
			if (ISALPHA(*p))
				abc[TOLOWER(*p) - 'a'] = *p;
	}
	db_hang(tries, word, abc);
	db_printf("\nScore: %lu/%lu\n", db_plays, ++db_guesses);
	word = NULL;

	return (!tries);
}

void
db_hangman(addr, haddr, count, modif)
	db_expr_t addr;
	int	haddr;
	db_expr_t count;
	char	*modif;
{
	if (modif[0] == 's' && '0' <= modif[1] && modif[1] <= '9')
		skill = modif[1] - '0';
	else
		skill = 3;

	while (db_hangon());
}
