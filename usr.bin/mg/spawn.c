/*
 * Name:	MicroGnuEmacs
 *		Spawn CLI for System V.
 *
 * Spawn for System V.
 */
#include	"def.h"

#include	<signal.h>

char	*shellp	= NULL;			/* Saved "SHELL" program.	*/
char	*shname = NULL;			/* Saved shell name		*/

/*
 * On System V, we no gots job control, so always run
 * a subshell using fork/exec. Bound to "C-C", and used
 * as a subcommand by "C-Z". (daveb)
 *
 * Returns 0 if the shell executed OK, something else if
 * we couldn't start shell or it exited badly.
 */
spawncli(f, n)
{
	register int	pid;
	register int	wpid;
	register void	(*oqsig)();
	register void	(*oisig)();
	int		status;
	int		errp = FALSE;

	if (shellp == NULL) {
		shellp = getenv("SHELL");
		if (shellp == NULL)
			shellp = getenv("shell");
		if (shellp == NULL)
			shellp = "/bin/sh";	/* Safer.		*/
		shname = strrchr( shellp, '/' ); 
		shname = shname ? shname++ : shellp;
		
	}
	ttcolor(CTEXT);
	ttnowindow();
	ttmove(nrow-1, 0);
	if (epresf != FALSE) {
		tteeol();
		epresf = FALSE;
	}
	ttclose();
	sgarbf = TRUE;				/* Force repaint.	*/
	oqsig = signal(SIGQUIT, SIG_IGN);
	oisig = signal(SIGINT,  SIG_IGN);
	if ((pid=fork()) == 0) {
		(void) signal(SIGINT, oisig);
		(void) signal(SIGQUIT, oqsig);
		execlp(shellp, shname, "-i", (char *)NULL);
		_exit(1);			/* Should do better!	*/
	}
	else if (pid > 0) {
		while ((wpid=wait(&status))>=0 && wpid!=pid)
			;
	}
	else errp = TRUE;

	signal(SIGINT,  oisig);
	signal(SIGQUIT, oqsig);
	ttopen();
	setttysize();
	ttwindow();

	if(errp)
		ewprintf("Failed to create process");

	return ( errp | status );
}

/*
 * Put the tty in normal mode, so he can do a second ^Z.  Then
 * wait for a char.  To use ^Z^Z to suspend and "fg %mg CR CR"
 * to continue;
 *
 * Returns 0 if it works, which presumably it must.
 */
attachtoparent(f, n)
{
	register int	pid;
	register int	wpid;
	register int	(*oqsig)();
	register int	(*oisig)();
	int		status;
	int		errp = FALSE;
	int		omask;
	sigset_t	newsig,oldsig;

	ttcolor(CTEXT);
	ttnowindow();
	ttmove(nrow-1, 0);
	if (epresf != FALSE) {
		tteeol();
		epresf = FALSE;
	}
	ttclose();
	sgarbf = TRUE;				/* Force repaint.	*/
#ifdef SIGTSTP
	sigemptyset(&newsig);
	sigprocmask(SIG_SETMASK, &newsig, &oldsig);
	(void) kill(0, SIGTSTP);
	sigprocmask(SIG_SETMASK, &oldsig, NULL);
#else
	getchar();
#endif
	ttopen();
	setttysize();
	ttwindow();

	return ( 0 );
}
