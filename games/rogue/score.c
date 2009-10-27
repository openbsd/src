/*	$OpenBSD: score.c,v 1.12 2009/10/27 23:59:26 deraadt Exp $	*/
/*	$NetBSD: score.c,v 1.5 1995/04/22 10:28:26 cgd Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * score.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "rogue.h"
#include "pathnames.h"

void
killed_by(const object *monster, short other)
{
	const char *mechanism = "killed by something unknown (?)";
	char mechanism_buf[128];
	const char *article;
	char message_buf[128];

	md_ignore_signals();

	if (other != QUIT) {
		rogue.gold = ((rogue.gold * 9) / 10);
	}

	if (other) {
		switch(other) {
		case HYPOTHERMIA:
			mechanism = "died of hypothermia";
			break;
		case STARVATION:
			mechanism = "died of starvation";
			break;
		case POISON_DART:
			mechanism = "killed by a dart";
			break;
		case QUIT:
			mechanism = "quit";
			break;
		case KFIRE:
			mechanism = "killed by fire";
			break;
		}
	} else {
		if (is_vowel(m_names[monster->m_char - 'A'][0])) {
			article = "an";
		} else {
			article = "a";
		}
		snprintf(mechanism_buf, sizeof(mechanism_buf),
		    "Killed by %s %s", article, m_names[monster->m_char - 'A']);
		mechanism = mechanism_buf;
	}
	snprintf(message_buf, sizeof(message_buf),
	    "%s with %ld gold", mechanism, rogue.gold);
	if ((!other) && (!no_skull)) {
		clear();
		mvaddstr(4, 32, "__---------__");
		mvaddstr(5, 30, "_~             ~_");
		mvaddstr(6, 29, "/                 \\");
		mvaddstr(7, 28, "~                   ~");
		mvaddstr(8, 27, "/                     \\");
		mvaddstr(9, 27, "|    XXXX     XXXX    |");
		mvaddstr(10, 27, "|    XXXX     XXXX    |");
		mvaddstr(11, 27, "|    XXX       XXX    |");
		mvaddstr(12, 28, "\\         @         /");
		mvaddstr(13, 29, "--\\     @@@     /--");
		mvaddstr(14, 30, "| |    @@@    | |");
		mvaddstr(15, 30, "| |           | |");
		mvaddstr(16, 30, "| vvVvvvvvvvVvv |");
		mvaddstr(17, 30, "|  ^^^^^^^^^^^  |");
		mvaddstr(18, 31, "\\_           _/");
		mvaddstr(19, 33, "~---------~");
		center(21, nick_name);
		center(22, message_buf);
	} else {
		messagef(0, "%s", message_buf);
	}
	messagef(0, "");
	put_scores(monster, other);
}

void
win(void)
{
	unwield(rogue.weapon);		/* disarm and relax */
	unwear(rogue.armor);
	un_put_on(rogue.left_ring);
	un_put_on(rogue.right_ring);

	clear();
	mvaddstr(10, 11, "@   @  @@@   @   @      @  @  @   @@@   @   @   @");
	mvaddstr(11, 11, " @ @  @   @  @   @      @  @  @  @   @  @@  @   @");
	mvaddstr(12, 11, "  @   @   @  @   @      @  @  @  @   @  @ @ @   @");
	mvaddstr(13, 11, "  @   @   @  @   @      @  @  @  @   @  @  @@");
	mvaddstr(14, 11, "  @    @@@    @@@        @@ @@    @@@   @   @   @");
	mvaddstr(17, 11, "Congratulations,  you have  been admitted  to  the");
	mvaddstr(18, 11, "Fighters' Guild.   You return home,  sell all your");
	mvaddstr(19, 11, "treasures at great profit and retire into comfort.");
	messagef(0, "");
	messagef(0, "");
	id_all();
	sell_pack();
	put_scores((object *) 0, WIN);
}

