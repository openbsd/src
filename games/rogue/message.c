/*	$OpenBSD: message.c,v 1.8 2002/07/18 07:13:57 pjanzen Exp $	*/
/*	$NetBSD: message.c,v 1.5 1995/04/22 10:27:43 cgd Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Timothy C. Stoehr.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)message.c	8.1 (Berkeley) 5/31/93";
#else
static const char rcsid[] = "$OpenBSD: message.c,v 1.8 2002/07/18 07:13:57 pjanzen Exp $";
#endif
#endif /* not lint */

/*
 * message.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#include <signal.h>
#include <termios.h>
#include <stdarg.h>
#include "rogue.h"

char msgs[NMESSAGES][DCOLS] = {"", "", "", "", ""};
short msg_col = 0, imsg = -1;
boolean msg_cleared = 1, rmsg = 0;
char hunger_str[HUNGER_STR_LEN] = "";
const char *more = "-more-";

static void message(const char *, boolean);

static void
message(msg, intrpt)
	const char *msg;
	boolean intrpt;
{
	cant_int = 1;

	if (!save_is_interactive) {
		return;
	}
	if (intrpt) {
		interrupted = 1;
		md_slurp();
	}

	if (!msg_cleared) {
		mvaddstr(MIN_ROW-1, msg_col, more);
		refresh();
		wait_for_ack();
		check_message();
	}
	if (!rmsg) {
		imsg = (imsg + 1) % NMESSAGES;
		strlcpy(msgs[imsg], msg, sizeof(msgs[imsg]));
	}
	mvaddstr(MIN_ROW-1, 0, msg);
	addch(' ');
	refresh();
	msg_cleared = 0;
	msg_col = strlen(msg);

	cant_int = 0;

	if (did_int) {
		did_int = 0;
		onintr(0);
	}
}

void
messagef(boolean intrpt, const char *fmt, ...)
{
	va_list ap;
	char buf[DCOLS];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	message(buf, intrpt);
}

void
remessage(c)
	short c;
{
	if (imsg != -1) {
		check_message();
		rmsg = 1;
		while (c > imsg) {
			c -= NMESSAGES;
		}
		message(msgs[((imsg - c) % NMESSAGES)], 0);
		rmsg = 0;
		move(rogue.row, rogue.col);
		refresh();
	}
}

void
check_message()
{
	if (msg_cleared) {
		return;
	}
	move(MIN_ROW-1, 0);
	clrtoeol();
	refresh();
	msg_cleared = 1;
}

int
get_input_line(prompt, insert, buf, buflen, if_cancelled, add_blank, do_echo)
	const char *prompt;
	const char *insert;
    	char *buf;
	size_t buflen;
	const char *if_cancelled;
	boolean add_blank;
	boolean do_echo;
{
	int ch;
	int i = 0, n;

	message(prompt, 0);
	n = strlen(prompt);

	if (insert[0]) {
		mvaddstr(0, n + 1, insert);
		strlcpy(buf, insert, buflen);
		i = strlen(buf);
		move(0, (n + i + 1));
		refresh();
	}

	while (((ch = rgetchar()) != '\r') && (ch != '\n') && (ch != CANCEL)) {
		if ((ch >= ' ') && (ch <= '~') && (i < (int)(buflen - 2))) {
			if ((ch != ' ') || (i > 0)) {
				buf[i++] = ch;
				if (do_echo) {
					addch(ch);
				}
			}
		}
		if (((ch == '\b') || (ch == erasechar())) && (i > 0)) {
			if (do_echo) {
				mvaddch(0, i + n, ' ');
				move(MIN_ROW-1, i+n);
			}
			i--;
		}
		refresh();
	}
	check_message();
	if (add_blank) {
		buf[i++] = ' ';
	} else {
		while ((i > 0) && (buf[i-1] == ' ')) {
			i--;
		}
	}

	buf[i] = '\0';

	if ((ch == CANCEL) || (i == 0) || ((i == 1) && add_blank)) {
		if (if_cancelled) {
			message(if_cancelled, 0);
		}
		return(0);
	}
	return(i);
}

int
rgetchar()
{
	int ch;

	for(;;) {
		ch = getchar();

		switch(ch) {
		case '\022':
			wrefresh(curscr);
			break;
		case '&':
			save_screen();
			break;
		case EOF:
			clean_up("");
			break;
		default:
			return(ch);
		}
	}
}

/*
Level: 99 Gold: 999999 Hp: 999(999) Str: 99(99) Arm: 99 Exp: 21/10000000 Hungry
0    5    1    5    2    5    3    5    4    5    5    5    6    5    7    5
*/

