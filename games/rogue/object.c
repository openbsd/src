/*	$OpenBSD: object.c,v 1.11 2009/10/27 23:59:26 deraadt Exp $	*/
/*	$NetBSD: object.c,v 1.3 1995/04/22 10:27:50 cgd Exp $	*/

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
 * object.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#include "rogue.h"

object level_objects;
unsigned short dungeon[DROWS][DCOLS];
short foods = 0;
object *free_list = (object *) 0;
char *fruit = (char *) 0;

fighter rogue = {
	INIT_AW,	/* armor */
	INIT_AW,	/* weapon */
	INIT_RINGS,	/* left ring */
	INIT_RINGS,	/* right ring */
	INIT_HP,	/* Hp current */
	INIT_HP,	/* Hp max */
	INIT_STR,	/* Str current */
	INIT_STR,	/* Str max */
	INIT_PACK,	/* pack */
	INIT_GOLD,	/* gold */
	INIT_EXPLEVEL,	/* exp level */
	INIT_EXP,	/* exp points */
	0, 0,		/* row, col */
	INIT_CHAR,	/* char */
	INIT_MOVES	/* moves */
};

struct id id_potions[POTIONS] = {
{100, "blue ",     "of increase strength ", 0},
{250, "red ",      "of restore strength ", 0},
{100, "green ",    "of healing ", 0},
{200, "grey ",     "of extra healing ", 0},
 {10, "brown ",    "of poison ", 0},
{300, "clear ",    "of raise level ", 0},
 {10, "pink ",     "of blindness ", 0},
 {25, "white ",    "of hallucination ", 0},
{100, "purple ",   "of detect monster ", 0},
{100, "black ",    "of detect things ", 0},
 {10, "yellow ",   "of confusion ", 0},
 {80, "plaid ",    "of levitation ", 0},
{150, "burgundy ", "of haste self ", 0},
{145, "beige ",    "of see invisible ", 0}
};

struct id id_scrolls[SCROLS] = {
{505, "", "of protect armor ", 0},
{200, "", "of hold monster ", 0},
{235, "", "of enchant weapon ", 0},
{235, "", "of enchant armor ", 0},
{175, "", "of identify ", 0},
{190, "", "of teleportation ", 0},
 {25, "", "of sleep ", 0},
{610, "", "of scare monster ", 0},
{210, "", "of remove curse ", 0},
 {80, "", "of create monster ",0},
 {25, "", "of aggravate monster ",0},
{180, "", "of magic mapping ", 0},
 {90, "", "of confuse monster ", 0}
};

struct id id_weapons[WEAPONS] = {
	{150, "short bow ", "", 0},
	  {8, "darts ", "", 0},
	 {15, "arrows ", "", 0},
	 {27, "daggers ", "", 0},
	 {35, "shurikens ", "", 0},
	{360, "mace ", "", 0},
	{470, "long sword ", "", 0},
	{580, "two-handed sword ", "", 0}
};

struct id id_armors[ARMORS] = {
	{300, "leather armor ", "", (UNIDENTIFIED)},
	{300, "ring mail ", "", (UNIDENTIFIED)},
	{400, "scale mail ", "", (UNIDENTIFIED)},
	{500, "chain mail ", "", (UNIDENTIFIED)},
	{600, "banded mail ", "", (UNIDENTIFIED)},
	{600, "splint mail ", "", (UNIDENTIFIED)},
	{700, "plate mail ", "", (UNIDENTIFIED)}
};

struct id id_wands[WANDS] = {
	 {25, "", "of teleport away ",0},
	 {50, "", "of slow monster ", 0},
	  {8, "", "of invisibility ",0},
	 {55, "", "of polymorph ",0},
	  {2, "", "of haste monster ",0},
	 {20, "", "of magic missile ",0},
	 {20, "", "of cancellation ",0},
	  {0, "", "of do nothing ",0},
	 {35, "", "of drain life ",0},
	 {20, "", "of cold ",0},
	 {20, "", "of fire ",0}
};