void
quit(boolean from_intrpt)
{
	char buf[DCOLS];
	short i, orow, ocol;
	boolean mc;
	short ch;

	md_ignore_signals();

	if (from_intrpt) {
		orow = rogue.row;
		ocol = rogue.col;

		mc = msg_cleared;

		for (i = 0; i < DCOLS; i++) {
			buf[i] = mvinch(0, i);
		}
	}
	check_message();
	messagef(1, "really quit?");
	if ((ch = rgetchar()) != 'y' && ch != 'Y') {
		md_heed_signals();
		check_message();
		if (from_intrpt) {
			for (i = 0; i < DCOLS; i++) {
				mvaddch(0, i, buf[i]);
			}
			msg_cleared = mc;
			move(orow, ocol);
			refresh();
		}
		return;
	}
	if (from_intrpt) {
		clean_up(byebye_string);
	}
	check_message();
	killed_by((object *) 0, QUIT);
}

/*
 * The score file on disk is up to ten entries of the form
 *      score block [80 bytes]
 *      nickname block [30 bytes]
 *
 * The score block is to be parsed as follows:
 *      bytes 0-1	Rank (" 1" to "10")
 *      bytes 2-4	space padding
 *      bytes 5-15	Score/gold
 *      byte 15 up to a ':'	Login name
 *      past the ':'    Death mechanism
 *
 * The nickname block is an alternate name to be printed in place of the
 * login name. Both blocks are supposed to contain a null-terminator.
 *
 * XXX This score file format is historic, but the sizes can lead to
 * truncation in cause of death and nickname.  Currently LOGIN_NAME_LEN
 * is short enough actual scorefile corruption is not possible.
 */

struct score_entry {
	long gold;
	char username[LOGIN_NAME_LEN];
	char death[80];
	char nickname[MAX_OPT_LEN + 1];
};

static void pad_spaces(char *, size_t);
static void unpad_spaces(char *);
static int read_score_entry(struct score_entry *, FILE *);
static void write_score_entry(const struct score_entry *, int, FILE *);
static void make_score(struct score_entry *, const object *, int);


static void
pad_spaces(char *str, size_t len)
{
	size_t x;

	for (x = strlen(str); x < len - 1; x++) {
		str[x] = ' ';
	}
	str[len-1] = '\0';
}

static void
unpad_spaces(char *str)
{
	size_t x;

	for (x = strlen(str); x > 0 && str[x - 1] == ' '; x--)
		;
	str[x] = '\0';
}

static int
read_score_entry(struct score_entry *se, FILE *fp)
{
	char score_block[80];
	char nickname_block[30];
	size_t n, x;

	n = fread(score_block, 1, sizeof(score_block), fp);
	if (n == 0) {
		/* EOF */
		return(0);
	}
	if (n != sizeof(score_block)) {
		sf_error();
	}

	n = fread(nickname_block, 1, sizeof(nickname_block), fp);
	if (n != sizeof(nickname_block)) {
		sf_error();
	}

	xxxx(score_block, sizeof(score_block));
	xxxx(nickname_block, sizeof(nickname_block));

	/* Ensure null termination */
	score_block[sizeof(score_block) - 1] = '\0';
	nickname_block[sizeof(nickname_block) - 1] = '\0';

	/* If there are other nulls in the score block, file is corrupt */
	if (strlen(score_block) != sizeof(score_block) - 1) {
		sf_error();
	}
	/* but this is NOT true of the nickname block */

	/* quash trailing spaces */
	unpad_spaces(score_block);
	unpad_spaces(nickname_block);

	se->gold = strtol(score_block + 5, NULL, 10);

	for (x = 15; score_block[x] != '\0' && score_block[x] != ':'; x++);
	if (score_block[x] == '\0') {
		sf_error();
	}
	score_block[x++] = '\0';
	strlcpy(se->username, score_block + 15, sizeof(se->username));

	strlcpy(se->death, score_block + x, sizeof(se->death));
	strlcpy(se->nickname, nickname_block, sizeof(se->nickname));

	return(1);
}