void
print_stats(stat_mask)
	int stat_mask;
{
	char buf[16];
	boolean label;
	short row = DROWS - 1;

	label = (stat_mask & STAT_LABEL) ? 1 : 0;

	if (stat_mask & STAT_LEVEL) {
		if (label) {
			mvaddstr(row, 0, "Level: ");
		}
		/* max level taken care of in make_level() */
		mvprintw(row, 7, "%-2d", cur_level);
	}
	if (stat_mask & STAT_GOLD) {
		if (label) {
			mvaddstr(row, 10, "Gold: ");
		}
		if (rogue.gold > MAX_GOLD) {
			rogue.gold = MAX_GOLD;
		}
		mvprintw(row, 16, "%-6ld", rogue.gold);
	}
	if (stat_mask & STAT_HP) {
		if (label) {
			mvaddstr(row, 23, "Hp: ");
		}
		if (rogue.hp_max > MAX_HP) {
			rogue.hp_current -= (rogue.hp_max - MAX_HP);
			rogue.hp_max = MAX_HP;
		}
		snprintf(buf, sizeof(buf), "%d(%d)",
		    rogue.hp_current, rogue.hp_max);
		mvprintw(row, 27, "%-8s", buf);
	}
	if (stat_mask & STAT_STRENGTH) {
		if (label) {
			mvaddstr(row, 36, "Str: ");
		}
		if (rogue.str_max > MAX_STRENGTH) {
			rogue.str_current -= (rogue.str_max - MAX_STRENGTH);
			rogue.str_max = MAX_STRENGTH;
		}
		snprintf(buf, sizeof(buf), "%d(%d)",
		    (rogue.str_current + add_strength), rogue.str_max);
		mvprintw(row, 41, "%-6s", buf);
	}
	if (stat_mask & STAT_ARMOR) {
		if (label) {
			mvaddstr(row, 48, "Arm: ");
		}
		if (rogue.armor && (rogue.armor->d_enchant > MAX_ARMOR)) {
			rogue.armor->d_enchant = MAX_ARMOR;
		}
		mvprintw(row, 53, "%-2d", get_armor_class(rogue.armor));
	}
	if (stat_mask & STAT_EXP) {
		if (label) {
			mvaddstr(row, 56, "Exp: ");
		}
		if (rogue.exp_points > MAX_EXP) {
			rogue.exp_points = MAX_EXP;
		}
		if (rogue.exp > MAX_EXP_LEVEL) {
			rogue.exp = MAX_EXP_LEVEL;
		}
		snprintf(buf, sizeof(buf), "%d/%ld",
		    rogue.exp, rogue.exp_points);
		mvprintw(row, 61, "%-11s", buf);
	}
	if (stat_mask & STAT_HUNGER) {
		mvaddstr(row, 73, hunger_str);
		clrtoeol();
	}
	refresh();
}

void
save_screen()
{
	FILE *fp;
	short i, j;
	char buf[DCOLS+2];

	if ((fp = fopen("rogue.screen", "w")) != NULL) {
		for (i = 0; i < DROWS; i++) {
			for (j = 0; j < DCOLS; j++) {
				buf[j] = mvinch(i, j);
			}
			for (j = DCOLS; j > 0 && buf[j - 1] == ' '; j--)
				;
			buf[j] = '\0';
			fputs(buf, fp);
			putc('\n', fp);
		}
		fclose(fp);
	} else {
		beep();
	}
}

boolean
is_digit(ch)
	short ch;
{
	return((ch >= '0') && (ch <= '9'));
}

int
r_index(str, ch, last)
	char *str;
	int ch;
	boolean last;
{
	int i = 0;

	if (last) {
		for (i = strlen(str) - 1; i >= 0; i--) {
			if (str[i] == ch) {
				return(i);
			}
		}
	} else {
		for (i = 0; str[i]; i++) {
			if (str[i] == ch) {
				return(i);
			}
		}
	}
	return(-1);
}
