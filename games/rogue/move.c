/*	$OpenBSD: move.c,v 1.8 2009/10/27 23:59:26 deraadt Exp $	*/
/*	$NetBSD: move.c,v 1.3 1995/04/22 10:27:47 cgd Exp $	*/

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
 * move.c
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

short m_moves = 0;
boolean jump = 0;
const char *you_can_move_again = "you can move again";

int
one_move_rogue(short dirch, short pickup)
{
	short row, col;
	object *obj;
	char desc[DCOLS];
	short status, d;

	row = rogue.row;
	col = rogue.col;

	if (confused) {
		dirch = gr_dir();
	}
	(void) is_direction(dirch, &d);
	get_dir_rc(d, &row, &col, 1);

	if (!can_move(rogue.row, rogue.col, row, col)) {
		return(MOVE_FAILED);
	}
	if (being_held || bear_trap) {
		if (!(dungeon[row][col] & MONSTER)) {
			if (being_held) {
				messagef(1, "you are being held");
			} else {
				messagef(0, "you are still stuck in the bear trap");
				(void) reg_move();
			}
			return(MOVE_FAILED);
		}
	}
	if (r_teleport) {
		if (rand_percent(R_TELE_PERCENT)) {
			tele();
			return(STOPPED_ON_SOMETHING);
		}
	}
	if (dungeon[row][col] & MONSTER) {
		rogue_hit(object_at(&level_monsters, row, col), 0);
		(void) reg_move();
		return(MOVE_FAILED);
	}
	if (dungeon[row][col] & DOOR) {
		if (cur_room == PASSAGE) {
			cur_room = get_room_number(row, col);
			light_up_room(cur_room);
			wake_room(cur_room, 1, row, col);
		} else {
			light_passage(row, col);
		}
	} else if ((dungeon[rogue.row][rogue.col] & DOOR) &&
		   (dungeon[row][col] & TUNNEL)) {
		light_passage(row, col);
		wake_room(cur_room, 0, rogue.row, rogue.col);
		darken_room(cur_room);
		cur_room = PASSAGE;
	} else if (dungeon[row][col] & TUNNEL) {
			light_passage(row, col);
	}
	mvaddch(rogue.row, rogue.col, get_dungeon_char(rogue.row, rogue.col));
	mvaddch(row, col, rogue.fchar);

	if (!jump) {
		refresh();
	}
	rogue.row = row;
	rogue.col = col;
	if (dungeon[row][col] & OBJECT) {
		if (levitate && pickup) {
			return(STOPPED_ON_SOMETHING);
		}
		if (pickup && !levitate) {
			if ((obj = pick_up(row, col, &status))) {
				get_desc(obj, desc, sizeof(desc));
				if (obj->what_is == GOLD) {
					free_object(obj);
					messagef(1, "%s", desc);
					goto NOT_IN_PACK;
				}
			} else if (!status) {
				goto MVED;
			} else {
				goto MOVE_ON;
			}
		} else {
MOVE_ON:
			obj = object_at(&level_objects, row, col);
			get_desc(obj, desc, sizeof(desc));
			messagef(1, "moved onto %s", desc);
			goto NOT_IN_PACK;
		}
		messagef(1, "%s(%c)", desc, obj->ichar);
NOT_IN_PACK:
		(void) reg_move();
		return(STOPPED_ON_SOMETHING);
	}
	if (dungeon[row][col] & (DOOR | STAIRS | TRAP)) {
		if ((!levitate) && (dungeon[row][col] & TRAP)) {
			trap_player(row, col);
		}
		(void) reg_move();
		return(STOPPED_ON_SOMETHING);
	}
MVED:	if (reg_move()) {			/* fainted from hunger */
			return(STOPPED_ON_SOMETHING);
	}
	return((confused ? STOPPED_ON_SOMETHING : MOVED));
}

void
multiple_move_rogue(short dirch)
{
	short row, col;
	short m;

	switch(dirch) {
	case '\010':
	case '\012':
	case '\013':
	case '\014':
	case '\031':
	case '\025':
	case '\016':
	case '\002':
		do {
			row = rogue.row;
			col = rogue.col;
			if (((m = one_move_rogue((dirch + 96), 1)) == MOVE_FAILED) ||
				(m == STOPPED_ON_SOMETHING) ||
				interrupted) {
				break;
			}
		} while (!next_to_something(row, col));
		if (	(!interrupted) && passgo && (m == MOVE_FAILED) &&
				(dungeon[rogue.row][rogue.col] & TUNNEL)) {
			turn_passage(dirch + 96, 0);
		}
		break;
	case 'H':
	case 'J':
	case 'K':
	case 'L':
	case 'B':
	case 'Y':
	case 'U':
	case 'N':
		while ((!interrupted) && (one_move_rogue((dirch + 32), 1) == MOVED)) ;

		if (	(!interrupted) && passgo &&
				(dungeon[rogue.row][rogue.col] & TUNNEL)) {
			turn_passage(dirch + 32, 1);
		}
		break;
	}
}