static void
write_score_entry(const struct score_entry *se, int rank, FILE *fp)
{
	char score_block[80];
	char nickname_block[30];

	/* avoid writing crap to score file */
	memset(nickname_block, 0, sizeof(nickname_block));

	snprintf(score_block, sizeof(score_block),
		 "%2d    %6ld   %s: %s",
		 rank + 1, se->gold, se->username, se->death);
	strlcpy(nickname_block, se->nickname, sizeof(nickname_block));

	/* pad blocks out with spaces */
	pad_spaces(score_block, sizeof(score_block));
	/*pad_spaces(nickname_block, sizeof(nickname_block)); -- wrong! */

	xxxx(score_block, sizeof(score_block));
	xxxx(nickname_block, sizeof(nickname_block));

	fwrite(score_block, 1, sizeof(score_block), fp);
	fwrite(nickname_block, 1, sizeof(nickname_block), fp);
}

void
put_scores(const object *monster, short other)
{
	short i, rank = -1, found_player = -1, numscores = 0;
	struct score_entry scores[NUM_SCORE_ENTRIES];
	const char *name;
	FILE *fp;
	boolean dopause = score_only;
	extern gid_t gid, egid;

	md_lock(1);

	setegid(egid);
	if ((fp = fopen(_PATH_SCOREFILE, "r+")) == NULL &&
	    (fp = fopen(_PATH_SCOREFILE, "w+")) == NULL) {
		setegid(gid);
		messagef(0, "cannot read/write/create score file");
		sf_error();
	}
	setegid(gid);
	rewind(fp);
	(void) xxx(1);

	for (numscores = 0; numscores < NUM_SCORE_ENTRIES; numscores++) {
		if (read_score_entry(&scores[numscores], fp) == 0)
			break;
	}

	/* Search the score list. */
	for (i = 0; i < numscores; i++) {
		if (!strcmp(scores[i].username, login_name)) {
			if (rogue.gold < scores[i].gold) {
				score_only = 1;
			} else {
				/* we did better; mark entry for removal */
				found_player = i;
			}
			break;
		}
	}

	/* Remove a superseded entry, if any. */
	if (found_player != -1) {
		numscores--;
		for (i = found_player; i < numscores; i++) {
			scores[i] = scores[i + 1];
		}
	}

	/* If we're going to insert ourselves, do it now */
	if (!score_only) {
		/* If we aren't better than anyone, add at end; otherwise, find
		 * our slot.
		 */
		rank = numscores;
		for (i = 0; i < numscores; i++) {
			if (rogue.gold >= scores[i].gold) {
				rank = i;
				break;
			}
		}
		if (rank < NUM_SCORE_ENTRIES) {
			for (i = numscores; i > rank; i--) {
				scores[i] = scores[i-1];
			}
			numscores++;
			make_score(&scores[rank], monster, other);
		}

		/* Now rewrite the score file */
		md_ignore_signals();
		rewind(fp);
		(void) xxx(1);
		for (i = 0; i < numscores; i++) {
			write_score_entry(&scores[i], i, fp);
		}
	}
	md_lock(0);
	fclose(fp);

	/* Display scores */
	clear();
	mvaddstr(3, 30, "Top  Ten  Rogueists");
	mvaddstr(8, 0, "Rank   Score   Name");
	for (i = 0; i < numscores; i++) {
		if (i == rank) {
			standout();
		}
		if (scores[i].nickname[0]) {
			name = scores[i].nickname;
		} else {
			name = scores[i].username;
		}
		mvprintw(i+10, 0, "%2d    %6ld   %s: %s",
		    i + 1, scores[i].gold, name, scores[i].death);
		if (i == rank) {
			standend();
		}
	}
	refresh();
	messagef(0, "");
	if (dopause) {
		messagef(0, "");
	}
	clean_up("");
}

static void
make_score(struct score_entry *se, const object *monster, int other)
{
	const char *death = "bolts from the blue (?)";
	const char *hasamulet;
	char deathbuf[80];

	se->gold = rogue.gold;
	strlcpy(se->username, login_name, sizeof(se->username));
	if (other) {
		switch(other) {
		case HYPOTHERMIA:
			death = "died of hypothermia";
			break;
		case STARVATION:
			death = "died of starvation";
			break;
		case POISON_DART:
			death = "killed by a dart";
			break;
		case QUIT:
			death = "quit";
			break;
		case WIN:
			death = "a total winner";
			break;
		case KFIRE:
			death = "killed by fire";
			break;
		}
	} else {
		const char *mn, *article;

		mn = m_names[monster->m_char - 'A'];
		if (is_vowel(mn[0])) {
			article = "an";
		} else {
			article = "a";
		}
		snprintf(deathbuf, sizeof(deathbuf), "killed by %s %s",
		    article, mn);
		death = deathbuf;
	}
	if (other != WIN && has_amulet()) {
		hasamulet = " with amulet";
	} else {
		hasamulet = "";
	}
	snprintf(se->death, sizeof(se->death), "%s on level %d%s",
	    death, max_level, hasamulet);
	strlcpy(se->nickname, nick_name, sizeof(se->nickname));
}

