/*	$OpenBSD: main.c,v 1.26 2003/10/27 11:21:12 vincent Exp $	*/

/*
 *	Mainline.
 */

#include	"def.h"
#include	"kbd.h"
#include	"funmap.h"

#ifndef NO_MACRO
#include	"macro.h"
#endif	/* NO_MACRO */

#include <err.h>

int		 thisflag;			/* flags, this command	*/
int		 lastflag;			/* flags, last command	*/
int		 curgoal;			/* goal column		*/
int		 startrow;			/* row to start         */
BUFFER		*curbp;				/* current buffer	*/
BUFFER		*bheadp;			/* BUFFER listhead	*/
MGWIN		*curwp;				/* current window	*/
MGWIN		*wheadp;			/* MGWIN listhead	*/
char		 pat[NPAT];			/* pattern		*/

static void	 edinit(PF);

int
main(int argc, char **argv)
{
	char	*cp, *init_fcn_name = NULL;
	PF init_fcn = NULL;
	int o;

	while ((o = getopt(argc, argv, "f:")) != -1)
		switch (o) {
		case 'f':
			if (init_fcn_name != NULL)
				errx(1, "cannot specify more than one "
				    "initial function");
			init_fcn_name = optarg;
			break;
		default:
			errx(1, "usage: mg [-f <mode>] [files...]");
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

		grep_init();
		theo_init();
		mail_init();
	}

	if (init_fcn_name &&
	    (init_fcn = name_function(init_fcn_name)) == NULL)
		errx(1, "Unknown function `%s'", init_fcn_name);

	vtinit();		/* Virtual terminal.		*/
#ifndef NO_DIR
	dirinit();		/* Get current directory.	*/
#endif	/* !NO_DIR */
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
	while (argc > 0) {
		if (argv[0][0] == '+' && strlen(argv[0]) >= 2) {
			long lval;
			char *ep;

			errno = 0;
			lval = strtoul(&argv[0][1], &ep, 10);
			if (argv[0][1] == '\0' || *ep != '\0')
				goto notnum;
			if ((errno == ERANGE &&
			    (lval == LONG_MAX || lval == LONG_MIN)) ||
			    (lval > INT_MAX || lval < INT_MIN))
				goto notnum;
			startrow = (int)lval;
		} else {
notnum:
			cp = adjustname(*argv);
			if (cp != NULL) {
				curbp = findbuffer(cp);
				(void)showbuffer(curbp, curwp, 0);
				(void)readin(cp);
				if (init_fcn_name)
					init_fcn(0, 1);
			}
		}
		argc--;
		argv++;
	}

	/* fake last flags */
	thisflag = 0;
	for (;;) {
#ifndef NO_DPROMPT
		if (epresf == KPROMPT)
			eerase();
#endif	/* !NO_DPROMPT */
		if (winch_flag) {
			refresh(0, 0);
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
			/* and fall through */
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
 */
static void
edinit(PF init_fcn)
{
	BUFFER	*bp;
	MGWIN	*wp;

	bheadp = NULL;
	bp = bfind("*scratch*", TRUE);		/* Text buffer.		 */
	wp = new_window(bp);
	if (wp == NULL)
		panic("Out of memory");
	if (bp == NULL || wp == NULL)
		panic("edinit");
	curbp = bp;				/* Current ones.	 */
	wheadp = wp;
	curwp = wp;
	wp->w_wndp = NULL;			/* Initialize window.	 */
	wp->w_linep = wp->w_dotp = bp->b_linep;
	wp->w_ntrows = nrow - 2;		/* 2 = mode, echo.	 */
	wp->w_flag = WFMODE | WFHARD;		/* Full.		 */

	if (init_fcn)
		init_fcn(0, 1);
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
		return ABORT;
	if (s == FALSE
	    || eyesno("Some modified buffers exist, really exit") == TRUE) {
		vttidy();
#ifdef SYSCLEANUP
		SYSCLEANUP;
#endif	/* SYSCLEANUP */
		exit(GOOD);
	}
	return TRUE;
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
