/*	$OpenBSD: rogue.h,v 1.8 2002/07/18 07:13:57 pjanzen Exp $	*/
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

#define NUM_SCORE_ENTRIES 10

#define GOLD_PERCENT 46

#define MAX_OPT_LEN 40

struct id {
	short value;
	char title[MAX_TITLE_LENGTH];
	const char *real;
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
#define INIT_PACK	{0, (char *)NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (struct obj *)NULL}
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

#include <curses.h>

/*
 * external routine declarations.
 */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

object	*alloc_object(void);
object	*check_duplicate(object *, object *);
char	*get_ench_color(void);
object	*get_letter_object(int);
object	*get_thrown_at_monster(object *, short, short *, short *);
object	*get_zapped_monster(short, short *, short *);
object	*gr_monster(object *, int);
object	*gr_object(void);
char	*md_getenv(char *);
char	*md_gln(void);
char	*md_malloc(int);
char	*mon_name(object *);
char	*name_of(const object *);
object	*object_at(object *, short, short);
object	*pick_up(short, short, short *);
void	add_exp(int, boolean);
void	add_mazes(void);
void	add_traps(void);
void	aggravate(void);
void	aim_monster(object *);
void	bounce(short, short, short, short, short);
void	byebye(int);
void	c_object_for_wizard(void);
void	call_it(void);
boolean	can_move(short, short, short, short);
boolean	can_turn(short, short);
void	center(short, const char *);
void	check_gold_seeker(object *);
boolean	check_hunger(boolean);
boolean	check_imitator(object *);
void	check_message(void);
int	check_up(void);
void	clean_up(char *);
void	clear_level(void);
void	cnfs(void);
int	coin_toss(void);
int	connect_rooms(short, short);
void	cough_up(object *);
void	create_monster(void);
int	damage_for_strength(void);
void	darken_room(short);
void	disappear(object *);
void	do_args(int, char **);
void	do_opts(void);
void	do_put_on(object *, boolean);
void	do_shell(void);
void	do_wear(object *);
void	do_wield(object *);
void	dr_course(object *, boolean, short, short);
void	drain_life(void);
void	draw_magic_map(void);
void	draw_simple_passage(short, short, short, short, short);
void	drop(void);
int	drop_check(void);
void	drop_level(void);
void	eat(void);
void	edit_opts(void);
void	env_get_value(char **, char *, boolean);
void	error_save(int);
void	fight(int);
void	fill_it(int, boolean);
void	fill_out_level(void);
boolean	flame_broil(object *);
int	flit(object *);
void	flop_weapon(object *, short, short);
void	free_object(object *);
void	free_stuff(object *);
void	freeze(object *);
int	get_armor_class(const object *);
int	get_com_id(int *, short);
int	get_damage(char *, boolean);
void	get_desc(const object *, char *, size_t);
int	get_dir(short, short, short, short);
void	get_dir_rc(short, short *, short *, short);
char	get_dungeon_char(short, short);
int	get_exp_level(long);
void	get_food(object *, boolean);
int	get_hit_chance(object *);
int	get_input_line(const char *, const char *, char *, size_t, const char *, boolean, boolean);
char	get_mask_char(unsigned short);
int	get_number(char *);
boolean	get_oth_room(short, short *, short *);
int	get_rand(int, int);
short	get_room_number(short, short);
int	get_value(object *);
int	get_w_damage(const object *);
void	get_wand_and_ring_materials(void);
int	get_weapon_damage(object *);
char	gmc(object *);
char	gmc_row_col(short, short);
void	go_blind(void);
boolean	gold_at(short, short);
void	gr_armor(object *);
char	gr_dir(void);
char	gr_obj_char(void);
void	gr_potion(object *);
void	gr_ring(object *, boolean);
short	gr_room(void);
void	gr_row_col(short *, short *, unsigned short);
void	gr_scroll(object *);
void	gr_wand(object *);
void	gr_weapon(object *, int);
void	hallucinate(void);
boolean	has_amulet(void);
boolean	has_been_touched(struct rogue_time *, struct rogue_time *);
void	heal(void);
void	hide_boxed_passage(short, short, short, short, short);
void	hold_monster(void);
int	hp_raise(void);
void	id_all(void);
void	id_com(void);
void	id_trap(void);
void	id_type(void);
void	idntfy(void);
boolean	imitating(short, short);
int	init(int, char **);
void	init_str(char **, char *);
void	inv_armor_weapon(boolean);
void	inv_rings(void);
void	inventory(object *, unsigned short);
boolean	is_all_connected(void);
boolean	is_digit(int);
boolean	is_direction(short, short *);
boolean	is_pack_letter(short *, unsigned short *);
boolean	is_passable(short, short);
boolean	is_vowel(short);
void	kick_into_pack(void);
void	killed_by(const object *, short);
void	light_passage(short, short);
void	light_up_room(int);
boolean	m_confuse(object *);
void	make_level(void);
void	make_maze(short, short, short, short, short, short);
void	make_party(void);
void	make_room(short, short, short, short);
void	make_scroll_titles(void);
boolean	mask_pack(object *, unsigned short);
boolean	mask_room(short, short *, short *, unsigned short);
boolean	md_df(char *);
void	md_exit(int);
void	md_gct(struct rogue_time *);
int	md_get_file_id(char *);
void	md_gfmt(char *, struct rogue_time *);
int	md_gseed(void);
void	md_heed_signals(void);
void	md_ignore_signals(void);
int	md_link_count(char *);
void	md_lock(boolean);
void	md_shell(const char *);
void	md_sleep(int);
void	md_slurp(void);
void	messagef(boolean, const char *, ...)
#ifdef __GNUC__
	  __attribute__((__format__(__printf__, 2, 3)))
#endif
		;
void	mix_colors(void);
void	mix_colors(void);
void	mix_random_rooms(void);
int	mon_can_go(object *, short, short);
int	mon_damage(object *, short);
void	mon_hit(object *);
boolean	mon_sees(object *, short, short);
int	move_confused(object *);
void	move_mon_to(object *, short, short);
void	move_onto(void);
int	mtry(object *, short, short);
void	multiple_move_rogue(short);
void	mv_1_monster(object *, short, short);
void	mv_aquatars(void);
void	mv_mons(void);
short	next_avail_ichar(void);
boolean	next_to_something(short, short);
int	no_room_for_monster(int);
int	one_move_rogue(short, short);
void	onintr(int);
void	opt_erase(int);
void	opt_go(int);
void	opt_show(int);
short	pack_count(object *);
short	pack_letter(char *, unsigned short);
void	party_monsters(int, int);
short	party_objects(int);
void	place_at(object *, short, short);
void	plant_gold(short, short, boolean);
void	play_level(void);
void	player_init(void);
void	player_init(void);
void	potion_heal(int);
int	pr_com_id(int);
int	pr_motion_char(int);
void	print_stats(int);
void	put_amulet(void);
void	put_door(room *, short, short *, short *);
void	put_gold(void);
void	put_m_at(short, short, object *);
void	put_mons(void);
void	put_objects(void);
void	put_on_ring(void);
void	put_player(short);
void	put_scores(const object *, short);
void	put_stairs(void);
void	quaff(void);
void	quit(boolean);
int	r_index(char *, int, boolean);
void	r_read(FILE *, char *, size_t);
void	r_write(FILE *, char *, size_t);
void	rand_around(short, short *, short *);
int	rand_percent(int);
void	rand_place(object *);
void	read_pack(object *, FILE *, boolean);
void	read_scroll(void);
void	read_string(char *, size_t, FILE *);
void	recursive_deadend(short, short *, short, short);
boolean	reg_move(void);
void	relight(void);
void	remessage(short);
void	remove_ring(void);
void	rest(int);
void	restore(char *);
int	rgetchar(void);
void	ring_stats(boolean);
int	rogue_can_see(short, short);
void	rogue_damage(short, object *, short);
void	rogue_hit(object *, boolean);
int	rogue_is_around(short, short);
long	rrandom(void);
void	rust(object *);
void	rw_dungeon(FILE *, boolean);
void	rw_id(struct id *, FILE *, int, boolean);
void	rw_rooms(FILE *, boolean);
void	s_con_mon(object *);
int	same_col(int, int);
int	same_row(int, int);
void	save_game(void);
void	save_into_file(char *);
void	save_screen(void);
void	search(short, boolean);
boolean	seek_gold(object *);
void	sell_pack(void);
void	sf_error(void);
void	show_average_hp(void);
void	show_monsters(void);
void	show_objects(void);
void	show_traps(void);
void	single_inv(short);
void	special_hit(object *);
void	srrandom(int);
void	start_window(void);
void	start_window(void);
void	steal_gold(object *);
void	steal_item(object *);
void	sting(object *);
void	stop_window(void);
void	stop_window(void);
void	take_a_nap(void);
void	take_from_pack(object *, object *);
void	take_off(void);
void	tele(void);
void	tele_away(object *);
void	throw(void);
boolean	throw_at_monster(object *, object *);
int	to_hit(object *);
short	trap_at(short, short);
void	trap_player(short, short);
boolean	try_to_cough(short, short, object *);
void	turn_passage(short, boolean);
void	un_put_on(object *);
void	unblind(void);
void	unconfuse(void);
void	uncurse_all(void);
void	unhallucinate(void);
void	unwear(object *);
void	unwield(object *);
void	vanish(object *, short, object *);
void	visit_rooms(int);
void	wait_for_ack(void);
void	wake_room(short, boolean, short, short);
void	wake_up(object *);
void	wanderer(void);
void	wdrain_life(object *);
void	wear(void);
void	wield(void);
void	win(void);
void	wizardize(void);
void	write_pack(object *, FILE *);
void	write_string(char *, FILE *);
long	xxx(boolean);
void	xxxx(char *, short);
void	zap_monster(object *, unsigned short);
void	zapp(void);
object *add_to_pack(object *, object *, int);
struct id *get_id_table(const object *);
unsigned short gr_what_is(void);

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
#define HIT_MESSAGE_LEN 80
extern	char	hit_message[HIT_MESSAGE_LEN];
#define HUNGER_STR_LEN 8
extern	char	hunger_str[HUNGER_STR_LEN];
#define LOGIN_NAME_LEN 40
extern	char	login_name[LOGIN_NAME_LEN];
extern	char   *byebye_string;
extern	char   *curse_message;
extern	char   *error_file;
extern	char   *fruit;
extern	char   *m_names[];
extern	const char   *more;
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
