/*	$OpenBSD: robots.h,v 1.2 1998/07/09 04:34:24 pjanzen Exp $	*/
/*	$NetBSD: robots.h,v 1.5 1995/04/24 12:24:54 cgd Exp $	*/

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)robots.h	8.1 (Berkeley) 5/31/93
 */

#include	<sys/ttydefaults.h>
#include	<sys/types.h>
#include	<ctype.h>
#include	<curses.h>
#include	<err.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<pwd.h>
#include	<setjmp.h>
#include	<signal.h>
#include	<string.h>
#include	<stdlib.h>
#include	<termios.h>
#include	<unistd.h>

/*
 * miscellaneous constants
 */

#define	Y_FIELDSIZE	23
#define	X_FIELDSIZE	60
#define	Y_SIZE		24
#define	X_SIZE		80
#define	MAXLEVELS	4
#define	MAXROBOTS	(MAXLEVELS * 10)
#define	ROB_SCORE	10
#define	S_BONUS		(60 * ROB_SCORE)
#define	Y_SCORE		21
#define	X_SCORE		(X_FIELDSIZE + 9)
#define	Y_PROMPT	(Y_FIELDSIZE - 1)
#define	X_PROMPT	(X_FIELDSIZE + 2)
#define	MAXSCORES	(Y_SIZE - 2)
#define	MAXNAME		16
#define	MS_NAME		"Ten"

/*
 * characters on screen
 */

#define	ROBOT	'+'
#define	HEAP	'*'
#define	PLAYER	'@'

/*
 * type definitions
 */

typedef struct {
	int	y, x;
} COORD;

typedef struct {
	uid_t	s_uid;
	int	s_score;
	char	s_name[MAXNAME];
} SCORE;

typedef struct passwd	PASSWD;

/*
 * global variables
 */

extern bool	Dead, Full_clear, Jump, Newscore, Real_time, Running,
		Teleport, Waiting, Was_bonus;

#ifdef	FANCY
extern bool	Pattern_roll, Stand_still;
#endif

extern char	Cnt_move, Field[Y_FIELDSIZE][X_FIELDSIZE], *Next_move,
		*Move_list, Run_ch;

extern int	Count, Level, Num_robots, Num_scores, Score,
		Start_level, Wait_bonus;

extern COORD	Max, Min, My_pos, Robots[];

extern jmp_buf	End_move;

/*
 * functions types
 */

void	add_score __P((int));
bool	another __P((void));
int	cmp_sc __P((const void *, const void *));
bool	do_move __P((int, int));
bool	eaten __P((COORD *));
void	flush_in __P((void));
void	get_move __P((void));
void	init_field __P((void));
bool jumping __P((void));
void	make_level __P((void));
void	move_robots __P((int));
bool	must_telep __P((void));
void	play_level __P((void));
int	query __P((char *));
void	quit __P((int));
void	reset_count __P((void));
int	rnd __P((int));
COORD	*rnd_pos __P((void));
void	score __P((int));
void	set_name __P((SCORE *));
void	show_score __P((void));
int	sign __P((int));