struct id id_rings[RINGS] = {
	 {250, "", "of stealth ",0},
	 {100, "", "of teleportation ", 0},
	 {255, "", "of regeneration ",0},
	 {295, "", "of slow digestion ",0},
	 {200, "", "of add strength ",0},
	 {250, "", "of sustain strength ",0},
	 {250, "", "of dexterity ",0},
	  {25, "", "of adornment ",0},
	 {300, "", "of see invisible ",0},
	 {290, "", "of maintain armor ",0},
	 {270, "", "of searching ",0},
};

void
put_objects(void)
{
	short i, n;
	object *obj;

	if (cur_level < max_level) {
		return;
	}
	n = coin_toss() ? get_rand(2, 4) : get_rand(3, 5);
	while (rand_percent(33)) {
		n++;
	}
	if (party_room != NO_ROOM) {
		make_party();
	}
	for (i = 0; i < n; i++) {
		obj = gr_object();
		rand_place(obj);
	}
	put_gold();
}

void
put_gold(void)
{
	short i, j;
	short row,col;
	boolean is_maze, is_room;

	for (i = 0; i < MAXROOMS; i++) {
		is_maze = (rooms[i].is_room & R_MAZE) ? 1 : 0;
		is_room = (rooms[i].is_room & R_ROOM) ? 1 : 0;

		if (!(is_room || is_maze)) {
			continue;
		}
		if (is_maze || rand_percent(GOLD_PERCENT)) {
			for (j = 0; j < 50; j++) {
				row = get_rand(rooms[i].top_row+1,
				rooms[i].bottom_row-1);
				col = get_rand(rooms[i].left_col+1,
				rooms[i].right_col-1);
				if ((dungeon[row][col] == FLOOR) ||
					(dungeon[row][col] == TUNNEL)) {
					plant_gold(row, col, is_maze);
					break;
				}
			}
		}
	}
}

void
plant_gold(short row, short col, boolean is_maze)
{
	object *obj;

	obj = alloc_object();
	obj->row = row; obj->col = col;
	obj->what_is = GOLD;
	obj->quantity = get_rand((2 * cur_level), (16 * cur_level));
	if (is_maze) {
		obj->quantity += obj->quantity / 2;
	}
	dungeon[row][col] |= OBJECT;
	(void) add_to_pack(obj, &level_objects, 0);
}

void
place_at(object *obj, short row, short col)
{
	obj->row = row;
	obj->col = col;
	dungeon[row][col] |= OBJECT;
	(void) add_to_pack(obj, &level_objects, 0);
}

object *
object_at(object *pack, short row, short col)
{
	object *obj = (object *) 0;

	if (dungeon[row][col] & (MONSTER | OBJECT)) {
		obj = pack->next_object;

		while (obj && ((obj->row != row) || (obj->col != col))) {
			obj = obj->next_object;
		}
		if (!obj) {
			messagef(1, "object_at(): inconsistent");
		}
	}
	return(obj);
}

object *
get_letter_object(int ch)
{
	object *obj;

	obj = rogue.pack.next_object;

	while (obj && (obj->ichar != ch)) {
		obj = obj->next_object;
	}
	return(obj);
}

void
free_stuff(object *objlist)
{
	object *obj;

	while (objlist->next_object) {
		obj = objlist->next_object;
		objlist->next_object =
			objlist->next_object->next_object;
		free_object(obj);
	}
}

