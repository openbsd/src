/*	$OpenBSD: c_ulimit.c,v 1.15 2006/03/12 00:26:58 deraadt Exp $	*/

/*
	ulimit -- handle "ulimit" builtin

	Reworked to use getrusage() and ulimit() at once (as needed on
	some schizophrenic systems, eg, HP-UX 9.01), made argument parsing
	conform to at&t ksh, added autoconf support.  Michael Rendell, May, '94

	Eric Gisin, September 1988
	Adapted to PD KornShell. Removed AT&T code.

	last edit:	06-Jun-1987	D A Gwyn

	This started out as the BRL UNIX System V system call emulation
	for 4.nBSD, and was later extended by Doug Kingston to handle
	the extended 4.nBSD resource limits.  It now includes the code
	that was originally under case SYSULIMIT in source file "xec.c".
*/

#include "sh.h"
#include <sys/resource.h>

#define SOFT	0x1
#define HARD	0x2

int
c_ulimit(char **wp)
{
	static const struct limits {
		const char	*name;
		enum { RLIMIT, ULIMIT } which;
		int	gcmd;	/* get command */
		int	scmd;	/* set command (or -1, if no set command) */
		int	factor;	/* multiply by to get rlim_{cur,max} values */
		char	option;
	} limits[] = {
		/* Do not use options -H, -S or -a */
		{ "time(cpu-seconds)", RLIMIT, RLIMIT_CPU, RLIMIT_CPU, 1, 't' },
		{ "file(blocks)", RLIMIT, RLIMIT_FSIZE, RLIMIT_FSIZE, 512, 'f' },
		{ "coredump(blocks)", RLIMIT, RLIMIT_CORE, RLIMIT_CORE, 512, 'c' },
		{ "data(kbytes)", RLIMIT, RLIMIT_DATA, RLIMIT_DATA, 1024, 'd' },
		{ "stack(kbytes)", RLIMIT, RLIMIT_STACK, RLIMIT_STACK, 1024, 's' },
		{ "lockedmem(kbytes)", RLIMIT, RLIMIT_MEMLOCK, RLIMIT_MEMLOCK,
		    1024, 'l' },
		{ "memory(kbytes)", RLIMIT, RLIMIT_RSS, RLIMIT_RSS, 1024, 'm' },
		{ "nofiles(descriptors)", RLIMIT, RLIMIT_NOFILE, RLIMIT_NOFILE,
		    1, 'n' },
		{ "processes", RLIMIT, RLIMIT_NPROC, RLIMIT_NPROC, 1, 'p' },
	#ifdef RLIMIT_VMEM
		{ "vmemory(kbytes)", RLIMIT, RLIMIT_VMEM, RLIMIT_VMEM, 1024, 'v' },
	#endif /* RLIMIT_VMEM */
		{ (char *) 0 }
	};
	static char	options[3 + NELEM(limits)];
	rlim_t		val = 0;
	int		how = SOFT | HARD;
	const struct limits	*l;
	int		set, all = 0;
	int		optc, what;
	struct rlimit	limit;
	if (!options[0]) {
		/* build options string on first call - yuck */
		char *p = options;

		*p++ = 'H'; *p++ = 'S'; *p++ = 'a';
		for (l = limits; l->name; l++)
			*p++ = l->option;
		*p = '\0';
	}
	what = 'f';
	while ((optc = ksh_getopt(wp, &builtin_opt, options)) != -1)
		switch (optc) {
		case 'H':
			how = HARD;
			break;
		case 'S':
			how = SOFT;
			break;
		case 'a':
			all = 1;
			break;
		case '?':
			return 1;
		default:
			what = optc;
		}

	for (l = limits; l->name && l->option != what; l++)
		;
	if (!l->name) {
		internal_errorf(0, "ulimit: %c", what);
		return 1;
	}

	wp += builtin_opt.optind;
	set = *wp ? 1 : 0;
	if (set) {
		if (all || wp[1]) {
			bi_errorf("too many arguments");
			return 1;
		}
		if (strcmp(wp[0], "unlimited") == 0)
			val = RLIM_INFINITY;
		else {
			long rval;

			if (!evaluate(wp[0], &rval, KSH_RETURN_ERROR, false))
				return 1;
			/* Avoid problems caused by typos that
			 * evaluate misses due to evaluating unset
			 * parameters to 0...
			 * If this causes problems, will have to
			 * add parameter to evaluate() to control
			 * if unset params are 0 or an error.
			 */
			if (!rval && !digit(wp[0][0])) {
				bi_errorf("invalid limit: %s", wp[0]);
				return 1;
			}
			val = rval * l->factor;
		}
	}
	if (all) {
		for (l = limits; l->name; l++) {
			if (l->which == RLIMIT) {
				getrlimit(l->gcmd, &limit);
				if (how & SOFT)
					val = limit.rlim_cur;
				else if (how & HARD)
					val = limit.rlim_max;
			}
			shprintf("%-20s ", l->name);
			if (val == RLIM_INFINITY)
				shprintf("unlimited\n");
			else {
				val /= l->factor;
				shprintf("%ld\n", (long) val);
			}
		}
		return 0;
	}
	if (l->which == RLIMIT) {
		getrlimit(l->gcmd, &limit);
		if (set) {
			if (how & SOFT)
				limit.rlim_cur = val;
			if (how & HARD)
				limit.rlim_max = val;
			if (setrlimit(l->scmd, &limit) < 0) {
				if (errno == EPERM)
					bi_errorf("exceeds allowable limit");
				else
					bi_errorf("bad limit: %s",
					    strerror(errno));
				return 1;
			}
		} else {
			if (how & SOFT)
				val = limit.rlim_cur;
			else if (how & HARD)
				val = limit.rlim_max;
		}
	}
	if (!set) {
		if (val == RLIM_INFINITY)
			shprintf("unlimited\n");
		else {
			val /= l->factor;
			shprintf("%ld\n", (long) val);
		}
	}
	return 0;
}
