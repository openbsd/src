/*
 *		Mainline
 */
#include	"def.h"
#ifndef NO_MACRO
#include	"macro.h"
#endif

int	thisflag;			/* Flags, this command		*/
int	lastflag;			/* Flags, last command		*/
int	curgoal;			/* Goal column			*/
BUFFER	*curbp;				/* Current buffer		*/
MGWIN	*curwp;				/* Current window		*/
BUFFER	*bheadp;			/* BUFFER listhead		*/
MGWIN	*wheadp = (MGWIN *)NULL;	/* MGWIN listhead		*/
char	pat[NPAT];			/* Pattern			*/
#ifndef NO_DPROMPT
extern char prompt[], *promptp;		/* delayed prompting		*/
#endif

static VOID	edinit();

int
main(argc, argv)
int  argc;
char **argv;
{
#ifndef NO_STARTUP
	char	*startupfile();
#endif
	char	*cp;
	VOID	vtinit(), makename(), eerase();
	BUFFER	*findbuffer();

#ifdef SYSINIT
	SYSINIT;				/* system dependent.	*/
#endif
	vtinit();				/* Virtual terminal.	*/
#ifndef NO_DIR
	dirinit();				/* Get current directory */
#endif
	edinit();				/* Buffers, windows.	*/
	ttykeymapinit();			/* Symbols, bindings.	*/
	/* doing update() before reading files causes the error messages from
	 * the file I/O show up on the screen.	(and also an extra display
	 * of the mode line if there are files specified on the command line.)
	 */
	update();
#ifndef NO_STARTUP				/* User startup file.	*/
	if ((cp = startupfile((char *)NULL)) != NULL)
		(VOID) load(cp);
#endif
	while (--argc > 0) {
		cp = adjustname(*++argv);
		curbp = findbuffer(cp);
		(VOID) showbuffer(curbp, curwp, 0);
		(VOID) readin(cp);
	}
	thisflag = 0;				/* Fake last flags.	*/
	for(;;) {
#ifndef NO_DPROMPT
	    *(promptp = prompt) = '\0';
	    if(epresf == KPROMPT) eerase();
#endif
	    update();
	    lastflag = thisflag;
	    thisflag = 0;
	    switch(doin()) {
		case TRUE: break;
		case ABORT:
		    ewprintf("Quit");		/* and fall through	*/
		case FALSE:
		default:
		    ttbeep();
#ifndef NO_MACRO
		    macrodef = FALSE;
#endif
	    }
	}
}

/*
 * Initialize default buffer and window.
 */
static VOID
edinit() {
	register BUFFER *bp;
	register MGWIN *wp;

	bheadp = NULL;
	bp = bfind("*scratch*", TRUE);		/* Text buffer.		*/
	wp = (MGWIN *)malloc(sizeof(MGWIN));	/* Initial window.	*/
	if (bp==NULL || wp==NULL) panic("edinit");
	curbp  = bp;				/* Current ones.	*/
	wheadp = wp;
	curwp  = wp;
	wp->w_wndp  = NULL;			/* Initialize window.	*/
	wp->w_bufp  = bp;
	bp->b_nwnd  = 1;			/* Displayed.		*/
	wp->w_linep = wp->w_dotp = bp->b_linep;
	wp->w_doto  = 0;
	wp->w_markp = NULL;
	wp->w_marko = 0;
	wp->w_toprow = 0;
	wp->w_ntrows = nrow-2;			/* 2 = mode, echo.	*/
	wp->w_force = 0;
	wp->w_flag  = WFMODE|WFHARD;		/* Full.		*/
}

/*
 * Quit command. If an argument, always
 * quit. Otherwise confirm if a buffer has been
 * changed and not written out. Normally bound
 * to "C-X C-C".
 */
/*ARGSUSED*/
quit(f, n)
{
	register int	s;
	VOID		vttidy();

	if ((s = anycb(FALSE)) == ABORT) return ABORT;
	if (s == FALSE
	|| eyesno("Some modified buffers exist, really exit") == TRUE) {
		vttidy();
#ifdef	SYSCLEANUP
	SYSCLEANUP;
#endif
		exit(GOOD);
	}
	return TRUE;
}

/*
 * User abort. Should be called by any input routine that sees a C-g
 * to abort whatever C-g is aborting these days. Currently does
 * nothing.
 */
/*ARGSUSED*/
ctrlg(f, n)
{
	return ABORT;
}