const char *
name_of(const object *obj)
{
	const char *retstring;

	switch(obj->what_is) {
	case SCROL:
		retstring = obj->quantity > 1 ? "scrolls " : "scroll ";
		break;
	case POTION:
		retstring = obj->quantity > 1 ? "potions " : "potion ";
		break;
	case FOOD:
		if (obj->which_kind == RATION) {
			retstring = "food ";
		} else {
			retstring = fruit;
		}
		break;
	case WAND:
		retstring = is_wood[obj->which_kind] ? "staff " : "wand ";
		break;
	case WEAPON:
		switch(obj->which_kind) {
		case DART:
			retstring=obj->quantity > 1 ? "darts " : "dart ";
			break;
		case ARROW:
			retstring=obj->quantity > 1 ? "arrows " : "arrow ";
			break;
		case DAGGER:
			retstring=obj->quantity > 1 ? "daggers " : "dagger ";
			break;
		case SHURIKEN:
			retstring=obj->quantity > 1?"shurikens ":"shuriken ";
			break;
		default:
			retstring = id_weapons[obj->which_kind].title;
		}
		break;
	case ARMOR:
		retstring = "armor ";
		break;
	case RING:
			retstring = "ring ";
		break;
	case AMULET:
		retstring = "amulet ";
		break;
	default:
		retstring = "unknown ";
		break;
	}
	return(retstring);
}

object *
gr_object(void)
{
	object *obj;

	obj = alloc_object();

	if (foods < (cur_level / 3)) {
		obj->what_is = FOOD;
		foods++;
	} else {
		obj->what_is = gr_what_is();
	}
	switch(obj->what_is) {
	case SCROL:
		gr_scroll(obj);
		break;
	case POTION:
		gr_potion(obj);
		break;
	case WEAPON:
		gr_weapon(obj, 1);
		break;
	case ARMOR:
		gr_armor(obj);
		break;
	case WAND:
		gr_wand(obj);
		break;
	case FOOD:
		get_food(obj, 0);
		break;
	case RING:
		gr_ring(obj, 1);
		break;
	}
	return(obj);
}

unsigned short
gr_what_is(void)
{
	short percent;
	unsigned short what_is;

	percent = get_rand(1, 91);

	if (percent <= 30) {
		what_is = SCROL;
	} else if (percent <= 60) {
		what_is = POTION;
	} else if (percent <= 64) {
		what_is = WAND;
	} else if (percent <= 74) {
		what_is = WEAPON;
	} else if (percent <= 83) {
		what_is = ARMOR;
	} else if (percent <= 88) {
		what_is = FOOD;
	} else {
		what_is = RING;
	}
	return(what_is);
}

void
gr_scroll(object *obj)
{
	short percent;

	percent = get_rand(0, 91);

	obj->what_is = SCROL;

	if (percent <= 5) {
		obj->which_kind = PROTECT_ARMOR;
	} else if (percent <= 10) {
		obj->which_kind = HOLD_MONSTER;
	} else if (percent <= 20) {
		obj->which_kind = CREATE_MONSTER;
	} else if (percent <= 35) {
		obj->which_kind = IDENTIFY;
	} else if (percent <= 43) {
		obj->which_kind = TELEPORT;
	} else if (percent <= 50) {
		obj->which_kind = SLEEP;
	} else if (percent <= 55) {
		obj->which_kind = SCARE_MONSTER;
	} else if (percent <= 64) {
		obj->which_kind = REMOVE_CURSE;
	} else if (percent <= 69) {
		obj->which_kind = ENCH_ARMOR;
	} else if (percent <= 74) {
		obj->which_kind = ENCH_WEAPON;
	} else if (percent <= 80) {
		obj->which_kind = AGGRAVATE_MONSTER;
	} else if (percent <= 86) {
		obj->which_kind = CON_MON;
	} else {
		obj->which_kind = MAGIC_MAPPING;
	}
}

