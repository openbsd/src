/*
 * Spawn.  Actually just suspends Mg.
 * Assumes POSIX job control.
 */
#include	"def.h"

#include	<signal.h>
#include	<termios.h>

/*
 * This causes mg to send itself a stop signal.
 * Assumes the parent shell supports POSIX job control.
 * If the terminal supports an alternate screen, we will sitch to it.
 */
/* ARGSUSED */
spawncli(f, n)
{
	sigset_t oset;
	int ttputc __P((int)); /* XXX */

	/* Very similar to what vttidy() does. */
	ttcolor(CTEXT);
	ttnowindow();
	ttmove(nrow - 1, 0);
	if (epresf != FALSE) {
		tteeol();
		epresf = FALSE;
	}
	if (ttcooked() == FALSE)
		return (FALSE);
	tttidy();		/* Exit application mode and tidy. */
	ttflush();
	(void) sigprocmask(SIG_SETMASK, NULL, &oset);
	(void) kill(0, SIGTSTP);
	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
	ttreinit();
	sgarbf = TRUE;		/* Force repaint.	 */
	return ttraw();
}
