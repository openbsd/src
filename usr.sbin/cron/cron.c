/*	$OpenBSD: cron.c,v 1.18 2001/12/20 23:27:47 millert Exp $	*/
/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * All rights reserved
 */

/*
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$OpenBSD: cron.c,v 1.18 2001/12/20 23:27:47 millert Exp $";
#endif

#define	MAIN_PROGRAM

#include "cron.h"

static	void	usage(void),
		run_reboot_jobs(cron_db *),
		find_jobs __P((int, cron_db *, int, int)),
		set_time __P((int)),
		cron_sleep __P((int)),
		sigchld_handler(int),
		sighup_handler(int),
		sigusr1_handler(int),
		sigchld_reaper(void),
		check_sigs(int),
		parse_args(int c, char *v[]);

static	volatile sig_atomic_t	got_sighup, got_sigchld, got_sigusr1;
static	int			timeRunning, virtualTime, clockTime;
static	long			GMToff;
static	cron_db			database;

static void
usage(void) {
	const char **dflags;

	fprintf(stderr, "usage:  %s [-x [", ProgramName);
	for (dflags = DebugFlagNames; *dflags; dflags++)
		fprintf(stderr, "%s%s", *dflags, dflags[1] ? "," : "]");
	fprintf(stderr, "]\n");
	exit(ERROR_EXIT);
}

int
main(int argc, char *argv[]) {
	struct sigaction sact;
	int fd;

	ProgramName = argv[0];

	setlocale(LC_ALL, "");

#if defined(BSD)
	setlinebuf(stdout);
	setlinebuf(stderr);
#endif

	parse_args(argc, argv);

	bzero((char *)&sact, sizeof sact);
	sigemptyset(&sact.sa_mask);
	sact.sa_flags = 0;
#ifdef SA_RESTART
	sact.sa_flags |= SA_RESTART;
#endif
	sact.sa_handler = sigchld_handler;
	(void) sigaction(SIGCHLD, &sact, NULL);
	sact.sa_handler = sighup_handler;
	(void) sigaction(SIGHUP, &sact, NULL);

	acquire_daemonlock(0);
	set_cron_uid();
	set_cron_cwd();

	if (putenv("PATH="_PATH_DEFPATH) == -1) {
		log_it("CRON",getpid(),"DEATH","can't malloc");
		exit(1);
	}

	/* if there are no debug flags turned on, fork as a daemon should.
	 */
	if (DebugFlags) {
#if DEBUGGING
		(void) fprintf(stderr, "[%ld] cron started\n", (long)getpid());
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
			if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
				(void) dup2(fd, STDIN);
				(void) dup2(fd, STDOUT);
				(void) dup2(fd, STDERR);
				if (fd > STDERR)
					(void) close(fd);
			}
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
	set_time(1);
	run_reboot_jobs(&database);
	timeRunning = virtualTime = clockTime;

	/*
	 * Too many clocks, not enough time (Al. Einstein)
	 * These clocks are in minutes since the epoch, adjusted for timezone.
	 * virtualTime: is the time it *would* be if we woke up
	 * promptly and nobody ever changed the clock. It is
	 * monotonically increasing... unless a timejump happens.
	 * At the top of the loop, all jobs for 'virtualTime' have run.
	 * timeRunning: is the time we last awakened.
	 * clockTime: is the time when set_time was last called.
	 */
	while (TRUE) {
		int timeDiff;
		int wakeupKind;

		check_sigs(TRUE);
		/* ... wait for the time (in minutes) to change ... */
		do {
			cron_sleep(timeRunning + 1);
			set_time(0);
		} while (clockTime == timeRunning);
		timeRunning = clockTime;

		/*
		 * Calculate how the current time differs from our virtual
		 * clock.  Classify the change into one of 4 cases.
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
				 * (wokeup late) run jobs for each virtual
				 * minute until caught up.
				 */
				Debug(DSCH, ("[%d], normal case %d minutes to go\n",
				    getpid(), timeDiff))
				do {
					if (job_runqueue())
						sleep(10);
					virtualTime++;
					find_jobs(virtualTime, &database,
					    TRUE, TRUE);
				} while (virtualTime < timeRunning);
				break;

			case 2:
				/*
				 * case 2: timeDiff is a medium-sized positive
				 * number, for example because we went to DST
				 * run wildcard jobs once, then run any
				 * fixed-time jobs that would otherwise be
				 * skipped if we use up our minute (possible,
				 * if there are a lot of jobs to run) go
				 * around the loop again so that wildcard jobs
				 * have a chance to run, and we do our
				 * housekeeping.
				 */
				Debug(DSCH, ("[%d], DST begins %d minutes to go\n",
				    getpid(), timeDiff))
				/* run wildcard jobs for current minute */
				find_jobs(timeRunning, &database, TRUE, FALSE);
	
				/* run fixed-time jobs for each minute missed */
				do {
					if (job_runqueue())
						sleep(10);
					virtualTime++;
					find_jobs(virtualTime, &database,
					    FALSE, TRUE);
					set_time(0);
				} while (virtualTime< timeRunning &&
				    clockTime == timeRunning);
				break;
	
			case 0:
				/*
				 * case 3: timeDiff is a small or medium-sized
				 * negative num, eg. because of DST ending.
				 * Just run the wildcard jobs. The fixed-time
				 * jobs probably have already run, and should
				 * not be repeated.  Virtual time does not
				 * change until we are caught up.
				 */
				Debug(DSCH, ("[%d], DST ends %d minutes to go\n",
				    getpid(), timeDiff))
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

		/* Jobs to be run (if any) are loaded; clear the queue. */
		job_runqueue();
	}
}