void
gr_potion(object *obj)
{
	short percent;

	percent = get_rand(1, 118);

	obj->what_is = POTION;

	if (percent <= 5) {
		obj->which_kind = RAISE_LEVEL;
	} else if (percent <= 15) {
		obj->which_kind = DETECT_OBJECTS;
	} else if (percent <= 25) {
		obj->which_kind = DETECT_MONSTER;
	} else if (percent <= 35) {
		obj->which_kind = INCREASE_STRENGTH;
	} else if (percent <= 45) {
		obj->which_kind = RESTORE_STRENGTH;
	} else if (percent <= 55) {
		obj->which_kind = HEALING;
	} else if (percent <= 65) {
		obj->which_kind = EXTRA_HEALING;
	} else if (percent <= 75) {
		obj->which_kind = BLINDNESS;
	} else if (percent <= 85) {
		obj->which_kind = HALLUCINATION;
	} else if (percent <= 95) {
		obj->which_kind = CONFUSION;
	} else if (percent <= 105) {
		obj->which_kind = POISON;
	} else if (percent <= 110) {
		obj->which_kind = LEVITATION;
	} else if (percent <= 114) {
		obj->which_kind = HASTE_SELF;
	} else {
		obj->which_kind = SEE_INVISIBLE;
	}
}

void
gr_weapon(object *obj, int assign_wk)
{
	short percent;
	short i;
	short blessing, increment;

	obj->what_is = WEAPON;
	if (assign_wk) {
		obj->which_kind = get_rand(0, (WEAPONS - 1));
	}
	if ((obj->which_kind == ARROW) || (obj->which_kind == DAGGER) ||
		(obj->which_kind == SHURIKEN) | (obj->which_kind == DART)) {
		obj->quantity = get_rand(3, 15);
		obj->quiver = get_rand(0, 126);
	} else {
		obj->quantity = 1;
	}
	obj->hit_enchant = obj->d_enchant = 0;

	percent = get_rand(1, 96);
	blessing = get_rand(1, 3);

	if (percent <= 32) {
		if (percent <= 16) {
			increment = 1;
		} else {
			increment = -1;
			obj->is_cursed = 1;
		}
		for (i = 0; i < blessing; i++) {
			if (coin_toss()) {
				obj->hit_enchant += increment;
			} else {
				obj->d_enchant += increment;
			}
		}
	}
	switch(obj->which_kind) {
	case BOW:
	case DART:
		obj->damage = "1d1";
		break;
	case ARROW:
		obj->damage = "1d2";
		break;
	case DAGGER:
		obj->damage = "1d3";
		break;
	case SHURIKEN:
		obj->damage = "1d4";
		break;
	case MACE:
		obj->damage = "2d3";
		break;
	case LONG_SWORD:
		obj->damage = "3d4";
		break;
	case TWO_HANDED_SWORD:
		obj->damage = "4d5";
		break;
	}
}

void
gr_armor(object *obj)
{
	short percent;
	short blessing;

	obj->what_is = ARMOR;
	obj->which_kind = get_rand(0, (ARMORS - 1));
	obj->class = obj->which_kind + 2;
	if ((obj->which_kind == PLATE) || (obj->which_kind == SPLINT)) {
		obj->class--;
	}
	obj->is_protected = 0;
	obj->d_enchant = 0;

	percent = get_rand(1, 100);
	blessing = get_rand(1, 3);

	if (percent <= 16) {
		obj->is_cursed = 1;
		obj->d_enchant -= blessing;
	} else if (percent <= 33) {
		obj->d_enchant += blessing;
	}
}

void
gr_wand(object *obj)
{
	obj->what_is = WAND;
	obj->which_kind = get_rand(0, (WANDS - 1));
	obj->class = get_rand(3, 7);
}

void
get_food(object *obj, boolean force_ration)
{
	obj->what_is = FOOD;

	if (force_ration || rand_percent(80)) {
		obj->which_kind = RATION;
	} else {
		obj->which_kind = FRUIT;
	}
}

void
put_stairs(void)
{
	short row, col;

	gr_row_col(&row, &col, (FLOOR | TUNNEL));
	dungeon[row][col] |= STAIRS;
}

int
get_armor_class(const object *obj)
{
	if (obj) {
		return(obj->class + obj->d_enchant);
	}
	return(0);
}