boolean
is_vowel(short ch)
{
	return( (ch == 'a') ||
		(ch == 'e') ||
		(ch == 'i') ||
		(ch == 'o') ||
		(ch == 'u') );
}

void
sell_pack(void)
{
	object *obj;
	short row = 2, val;
	char buf[DCOLS];

	obj = rogue.pack.next_object;

	clear();
	mvaddstr(1, 0, "Value      Item");

	while (obj) {
		if (obj->what_is != FOOD) {
			obj->identified = 1;
			val = get_value(obj);
			rogue.gold += val;

			if (row < DROWS) {
				get_desc(obj, buf, sizeof(buf));
				mvprintw(row++, 0, "%5d      %s", val, buf);
			}
		}
		obj = obj->next_object;
	}
	refresh();
	if (rogue.gold > MAX_GOLD) {
		rogue.gold = MAX_GOLD;
	}
	messagef(0, "");
}

int
get_value(object *obj)
{
	short wc;
	int val;

	val = 0;
	wc = obj->which_kind;

	switch(obj->what_is) {
	case WEAPON:
		val = id_weapons[wc].value;
		if ((wc == ARROW) || (wc == DAGGER) || (wc == SHURIKEN) ||
			(wc == DART)) {
			val *= obj->quantity;
		}
		val += (obj->d_enchant * 85);
		val += (obj->hit_enchant * 85);
		break;
	case ARMOR:
		val = id_armors[wc].value;
		val += (obj->d_enchant * 75);
		if (obj->is_protected) {
			val += 200;
		}
		break;
	case WAND:
		val = id_wands[wc].value * (obj->class + 1);
		break;
	case SCROL:
		val = id_scrolls[wc].value * obj->quantity;
		break;
	case POTION:
		val = id_potions[wc].value * obj->quantity;
		break;
	case AMULET:
		val = 5000;
		break;
	case RING:
		val = id_rings[wc].value * (obj->class + 1);
		break;
	}
	if (val <= 0) {
		val = 10;
	}
	return(val);
}

void
id_all(void)
{
	short i;

	for (i = 0; i < SCROLS; i++) {
		id_scrolls[i].id_status = IDENTIFIED;
	}
	for (i = 0; i < WEAPONS; i++) {
		id_weapons[i].id_status = IDENTIFIED;
	}
	for (i = 0; i < ARMORS; i++) {
		id_armors[i].id_status = IDENTIFIED;
	}
	for (i = 0; i < WANDS; i++) {
		id_wands[i].id_status = IDENTIFIED;
	}
	for (i = 0; i < POTIONS; i++) {
		id_potions[i].id_status = IDENTIFIED;
	}
}

void
xxxx(char *buf, short n)
{
	short i;
	unsigned char c;

	for (i = 0; i < n; i++) {

		/* It does not matter if accuracy is lost during this assignment */
		c = (unsigned char) xxx(0);

		buf[i] ^= c;
	}
}

long
xxx(boolean st)
{
	static long f, s;
	long r;

	if (st) {
		f = 37;
		s = 7;
		return(0L);
	}
	r = ((f * s) + 9337) % 8887;
	f = s;
	s = r;
	return(r);
}

void
center(short row, const char *buf)
{
	short margin;

	margin = ((DCOLS - strlen(buf)) / 2);
	mvaddstr(row, margin, buf);
}

void
sf_error(void)
{
	md_lock(0);
	messagef(1, "");
	clean_up("sorry, score file is out of order");
}