static void
run_reboot_jobs(cron_db *db) {
	user *u;
	entry *e;

	for (u = db->head; u != NULL; u = u->next) {
		for (e = u->crontab; e != NULL; e = e->next) {
			if (e->flags & WHEN_REBOOT)
				job_add(e, u);
		}
	}
	(void) job_runqueue();
}

static void
find_jobs(int vtime, cron_db *db, int doWild, int doNonWild) {
	time_t virtualSecond  = vtime * SECONDS_PER_MINUTE;
	struct tm *tm = gmtime(&virtualSecond);
	int minute, hour, dom, month, dow;
	user *u;
	entry *e;

	/* make 0-based values out of these so we can use them as indicies
	 */
	minute = tm->tm_min -FIRST_MINUTE;
	hour = tm->tm_hour -FIRST_HOUR;
	dom = tm->tm_mday -FIRST_DOM;
	month = tm->tm_mon +1 /* 0..11 -> 1..12 */ -FIRST_MONTH;
	dow = tm->tm_wday -FIRST_DOW;

	Debug(DSCH, ("[%ld] tick(%d,%d,%d,%d,%d) %s %s\n",
		     (long)getpid(), minute, hour, dom, month, dow,
		     doWild?" ":"No wildcard",doNonWild?" ":"Wildcard only"))

	/* the dom/dow situation is odd.  '* * 1,15 * Sun' will run on the
	 * first and fifteenth AND every Sunday;  '* * * * Sun' will run *only*
	 * on Sundays;  '* * 1,15 * *' will run *only* the 1st and 15th.  this
	 * is why we keep 'e->dow_star' and 'e->dom_star'.  yes, it's bizarre.
	 * like many bizarre things, it's the standard.
	 */
	for (u = db->head; u != NULL; u = u->next) {
		for (e = u->crontab; e != NULL; e = e->next) {
			Debug(DSCH|DEXT, ("user [%s:%ld:%ld:...] cmd=\"%s\"\n",
					  env_get("LOGNAME", e->envp),
					  (long)e->uid, (long)e->gid, e->cmd))
			if (bit_test(e->minute, minute) &&
			    bit_test(e->hour, hour) &&
			    bit_test(e->month, month) &&
			    ( ((e->flags & DOM_STAR) || (e->flags & DOW_STAR))
			      ? (bit_test(e->dow,dow) && bit_test(e->dom,dom))
			      : (bit_test(e->dow,dow) || bit_test(e->dom,dom))
			    )
			   ) {
				if ((doNonWild &&
				    !(e->flags & (MIN_STAR|HR_STAR))) || 
				    (doWild && (e->flags & (MIN_STAR|HR_STAR))))
					job_add(e, u);
			}
		}
	}
}

