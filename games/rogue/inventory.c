/*	$OpenBSD: inventory.c,v 1.6 2002/07/26 19:52:03 pjanzen Exp $	*/
/*	$NetBSD: inventory.c,v 1.3 1995/04/22 10:27:35 cgd Exp $	*/

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
static char sccsid[] = "@(#)inventory.c	8.1 (Berkeley) 5/31/93";
#else
static const char rcsid[] = "$OpenBSD: inventory.c,v 1.6 2002/07/26 19:52:03 pjanzen Exp $";
#endif
#endif /* not lint */

/*
 * inventory.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#include <stdarg.h>
#include "rogue.h"

boolean is_wood[WANDS];
char *press_space = " --press space to continue--";

char *wand_materials[WAND_MATERIALS] = {
	"steel ",
	"bronze ",
	"gold ",
	"silver ",
	"copper ",
	"nickel ",
	"cobalt ",
	"tin ",
	"iron ",
	"magnesium ",
	"chrome ",
	"carbon ",
	"platinum ",
	"silicon ",
	"titanium ",

	"teak ",
	"oak ",
	"cherry ",
	"birch ",
	"pine ",
	"cedar ",
	"redwood ",
	"balsa ",
	"ivory ",
	"walnut ",
	"maple ",
	"mahogany ",
	"elm ",
	"palm ",
	"wooden "
};

char *gems[GEMS] = {
	"diamond ",
	"stibotantalite ",
	"lapi-lazuli ",
	"ruby ",
	"emerald ",
	"sapphire ",
	"amethyst ",
	"quartz ",
	"tiger-eye ",
	"opal ",
	"agate ",
	"turquoise ",
	"pearl ",
	"garnet "
};

char *syllables[MAXSYLLABLES] = {
	"blech ",
	"foo ",
	"barf ",
	"rech ",
	"bar ",
	"blech ",
	"quo ",
	"bloto ",
	"oh ",
	"caca ",
	"blorp ",
	"erp ",
	"festr ",
	"rot ",
	"slie ",
	"snorf ",
	"iky ",
	"yuky ",
	"ooze ",
	"ah ",
	"bahl ",
	"zep ",
	"druhl ",
	"flem ",
	"behil ",
	"arek ",
	"mep ",
	"zihr ",
	"grit ",
	"kona ",
	"kini ",
	"ichi ",
	"tims ",
	"ogr ",
	"oo ",
	"ighr ",
	"coph ",
	"swerr ",
	"mihln ",
	"poxi "
};

#define COMS 48

struct id_com_s {
	short com_char;
	char *com_desc;
};

struct id_com_s com_id_tab[COMS] = {
	{'?',	"?       prints help"},
	{'r',	"r       read scroll"},
	{'/',	"/       identify object"},
	{'e',	"e       eat food"},
	{'h',	"h       left "},
	{'w',	"w       wield a weapon"},
	{'j',	"j       down"},
	{'W',	"W       wear armor"},
	{'k',	"k       up"},
	{'T',	"T       take armor off"},
	{'l',	"l       right"},
	{'P',	"P       put on ring"},
	{'y',	"y       up & left"},
	{'R',	"R       remove ring"},
	{'u',	"u       up & right"},
	{'d',	"d       drop object"},
	{'b',	"b       down & left"},
	{'c',	"c       call object"},
	{'n',	"n       down & right"},
	{'\0',	"<SHIFT><dir>: run that way"},
	{')',	")       print current weapon"},
	{'\0',	"<CTRL><dir>: run till adjacent"},
	{']',	"]       print current armor"},
	{'f',	"f<dir>  fight till death or near death"},
	{'=',	"=       print current rings"},
	{'t',	"t<dir>  throw something"},
	{'\001',	"^A      print Hp-raise average"},
	{'m',	"m<dir>  move onto without picking up"},
	{'z',	"z<dir>  zap a wand in a direction"},
	{'o',	"o       examine/set options"},
	{'^',	"^<dir>  identify trap type"},
	{'\022',	"^R      redraw screen"},
	{'&',	"&       save screen into 'rogue.screen'"},
	{'s',	"s       search for trap/secret door"},
	{'\020',	"^P      repeat last message"},
	{'>',	">       go down a staircase"},
	{'\033',	"^[      cancel command"},
	{'<',	"<       go up a staircase"},
	{'S',	"S       save game"},
	{'.',	".       rest for a turn"},
	{'Q',	"Q       quit"},
	{',',	",       pick something up"},
	{'!',	"!       shell escape"},
	{'i',	"i       inventory"},
	{'F',	"F<dir>  fight till either of you dies"},
	{'I',	"I       inventory single item"},
	{'v',	"v       print version number"},
	{'q',	"q       quaff potion"}
};

void
inventory(pack, mask)
	object *pack;
	unsigned short mask;
{
	object *obj;
	short i = 0, j;
	size_t maxlen = 0, n;
	short row, col;
	struct {
		short letter;
		short sepchar;
		char desc[DCOLS];
		char savebuf[DCOLS+8];
	} descs[MAX_PACK_COUNT+1];

	obj = pack->next_object;

	if (!obj) {
		messagef(0, "your pack is empty");
		return;
	}
	while (obj) {
		if (obj->what_is & mask) {
			descs[i].letter = obj->ichar;
			descs[i].sepchar = ((obj->what_is & ARMOR) && obj->is_protected)
				? '}' : ')';
			get_desc(obj, descs[i].desc, sizeof(descs[i].desc));
			n = strlen(descs[i].desc) + 4;
			if (n > maxlen) {
				maxlen = n;
			}
			i++;
			if (i > MAX_PACK_COUNT) {
				clean_up("Too many objects in pack?!?");
			}
		}
		obj = obj->next_object;
	}
	if (maxlen < 27)
		maxlen = 27;
	if (maxlen > DCOLS - 2)
		maxlen = DCOLS - 2;
	col = DCOLS - (maxlen + 2);

	for (row = 0; ((row <= i) && (row < DROWS)); row++) {
		for (j = col; j < DCOLS; j++) {
			descs[row].savebuf[j - col] = mvinch(row, j);
		}
		descs[row].savebuf[j - col] = '\0';
		if (row < i) {
			mvprintw(row, col, " %c%c %s",
			    descs[row].letter, descs[row].sepchar,
			    descs[row].desc);
		} else {
			mvaddstr(row, col, press_space);
		}
		clrtoeol();
	}
	refresh();
	wait_for_ack();

	move(0, 0);
	clrtoeol();

	for (j = 1; ((j <= i) && (j < DROWS)); j++) {
		mvaddstr(j, col, descs[j].savebuf);
	}
}

void
id_com()
{
	int ch = 0;
	short i, j, k;

	while (ch != CANCEL) {
		check_message();
		messagef(0, "Character you want help for (* for all):");

		refresh();
		ch = getch();

		switch(ch) {
		case LIST:
			{
				char save[(((COMS / 2) + (COMS % 2)) + 1)][DCOLS];
				short rows = (((COMS / 2) + (COMS % 2)) + 1);
				boolean need_two_screens = FALSE;

				if (rows > LINES) {
					need_two_screens = 1;
					rows = LINES;
				}
				k = 0;

				for (i = 0; i < rows; i++) {
					for (j = 0; j < DCOLS; j++) {
						save[i][j] = mvinch(i, j);
					}
				}
MORE:
				for (i = 0; i < rows; i++) {
					move(i, 0);
					clrtoeol();
				}
				for (i = 0; i < (rows-1); i++) {
					if (i < (LINES-1)) {
						if (((i + i) < COMS) && ((i+i+k) < COMS)) {
							mvaddstr(i, 0, com_id_tab[i+i+k].com_desc);
						}
						if (((i + i + 1) < COMS) && ((i+i+k+1) < COMS)) {
							mvaddstr(i, (DCOLS/2),
										com_id_tab[i+i+k+1].com_desc);
						}
					}
				}
				mvaddstr(rows - 1, 0, need_two_screens ? more : press_space);
				refresh();
				wait_for_ack();

				if (need_two_screens) {
					k += ((rows-1) * 2);
					need_two_screens = 0;
					goto MORE;
				}
				for (i = 0; i < rows; i++) {
					move(i, 0);
					for (j = 0; j < DCOLS; j++) {
						addch(save[i][j]);
					}
				}
			}
			break;
		default:
			if (!pr_com_id(ch)) {
				if (!pr_motion_char(ch)) {
					check_message();
					messagef(0, "unknown character");
				}
			}
			ch = CANCEL;
			break;
		}
	}
}

int
pr_com_id(ch)
	int ch;
{
	int i;

	if (!get_com_id(&i, ch)) {
		return(0);
	}
	check_message();
	messagef(0, "%s", com_id_tab[i].com_desc);
	return(1);
}

int
get_com_id(indexp, ch)
	int *indexp;
	short ch;
{
	short i;

	for (i = 0; i < COMS; i++) {
		if (com_id_tab[i].com_char == ch) {
			*indexp = i;
			return(1);
		}
	}
	return(0);
}

int
pr_motion_char(ch)
	int ch;
{
	if (	(ch == 'J') ||
			(ch == 'K') ||
			(ch == 'L') ||
			(ch == 'H') ||
			(ch == 'Y') ||
			(ch == 'U') ||
			(ch == 'N') ||
			(ch == 'B') ||
			(ch == '\012') ||
			(ch == '\013') ||
			(ch == '\010') ||
			(ch == '\014') ||
			(ch == '\025') ||
			(ch == '\031') ||
			(ch == '\016') ||
			(ch == '\002')) {
		const char *until;
		int n;

		if (ch <= '\031') {
			ch += 96;
			until = " until adjacent";
		} else {
			ch += 32;
			until = "";
		}
		(void) get_com_id(&n, ch);
		check_message();
		messagef(0, "run %s%s", com_id_tab[n].com_desc + 8, until);
		return(1);
	} else {
		return(0);
	}
}

void
mix_colors()
{
	short i, j, k;
	char t[MAX_TITLE_LENGTH];

	for (i = 0; i <= 32; i++) {
		j = get_rand(0, (POTIONS - 1));
		k = get_rand(0, (POTIONS - 1));
		strlcpy(t, id_potions[j].title, sizeof(t));
		strlcpy(id_potions[j].title, id_potions[k].title,
		    sizeof(id_potions[j].title));
		strlcpy(id_potions[k].title, t, sizeof(id_potions[k].title));
	}
}

void
make_scroll_titles()
{
	short i, j, n;
	short sylls, s;
	size_t maxlen;

	maxlen = sizeof(id_scrolls[0].title);
	for (i = 0; i < SCROLS; i++) {
		sylls = get_rand(2, 5);
		(void) strlcpy(id_scrolls[i].title, "'", maxlen);

		for (j = 0; j < sylls; j++) {
			s = get_rand(1, (MAXSYLLABLES-1));
			(void) strlcat(id_scrolls[i].title, syllables[s], maxlen);
		}
		/* trim trailing space */
		n = strlen(id_scrolls[i].title);
		id_scrolls[i].title[n-1] = '\'';
		strlcat(id_scrolls[i].title, " ", maxlen);
	}
}

