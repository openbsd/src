/* $OpenBSD: mail.c,v 1.2 2004/01/12 22:55:00 vincent Exp $ */
/*
 * This file is in the public domain.
 *
 * Author: Vincent Labrecque, April 2003
 */
#include <ctype.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

#define LIMIT	72

static int	 fake_self_insert(int, int);
static int	 mail(int, int);

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
	fake_self_insert, fake_self_insert, fake_self_insert,
};

static struct KEYMAPE (1 + IMAPEXT) mailmap = {
	1,
	1 + IMAPEXT,
	rescan,
	{
		{ ' ', '~', mail_fake, NULL },
	}
};

void
mail_init(void)
{
	funmap_add(mail, "mail-mode");
	maps_add((KEYMAP *)&mailmap, "mail-mode");
}

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
	if (curwp->w_doto >= LIMIT - 1) {
		int save = curwp->w_doto;

		/*
		 * Find the last word boundary.
		 */
		while (curwp->w_doto > 0 &&
		    !isspace(curwp->w_dotp->l_text[curwp->w_doto - 1]))
			curwp->w_doto--;
		/*
		 * handle lines without any spaces correctly!
		 */
		if (curwp->w_doto == 0 && !isspace(curwp->w_dotp->l_text[0]))
			curwp->w_doto = save;
		newline(FFRAND, 1);
		gotoeol(0, 1);
	}
	selfinsert(f, n);
	return (TRUE);
}