boolean
is_passable(short row, short col)
{
	if ((row < MIN_ROW) || (row > (DROWS - 2)) || (col < 0) ||
		(col > (DCOLS-1))) {
		return(0);
	}
	if (dungeon[row][col] & HIDDEN) {
		return((dungeon[row][col] & TRAP) ? 1 : 0);
	}
	return(dungeon[row][col] & (FLOOR | TUNNEL | DOOR | STAIRS | TRAP));
}

boolean
next_to_something(short drow, short dcol)
{
	short i, j, i_end, j_end, row, col;
	short pass_count = 0;
	unsigned short s;

	if (confused) {
		return(1);
	}
	if (blind) {
		return(0);
	}
	i_end = (rogue.row < (DROWS-2)) ? 1 : 0;
	j_end = (rogue.col < (DCOLS-1)) ? 1 : 0;

	for (i = ((rogue.row > MIN_ROW) ? -1 : 0); i <= i_end; i++) {
		for (j = ((rogue.col > 0) ? -1 : 0); j <= j_end; j++) {
			if ((i == 0) && (j == 0)) {
				continue;
			}
			if (((rogue.row+i) == drow) && ((rogue.col+j) == dcol)) {
				continue;
			}
			row = rogue.row + i;
			col = rogue.col + j;
			s = dungeon[row][col];
			if (s & HIDDEN) {
				continue;
			}
			/* If the rogue used to be right, up, left, down, or right of
			 * row,col, and now isn't, then don't stop */
			if (s & (MONSTER | OBJECT | STAIRS)) {
				if (((row == drow) || (col == dcol)) &&
					(!((row == rogue.row) || (col == rogue.col)))) {
					continue;
				}
				return(1);
			}
			if (s & TRAP) {
				if (!(s & HIDDEN)) {
					if (((row == drow) || (col == dcol)) &&
						(!((row == rogue.row) || (col == rogue.col)))) {
						continue;
					}
					return(1);
				}
			}
			if ((((i - j) == 1) || ((i - j) == -1)) && (s & TUNNEL)) {
				if (++pass_count > 1) {
					return(1);
				}
			}
			if ((s & DOOR) && ((i == 0) || (j == 0))) {
					return(1);
			}
		}
	}
	return(0);
}

boolean
can_move(short row1, short col1, short row2, short col2) 
{
	if (!is_passable(row2, col2)) {
		return(0);
	}
	if ((row1 != row2) && (col1 != col2)) {
		if ((dungeon[row1][col1] & DOOR) || (dungeon[row2][col2] & DOOR)) {
			return(0);
		}
		if ((!dungeon[row1][col2]) || (!dungeon[row2][col1])) {
			return(0);
		}
	}
	return(1);
}

void
move_onto(void)
{
	short ch, d;
	boolean first_miss = 1;

	while (!is_direction(ch = rgetchar(), &d)) {
		beep();
		if (first_miss) {
			messagef(0, "direction? ");
			first_miss = 0;
		}
	}
	check_message();
	if (ch != CANCEL) {
		(void) one_move_rogue(ch, 0);
	}
}

boolean
is_direction(short c, short *d)
{
	switch(c) {
	case 'h':
		*d = LEFT;
		break;
	case 'j':
		*d = DOWN;
		break;
	case 'k':
		*d = UPWARD;
		break;
	case 'l':
		*d = RIGHT;
		break;
	case 'b':
		*d = DOWNLEFT;
		break;
	case 'y':
		*d = UPLEFT;
		break;
	case 'u':
		*d = UPRIGHT;
		break;
	case 'n':
		*d = DOWNRIGHT;
		break;
	case CANCEL:
		break;
	default:
		return(0);
	}
	return(1);
}

boolean
check_hunger(boolean msg_only)
{
	short i, n;
	boolean fainted = 0;

	if (rogue.moves_left == HUNGRY) {
		(void) strlcpy(hunger_str, "hungry", HUNGER_STR_LEN);
		messagef(0, "%s", hunger_str);
		print_stats(STAT_HUNGER);
	}
	if (rogue.moves_left == WEAK) {
		(void) strlcpy(hunger_str, "weak", HUNGER_STR_LEN);
		messagef(1, "%s", hunger_str);
		print_stats(STAT_HUNGER);
	}
	if (rogue.moves_left <= FAINT) {
		if (rogue.moves_left == FAINT) {
			(void) strlcpy(hunger_str, "faint", HUNGER_STR_LEN);
			messagef(1, "%s", hunger_str);
			print_stats(STAT_HUNGER);
		}
		n = get_rand(0, (FAINT - rogue.moves_left));
		if (n > 0) {
			fainted = 1;
			if (rand_percent(40)) {
				rogue.moves_left++;
			}
			messagef(1, "you faint");
			for (i = 0; i < n; i++) {
				if (coin_toss()) {
					mv_mons();
				}
			}
			messagef(1, "%s", you_can_move_again);
		}
	}
	if (msg_only) {
		return(fainted);
	}
	if (rogue.moves_left <= STARVE) {
		killed_by((object *) 0, STARVATION);
	}

	switch(e_rings) {
	/*case -2:
		Subtract 0, i.e. do nothing.
		break;*/
	case -1:
		rogue.moves_left -= (rogue.moves_left % 2);
		break;
	case 0:
		rogue.moves_left--;
		break;
	case 1:
		rogue.moves_left--;
		(void) check_hunger(1);
		rogue.moves_left -= (rogue.moves_left % 2);
		break;
	case 2:
		rogue.moves_left--;
		(void) check_hunger(1);
		rogue.moves_left--;
		break;
	}
	return(fainted);
}

