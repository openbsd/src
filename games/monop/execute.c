/*	$OpenBSD: execute.c,v 1.2 1998/09/20 23:36:50 pjanzen Exp $	*/
/*	$NetBSD: execute.c,v 1.3 1995/03/23 08:34:38 cgd Exp $	*/

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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)execute.c	8.1 (Berkeley) 5/31/93";
#else
static char rcsid[] = "$OpenBSD: execute.c,v 1.2 1998/09/20 23:36:50 pjanzen Exp $";
#endif
#endif /* not lint */

#include	"monop.ext"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/time.h>
#include	<err.h>
#include 	<fcntl.h>
#include	<stdlib.h>
#include	<unistd.h>

#define	SEGSIZE	8192

typedef	struct stat	STAT;
typedef	struct tm	TIME;

static char	buf[257];

static bool	new_play;	/* set if move on to new player		*/

static void	show_move __P((void));

/*
 *	This routine takes user input and puts it in buf
 */
void
getbuf()
{
	char	*sp;
	int	tmpin, i;

	i = 0;
	sp = buf;
	while (((tmpin = getchar()) != '\n') && (i < sizeof(buf)) &&
	    (tmpin != EOF)) {
		*sp++ = tmpin;
		i++;
	}
	if (tmpin == EOF) {
		printf("user closed input stream, quitting...\n");
                exit(0);
	}
	*sp = '\0';
}
/*
 *	This routine executes the given command by index number
 */
void
execute(com_num)
	int	com_num;
{
	new_play = FALSE;	/* new_play is true if fixing	*/
	(*func[com_num])();
	notify();
	force_morg();
	if (new_play)
		next_play();
	else if (num_doub)
		printf("%s rolled doubles.  Goes again\n", cur_p->name);
}
/*
 *	This routine moves a piece around.
 */
void
do_move()
{
	int	r1, r2;
	bool	was_jail;

	new_play = was_jail = FALSE;
	printf("roll is %d, %d\n", r1 = roll(1, 6), r2 = roll(1, 6));
	if (cur_p->loc == JAIL) {
		was_jail++;
		if (!move_jail(r1, r2)) {
			new_play++;
			goto ret;
		}
	}
	else {
		if (r1 == r2 && ++num_doub == 3) {
			printf("That's 3 doubles.  You go to jail\n");
			goto_jail();
			new_play++;
			goto ret;
		}
		move(r1 + r2);
	}
	if (r1 != r2 || was_jail)
		new_play++;
ret:
	return;
}
/*
 *	This routine moves a normal move
 */
void
move(rl)
	int	rl;
{
	int	old_loc;

	old_loc = cur_p->loc;
	cur_p->loc = (cur_p->loc + rl) % N_SQRS;
	if (cur_p->loc < old_loc && rl > 0) {
		cur_p->money += 200;
		printf("You pass %s and get $200\n", board[0].name);
	}
	show_move();
}
/*
 *	This routine shows the results of a move
 */
static void
show_move()
{
	SQUARE	*sqp;

	sqp = &board[cur_p->loc];
	printf("That puts you on %s\n", sqp->name);
	switch (sqp->type) {
	case SAFE:
		printf("That is a safe place\n");
		break;
	case CC:
		cc();
		break;
	case CHANCE:
		chance();
		break;
	case INC_TAX:
		inc_tax();
		break;
	case GOTO_J:
		goto_jail();
		break;
	case LUX_TAX:
		lux_tax();
		break;
	case PRPTY:
	case RR:
	case UTIL:
		if (sqp->owner < 0) {
			printf("That would cost $%d\n", sqp->cost);
			if (getyn("Do you want to buy? ") == 0) {
				buy(player, sqp);
				cur_p->money -= sqp->cost;
			}
			else if (num_play > 2)
				bid();
		}
		else if (sqp->owner == player)
			printf("You own it.\n");
		else
			rent(sqp);
	}
}
/*
 *	This routine saves the current game for use at a later date
 */
void
save()
{
	char		*sp;
	int		outf, num;
	time_t		t;
	struct stat	sb;
	char		*start, *end;

	printf("Which file do you wish to save it in? ");
	getbuf();

	/*
	 * check for existing files, and confirm overwrite if needed
	 */

	if (stat(buf, &sb) == 0
	    && getyn("File exists.  Do you wish to overwrite? ") > 0)
		return;

	if ((outf=creat(buf, 0644)) < 0) {
		warn(buf);
		return;
	}
	printf("\"%s\" ", buf);
	time(&t);			/* get current time		*/
	strcpy(buf, ctime(&t));
	for (sp = buf; *sp != '\n'; sp++)
		continue;
	*sp = '\0';
#if 0
	start = (((int) etext + (SEGSIZE-1)) / SEGSIZE ) * SEGSIZE;
#else
	start = 0;
#endif
	end = sbrk(0);
	while (start < end) {		/* write out entire data space */
		num = start + 16 * 1024 > end ? end - start : 16 * 1024;
		write(outf, start, num);
		start += num;
	}
	close(outf);
	printf("[%s]\n", buf);
}
/*
 *	This routine restores an old game from a file
 */
void
restore()
{
	printf("Which file do you wish to restore from? ");
	getbuf();
	rest_f(buf);
}
/*
 *	This does the actual restoring.  It returns TRUE if the
 *	backup was successful, else FALSE.
 */
int
rest_f(file)
	char	*file;
{
	char	*sp;
	int	inf, num;
	char	*start, *end;
	STAT	sbuf;

	if ((inf = open(file, 0)) < 0) {
		warn(file);
		return FALSE;
	}
	printf("\"%s\" ", file);
	if (fstat(inf, &sbuf) < 0)		/* get file stats	*/
		err(1, file);
#if 0
	start = (((int) etext + (SEGSIZE-1)) / SEGSIZE ) * SEGSIZE;
#else
	start = 0;
#endif
	brk(end = start + sbuf.st_size);
	while (start < end) {		/* write out entire data space */
		num = start + 16 * 1024 > end ? end - start : 16 * 1024;
		read(inf, start, num);
		start += num;
	}
	close(inf);
	strcpy(buf, ctime(&sbuf.st_mtime));
	for (sp = buf; *sp != '\n'; sp++)
		continue;
	*sp = '\0';
	printf("[%s]\n", buf);
	return TRUE;
}
