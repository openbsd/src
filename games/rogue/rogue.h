/*	$OpenBSD: rogue.h,v 1.3 1998/08/22 08:55:43 pjanzen Exp $	*/
/*	$NetBSD: rogue.h,v 1.4 1995/04/24 12:25:04 cgd Exp $	*/

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
 *
 *	@(#)rogue.h	8.1 (Berkeley) 5/31/93
 */

/*
 * rogue.h
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  This notice shall not be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 */

#define boolean char

#define NOTHING		((unsigned short)     0)
#define OBJECT		((unsigned short)    01)
#define MONSTER		((unsigned short)    02)
#define STAIRS		((unsigned short)    04)
#define HORWALL		((unsigned short)   010)
#define VERTWALL	((unsigned short)   020)
#define DOOR		((unsigned short)   040)
#define FLOOR		((unsigned short)  0100)
#define TUNNEL		((unsigned short)  0200)
#define TRAP		((unsigned short)  0400)
#define HIDDEN		((unsigned short) 01000)

#define ARMOR		((unsigned short)   01)
#define WEAPON		((unsigned short)   02)
#define SCROL		((unsigned short)   04)
#define POTION		((unsigned short)  010)
#define GOLD		((unsigned short)  020)
#define FOOD		((unsigned short)  040)
#define WAND		((unsigned short) 0100)
#define RING		((unsigned short) 0200)
#define AMULET		((unsigned short) 0400)
#define ALL_OBJECTS	((unsigned short) 0777)

#define LEATHER 0
#define RINGMAIL 1
#define SCALE 2
#define CHAIN 3
#define BANDED 4
#define SPLINT 5
#define PLATE 6
#define ARMORS 7

#define BOW 0
#define DART 1
#define ARROW 2
#define DAGGER 3
#define SHURIKEN 4
#define MACE 5
#define LONG_SWORD 6
#define TWO_HANDED_SWORD 7
#define WEAPONS 8

#define MAX_PACK_COUNT 24

#define PROTECT_ARMOR 0
#define HOLD_MONSTER 1
#define ENCH_WEAPON 2
#define ENCH_ARMOR 3
#define IDENTIFY 4
#define TELEPORT 5
#define SLEEP 6
#define SCARE_MONSTER 7
#define REMOVE_CURSE 8
#define CREATE_MONSTER 9
#define AGGRAVATE_MONSTER 10
#define MAGIC_MAPPING 11
#define CON_MON 12
#define SCROLS 13

#define INCREASE_STRENGTH 0
#define RESTORE_STRENGTH 1
#define HEALING 2
#define EXTRA_HEALING 3
#define POISON 4
#define RAISE_LEVEL 5
#define BLINDNESS 6
#define HALLUCINATION 7
#define DETECT_MONSTER 8
#define DETECT_OBJECTS 9
#define CONFUSION 10
#define LEVITATION 11
#define HASTE_SELF 12
#define SEE_INVISIBLE 13
#define POTIONS 14

#define TELE_AWAY 0
#define SLOW_MONSTER 1
#define INVISIBILITY 2
#define POLYMORPH 3
#define HASTE_MONSTER 4
#define MAGIC_MISSILE 5
#define CANCELLATION 6
#define DO_NOTHING 7
#define DRAIN_LIFE 8
#define COLD 9
#define FIRE 10
#define WANDS 11

#define STEALTH 0
#define R_TELEPORT 1
#define REGENERATION 2
#define SLOW_DIGEST 3
#define ADD_STRENGTH 4
#define SUSTAIN_STRENGTH 5
#define DEXTERITY 6
#define ADORNMENT 7
#define R_SEE_INVISIBLE 8
#define MAINTAIN_ARMOR 9
#define SEARCHING 10
#define RINGS 11

#define RATION 0
#define FRUIT 1

#define NOT_USED	((unsigned short)   0)
#define BEING_WIELDED	((unsigned short)  01)
#define BEING_WORN	((unsigned short)  02)
#define ON_LEFT_HAND	((unsigned short)  04)
#define ON_RIGHT_HAND	((unsigned short) 010)
#define ON_EITHER_HAND	((unsigned short) 014)
#define BEING_USED	((unsigned short) 017)

#define NO_TRAP -1
#define TRAP_DOOR 0
#define BEAR_TRAP 1
#define TELE_TRAP 2
#define DART_TRAP 3
#define SLEEPING_GAS_TRAP 4
#define RUST_TRAP 5
#define TRAPS 6

#define STEALTH_FACTOR 3
#define R_TELE_PERCENT 8

#define UNIDENTIFIED	((unsigned short) 00)	/* MUST BE ZERO! */
#define IDENTIFIED	((unsigned short) 01)
#define CALLED		((unsigned short) 02)

#define DROWS 24
#define DCOLS 80
#define NMESSAGES 5
#define MAX_TITLE_LENGTH 30
#define MAXSYLLABLES 40
#define MAX_METAL 14
#define WAND_MATERIALS 30
#define GEMS 14

#define GOLD_PERCENT 46

#define MAX_OPT_LEN 40

struct id {
	short value;
	char *title;
	char *real;
	unsigned short id_status;
};

/* The following #defines provide more meaningful names for some of the
 * struct object fields that are used for monsters.  This, since each monster
 * and object (scrolls, potions, etc) are represented by a struct object.
 * Ideally, this should be handled by some kind of union structure.
 */

#define m_damage damage
#define hp_to_kill quantity
#define m_char ichar
#define first_level is_protected
#define last_level is_cursed
#define m_hit_chance class
#define stationary_damage identified
#define drop_percent which_kind
#define trail_char d_enchant
#define slowed_toggle quiver
#define moves_confused hit_enchant
#define nap_length picked_up
#define disguise what_is
#define next_monster next_object

struct obj {				/* comment is monster meaning */
	unsigned long m_flags;	/* monster flags */
	char *damage;			/* damage it does */
	short quantity;			/* hit points to kill */
	short ichar;			/* 'A' is for aquatar */
	short kill_exp;			/* exp for killing it */
	short is_protected;		/* level starts */
	short is_cursed;		/* level ends */
	short class;			/* chance of hitting you */
	short identified;		/* 'F' damage, 1,2,3... */
	unsigned short which_kind; /* item carry/drop % */
	short o_row, o_col, o;	/* o is how many times stuck at o_row, o_col */
	short row, col;			/* current row, col */
	short d_enchant;		/* room char when detect_monster */
	short quiver;			/* monster slowed toggle */
	short trow, tcol;		/* target row, col */
	short hit_enchant;		/* how many moves is confused */
	unsigned short what_is;	/* imitator's charactor (?!%: */
	short picked_up;		/* sleep from wand of sleep */
	unsigned short in_use_flags;
	struct obj *next_object;	/* next monster */
};

typedef struct obj object;

#define INIT_AW		(object*)0
#define INIT_RINGS	(object*)0
#define INIT_HP		12
#define INIT_STR	16
#define INIT_EXPLEVEL	1
#define INIT_EXP	0
#define INIT_PACK	{0}
#define INIT_GOLD	0
#define INIT_CHAR	'@'
#define INIT_MOVES	1250

struct fightr {
	object *armor;
	object *weapon;
	object *left_ring, *right_ring;
	short hp_current;
	short hp_max;
	short str_current;
	short str_max;
	object pack;
	long gold;
	short exp;
	long exp_points;
	short row, col;
	short fchar;
	short moves_left;
};

typedef struct fightr fighter;

struct dr {
	short oth_room;
	short oth_row,
	      oth_col;
	short door_row,
		  door_col;
};

typedef struct dr door;

struct rm {
	short bottom_row, right_col, left_col, top_row;
	door doors[4];
	unsigned short is_room;
};

typedef struct rm room;

#define MAXROOMS 9
#define BIG_ROOM 10

#define NO_ROOM -1

#define PASSAGE -3		/* cur_room value */

#define AMULET_LEVEL 26

#define R_NOTHING	((unsigned short) 01)
#define R_ROOM		((unsigned short) 02)
#define R_MAZE		((unsigned short) 04)
#define R_DEADEND	((unsigned short) 010)
#define R_CROSS		((unsigned short) 020)

#define MAX_EXP_LEVEL 21
#define MAX_EXP 10000001L
#define MAX_GOLD 999999
#define MAX_ARMOR 99
#define MAX_HP 999
#define MAX_STRENGTH 99
#define LAST_DUNGEON 99

#define STAT_LEVEL 01
#define STAT_GOLD 02
#define STAT_HP 04
#define STAT_STRENGTH 010
#define STAT_ARMOR 020
#define STAT_EXP 040
#define STAT_HUNGER 0100
#define STAT_LABEL 0200
#define STAT_ALL 0377

#define PARTY_TIME 10	/* one party somewhere in each 10 level span */

#define MAX_TRAPS 10	/* maximum traps per level */

#define HIDE_PERCENT 12

struct tr {
	short trap_type;
	short trap_row, trap_col;
};

typedef struct tr trap;

extern fighter rogue;
extern room rooms[];
extern trap traps[];
extern unsigned short dungeon[DROWS][DCOLS];
extern object level_objects;

extern struct id id_scrolls[];
extern struct id id_potions[];
extern struct id id_wands[];
extern struct id id_rings[];
extern struct id id_weapons[];
extern struct id id_armors[];

extern object mon_tab[];
extern object level_monsters;

#define MONSTERS 26

#define HASTED					01L
#define SLOWED					02L
#define INVISIBLE				04L
#define ASLEEP				   010L
#define WAKENS				   020L
#define WANDERS				   040L
#define FLIES				  0100L
#define FLITS				  0200L
#define CAN_FLIT			  0400L		/* can, but usually doesn't, flit */
#define CONFUSED	 		 01000L
#define RUSTS				 02000L
#define HOLDS				 04000L
#define FREEZES				010000L
#define STEALS_GOLD			020000L
#define STEALS_ITEM			040000L
#define STINGS			   0100000L
#define DRAINS_LIFE		   0200000L
#define DROPS_LEVEL		   0400000L
#define SEEKS_GOLD		  01000000L
#define FREEZING_ROGUE	  02000000L
#define RUST_VANISHED	  04000000L
#define CONFUSES		 010000000L
#define IMITATES		 020000000L
#define FLAMES			 040000000L
#define STATIONARY		0100000000L		/* damage will be 1,2,3,... */
#define NAPPING			0200000000L		/* can't wake up for a while */
#define ALREADY_MOVED	0400000000L

#define SPECIAL_HIT		(RUSTS|HOLDS|FREEZES|STEALS_GOLD|STEALS_ITEM|STINGS|DRAINS_LIFE|DROPS_LEVEL)

#define WAKE_PERCENT 45
#define FLIT_PERCENT 40
#define PARTY_WAKE_PERCENT 75

#define HYPOTHERMIA 1
#define STARVATION 2
#define POISON_DART 3
#define QUIT 4
#define WIN 5
#define KFIRE 6

#define UPWARD 0
#define UPRIGHT 1
#define RIGHT 2
#define DOWNRIGHT 3
#define DOWN 4
#define DOWNLEFT 5
#define LEFT 6
#define UPLEFT 7
#define DIRS 8

#define ROW1 7
#define ROW2 15

#define COL1 26
#define COL2 52

#define MOVED 0
#define MOVE_FAILED -1
#define STOPPED_ON_SOMETHING -2
#define CANCEL '\033'
#define LIST '*'

#define HUNGRY 300
#define WEAK 150
#define FAINT 20
#define STARVE 0

#define MIN_ROW 1

struct rogue_time {
	short year;		/* >= 1987 */
	short month;	/* 1 - 12 */
	short day;		/* 1 - 31 */
	short hour;		/* 0 - 23 */
	short minute;	/* 0 - 59 */
	short second;	/* 0 - 59 */
};

#ifdef CURSES
struct _win_st {
	short _cury, _curx;
	short _maxy, _maxx;
};

typedef struct _win_st WINDOW;

#ifndef FALSE
#define FALSE 0
#endif

extern int LINES, COLS;
extern WINDOW *curscr;
extern char *CL;

void	initscr __P((void));
void	endwin __P((void));
void	move __P((short, short));
void	mvaddstr __P((short, short, char *));
void	addstr __P((char *));
void	addch __P((int));
void	mvaddch __P((short, short, int));
void	refresh __P((void));
void	wrefresh __P((WINDOW *scr));
int	mvinch __P((short, short));
void	clear __P((void));
void	clrtoeol __P((void));
void	standout __P((void));
void	standend __P((void));
void	crmode __P((void));
void	noecho __P((void));
void	nonl __P((void));
void	clear_buffers __P((void));
void	put_char_at __P((short, short, int));
void	put_cursor __P((short, short));
void	put_st_char __P((int));
void	get_term_info __P((void));
boolean	tc_tname __P((FILE *, char *, char *));
void	tc_gtdata __P((FILE *, char *));
void	tc_gets __P((char *, char **));
void	tc_gnum __P((char *, int *));
void	tstp __P((void));
void	tc_cmget __P((void));

#else
#include <curses.h>
#endif

/*
 * external routine declarations.
 */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

object	*alloc_object __P((void));
object	*check_duplicate __P((object *, object *));
char	*get_ench_color __P((void));
object	*get_letter_object __P((int));
object	*get_thrown_at_monster __P((object *, short, short *, short *));
object	*get_zapped_monster __P((short, short *, short *));
object	*gr_monster __P((object *, int));
object	*gr_object __P((void));
char	*md_getenv __P((char *));
char	*md_gln __P((void));
char	*md_malloc __P((int));
char	*mon_name __P((object *));
char	*name_of __P((object *));
object	*object_at __P((object *, short, short));
object	*pick_up __P((short, short, short *));
void	add_exp __P((int, boolean));
void	add_mazes __P((void));
void	add_traps __P((void));
void	aggravate __P((void));
void	aim_monster __P((object *));
void	bounce __P((short, short, short, short, short));
void	byebye __P((int));
void	c_object_for_wizard __P((void));
void	call_it __P((void));
boolean	can_move __P((short, short, short, short));
boolean	can_turn __P((short, short));
void	center __P((short, char *));
void	check_gold_seeker __P((object *));
boolean	check_hunger __P((boolean));
boolean	check_imitator __P((object *));
void	check_message __P((void));
int	check_up __P((void));
void	clean_up __P((char *));
void	clear_level __P((void));
void	cnfs __P((void));
int	coin_toss __P((void));
int	connect_rooms __P((short, short));
void	cough_up __P((object *));
void	create_monster __P((void));
int	damage_for_strength __P((void));
void	darken_room __P((short));
void	disappear __P((object *));
void	do_args __P((int, char **));
void	do_opts __P((void));
void	do_put_on __P((object *, boolean));
void	do_shell __P((void));
void	do_wear __P((object *));
void	do_wield __P((object *));
void	dr_course __P((object *, boolean, short, short));
void	drain_life __P((void));
void	draw_magic_map __P((void));
void	draw_simple_passage __P((short, short, short, short, short));
void	drop __P((void));
int	drop_check __P((void));
void	drop_level __P((void));
void	eat __P((void));
void	edit_opts __P((void));
void	env_get_value __P((char **, char *, boolean));
void	error_save __P((int));
void	fight __P((int));
void	fill_it __P((int, boolean));
void	fill_out_level __P((void));
boolean	flame_broil __P((object *));
int	flit __P((object *));
void	flop_weapon __P((object *, short, short));
void	free_object __P((object *));
void	free_stuff __P((object *));
void	freeze __P((object *));
int	get_armor_class __P((object *));
int	get_com_id __P((int *, short));
int	get_damage __P((char *, boolean));
void	get_desc __P((object *, char *));
int	get_dir __P((short, short, short, short));
void	get_dir_rc __P((short, short *, short *, short));
char	get_dungeon_char __P((short, short));
int	get_exp_level __P((long));
void	get_food __P((object *, boolean));
int	get_hit_chance __P((object *));
int	get_input_line __P((char *, char *, char *, char *, boolean, boolean));
char	get_mask_char __P((unsigned short));
int	get_number __P((char *));
boolean	get_oth_room __P((short, short *, short *));
int	get_rand __P((int, int));
short	get_room_number __P((short, short));
int	get_value __P((object *));
int	get_w_damage __P((object *));
void	get_wand_and_ring_materials __P((void));
int	get_weapon_damage __P((object *));
char	gmc __P((object *));
char	gmc_row_col __P((short, short));
void	go_blind __P((void));
boolean	gold_at __P((short, short));
void	gr_armor __P((object *));
char	gr_dir __P((void));
char	gr_obj_char __P((void));
void	gr_potion __P((object *));
void	gr_ring __P((object *, boolean));
short	gr_room __P((void));
void	gr_row_col __P((short *, short *, unsigned short));
void	gr_scroll __P((object *));
void	gr_wand __P((object *));
void	gr_weapon __P((object *, int));
void	hallucinate __P((void));
boolean	has_amulet __P((void));
boolean	has_been_touched __P((struct rogue_time *, struct rogue_time *));
void	heal __P((void));
void	hide_boxed_passage __P((short, short, short, short, short));
void	hold_monster __P((void));
int	hp_raise __P((void));
void	id_all __P((void));
void	id_com __P((void));
void	id_trap __P((void));
void	id_type __P((void));
void	idntfy __P((void));
boolean	imitating __P((short, short));
int	init __P((int, char **));
void	init_str __P((char **, char *));
void	insert_score __P((char [][], char [][], char *, short, short, object *, int));
void	inv_armor_weapon __P((boolean));
void	inv_rings __P((void));
void	inventory __P((object *, unsigned short));
boolean	is_all_connected __P((void));
boolean	is_digit __P((int));
boolean	is_direction __P((short, short *));
boolean	is_pack_letter __P((short *, unsigned short *));
boolean	is_passable __P((short, short));
boolean	is_vowel __P((short));
void	kick_into_pack __P((void));
void	killed_by __P((object *, short));
long	lget_number __P((char *));
void	light_passage __P((short, short));
void	light_up_room __P((int));
boolean	m_confuse __P((object *));
void	make_level __P((void));
void	make_maze __P((short, short, short, short, short, short));
void	make_party __P((void));
void	make_room __P((short, short, short, short));
void	make_scroll_titles __P((void));
boolean	mask_pack __P((object *, unsigned short));
boolean	mask_room __P((short, short *, short *, unsigned short));
void	md_cbreak_no_echo_nonl __P((boolean));
boolean	md_df __P((char *));
void	md_exit __P((int));
void	md_gct __P((struct rogue_time *));
char   *md_gdtcf __P((void));
int	md_get_file_id __P((char *));
void	md_gfmt __P((char *, struct rogue_time *));
int	md_gseed __P((void));
void	md_heed_signals __P((void));
void	md_ignore_signals __P((void));
int	md_link_count __P((char *));
void	md_lock __P((boolean));
void	md_shell __P((char *));
void	md_sleep __P((int));
void	md_slurp __P((void));
void	md_tstp __P((void));
void	message __P((char *, boolean));
void	mix_colors __P((void));
void	mix_colors __P((void));
void	mix_random_rooms __P((void));
int	mon_can_go __P((object *, short, short));
int	mon_damage __P((object *, short));
void	mon_hit __P((object *));
boolean	mon_sees __P((object *, short, short));
int	move_confused __P((object *));
void	move_mon_to __P((object *, short, short));
void	move_onto __P((void));
int	mtry __P((object *, short, short));
void	multiple_move_rogue __P((short));
void	mv_1_monster __P((object *, short, short));
void	mv_aquatars __P((void));
void	mv_mons __P((void));
int	name_cmp __P((char *, char *));
short	next_avail_ichar __P((void));
boolean	next_to_something __P((short, short));
void	nickize __P((char *, char *, char *));
int	no_room_for_monster __P((int));
int	one_move_rogue __P((short, short));
void	onintr __P((int));
void	opt_erase __P((int));
void	opt_go __P((int));
void	opt_show __P((int));
short	pack_count __P((object *));
short	pack_letter __P((char *, unsigned short));
void	pad __P((char *, short));
void	party_monsters __P((int, int));
short	party_objects __P((int));
void	place_at __P((object *, short, short));
void	plant_gold __P((short, short, boolean));
void	play_level __P((void));
void	player_init __P((void));
void	player_init __P((void));
void	potion_heal __P((int));
int	pr_com_id __P((int));
int	pr_motion_char __P((int));
void	print_stats __P((int));
void	put_amulet __P((void));
void	put_door __P((room *, short, short *, short *));
void	put_gold __P((void));
void	put_m_at __P((short, short, object *));
void	put_mons __P((void));
void	put_objects __P((void));
void	put_on_ring __P((void));
void	put_player __P((short));
void	put_scores __P((object *, short));
void	put_stairs __P((void));
void	quaff __P((void));
void	quit __P((boolean));
int	r_index __P((char *, int, boolean));
void	r_read __P((FILE *, char *, int));
void	r_write __P((FILE *, char *, int));
void	rand_around __P((short, short *, short *));
int	rand_percent __P((int));
void	rand_place __P((object *));
void	read_pack __P((object *, FILE *, boolean));
void	read_scroll __P((void));
void	read_string __P((char *, FILE *));
void	recursive_deadend __P((short, short *, short, short));
boolean	reg_move __P((void));
void	relight __P((void));
void	remessage __P((short));
void	remove_ring __P((void));
void	rest __P((int));
void	restore __P((char *));
int	rgetchar __P((void));
void	ring_stats __P((boolean));
int	rogue_can_see __P((short, short));
void	rogue_damage __P((short, object *, short));
void	rogue_hit __P((object *, boolean));
int	rogue_is_around __P((short, short));
long	rrandom __P((void));
void	rust __P((object *));
void	rw_dungeon __P((FILE *, boolean));
void	rw_id __P((struct id *, FILE *, int, boolean));
void	rw_rooms __P((FILE *, boolean));
void	s_con_mon __P((object *));
int	same_col __P((int, int));
int	same_row __P((int, int));
void	save_game __P((void));
void	save_into_file __P((char *));
void	save_screen __P((void));
void	search __P((short, boolean));
boolean	seek_gold __P((object *));
void	sell_pack __P((void));
void	sf_error __P((void));
void	show_average_hp __P((void));
void	show_monsters __P((void));
void	show_objects __P((void));
void	show_traps __P((void));
void	single_inv __P((short));
void	sound_bell __P((void));
void	special_hit __P((object *));
void	srrandom __P((int));
void	start_window __P((void));
void	start_window __P((void));
void	steal_gold __P((object *));
void	steal_item __P((object *));
void	sting __P((object *));
void	stop_window __P((void));
void	stop_window __P((void));
void	take_a_nap __P((void));
void	take_from_pack __P((object *, object *));
void	take_off __P((void));
void	tele __P((void));
void	tele_away __P((object *));
void	throw __P((void));
boolean	throw_at_monster __P((object *, object *));
int	to_hit __P((object *));
short	trap_at __P((short, short));
void	trap_player __P((short, short));
boolean	try_to_cough __P((short, short, object *));
void	turn_passage __P((short, boolean));
void	un_put_on __P((object *));
void	unblind __P((void));
void	unconfuse __P((void));
void	uncurse_all __P((void));
void	unhallucinate __P((void));
void	unwear __P((object *));
void	unwield __P((object *));
void	vanish __P((object *, short, object *));
void	visit_rooms __P((int));
void	wait_for_ack __P((void));
void	wake_room __P((short, boolean, short, short));
void	wake_up __P((object *));
void	wanderer __P((void));
void	wdrain_life __P((object *));
void	wear __P((void));
void	wield __P((void));
void	win __P((void));
void	wizardize __P((void));
void	write_pack __P((object *, FILE *));
void	write_string __P((char *, FILE *));
long	xxx __P((boolean));
void	xxxx __P((char *, short));
void	zap_monster __P((object *, unsigned short));
void	zapp __P((void));
object *add_to_pack __P((object *, object *, int));
struct id *get_id_table __P((object *));
unsigned short gr_what_is __P((void));

extern	boolean	ask_quit;
extern	boolean	being_held;
extern	boolean	cant_int;
extern	boolean	con_mon;
extern	boolean	detect_monster;
extern	boolean	did_int;
extern	boolean	interrupted;
extern	boolean	is_wood[];
extern	boolean	jump;
extern	boolean	maintain_armor;
extern	boolean	mon_disappeared;
extern	boolean	msg_cleared;
extern	boolean	no_skull;
extern	boolean	passgo;
extern	boolean	r_see_invisible;
extern	boolean	r_teleport;
extern	boolean	save_is_interactive;
extern	boolean	score_only;
extern	boolean	see_invisible;
extern	boolean	sustain_strength;
extern	boolean	trap_door;
extern	boolean	wizard;
extern	char	hit_message[];
extern	char	hunger_str[];
extern	char	login_name[];
extern	char   *byebye_string;
extern	char   *curse_message;
extern	char   *error_file;
extern	char   *fruit;
extern	char   *m_names[];
extern	char   *more;
extern	char   *new_level_message;
extern	char   *nick_name;
extern	char   *press_space;
extern	char   *save_file;
extern	char   *you_can_move_again;
extern	long	level_points[];
extern	short	add_strength;
extern	short	auto_search;
extern	short	bear_trap;
extern	short	blind;
extern	short	confused;
extern	short	cur_level;
extern	short	cur_room;
extern	short	e_rings;
extern	short	extra_hp;
extern	short	foods;
extern	short	halluc;
extern	short	haste_self;
extern	short	less_hp;
extern	short	levitate;
extern	short	m_moves;
extern	short	max_level;
extern	short	party_room;
extern	short	r_rings;
extern	short	regeneration;
extern	short	ring_exp;
extern	short	stealthy;