struct sbuf {
	char *buf;
	size_t maxlen;
};

static void sbuf_init(struct sbuf *s, char *buf, size_t maxlen);
static void sbuf_addstr(struct sbuf *s, const char *str);
static void sbuf_addf(struct sbuf *s, const char *fmt, ...);
static void desc_count(struct sbuf *s, int n);
static void desc_called(struct sbuf *s, const object *);

static void
sbuf_init(s, buf, maxlen)
	struct sbuf *s;
	char *buf;
	size_t maxlen;
{
	s->buf = buf;
	s->maxlen = maxlen;
	s->buf[0] = 0;
}

static void
sbuf_addstr(s, str)
	struct sbuf *s;
	const char *str;
{
	strlcat(s->buf, str, s->maxlen);
}

static void
sbuf_addf(struct sbuf *s, const char *fmt, ...)
{
	va_list ap;
	size_t initlen;

	initlen = strlen(s->buf);
	va_start(ap, fmt);
	vsnprintf(s->buf+initlen, s->maxlen-initlen, fmt, ap);
	va_end(ap);
}

static void
desc_count(s, n)
	struct sbuf *s;
	int n;
{
	if (n == 1) {
		sbuf_addstr(s, "an ");
	} else {
		sbuf_addf(s, "%d ", n);
	}
}