/*
 * Set StartTime and clockTime to the current time.
 * These are used for computing what time it really is right now.
 * Note that clockTime is a unix wallclock time converted to minutes.
 */
static void
set_time(int initialize) {
	struct tm *tm;
	static int isdst;

	StartTime = time(NULL);

	/* We adjust the time to GMT so we can catch DST changes. */
	tm = localtime(&StartTime);
	if (initialize || tm->tm_isdst != isdst) {
		isdst = tm->tm_isdst;
		GMToff = get_gmtoff(&StartTime, tm);
	}
	clockTime = (StartTime + GMToff) / (time_t)SECONDS_PER_MINUTE;
}

/*
 * Try to just hit the next minute.
 */
static void
cron_sleep(int target) {
	time_t t1, t2;
	int seconds_to_wait;
	struct sigaction sact;

	bzero((char *)&sact, sizeof sact);
	sigemptyset(&sact.sa_mask);
	sact.sa_flags = 0;
#ifdef SA_RESTART
	sact.sa_flags |= SA_RESTART;
#endif
	sact.sa_handler = sigusr1_handler;
	(void) sigaction(SIGUSR1, &sact, NULL);

	t1 = time(NULL) + GMToff;
	seconds_to_wait = (int)(target * SECONDS_PER_MINUTE - t1) + 1;
	Debug(DSCH, ("[%ld] Target time=%ld, sec-to-wait=%d\n",
	    (long)getpid(), (long)target*SECONDS_PER_MINUTE, seconds_to_wait))

	while (seconds_to_wait > 0 && seconds_to_wait < 65) {
		sleep((unsigned int) seconds_to_wait);

		/*
		 * Check to see if we were interrupted by a signal.
		 * If so, service the signal(s) then continue sleeping
		 * where we left off.
		 */
		check_sigs(FALSE);
		t2 = time(NULL) + GMToff;
		seconds_to_wait -= (int)(t2 - t1);
		t1 = t2;
	}

	sact.sa_handler = SIG_DFL;
	(void) sigaction(SIGUSR1, &sact, NULL);
}

static void
sighup_handler(int x) {
	got_sighup = 1;
}

static void
sigchld_handler(int x) {
	got_sigchld = 1;
}

static void
sigusr1_handler(int x) {
	got_sigusr1 = 1;
}

static void
sigchld_reaper() {
	WAIT_T waiter;
	PID_T pid;

	do {
		pid = waitpid(-1, &waiter, WNOHANG);
		switch (pid) {
		case -1:
			if (errno == EINTR)
				continue;
			Debug(DPROC,
			      ("[%ld] sigchld...no children\n",
			       (long)getpid()))
			break;
		case 0:
			Debug(DPROC,
			      ("[%ld] sigchld...no dead kids\n",
			       (long)getpid()))
			break;
		default:
			Debug(DPROC,
			      ("[%ld] sigchld...pid #%ld died, stat=%d\n",
			       (long)getpid(), (long)pid, WEXITSTATUS(waiter)))
		}
	} while (pid > 0);
}

static void
check_sigs(int force_dbload) {
	if (got_sighup) {
		got_sighup = 0;
		log_close();
	}
	if (got_sigchld) {
		got_sigchld = 0;
		sigchld_reaper();
	}
	if (got_sigusr1 || force_dbload) {
		got_sigusr1 = 0;
		load_database(&database);
	}
}

static void
parse_args(int argc, char *argv[]) {
	int argch;

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
