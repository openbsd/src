/* $OpenBSD: mail.c,v 1.8 2005/10/14 19:46:46 kjell Exp $ */
/*
 * This file is in the public domain.
 *
 * Author: Vincent Labrecque, April 2003
 */
#include <ctype.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

void		 mail_init(void);
static int	 fake_self_insert(int, int);
static int	 mail(int, int);

int limit = 72;

/* mappings for all "printable" characters ('-' -> '~') */
static PF mail_fake[] = {
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert, fake_self_insert,
	fake_self_insert, fake_self_insert, fake_self_insert
};

static struct KEYMAPE (1 + IMAPEXT) mailmap = {
	1,
	1 + IMAPEXT,
	rescan,
	{
		{ ' ', '~', mail_fake, NULL }
	}
};

int
mail_set_limit(int f, int n)
{
	char buf[32], *rep;

	if ((f & FFARG) != 0) {
		limit = n;
	} else {
		if ((rep = eread("Margin: ", buf, sizeof(buf),
		    EFNEW | EFCR)) == NULL)
			return (ABORT);
		else if (rep[0] == '\0')
			return (FALSE);
		limit = atoi(rep);
	}
	return (TRUE);
}

void
mail_init(void)
{
	funmap_add(mail, "mail-mode");
	funmap_add(mail_set_limit, "mail-set-margin");
	maps_add((KEYMAP *)&mailmap, "mail-mode");
}

/* ARGSUSED */
static int
mail(int f, int n)
{
	curbp->b_modes[0] = name_mode("fundamental");
	curbp->b_modes[1] = name_mode("mail-mode");
	if (curbp->b_modes[1] == NULL) {
		panic("can't happen");
		mail_init();
		curbp->b_modes[1] = name_mode("mail-mode");
	}
	curbp->b_nmodes = 1;
	curwp->w_flag |= WFMODE;
	return (TRUE);
}

static int
fake_self_insert(int f, int n)
{
	int len = llength(curwp->w_dotp), col;

	if (len + 1 > limit) {
		/*
		 * Find the column at which we should cut, taking
		 * word boundaries into account.
		 */
		for (col = limit; col > 0; col--)
			if (isspace(curwp->w_dotp->l_text[col])) {
				col++;	/* XXX - skip the space */
				break;
			}

		if (curbp->b_doto == len) {
			/*
			 * User is appending to the line; simple case.
			 */
			if (col) {
				curwp->w_doto = col;
				lnewline();
				gotoeol(0, 1);
			}
			curwp->w_wrapline = NULL;
		} else if ((len - col) > 0) {
			/*
			 * User is shifting words by inserting in the middle.
			 */
			const char *trail;
			int save_doto = curwp->w_doto;
			LINE *save_dotp = curwp->w_dotp;
			int tlen = len - col;

			trail = curwp->w_dotp->l_text + col;

			/*
			 * Create a new line or reuse the last wrapping line
			 * unless we could fill it.
			 */
			if (curwp->w_wrapline != lforw(curwp->w_dotp) ||
			    llength(curwp->w_wrapline) + tlen >= limit) {
				curwp->w_doto = col;
				lnewline();
				curwp->w_wrapline = curwp->w_dotp;
				curwp->w_dotp = save_dotp;
			} else {
				curwp->w_dotp = curwp->w_wrapline;
				curwp->w_doto = 0;

				/* insert trail */
				linsert_str(trail, tlen);

				/* delete trail */
				curwp->w_dotp = save_dotp;
				curwp->w_doto = col;
				ldelete(tlen, KNONE);	/* don't store in kill ring */
			}

			/*
			 * Readjust dot to point at where the user expects
			 * it to be after an insertion.
			 */
			curwp->w_doto = save_doto;
			if (curwp->w_doto >= col) {
				curwp->w_dotp = curwp->w_wrapline;
				curwp->w_doto -= col;
			}
		}
	}
	selfinsert(f, n);
	return (TRUE);
}