object *
alloc_object(void)
{
	object *obj;

	if (free_list) {
		obj = free_list;
		free_list = free_list->next_object;
	} else if (!(obj = (object *) md_malloc(sizeof(object)))) {
		messagef(0, "cannot allocate object, saving game");
		save_into_file(error_file);
	}
	obj->quantity = 1;
	obj->ichar = 'L';
	obj->picked_up = obj->is_cursed = 0;
	obj->in_use_flags = NOT_USED;
	obj->identified = UNIDENTIFIED;
	obj->damage = "1d1";
	return(obj);
}

void
free_object(object *obj)
{
	obj->next_object = free_list;
	free_list = obj;
}

void
make_party(void)
{
	short n;

	party_room = gr_room();

	n = rand_percent(99) ? party_objects(party_room) : 11;
	if (rand_percent(99)) {
		party_monsters(party_room, n);
	}
}

void
show_objects(void)
{
	object *obj;
	short mc, rc, row, col;
	object *monster;

	obj = level_objects.next_object;

	while (obj) {
		row = obj->row;
		col = obj->col;

		rc = get_mask_char(obj->what_is);

		if (dungeon[row][col] & MONSTER) {
			if ((monster =
			    object_at(&level_monsters, row, col))) {
				monster->trail_char = rc;
			}
		}
		mc = mvinch(row, col);
		if (((mc < 'A') || (mc > 'Z')) &&
			((row != rogue.row) || (col != rogue.col))) {
			mvaddch(row, col, rc);
		}
		obj = obj->next_object;
	}

	monster = level_monsters.next_object;

	while (monster) {
		if (monster->m_flags & IMITATES) {
			mvaddch(monster->row, monster->col, (int) monster->disguise);
		}
		monster = monster->next_monster;
	}
}

void
put_amulet(void)
{
	object *obj;

	obj = alloc_object();
	obj->what_is = AMULET;
	rand_place(obj);
}

void
rand_place(object *obj)
{
	short row, col;

	gr_row_col(&row, &col, (FLOOR | TUNNEL));
	place_at(obj, row, col);
}

void
c_object_for_wizard(void)
{
	short ch, max, wk;
	object *obj;
	char buf[80];

	max = 0;
	if (pack_count((object *) 0) >= MAX_PACK_COUNT) {
		messagef(0, "pack full");
		return;
	}
	messagef(0, "type of object?");

	while (r_index("!?:)]=/,\033", (ch = rgetchar()), 0) == -1) {
		beep();
	}
	check_message();

	if (ch == '\033') {
		return;
	}
	obj = alloc_object();

	switch(ch) {
	case '!':
		obj->what_is = POTION;
		max = POTIONS - 1;
		break;
	case '?':
		obj->what_is = SCROL;
		max = SCROLS - 1;
		break;
	case ',':
		obj->what_is = AMULET;
		break;
	case ':':
		get_food(obj, 0);
		break;
	case ')':
		gr_weapon(obj, 0);
		max = WEAPONS - 1;
		break;
	case ']':
		gr_armor(obj);
		max = ARMORS - 1;
		break;
	case '/':
		gr_wand(obj);
		max = WANDS - 1;
		break;
	case '=':
		max = RINGS - 1;
		obj->what_is = RING;
		break;
	}
	if ((ch != ',') && (ch != ':')) {
GIL:
		if (get_input_line("which kind?", "", buf, sizeof(buf), "", 0, 1)) {
			wk = get_number(buf);
			if ((wk >= 0) && (wk <= max)) {
				obj->which_kind = (unsigned short) wk;
				if (obj->what_is == RING) {
					gr_ring(obj, 0);
				}
			} else {
				beep();
				goto GIL;
			}
		} else {
			free_object(obj);
			return;
		}
	}
	get_desc(obj, buf, sizeof(buf));
	messagef(0, "%s", buf);
	(void) add_to_pack(obj, &rogue.pack, 1);
}
