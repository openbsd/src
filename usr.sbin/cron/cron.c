/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie          <paul@vix.com>          uunet!decwrl!vixie!paul
 */

#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$Id: cron.c,v 1.6 1999/05/23 17:19:23 aaron Exp $";
#endif


#define	MAIN_PROGRAM


#include "cron.h"
#include <sys/signal.h>
#if SYS_TIME_H
# include <sys/time.h>
#else
# include <time.h>
#endif


static	void	usage __P((void)),
		run_reboot_jobs __P((cron_db *)),
		find_jobs __P((time_min, cron_db *, int, int)),
		set_time __P((void)),
		cron_sleep __P((time_min)),
#ifdef USE_SIGCHLD
		sigchld_handler __P((int)),
#endif
		sighup_handler __P((int)),
		parse_args __P((int c, char *v[]));


static void
usage() {
	char **dflags;

	fprintf(stderr, "usage:  %s [-x [", ProgramName);
	for(dflags = DebugFlagNames; *dflags; dflags++)
		fprintf(stderr, "%s%s", *dflags, dflags[1] ? "," : "]");
	fprintf(stderr, "]\n");
	exit(ERROR_EXIT);
}


int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	cron_db	database;

	ProgramName = argv[0];

#if defined(BSD)
	setlinebuf(stdout);
	setlinebuf(stderr);
#endif

	parse_args(argc, argv);

#ifdef USE_SIGCHLD
	(void) signal(SIGCHLD, sigchld_handler);
#else
	(void) signal(SIGCLD, SIG_IGN);
#endif
	(void) signal(SIGHUP, sighup_handler);

	acquire_daemonlock(0);
	set_cron_uid();
	set_cron_cwd();

#if defined(POSIX)
	setenv("PATH", _PATH_DEFPATH, 1);
#endif

	/* if there are no debug flags turned on, fork as a daemon should.
	 */

	if (DebugFlags) {
#if DEBUGGING
		(void) fprintf(stderr, "[%d] cron started\n", getpid());
#endif
	} else {
		switch (fork()) {
		case -1:
			log_it("CRON",getpid(),"DEATH","can't fork");
			exit(0);
			break;
		case 0:
			/* child process */
			log_it("CRON",getpid(),"STARTUP","fork ok");
			(void) setsid();
			break;
		default:
			/* parent process should just die */
			_exit(0);
		}
	}

	acquire_daemonlock(0);
	database.head = NULL;
	database.tail = NULL;
	database.mtime = (time_t) 0;
	load_database(&database);

	set_time();
	run_reboot_jobs(&database);
	timeRunning = virtualTime = clockTime;

	/*
	 * too many clocks, not enough time (Al. Einstein)
	 * These clocks are in minutes since the epoch (time()/60).
	 * virtualTime: is the time it *would* be if we woke up
	 * promptly and nobody ever changed the clock. It is
	 * monotonically increasing... unless a timejump happens.
	 * At the top of the loop, all jobs for 'virtualTime' have run.
	 * timeRunning: is the time we last awakened.
	 * clockTime: is the time when set_time was last called.
	 */
	while (TRUE) {
		time_min timeDiff;
		int wakeupKind;

		load_database(&database);

		/* ... wait for the time (in minutes) to change ... */
		do {
			cron_sleep(timeRunning + 1);
			set_time();
		} while (clockTime == timeRunning);
		timeRunning = clockTime;

		/*
		 * ... calculate how the current time differs from
		 * our virtual clock. Classify the change into one
		 * of 4 cases
		 */
		timeDiff = timeRunning - virtualTime;

		/* shortcut for the most common case */
		if (timeDiff == 1) {
			virtualTime = timeRunning;
			find_jobs(virtualTime, &database, TRUE, TRUE);
		} else {
			wakeupKind = -1;
			if (timeDiff > -(3*MINUTE_COUNT))
				wakeupKind = 0;
			if (timeDiff > 0)
				wakeupKind = 1;
			if (timeDiff > 5)
				wakeupKind = 2;
			if (timeDiff > (3*MINUTE_COUNT))
				wakeupKind = 3;

			switch (wakeupKind) {
			case 1:
				/*
				 * case 1: timeDiff is a small positive number
				 * (wokeup late) run jobs for each virtual minute
				 * until caught up.
				 */
				Debug(DSCH, ("[%d], normal case %d minutes to go\n",
				    getpid(), timeRunning - virtualTime))
				do {
					if (job_runqueue())
						sleep(10);
					virtualTime++;
					find_jobs(virtualTime, &database, TRUE, TRUE);
				} while (virtualTime< timeRunning);
				break;

			case 2:
				/*
				 * case 2: timeDiff is a medium-sized positive number,
				 * for example because we went to DST run wildcard
				 * jobs once, then run any fixed-time jobs that would
				 * otherwise be skipped if we use up our minute
				 * (possible, if there are a lot of jobs to run) go
				 * around the loop again so that wildcard jobs have
				 * a chance to run, and we do our housekeeping
				 */
				Debug(DSCH, ("[%d], DST begins %d minutes to go\n",
				    getpid(), timeRunning - virtualTime))
				/* run wildcard jobs for current minute */
				find_jobs(timeRunning, &database, TRUE, FALSE);
	
				/* run fixed-time jobs for each minute missed */ 
				do {
					if (job_runqueue())
						sleep(10);
					virtualTime++;
					find_jobs(virtualTime, &database, FALSE, TRUE);
					set_time();
				} while (virtualTime< timeRunning &&
				    clockTime == timeRunning);
				break;
	
			case 0:
				/*
				 * case 3: timeDiff is a small or medium-sized
				 * negative num, eg. because of DST ending just run
				 * the wildcard jobs. The fixed-time jobs probably
				 * have already run, and should not be repeated
				 * virtual time does not change until we are caught up
				 */
				Debug(DSCH, ("[%d], DST ends %d minutes to go\n",
				    getpid(), virtualTime - timeRunning))
				find_jobs(timeRunning, &database, TRUE, FALSE);
				break;
			default:
				/*
				 * other: time has changed a *lot*,
				 * jump virtual time, and run everything
				 */
				Debug(DSCH, ("[%d], clock jumped\n", getpid()))
				virtualTime = timeRunning;
				find_jobs(timeRunning, &database, TRUE, TRUE);
			}
		}
		/* jobs to be run (if any) are loaded. clear the queue */
		job_runqueue();
	}
}


