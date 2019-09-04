/*
 * Copyright (C) 1984-2012  Mark Nudelman
 * Modified for use with illumos by Garrett D'Amore.
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Routines to execute other programs.
 * Necessarily very OS dependent.
 */

#include <signal.h>

#include "less.h"
#include "position.h"

extern int screen_trashed;
extern IFILE curr_ifile;

static int pipe_data(char *cmd, off_t spos, off_t epos);

/*
 * Pass the specified command to a shell to be executed.
 * Like plain "system()", but handles resetting terminal modes, etc.
 */
void
lsystem(const char *cmd, const char *donemsg)
{
	int inp;
	char *shell;
	char *p;
	IFILE save_ifile;

	/*
	 * Print the command which is to be executed,
	 * unless the command starts with a "-".
	 */
	if (cmd[0] == '-')
		cmd++;
	else {
		clear_bot();
		putstr("!");
		putstr(cmd);
		putstr("\n");
	}

	/*
	 * Close the current input file.
	 */
	save_ifile = save_curr_ifile();
	(void) edit_ifile(NULL);

	/*
	 * De-initialize the terminal and take out of raw mode.
	 */
	deinit();
	flush(0);	/* Make sure the deinit chars get out */
	raw_mode(0);

	/*
	 * Restore signals to their defaults.
	 */
	init_signals(0);

	/*
	 * Force standard input to be the user's terminal
	 * (the normal standard input), even if less's standard input
	 * is coming from a pipe.
	 */
	inp = dup(0);
	(void) close(0);
	if (open("/dev/tty", O_RDONLY) == -1)
		(void) dup(inp);

	/*
	 * Pass the command to the system to be executed.
	 * If we have a SHELL environment variable, use
	 * <$SHELL -c "command"> instead of just <command>.
	 * If the command is empty, just invoke a shell.
	 */
	p = NULL;
	if ((shell = lgetenv("SHELL")) != NULL && *shell != '\0') {
		if (*cmd == '\0') {
			p = estrdup(shell);
		} else {
			char *esccmd = shell_quote(cmd);
			if (esccmd != NULL) {
				p = easprintf("%s -c %s", shell, esccmd);
				free(esccmd);
			}
		}
	}
	if (p == NULL) {
		if (*cmd == '\0')
			p = estrdup("sh");
		else
			p = estrdup(cmd);
	}
	(void) system(p);
	free(p);

	/*
	 * Restore standard input, reset signals, raw mode, etc.
	 */
	(void) close(0);
	(void) dup(inp);
	(void) close(inp);

	init_signals(1);
	raw_mode(1);
	if (donemsg != NULL) {
		putstr(donemsg);
		putstr("  (press RETURN)");
		get_return();
		(void) putchr('\n');
		flush(0);
	}
	init();
	screen_trashed = 1;

	/*
	 * Reopen the current input file.
	 */
	reedit_ifile(save_ifile);

	/*
	 * Since we were ignoring window change signals while we executed
	 * the system command, we must assume the window changed.
	 * Warning: this leaves a signal pending (in "signal_winch"),
	 * so psignals() should be called soon after lsystem().
	 */
	sigwinch(0);
}

/*
 * Pipe a section of the input file into the given shell command.
 * The section to be piped is the section "between" the current
 * position and the position marked by the given letter.
 *
 * If the mark is after the current screen, the section between
 * the top line displayed and the mark is piped.
 * If the mark is before the current screen, the section between
 * the mark and the bottom line displayed is piped.
 * If the mark is on the current screen, or if the mark is ".",
 * the whole current screen is piped.
 */
int
pipe_mark(int c, char *cmd)
{
	off_t mpos, tpos, bpos;

	/*
	 * mpos = the marked position.
	 * tpos = top of screen.
	 * bpos = bottom of screen.
	 */
	mpos = markpos(c);
	if (mpos == -1)
		return (-1);
	tpos = position(TOP);
	if (tpos == -1)
		tpos = ch_zero();
	bpos = position(BOTTOM);

	if (c == '.')
		return (pipe_data(cmd, tpos, bpos));
	else if (mpos <= tpos)
		return (pipe_data(cmd, mpos, bpos));
	else if (bpos == -1)
		return (pipe_data(cmd, tpos, bpos));
	else
		return (pipe_data(cmd, tpos, mpos));
}

/*
 * Create a pipe to the given shell command.
 * Feed it the file contents between the positions spos and epos.
 */
static int
pipe_data(char *cmd, off_t spos, off_t epos)
{
	FILE *f;
	int c;

	/*
	 * This is structured much like lsystem().
	 * Since we're running a shell program, we must be careful
	 * to perform the necessary deinitialization before running
	 * the command, and reinitialization after it.
	 */
	if (ch_seek(spos) != 0) {
		error("Cannot seek to start position", NULL);
		return (-1);
	}

	if ((f = popen(cmd, "w")) == NULL) {
		error("Cannot create pipe", NULL);
		return (-1);
	}
	clear_bot();
	putstr("!");
	putstr(cmd);
	putstr("\n");

	deinit();
	flush(0);
	raw_mode(0);
	init_signals(0);
	lsignal(SIGPIPE, SIG_IGN);

	c = EOI;
	while (epos == -1 || spos++ <= epos) {
		/*
		 * Read a character from the file and give it to the pipe.
		 */
		c = ch_forw_get();
		if (c == EOI)
			break;
		if (putc(c, f) == EOF)
			break;
	}

	/*
	 * Finish up the last line.
	 */
	while (c != '\n' && c != EOI) {
		c = ch_forw_get();
		if (c == EOI)
			break;
		if (putc(c, f) == EOF)
			break;
	}

	(void) pclose(f);

	lsignal(SIGPIPE, SIG_DFL);
	init_signals(1);
	raw_mode(1);
	init();
	screen_trashed = 1;
	/* {{ Probably don't need this here. }} */
	sigwinch(0);
	return (0);
}
