/*	$OpenBSD: use.c,v 1.5 2002/07/18 07:13:57 pjanzen Exp $	*/
/*	$NetBSD: use.c,v 1.3 1995/04/22 10:28:38 cgd Exp $	*/

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
static char sccsid[] = "@(#)use.c	8.1 (Berkeley) 5/31/93";
#else
static const char rcsid[] = "$OpenBSD: use.c,v 1.5 2002/07/18 07:13:57 pjanzen Exp $";
#endif
#endif /* not lint */

/*
 * use.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *	   gain or profit.
 *
 */

#include "rogue.h"

short halluc = 0;
short blind = 0;
short confused = 0;
short levitate = 0;
short haste_self = 0;
boolean see_invisible = 0;
short extra_hp = 0;
boolean detect_monster = 0;
boolean con_mon = 0;
char *strange_feeling = "you have a strange feeling for a moment, then it passes";

void
quaff()
{
	short ch;
	object *obj;

	ch = pack_letter("quaff what?", POTION);

	if (ch == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		messagef(0, "no such item.");
		return;
	}
	if (obj->what_is != POTION) {
		messagef(0, "you can't drink that");
		return;
	}
	switch(obj->which_kind) {
		case INCREASE_STRENGTH:
			messagef(0, "you feel stronger now, what bulging muscles!");
			rogue.str_current++;
			if (rogue.str_current > rogue.str_max) {
				rogue.str_max = rogue.str_current;
			}
			break;
		case RESTORE_STRENGTH:
			rogue.str_current = rogue.str_max;
			messagef(0, "this tastes great, you feel warm all over");
			break;
		case HEALING:
			messagef(0, "you begin to feel better");
			potion_heal(0);
			break;
		case EXTRA_HEALING:
			messagef(0, "you begin to feel much better");
			potion_heal(1);
			break;
		case POISON:
			if (!sustain_strength) {
				rogue.str_current -= get_rand(1, 3);
				if (rogue.str_current < 1) {
					rogue.str_current = 1;
				}
			}
			messagef(0, "you feel very sick now");
			if (halluc) {
				unhallucinate();
			}
			break;
		case RAISE_LEVEL:
			rogue.exp_points = level_points[rogue.exp - 1];
			messagef(0, "you suddenly feel much more skillful");
			add_exp(1, 1);
			break;
		case BLINDNESS:
			go_blind();
			break;
		case HALLUCINATION:
			messagef(0, "oh wow, everything seems so cosmic");
			halluc += get_rand(500, 800);
			break;
		case DETECT_MONSTER:
			show_monsters();
			if (!(level_monsters.next_monster)) {
				messagef(0, "%s", strange_feeling);
			}
			break;
		case DETECT_OBJECTS:
			if (level_objects.next_object) {
				if (!blind) {
					show_objects();
				}
			} else {
				messagef(0, "%s", strange_feeling);
			}
			break;
		case CONFUSION:
			messagef(0, (halluc ? "what a trippy feeling" :
			    "you feel confused"));
			cnfs();
			break;
		case LEVITATION:
			messagef(0, "you start to float in the air");
			levitate += get_rand(15, 30);
			being_held = bear_trap = 0;
			break;
		case HASTE_SELF:
			messagef(0, "you feel yourself moving much faster");
			haste_self += get_rand(11, 21);
			if (!(haste_self % 2)) {
				haste_self++;
			}
			break;
		case SEE_INVISIBLE:
			messagef(0, "hmm, this potion tastes like %s juice", fruit);
			if (blind) {
				unblind();
			}
			see_invisible = 1;
			relight();
			break;
	}
	print_stats((STAT_STRENGTH | STAT_HP));
	if (id_potions[obj->which_kind].id_status != CALLED) {
		id_potions[obj->which_kind].id_status = IDENTIFIED;
	}
	vanish(obj, 1, &rogue.pack);
}