static void
desc_called(s, obj)
	struct sbuf *s;
	const object *obj;
{
	struct id *id_table;

	id_table = get_id_table(obj);
	sbuf_addstr(s, name_of(obj));
	sbuf_addstr(s, "called ");
	sbuf_addstr(s, id_table[obj->which_kind].title);
}

void
get_desc(obj, desc, desclen)
	const object *obj;
	char *desc;
	size_t desclen;
{
	const char *item_name;
	struct id *id_table;
	struct sbuf db;
	unsigned short objtype_id_status;

	if (obj->what_is == AMULET) {
		(void) strlcpy(desc, "the amulet of Yendor ", desclen);
		return;
	}
	if (obj->what_is == GOLD) {
		snprintf(desc, desclen, "%d pieces of gold", obj->quantity);
		return;
	}

	item_name = name_of(obj);
	id_table = get_id_table(obj);
	if (wizard || id_table == NULL) {
		objtype_id_status = IDENTIFIED;
	} else {
		objtype_id_status = id_table[obj->which_kind].id_status;
	}
	if (obj->what_is & (WEAPON | ARMOR | WAND | RING)) {
		if (obj->identified) {
			objtype_id_status = IDENTIFIED;
		}
	}
	sbuf_init(&db, desc, desclen);

	switch(obj->what_is) {
	case FOOD:
		if (obj->which_kind == RATION) {
			if (obj->quantity > 1) {
				sbuf_addf(&db, "%d rations of %s", obj->quantity,
				    item_name);
			} else {
				sbuf_addf(&db, "some %s", item_name);
			}
		} else {
			sbuf_addf(&db, "an %s", item_name);
		}
		break;
	case SCROL:
		desc_count(&db, obj->quantity);
		if (objtype_id_status==UNIDENTIFIED) {
			sbuf_addstr(&db, item_name);
			sbuf_addstr(&db, "entitled: ");
			sbuf_addstr(&db, id_table[obj->which_kind].title);
		} else if (objtype_id_status==CALLED) {
			desc_called(&db, obj);
		} else {
			sbuf_addstr(&db, item_name);
			sbuf_addstr(&db, id_table[obj->which_kind].real);
		}
		break;
	case POTION:
		desc_count(&db, obj->quantity);
		if (objtype_id_status==UNIDENTIFIED) {
			sbuf_addstr(&db, id_table[obj->which_kind].title);
			sbuf_addstr(&db, item_name);
		} else if (objtype_id_status==CALLED) {
			desc_called(&db, obj);
		} else {
			sbuf_addstr(&db, item_name);
			sbuf_addstr(&db, id_table[obj->which_kind].real);
		}
		break;
	case WAND:
		desc_count(&db, obj->quantity);
		if (objtype_id_status==UNIDENTIFIED) {
			sbuf_addstr(&db, id_table[obj->which_kind].title);
			sbuf_addstr(&db, item_name);
		} else if (objtype_id_status==CALLED) {
			desc_called(&db, obj);
		} else {
			sbuf_addstr(&db, item_name);
			sbuf_addstr(&db, id_table[obj->which_kind].real);
			if (wizard || obj->identified) {
				sbuf_addf(&db, "[%d]", obj->class);
			}
		}
		break;
	case RING:
		desc_count(&db, obj->quantity);
		if (objtype_id_status==UNIDENTIFIED) {
			sbuf_addstr(&db, id_table[obj->which_kind].title);
			sbuf_addstr(&db, item_name);
		} else if (objtype_id_status==CALLED) {
			desc_called(&db, obj);
		} else {
			if ((wizard || obj->identified) &&
			    (obj->which_kind == DEXTERITY ||
			    obj->which_kind == ADD_STRENGTH))
				sbuf_addf(&db, "%+d ", obj->class);
			sbuf_addstr(&db, item_name);
			sbuf_addstr(&db, id_table[obj->which_kind].real);
		}
		break;
	case ARMOR:
		/* no desc_count() */
		if (objtype_id_status==UNIDENTIFIED) {
			sbuf_addstr(&db, id_table[obj->which_kind].title);
		} else {
			sbuf_addf(&db, "%+d %s[%d] ", obj->d_enchant,
			    id_table[obj->which_kind].title,
			    get_armor_class(obj));
		}
		break;
	case WEAPON:
		desc_count(&db, obj->quantity);
		if (objtype_id_status==UNIDENTIFIED) {
			sbuf_addstr(&db, name_of(obj));
		} else {
			sbuf_addf(&db, "%+d,%+d %s", obj->hit_enchant,
			    obj->d_enchant, name_of(obj));
		}
		break;
	/* Should never execute */
	default:
		sbuf_addstr(&db, "grot");
		break;
	}

	if (obj->in_use_flags & BEING_WIELDED) {
		sbuf_addstr(&db, "in hand");
	} else if (obj->in_use_flags & BEING_WORN) {
		sbuf_addstr(&db, "being worn");
	} else if (obj->in_use_flags & ON_LEFT_HAND) {
		sbuf_addstr(&db, "on left hand");
	} else if (obj->in_use_flags & ON_RIGHT_HAND) {
		sbuf_addstr(&db, "on right hand");
	}

	if (!strncmp(db.buf, "an ", 3)) {
		if (!is_vowel(db.buf[3])) {
			memmove(db.buf+2, db.buf+3, strlen(db.buf+3)+1);
			db.buf[1] = ' ';
		}
	}
}

