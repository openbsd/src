/*	$OpenBSD: expl.c,v 1.4 1999/02/01 06:53:56 d Exp $	*/
/*	$NetBSD: expl.c,v 1.2 1997/10/10 16:33:18 lukem Exp $	*/
/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include "hunt.h"
#include "server.h"
#include "conf.h"

static	void	remove_wall __P((int, int));
static	void	init_removed __P((void));


/*
 * showexpl:
 *	Show the explosions as they currently are
 */
void
showexpl(y, x, type)
	int	y, x;
	char	type;
{
	PLAYER	*pp;
	EXPL	*ep;

	if (y < 0 || y >= HEIGHT)
		return;
	if (x < 0 || x >= WIDTH)
		return;
	ep = (EXPL *) malloc(sizeof (EXPL));	/* NOSTRICT */
	ep->e_y = y;
	ep->e_x = x;
	ep->e_char = type;
	ep->e_next = NULL;
	if (Last_expl == NULL)
		Expl[0] = ep;
	else
		Last_expl->e_next = ep;
	Last_expl = ep;
	for (pp = Player; pp < End_player; pp++) {
		if (pp->p_maze[y][x] == type)
			continue;
		pp->p_maze[y][x] = type;
		cgoto(pp, y, x);
		outch(pp, type);
	}
	for (pp = Monitor; pp < End_monitor; pp++) {
		if (pp->p_maze[y][x] == type)
			continue;
		pp->p_maze[y][x] = type;
		cgoto(pp, y, x);
		outch(pp, type);
	}
	switch (Maze[y][x]) {
	  case WALL1:
	  case WALL2:
	  case WALL3:
	  case DOOR:
	  case WALL4:
	  case WALL5:
		if (y >= UBOUND && y < DBOUND && x >= LBOUND && x < RBOUND)
			remove_wall(y, x);
		break;
	}
}

/*
 * rollexpl:
 *	Roll the explosions over, so the next one in the list is at the
 *	top
 */
void
rollexpl()
{
	EXPL	*ep;
	PLAYER	*pp;
	int	y, x;
	char	c;
	EXPL	*nextep;

	for (ep = Expl[EXPLEN - 1]; ep != NULL; ep = nextep) {
		nextep = ep->e_next;
		y = ep->e_y;
		x = ep->e_x;
		if (y < UBOUND || y >= DBOUND || x < LBOUND || x >= RBOUND)
			c = Maze[y][x];
		else
			c = SPACE;
		for (pp = Player; pp < End_player; pp++)
			if (pp->p_maze[y][x] == ep->e_char) {
				pp->p_maze[y][x] = c;
				cgoto(pp, y, x);
				outch(pp, c);
			}
		for (pp = Monitor; pp < End_monitor; pp++)
			check(pp, y, x);
		free((char *) ep);
	}
	memmove(&Expl[1], &Expl[0], (EXPLEN - 1) * sizeof Expl[0]);
	/* for (x = EXPLEN - 1; x > 0; x--)
		Expl[x] = Expl[x - 1]; */
	Last_expl = Expl[0] = NULL;
}

int
can_rollexpl()
{
	int i;

	for (i = EXPLEN - 1; i >= 0; i--)
		if (Expl[i] != NULL)
			return 1;
	return 0;
}

static	REGEN	*removed = NULL;
static	REGEN	*rem_index = NULL;

static void
init_removed()
{
	rem_index = removed = malloc(conf_maxremove * sizeof(REGEN));
	if (rem_index == NULL) {
		log(LOG_ERR, "malloc");
		cleanup(1);
	}
}

/*
 * remove_wall - add a location where the wall was blown away.
 *		 if there is no space left over, put the a wall at
 *		 the location currently pointed at.
 */
static void
remove_wall(y, x)
	int	y, x;
{
	REGEN	*r;
	PLAYER	*pp;
	char	save_char = 0;

	if (removed == NULL)
		clearwalls();

	r = rem_index;
	while (r->r_y != 0) {
		switch (Maze[r->r_y][r->r_x]) {
		  case SPACE:
		  case LEFTS:
		  case RIGHT:
		  case ABOVE:
		  case BELOW:
		  case FLYER:
			save_char = Maze[r->r_y][r->r_x];
			goto found;
		}
		if (++r >= removed + conf_maxremove)
			r = removed;
	}

found:
	if (r->r_y != 0) {
		/* Slot being used, put back this wall */
		if (save_char == SPACE)
			Maze[r->r_y][r->r_x] = Orig_maze[r->r_y][r->r_x];
		else {
			/* We throw the player off the wall: */
			pp = play_at(r->r_y, r->r_x);
			if (pp->p_flying >= 0)
				pp->p_flying += rand_num(conf_flytime / 2);
			else {
				pp->p_flying = rand_num(conf_flytime);
				pp->p_flyx = 2 * rand_num(conf_flystep + 1) -
				    conf_flystep;
				pp->p_flyy = 2 * rand_num(conf_flystep + 1) -
				    conf_flystep;
			}
			pp->p_over = Orig_maze[r->r_y][r->r_x];
			pp->p_face = FLYER;
			Maze[r->r_y][r->r_x] = FLYER;
			showexpl(r->r_y, r->r_x, FLYER);
		}
		Maze[r->r_y][r->r_x] = Orig_maze[r->r_y][r->r_x];
		if (conf_random && rand_num(100) < conf_prandom)
			Maze[r->r_y][r->r_x] = DOOR;
		if (conf_reflect && rand_num(100) == conf_preflect)
			Maze[r->r_y][r->r_x] = WALL4;
		for (pp = Monitor; pp < End_monitor; pp++)
			check(pp, r->r_y, r->r_x);
	}

	r->r_y = y;
	r->r_x = x;
	if (++r >= removed + conf_maxremove)
		rem_index = removed;
	else
		rem_index = r;

	Maze[y][x] = SPACE;
	for (pp = Monitor; pp < End_monitor; pp++)
		check(pp, y, x);
}

/*
 * clearwalls:
 *	Clear out the walls array
 */
void
clearwalls()
{
	REGEN	*rp;

	if (removed == NULL)
		init_removed();
	for (rp = removed; rp < removed + conf_maxremove; rp++)
		rp->r_y = 0;
	rem_index = removed;
}