void
read_scroll()
{
	short ch;
	object *obj;

	ch = pack_letter("read what?", SCROL);

	if (ch == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		messagef(0, "no such item.");
		return;
	}
	if (obj->what_is != SCROL) {
		messagef(0, "you can't read that");
		return;
	}
	switch(obj->which_kind) {
		case SCARE_MONSTER:
			messagef(0, "you hear a maniacal laughter in the distance");
			break;
		case HOLD_MONSTER:
			hold_monster();
			break;
		case ENCH_WEAPON:
			if (rogue.weapon) {
				if (rogue.weapon->what_is == WEAPON) {
					messagef(0, "your %sglow%s %sfor a moment",
					    name_of(rogue.weapon),
					    ((rogue.weapon->quantity <= 1) ? "s" : ""),
					    get_ench_color());
					if (coin_toss()) {
						rogue.weapon->hit_enchant++;
					} else {
						rogue.weapon->d_enchant++;
					}
				}
				rogue.weapon->is_cursed = 0;
			} else {
				messagef(0, "your hands tingle");
			}
			break;
		case ENCH_ARMOR:
			if (rogue.armor) {
				messagef(0, "your armor glows %sfor a moment",
				    get_ench_color());
				rogue.armor->d_enchant++;
				rogue.armor->is_cursed = 0;
				print_stats(STAT_ARMOR);
			} else {
				messagef(0, "your skin crawls");
			}
			break;
		case IDENTIFY:
			messagef(0, "this is a scroll of identify");
			obj->identified = 1;
			id_scrolls[obj->which_kind].id_status = IDENTIFIED;
			idntfy();
			break;
		case TELEPORT:
			tele();
			break;
		case SLEEP:
			messagef(0, "you fall asleep");
			take_a_nap();
			break;
		case PROTECT_ARMOR:
			if (rogue.armor) {
				messagef(0,  "your armor is covered by a shimmering gold shield");
				rogue.armor->is_protected = 1;
				rogue.armor->is_cursed = 0;
			} else {
				messagef(0, "your acne seems to have disappeared");
			}
			break;
		case REMOVE_CURSE:
			messagef(0, (!halluc) ?
					"you feel as though someone is watching over you" :
					"you feel in touch with the universal oneness");
			uncurse_all();
			break;
		case CREATE_MONSTER:
			create_monster();
			break;
		case AGGRAVATE_MONSTER:
			aggravate();
			break;
		case MAGIC_MAPPING:
			messagef(0, "this scroll seems to have a map on it");
			draw_magic_map();
			break;
		case CON_MON:
			con_mon = 1;
			messagef(0, "your hands glow %sfor a moment", get_ench_color());
			break;
	}
	if (id_scrolls[obj->which_kind].id_status != CALLED) {
		id_scrolls[obj->which_kind].id_status = IDENTIFIED;
	}
	vanish(obj, (obj->which_kind != SLEEP), &rogue.pack);
}

/* vanish() does NOT handle a quiver of weapons with more than one
 *  arrow (or whatever) in the quiver.  It will only decrement the count.
 */

void
vanish(obj, rm, pack)
	object *obj;
	short rm;
	object *pack;
{
	if (obj->quantity > 1) {
		obj->quantity--;
	} else {
		if (obj->in_use_flags & BEING_WIELDED) {
			unwield(obj);
		} else if (obj->in_use_flags & BEING_WORN) {
			unwear(obj);
		} else if (obj->in_use_flags & ON_EITHER_HAND) {
			un_put_on(obj);
		}
		take_from_pack(obj, pack);
		free_object(obj);
	}
	if (rm) {
		(void) reg_move();
	}
}

void
potion_heal(extra)
	int extra;
{
	float ratio;
	short add;

	rogue.hp_current += rogue.exp;

	ratio = ((float)rogue.hp_current) / rogue.hp_max;

	if (ratio >= 1.00) {
		rogue.hp_max += (extra ? 2 : 1);
		extra_hp += (extra ? 2 : 1);
		rogue.hp_current = rogue.hp_max;
	} else if (ratio >= 0.90) {
		rogue.hp_max += (extra ? 1 : 0);
		extra_hp += (extra ? 1 : 0);
		rogue.hp_current = rogue.hp_max;
	} else {
		if (ratio < 0.33) {
			ratio = 0.33;
		}
		if (extra) {
			ratio += ratio;
		}
		add = (short)(ratio * ((float)rogue.hp_max - rogue.hp_current));
		rogue.hp_current += add;
		if (rogue.hp_current > rogue.hp_max) {
			rogue.hp_current = rogue.hp_max;
		}
	}
	if (blind) {
		unblind();
	}
	if (confused && extra) {
			unconfuse();
	} else if (confused) {
		confused = (confused / 2) + 1;
	}
	if (halluc && extra) {
		unhallucinate();
	} else if (halluc) {
		halluc = (halluc / 2) + 1;
	}
}

void
idntfy()
{
	short ch;
	object *obj;
	struct id *id_table;
	char desc[DCOLS];
AGAIN:
	ch = pack_letter("what would you like to identify?", ALL_OBJECTS);

	if (ch == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		messagef(0, "no such item, try again");
		messagef(0, "");
		check_message();
		goto AGAIN;
	}
	obj->identified = 1;
	if (obj->what_is & (SCROL | POTION | WEAPON | ARMOR | WAND | RING)) {
		id_table = get_id_table(obj);
		id_table[obj->which_kind].id_status = IDENTIFIED;
	}
	get_desc(obj, desc, sizeof(desc));
	messagef(0, "%s", desc);
}

