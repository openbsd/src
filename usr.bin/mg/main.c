/*	$OpenBSD: main.c,v 1.22 2002/07/25 16:37:54 vincent Exp $	*/

/*
 *	Mainline.
 */

#include	"def.h"
#include	"kbd.h"
#include	"funmap.h"

#ifndef NO_MACRO
#include	"macro.h"
#endif	/* NO_MACRO */

int		 thisflag;			/* flags, this command	*/
int		 lastflag;			/* flags, last command	*/
int		 curgoal;			/* goal column		*/
BUFFER		*curbp;				/* current buffer	*/
BUFFER		*bheadp;			/* BUFFER listhead	*/
MGWIN		*curwp;				/* current window	*/
MGWIN		*wheadp;			/* MGWIN listhead	*/
char		 pat[NPAT];			/* pattern		*/

static void	 edinit(void);

int
main(int argc, char **argv)
{
	char	*cp;

	vtinit();		/* Virtual terminal.		*/
#ifndef NO_DIR
	dirinit();		/* Get current directory.	*/
#endif	/* !NO_DIR */
	edinit();		/* Buffers, windows.		*/
	maps_init();		/* Keymaps and modes.		*/
	funmap_init();		/* Functions.			*/
	ttykeymapinit();	/* Symbols, bindings.		*/

	/*
	 * This is where we initialize standalone extensions that should
	 * be loaded dynamically sometime in the future.
	 */
	{
		extern void grep_init(void);
		extern void theo_init(void);

		grep_init();
		theo_init();
	}

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
	while (--argc > 0) {
		cp = adjustname(*++argv);
		if (cp != NULL) {
			curbp = findbuffer(cp);
			(void)showbuffer(curbp, curwp, 0);
			(void)readin(cp);
		}
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
edinit(void)
{
	BUFFER	*bp;
	MGWIN	*wp;

	bheadp = NULL;
	bp = bfind("*scratch*", TRUE);		/* Text buffer.		 */
	wp = (MGWIN *)malloc(sizeof(MGWIN));	/* Initial window.	 */
	if (wp == NULL)
		panic("Out of memory");
	if (bp == NULL || wp == NULL)
		panic("edinit");
	curbp = bp;				/* Current ones.	 */
	wheadp = wp;
	curwp = wp;
	wp->w_wndp = NULL;			/* Initialize window.	 */
	wp->w_bufp = bp;
	bp->b_nwnd = 1;				/* Displayed.		 */
	wp->w_linep = wp->w_dotp = bp->b_linep;
	wp->w_doto = 0;
	wp->w_markp = NULL;
	wp->w_marko = 0;
	wp->w_toprow = 0;
	wp->w_ntrows = nrow - 2;		/* 2 = mode, echo.	 */
	wp->w_force = 0;
	wp->w_flag = WFMODE | WFHARD;		/* Full.		 */
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
	return ABORT;
}
