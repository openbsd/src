/*	$OpenBSD: listen.c,v 1.2 1998/06/03 16:20:29 deraadt Exp $	*/

/*-
 * Copyright 1987, 1988 by the Student Information Processing Board
 *	of the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

/*
 * Listener loop for subsystem library libss.a.
 */

#include "ss_internal.h"
#include <stdio.h>
#ifndef NPOSIX
#include <string.h>
#else
#include <strings.h>
#define memcpy(a, b, c) bcopy(b, a, c)
#endif
#include <setjmp.h>
#include <signal.h>
#include <sys/param.h>
#ifdef BSD
#include <sgtty.h>
#endif

#ifndef	lint
static char const rcs_id[] =
    "$Id: listen.c,v 1.2 1998/06/03 16:20:29 deraadt Exp $";
#endif

#ifndef NPOSIX
#define sigtype void
#else
#define sigtype int
#endif

static ss_data *current_info;
static jmp_buf listen_jmpb;

static sigtype
print_prompt()
{
#ifdef BSD
    /* put input into a reasonable mode */
    struct sgttyb ttyb;
    if (ioctl(fileno(stdin), TIOCGETP, &ttyb) != -1) {
	if (ttyb.sg_flags & (CBREAK|RAW)) {
	    ttyb.sg_flags &= ~(CBREAK|RAW);
	    (void) ioctl(0, TIOCSETP, &ttyb);
	}
    }
#endif
    (void) fputs(current_info->prompt, stdout);
    (void) fflush(stdout);
}

static sigtype
listen_int_handler()
{
    putc('\n', stdout);
    longjmp(listen_jmpb, 1);
}

static sigtype print_prompt();

int
ss_listen (sci_idx)
    int sci_idx;
{
    register char *cp;
    register ss_data *info;
    char input[BUFSIZ];
    char expanded_input[BUFSIZ];
    char buffer[BUFSIZ];
    char *end = buffer;
    int code;
    jmp_buf old_jmpb;
    ss_data *old_info = current_info;
#ifndef NPOSIX
    struct sigaction isig, csig, nsig, osig;
    sigset_t nmask, omask;
#else
    register sigtype (*sig_cont)();
    sigtype (*sig_int)(), (*old_sig_cont)();
    int mask;
#endif

    current_info = info = ss_info(sci_idx);
    info->abort = 0;
#ifndef NPOSIX
    csig.sa_handler = (sigtype (*)())0;
    
    sigemptyset(&nmask);
    sigaddset(&nmask, SIGINT);
    sigprocmask(SIG_BLOCK, &nmask, &omask);
#else
    sig_cont = (sigtype (*)())0;
    mask = sigblock(sigmask(SIGINT));
#endif

    memcpy(old_jmpb, listen_jmpb, sizeof(jmp_buf));

#ifndef NPOSIX    
    memset(&nsig, 0, sizeof nsig);
    nsig.sa_handler = listen_int_handler;
    sigemptyset(&nsig.sa_mask);
    nsig.sa_flags = 0;
    sigaction(SIGINT, &nsig, &isig);
#else
    sig_int = signal(SIGINT, listen_int_handler);
#endif

    setjmp(listen_jmpb);

#ifndef NPOSIX
    sigprocmask(SIG_SETMASK, &omask, (sigset_t *)0);
#else
    (void) sigsetmask(mask);
#endif
    while(!info->abort) {
	print_prompt();
	*end = '\0';
#ifndef NPOSIX
	nsig.sa_handler = listen_int_handler;	/* fgets is not signal-safe */
	osig = csig;
	sigaction(SIGCONT, &nsig, &csig);
	if ((sigtype (*)())csig.sa_handler==(sigtype (*)())listen_int_handler)
	    csig = osig;
#else
	old_sig_cont = sig_cont;
	sig_cont = signal(SIGCONT, print_prompt);
#ifdef mips
	/* The mips compiler breaks on determining the types, we help */
	if ( (sigtype *) sig_cont == (sigtype *) print_prompt)
	    sig_cont = old_sig_cont;
#else
	if (sig_cont == print_prompt)
	    sig_cont = old_sig_cont;
#endif
#endif
	if (fgets(input, BUFSIZ, stdin) != input) {
	    code = SS_ET_EOF;
	    goto egress;
	}
	cp = strchr(input, '\n');
	if (cp) {
	    *cp = '\0';
	    if (cp == input)
		continue;
	}
#ifndef NPOSIX
	sigaction(SIGCONT, &csig, (struct sigaction *)0);
#else
	(void) signal(SIGCONT, sig_cont);
#endif
	for (end = input; *end; end++)
	    ;

	code = ss_execute_line (sci_idx, input);
	if (code == SS_ET_COMMAND_NOT_FOUND) {
	    register char *c = input;
	    while (*c == ' ' || *c == '\t')
		c++;
	    cp = strchr (c, ' ');
	    if (cp)
		*cp = '\0';
	    cp = strchr (c, '\t');
	    if (cp)
		*cp = '\0';
	    ss_error (sci_idx, 0,
		    "Unknown request \"%s\".  Type \"?\" for a request list.",
		       c);
	}
    }
    code = 0;
egress:
#ifndef NPOSIX
    sigaction(SIGINT, &isig, (struct sigaction *)0);
#else
    (void) signal(SIGINT, sig_int);
#endif
    memcpy(listen_jmpb, old_jmpb, sizeof(jmp_buf));
    current_info = old_info;
    return code;
}

void
ss_abort_subsystem(sci_idx, code)
    int sci_idx;
{
    ss_info(sci_idx)->abort = 1;
    ss_info(sci_idx)->exit_status = code;
    
}

void
ss_quit(argc, argv, sci_idx, infop)
    int argc;
    const char * const *argv;
    int sci_idx;
    pointer infop;
{
    ss_abort_subsystem(sci_idx, 0);
}