void
eat()
{
	short ch;
	short moves;
	object *obj;

	ch = pack_letter("eat what?", FOOD);

	if (ch == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		messagef(0, "no such item.");
		return;
	}
	if (obj->what_is != FOOD) {
		messagef(0, "you can't eat that");
		return;
	}
	if ((obj->which_kind == FRUIT) || rand_percent(60)) {
		moves = get_rand(950, 1150);
		if (obj->which_kind == RATION) {
			messagef(0, "yum, that tasted good");
		} else {
			messagef(0, "my, that was a yummy %s", fruit);
		}
	} else {
		moves = get_rand(750, 950);
		messagef(0, "yuk, that food tasted awful");
		add_exp(2, 1);
	}
	rogue.moves_left /= 3;
	rogue.moves_left += moves;
	hunger_str[0] = 0;
	print_stats(STAT_HUNGER);

	vanish(obj, 1, &rogue.pack);
}

void
hold_monster()
{
	short i, j;
	short mcount = 0;
	object *monster;
	short row, col;

	for (i = -2; i <= 2; i++) {
		for (j = -2; j <= 2; j++) {
			row = rogue.row + i;
			col = rogue.col + j;
			if ((row < MIN_ROW) || (row > (DROWS-2)) || (col < 0) ||
				 (col > (DCOLS-1))) {
				continue;
			}
			if (dungeon[row][col] & MONSTER) {
				monster = object_at(&level_monsters, row, col);
				monster->m_flags |= ASLEEP;
				monster->m_flags &= (~WAKENS);
				mcount++;
			}
		}
	}
	if (mcount == 0) {
		messagef(0, "you feel a strange sense of loss");
	} else if (mcount == 1) {
		messagef(0, "the monster freezes");
	} else {
		messagef(0, "the monsters around you freeze");
	}
}

void
tele()
{
	mvaddch(rogue.row, rogue.col, get_dungeon_char(rogue.row, rogue.col));

	if (cur_room >= 0) {
		darken_room(cur_room);
	}
	put_player(get_room_number(rogue.row, rogue.col));
	being_held = 0;
	bear_trap = 0;
}

void
hallucinate()
{
	object *obj, *monster;
	short ch;

	if (blind) return;

	obj = level_objects.next_object;

	while (obj) {
		ch = mvinch(obj->row, obj->col);
		if (((ch < 'A') || (ch > 'Z')) &&
			((obj->row != rogue.row) || (obj->col != rogue.col)))
		if ((ch != ' ') && (ch != '.') && (ch != '#') && (ch != '+')) {
			addch(gr_obj_char());
		}
		obj = obj->next_object;
	}
	monster = level_monsters.next_monster;

	while (monster) {
		ch = mvinch(monster->row, monster->col);
		if ((ch >= 'A') && (ch <= 'Z')) {
			addch(get_rand('A', 'Z'));
		}
		monster = monster->next_monster;
	}
}

void
unhallucinate()
{
	halluc = 0;
	relight();
	messagef(1, "everything looks SO boring now");
}

void
unblind()
{
	blind = 0;
	messagef(1, "the veil of darkness lifts");
	relight();
	if (halluc) {
		hallucinate();
	}
	if (detect_monster) {
		show_monsters();
	}
}

void
relight()
{
	if (cur_room == PASSAGE) {
		light_passage(rogue.row, rogue.col);
	} else {
		light_up_room(cur_room);
	}
	mvaddch(rogue.row, rogue.col, rogue.fchar);
}

void
take_a_nap()
{
	short i;

	i = get_rand(2, 5);
	md_sleep(1);

	while (i--) {
		mv_mons();
	}
	md_sleep(1);
	messagef(0, "%s", you_can_move_again);
}

void
go_blind()
{
	short i, j;

	if (!blind) {
		messagef(0, "a cloak of darkness falls around you");
	}
	blind += get_rand(500, 800);

	if (detect_monster) {
		object *monster;

		monster = level_monsters.next_monster;

		while (monster) {
			mvaddch(monster->row, monster->col, monster->trail_char);
			monster = monster->next_monster;
		}
	}
	if (cur_room >= 0) {
		for (i = rooms[cur_room].top_row + 1;
			 i < rooms[cur_room].bottom_row; i++) {
			for (j = rooms[cur_room].left_col + 1;
				 j < rooms[cur_room].right_col; j++) {
				mvaddch(i, j, ' ');
			}
		}
	}
	mvaddch(rogue.row, rogue.col, rogue.fchar);
}

char *
get_ench_color()
{
	if (halluc) {
		return(id_potions[get_rand(0, POTIONS-1)].title);
	} else if (con_mon) {
		return("red ");
	} 
	return("blue ");
}

void
cnfs()
{
	confused += get_rand(12, 22);
}

void
unconfuse()
{
	confused = 0;
	messagef(1, "you feel less %s now", (halluc ? "trippy" : "confused"));
}

void
uncurse_all()
{
	object *obj;

	obj = rogue.pack.next_object;

	while (obj) {
		obj->is_cursed = 0;
		obj = obj->next_object;
	}
}