boolean
reg_move(void)
{
	boolean fainted;

	if ((rogue.moves_left <= HUNGRY) || (cur_level >= max_level)) {
		fainted = check_hunger(0);
	} else {
		fainted = 0;
	}

	mv_mons();

	if (++m_moves >= 120) {
		m_moves = 0;
		wanderer();
	}
	if (halluc) {
		if (!(--halluc)) {
			unhallucinate();
		} else {
			hallucinate();
		}
	}
	if (blind) {
		if (!(--blind)) {
			unblind();
		}
	}
	if (confused) {
		if (!(--confused)) {
			unconfuse();
		}
	}
	if (bear_trap) {
		bear_trap--;
	}
	if (levitate) {
		if (!(--levitate)) {
			messagef(1, "you float gently to the ground");
			if (dungeon[rogue.row][rogue.col] & TRAP) {
				trap_player(rogue.row, rogue.col);
			}
		}
	}
	if (haste_self) {
		if (!(--haste_self)) {
			messagef(0, "you feel yourself slowing down");
		}
	}
	heal();
	if (auto_search > 0) {
		search(auto_search, auto_search);
	}
	return(fainted);
}

void
rest(int count)
{
	int i;

	interrupted = 0;

	for (i = 0; i < count; i++) {
		if (interrupted) {
			break;
		}
		(void) reg_move();
	}
}

char
gr_dir(void)
{
	short d;

	d = get_rand(1, 8);

	switch(d) {
		case 1:
			d = 'j';
			break;
		case 2:
			d = 'k';
			break;
		case 3:
			d = 'l';
			break;
		case 4:
			d = 'h';
			break;
		case 5:
			d = 'y';
			break;
		case 6:
			d = 'u';
			break;
		case 7:
			d = 'b';
			break;
		case 8:
			d = 'n';
			break;
	}
	return(d);
}

void
heal(void)
{
	static short heal_exp = -1, n, c = 0;
	static boolean alt;

	if (rogue.hp_current == rogue.hp_max) {
		c = 0;
		return;
	}
	if (rogue.exp != heal_exp) {
		heal_exp = rogue.exp;

		switch(heal_exp) {
		case 1:
			n = 20;
			break;
		case 2:
			n = 18;
			break;
		case 3:
			n = 17;
			break;
		case 4:
			n = 14;
			break;
		case 5:
			n = 13;
			break;
		case 6:
			n = 10;
			break;
		case 7:
			n = 9;
			break;
		case 8:
			n = 8;
			break;
		case 9:
			n = 7;
			break;
		case 10:
			n = 4;
			break;
		case 11:
			n = 3;
			break;
		case 12:
		default:
			n = 2;
		}
	}
	if (++c >= n) {
		c = 0;
		rogue.hp_current++;
		if ((alt = !alt)) {
			rogue.hp_current++;
		}
		if ((rogue.hp_current += regeneration) > rogue.hp_max) {
			rogue.hp_current = rogue.hp_max;
		}
		print_stats(STAT_HP);
	}
}

boolean
can_turn(short nrow, short ncol)
{
	if ((dungeon[nrow][ncol] & TUNNEL) && is_passable(nrow, ncol)) {
		return(1);
	}
	return(0);
}

void
turn_passage(short dir, boolean fast)
{
	short crow = rogue.row, ccol = rogue.col, turns = 0;
	short ndir = 0;

	if ((dir != 'h') && can_turn(crow, ccol + 1)) {
		turns++;
		ndir = 'l';
	}
	if ((dir != 'l') && can_turn(crow, ccol - 1)) {
		turns++;
		ndir = 'h';
	}
	if ((dir != 'k') && can_turn(crow + 1, ccol)) {
		turns++;
		ndir = 'j';
	}
	if ((dir != 'j') && can_turn(crow - 1, ccol)) {
		turns++;
		ndir = 'k';
	}
	if (turns == 1) {
		multiple_move_rogue(ndir - (fast ? 32 : 96));
	}
}