void
get_wand_and_ring_materials()
{
	short i, j;
	boolean used[WAND_MATERIALS];

	for (i = 0; i < WAND_MATERIALS; i++) {
		used[i] = 0;
	}
	for (i = 0; i < WANDS; i++) {
		do {
			j = get_rand(0, WAND_MATERIALS-1);
		} while (used[j]);
		used[j] = 1;
		strlcpy(id_wands[i].title, wand_materials[j],
		    sizeof(id_wands[i].title));
		is_wood[i] = (j > MAX_METAL);
	}
	for (i = 0; i < GEMS; i++) {
		used[i] = 0;
	}
	for (i = 0; i < RINGS; i++) {
		do {
			j = get_rand(0, GEMS-1);
		} while (used[j]);
		used[j] = 1;
		strlcpy(id_rings[i].title, gems[j],
		    sizeof(id_rings[i].title));
	}
}

void
single_inv(ichar)
	short ichar;
{
	short ch, ch2;
	char desc[DCOLS];
	object *obj;

	ch = ichar ? ichar : pack_letter("inventory what?", ALL_OBJECTS);

	if (ch == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		messagef(0, "no such item.");
		return;
	}
	ch2 = ((obj->what_is & ARMOR) && obj->is_protected) ? '}' : ')';
	get_desc(obj, desc, sizeof(desc));
	messagef(0, "%c%c %s", ch, ch2, desc);
}

