/*
 * $OpenBSD: sigret.c,v 1.5 2003/07/31 21:48:04 deraadt Exp $
 *
 * Public Domain
 *
 * Playing games with sigreturn.   Check if calling sigreturn from a
 * signal handler screws anything up.
 *
 * Run with:
 *	-a:	use an alternate signal stack
 *
 *	-b:	call sigreturn from outside of a signal handler
 *		An error is OK
 *
 *	-c:	clobber the sigcontext before calling sigreturn
 *		the program should die
 *
 *	-f:	don't use sigreturn -- fall through the signal handler
 *		-c, and -i options ignored when used
 *
 *	-i:	call sigreturn from a function called by the signal handler
 *
 */

#include <sys/time.h>

#include <err.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * sigalarm occurs 50 times/second.  Stop running after 10 seconds
 * (100 interrupts).
 */
#define MAX_INTERRUPTS	500

int failed;
int altstack;
int badcall;
int clobbercall;
int fallthru;
int indirect;

volatile int count;
struct sigcontext gscp;
int gscp_loaded;

static void
usage(const char * err, ...)
{
	extern const char * __progname;

	if (err) {
		va_list ap;
		va_start(ap, err);
		vwarnx(err, ap);
		va_end(ap);
	}
	fprintf(stderr, "usage: %s [-abcfi]\n", __progname);
	exit(1);
}

static void
indirect_return(struct sigcontext * scp)
{
	sigreturn(scp);
}

static void
sig_handler(int sig, siginfo_t *blah, void *x)
{
	struct sigcontext * scp = x;

	count++;

	if (!fallthru) {
		if (clobbercall)
			memset(scp, 0, sizeof *scp);
		if (indirect)
			indirect_return(scp);
		else if (badcall) {
			gscp = *scp;
			gscp_loaded = 1;
		} else
			sigreturn(scp);
	}
}

static void
test2(char *fmt)
{
	char *ofmt = fmt;

	if (gscp_loaded) {
		gscp_loaded = 0;
		sigreturn(&gscp);
	}

	for (; *fmt; fmt++)
	  switch (*fmt) {
	  case 'i':
	  case 'c':
	  case 'l':
	  case 'p':
	    break;
	  default:
	    failed = 1;
	    fprintf(stderr,
		    "unexpected character 0x%02x `%c' in %s: count %d\n",
		    *fmt, *fmt, ofmt, count);
	  }
}

int
main(int argc, char * argv[])
{
	extern char *optarg;
	extern int optind;

	int opt;

	struct sigaction act;
	struct sigaltstack ss;

	while ((opt = getopt(argc, argv, "abcfi")) != -1) {
		switch (opt) {
		case 'a':
			/* use sigaltstack */
			altstack = 1;
			break;
		case 'b':
			/* call outside of sig_handler */
			badcall = 1;
			break;
		case 'c':
			/* force error by munging sigcontext */
			clobbercall = 1;
			break;
		case 'f':
			/* don't use sigreturn */
			fallthru = 1;
			break;
		case 'i':
			/* call sigreturn indirectly */
			indirect = 1;
			break;
		}
	}

	/* make sure there is no other cruft left on the command line */
	if (optind != argc)
		usage("unknown argument -- %s", argv[ optind ]);

	if (altstack) {
		if ((ss.ss_sp = malloc(SIGSTKSZ)) == NULL)
			errx(1, "ss_sp malloc");

		ss.ss_size = SIGSTKSZ;
		ss.ss_flags = 0;
		if (sigaltstack(&ss,0) == -1)
			err(1, "sigaltstack");
	}

	sigfillset(&act.sa_mask);
	act.sa_sigaction = sig_handler;
	act.sa_flags = SA_RESTART;
	if (altstack)
		act.sa_flags |= SA_ONSTACK;
	sigaction(SIGALRM, &act, NULL);

	ualarm(10000, 10000);

	while (count < MAX_INTERRUPTS)
		test2("iclp");

	return failed;
}