static void
run_reboot_jobs(db)
	cron_db *db;
{
	register user		*u;
	register entry		*e;

	for (u = db->head;  u != NULL;  u = u->next) {
		for (e = u->crontab;  e != NULL;  e = e->next) {
			if (e->flags & WHEN_REBOOT) {
				job_add(e, u);
			}
		}
	}
	(void) job_runqueue();
}


static void
find_jobs(vtime, db, doWild, doNonWild)
	time_min vtime;
	cron_db	*db;
	int doWild;
	int doNonWild;
{
	time_t   virtualSecond  = vtime * SECONDS_PER_MINUTE;
	register struct tm	*tm = localtime(&virtualSecond);
	register int		minute, hour, dom, month, dow;
	register user		*u;
	register entry		*e;

	/* make 0-based values out of these so we can use them as indicies
	 */
	minute = tm->tm_min -FIRST_MINUTE;
	hour = tm->tm_hour -FIRST_HOUR;
	dom = tm->tm_mday -FIRST_DOM;
	month = tm->tm_mon +1 /* 0..11 -> 1..12 */ -FIRST_MONTH;
	dow = tm->tm_wday -FIRST_DOW;

	Debug(DSCH, ("[%d] tick(%d,%d,%d,%d,%d) %s %s\n",
		getpid(), minute, hour, dom, month, dow,
		doWild?" ":"No wildcard",doNonWild?" ":"Wildcard only"))

	/* the dom/dow situation is odd.  '* * 1,15 * Sun' will run on the
	 * first and fifteenth AND every Sunday;  '* * * * Sun' will run *only*
	 * on Sundays;  '* * 1,15 * *' will run *only* the 1st and 15th.  this
	 * is why we keep 'e->dow_star' and 'e->dom_star'.  yes, it's bizarre.
	 * like many bizarre things, it's the standard.
	 */
	for (u = db->head;  u != NULL;  u = u->next) {
		for (e = u->crontab;  e != NULL;  e = e->next) {
			Debug(DSCH|DEXT, ("user [%s:%d:%d:...] cmd=\"%s\"\n",
			    env_get("LOGNAME", e->envp),
			    e->uid, e->gid, e->cmd))
			if (bit_test(e->minute, minute) &&
			    bit_test(e->hour, hour) &&
			    bit_test(e->month, month) &&
			    ( ((e->flags & DOM_STAR) || (e->flags & DOW_STAR))
			      ? (bit_test(e->dow,dow) && bit_test(e->dom,dom))
			      : (bit_test(e->dow,dow) || bit_test(e->dom,dom)))) {
				if ((doNonWild && !(e->flags & (MIN_STAR|HR_STAR)))
				    || (doWild && (e->flags & (MIN_STAR|HR_STAR))))
					job_add(e, u);
			}
		}
	}
}


/*
 * set StartTime and clockTime to the current time.
 * these are used for computing what time it really is right now.
 * note that clockTime is a unix wallclock time converted to minutes
 */
static void
set_time()
{
	StartTime = time((time_t *)0);
	clockTime = StartTime / (unsigned long)SECONDS_PER_MINUTE;
}

/*
 * try to just hit the next minute
 */
static void
cron_sleep(target)
	time_min target;
{
	register int	seconds_to_wait;

	seconds_to_wait = (int)(target*SECONDS_PER_MINUTE - time((time_t*)0)) + 1;
	Debug(DSCH, ("[%d] TargetTime=%ld, sec-to-wait=%d\n",
	    getpid(), (long)target*SECONDS_PER_MINUTE, seconds_to_wait))

	if (seconds_to_wait > 0 && seconds_to_wait< 65)
		sleep((unsigned int) seconds_to_wait);
}


#ifdef USE_SIGCHLD
static void
sigchld_handler(x) {
	int save_errno = errno;
	WAIT_T		waiter;
	PID_T		pid;

	for (;;) {
#ifdef POSIX
		pid = waitpid(-1, &waiter, WNOHANG);
#else
		pid = wait3(&waiter, WNOHANG, (struct rusage *)0);
#endif
		switch (pid) {
		case -1:
			Debug(DPROC,
				("[%d] sigchld...no children\n", getpid()))
			errno = save_errno;
			return;
		case 0:
			Debug(DPROC,
				("[%d] sigchld...no dead kids\n", getpid()))
			errno = save_errno;
			return;
		default:
			Debug(DPROC,
				("[%d] sigchld...pid #%d died, stat=%d\n",
				getpid(), pid, WEXITSTATUS(waiter)))
		}
	}
	errno = save_errno;
}
#endif /*USE_SIGCHLD*/


static void
sighup_handler(x) {
	log_close();
}


static void
parse_args(argc, argv)
	int	argc;
	char	*argv[];
{
	int	argch;

	while (-1 != (argch = getopt(argc, argv, "x:"))) {
		switch (argch) {
		default:
			usage();
		case 'x':
			if (!set_debug_flags(optarg))
				usage();
			break;
		}
	}
}
