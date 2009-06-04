/*	$OpenBSD: main.c,v 1.61 2009/06/04 02:23:37 kjell Exp $	*/

/* This file is in the public domain. */

/*
 *	Mainline.
 */

#include "def.h"
#include "kbd.h"
#include "funmap.h"

#ifndef NO_MACRO
#include "macro.h"
#endif	/* NO_MACRO */

#include <err.h>

int		 thisflag;			/* flags, this command	*/
int		 lastflag;			/* flags, last command	*/
int		 curgoal;			/* goal column		*/
int		 startrow;			/* row to start		*/
struct buffer	*curbp;				/* current buffer	*/
struct buffer	*bheadp;			/* BUFFER list head	*/
struct mgwin	*curwp;				/* current window	*/
struct mgwin	*wheadp;			/* MGWIN listhead	*/
char		 pat[NPAT];			/* pattern		*/

static void	 edinit(PF);
static __dead void usage(void);

extern char	*__progname;

static __dead void
usage()
{
	fprintf(stderr, "usage: %s [-n] [-f mode] [+number] [file ...]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	char	*cp, *init_fcn_name = NULL;
	PF	 init_fcn = NULL;
	int	 o, i, nfiles;
	int	 nobackups = 0;
	struct buffer *bp;

	while ((o = getopt(argc, argv, "nf:")) != -1)
		switch (o) {
		case 'n':
			nobackups = 1;
			break;
		case 'f':
			if (init_fcn_name != NULL)
				errx(1, "cannot specify more than one "
				    "initial function");
			init_fcn_name = optarg;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	maps_init();		/* Keymaps and modes.		*/
	funmap_init();		/* Functions.			*/

	/*
	 * This is where we initialize standalone extensions that should
	 * be loaded dynamically sometime in the future.
	 */
	{
		extern void grep_init(void);
		extern void theo_init(void);
		extern void cmode_init(void);
		extern void dired_init(void);

		dired_init();
		grep_init();
		theo_init();
		cmode_init();
	}

	if (init_fcn_name &&
	    (init_fcn = name_function(init_fcn_name)) == NULL)
		errx(1, "Unknown function `%s'", init_fcn_name);

	vtinit();		/* Virtual terminal.		*/
	dirinit();		/* Get current directory.	*/
	edinit(init_fcn);	/* Buffers, windows.		*/
	ttykeymapinit();	/* Symbols, bindings.		*/

	/*
	 * doing update() before reading files causes the error messages from
	 * the file I/O show up on the screen.	(and also an extra display of
	 * the mode line if there are files specified on the command line.)
	 */
	update();

#ifndef NO_STARTUP
	/* user startup file */
	if ((cp = startupfile(NULL)) != NULL)
		(void)load(cp);
#endif	/* !NO_STARTUP */

	/*
	 * Create scratch buffer now, killing old *init* buffer.
	 * This causes *scratch* to be created and made curbp,
	 * ensuring default modes are inherited from the startup
	 * file correctly
	 */

	if ((bp = bfind("*init*", FALSE)) != NULL)
		killbuffer(bp);

	/* Force FFOTHARG=1 so that this mode is enabled, not simply toggled */
	if (init_fcn)
		init_fcn(FFOTHARG, 1);

	if (nobackups)
		makebkfile(FFARG, 0);

	for (nfiles = 0, i = 0; i < argc; i++) {
		if (argv[i][0] == '+' && strlen(argv[i]) >= 2) {
			long long lval;
			const char *errstr;

			lval = strtonum(&argv[i][1], INT_MIN, INT_MAX, &errstr);
			if (argv[i][1] == '\0' || errstr != NULL)
				goto notnum;
			startrow = lval;
		} else {
notnum:
			cp = adjustname(argv[i], FALSE);
			if (cp != NULL) {
				if (nfiles == 1)
					splitwind(0, 1);

				if ((curbp = findbuffer(cp)) == NULL) {
					vttidy();
					errx(1, "Can't find current buffer!");
				}
				(void)showbuffer(curbp, curwp, 0);
				if (readin(cp) != TRUE)
					killbuffer(curbp);
				else {
					/* Ensure enabled, not just toggled */
					if (init_fcn_name)
						init_fcn(FFOTHARG, 1);
					nfiles++;
				}
			}
		}
	}

	if (nfiles > 2)
		listbuffers(0, 1);

	/* fake last flags */
	thisflag = 0;
	for (;;) {
		if (epresf == KCLEAR)
			eerase();
		if (epresf == TRUE)
			epresf = KCLEAR;
		if (winch_flag) {
			do_redraw(0, 0, TRUE);
			winch_flag = 0;
		}
		update();
		lastflag = thisflag;
		thisflag = 0;

		switch (doin()) {
		case TRUE:
			break;
		case ABORT:
			ewprintf("Quit");
			/* FALLTHRU */
		case FALSE:
		default:
			ttbeep();
#ifndef NO_MACRO
			macrodef = FALSE;
#endif	/* !NO_MACRO */
		}
	}
}

/*
 * Initialize default buffer and window.
 * Initially, buffer is named *init*. This is changed later
 * to *scratch* after the startup files are read.
 */
static void
edinit(PF init_fcn)
{
	struct buffer	*bp;
	struct mgwin	*wp;

	bheadp = NULL;
	bp = bfind("*init*", TRUE);		/* Text buffer.		 */
	wp = new_window(bp);
	if (wp == NULL)
		panic("Out of memory");
	if (bp == NULL || wp == NULL)
		panic("edinit");
	curbp = bp;				/* Current ones.	 */
	wheadp = wp;
	curwp = wp;
	wp->w_wndp = NULL;			/* Initialize window.	 */
	wp->w_linep = wp->w_dotp = bp->b_headp;
	wp->w_ntrows = nrow - 2;		/* 2 = mode, echo.	 */
	wp->w_rflag = WFMODE | WFFULL;		/* Full.		 */
}

/*
 * Quit command.  If an argument, always quit.  Otherwise confirm if a buffer
 * has been changed and not written out.  Normally bound to "C-X C-C".
 */
/* ARGSUSED */
int
quit(int f, int n)
{
	int	 s;

	if ((s = anycb(FALSE)) == ABORT)
		return (ABORT);
	if (s == FALSE
	    || eyesno("Modified buffers exist; really exit") == TRUE) {
		vttidy();
#ifdef SYSCLEANUP
		SYSCLEANUP;
#endif	/* SYSCLEANUP */
		exit(GOOD);
	}
	return (TRUE);
}

/*
 * User abort.  Should be called by any input routine that sees a C-g to abort
 * whatever C-g is aborting these days. Currently does nothing.
 */
/* ARGSUSED */
int
ctrlg(int f, int n)
{
	return (ABORT);
}