struct id *
get_id_table(obj)
	const object *obj;
{
	switch(obj->what_is) {
	case SCROL:
		return(id_scrolls);
	case POTION:
		return(id_potions);
	case WAND:
		return(id_wands);
	case RING:
		return(id_rings);
	case WEAPON:
		return(id_weapons);
	case ARMOR:
		return(id_armors);
	}
	return((struct id *) 0);
}

void
inv_armor_weapon(is_weapon)
	boolean is_weapon;
{
	if (is_weapon) {
		if (rogue.weapon) {
			single_inv(rogue.weapon->ichar);
		} else {
			messagef(0, "not wielding anything");
		}
	} else {
		if (rogue.armor) {
			single_inv(rogue.armor->ichar);
		} else {
			messagef(0, "not wearing anything");
		}
	}
}

void
id_type()
{
	const char *id;
	int ch;

	messagef(0, "what do you want identified?");

	ch = rgetchar();

	if ((ch >= 'A') && (ch <= 'Z')) {
		id = m_names[ch-'A'];
	} else if (ch < 32) {
		check_message();
		return;
	} else {
		switch(ch) {
		case '@':
			id = "you";
			break;
		case '%':
			id = "staircase";
			break;
		case '^':
			id = "trap";
			break;
		case '+':
			id = "door";
			break;
		case '-':
		case '|':
			id = "wall of a room";
			break;
		case '.':
			id = "floor";
			break;
		case '#':
			id = "passage";
			break;
		case ' ':
			id = "solid rock";
			break;
		case '=':
			id = "ring";
			break;
		case '?':
			id = "scroll";
			break;
		case '!':
			id = "potion";
			break;
		case '/':
			id = "wand or staff";
			break;
		case ')':
			id = "weapon";
			break;
		case ']':
			id = "armor";
			break;
		case '*':
			id = "gold";
			break;
		case ':':
			id = "food";
			break;
		case ',':
			id = "the Amulet of Yendor";
			break;
		default:
			id = "unknown character";
			break;
		}
	}
	check_message();
	messagef(0, "'%c': %s", ch, id);
}
