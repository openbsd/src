/*	$OpenBSD: authpf.c,v 1.37 2002/12/19 14:04:03 henning Exp $	*/

/*
 * Copyright (C) 1998 - 2002 Bob Beck (beck@openbsd.org).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <login_cap.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>
#include <resolv.h>

#include <pfctl_parser.h>

#include "pathnames.h"

int	Rule_Action = PF_CHANGE_ADD_TAIL;
int	Nat_Action = PF_CHANGE_ADD_HEAD;
int	Rdr_Action = PF_CHANGE_ADD_HEAD;
int	dev;			/* pf device */
int	Delete_Rules;		/* for parse_rules callbacks */

FILE	*pidfp;
char	*infile;		/* infile name needed by parse_[rules|nat] */
char	 luser[MAXLOGNAME];	/* username */
char	 ipsrc[256];		/* ip as a string */
char	 pidfile[MAXPATHLEN];	/* we save pid in this file. */

struct timeval	Tstart, Tend;	/* start and end times of session */

int	pfctl_add_pool(struct pfctl *, struct pf_pool *, sa_family_t);
int	pfctl_add_rule(struct pfctl *, struct pf_rule *);

int	pfctl_compare_addr_wraps(struct pf_addr_wrap *,
	    struct pf_addr_wrap *, sa_family_t);
int	pfctl_compare_pooladdrs(struct pf_pooladdr *,
	    struct pf_pooladdr *, sa_family_t);
int	pfctl_compare_pools(struct pf_pool *, struct pf_pool *, sa_family_t);
int	pfctl_compare_rules(struct pf_rule *, struct pf_rule *);

static int	read_config(FILE *);
static void	print_message(char *);
static int	allowed_luser(char *);
static int	check_luser(char *, char *);
static int	changefilter(int, char *, char *);
static void	authpf_kill_states(void);

volatile sig_atomic_t	want_death;
static void		need_death(int signo);
static __dead void	do_death(int);

/*
 * User shell for authenticating gateways. sole purpose is to allow
 * a user to ssh to a gateway, and have the gateway modify packet
 * filters to allow access, then remove access when the user finishes
 * up. Meant to be used only from ssh(1) connections.
 */
int
main(int argc, char *argv[])
{
	int		 lockcnt = 0, n, pidfd;
	FILE		*config;
	struct in_addr	 ina;
	struct passwd	*pw;
	char		*cp;
	uid_t		 uid;

	config = fopen(PATH_CONFFILE, "r");
	if (config == NULL)
		exit(1);

	if ((cp = getenv("SSH_TTY")) == NULL) {
		syslog(LOG_ERR, "Non-interactive session connection for authpf");
		exit(1);
	}

	if ((cp = getenv("SSH_CLIENT")) == NULL) {
		syslog(LOG_ERR, "Can't determine connection source");
		exit(1);
	}

	if (strlcpy(ipsrc, cp, sizeof(ipsrc)) >= sizeof(ipsrc)) {
		syslog(LOG_ERR, "SSH_CLIENT variable too long");
		exit(1);
	}
	cp = strchr(ipsrc, ' ');
	if (!cp) {
		syslog(LOG_ERR, "Corrupt SSH_CLIENT variable %s", ipsrc);
		exit(1);
	}
	*cp = '\0';
	if (inet_pton(AF_INET, ipsrc, &ina) != 1) {
		syslog(LOG_ERR,
		    "Cannot determine IP from SSH_CLIENT %s", ipsrc);
		exit(1);
	}

	/* open the pf device */
	dev = open(PATH_DEVFILE, O_RDWR);
	if (dev == -1) {
		syslog(LOG_ERR, "Can't open filter device (%m)");
		goto die;
	}

	uid = getuid();
	pw = getpwuid(uid);
	if (pw == NULL) {
		syslog(LOG_ERR, "can't find user for uid %u", uid);
		goto die;
	}
	if (strcmp(pw->pw_shell, PATH_AUTHPF_SHELL)) {
		syslog(LOG_ERR, "wrong shell for user %s, uid %u",
		    pw->pw_name, pw->pw_uid);
		goto die;
	}

	/*
	 * Paranoia, but this data _does_ come from outside authpf, and
	 * truncation would be bad.
	 */
	if (strlcpy(luser, pw->pw_name, sizeof(luser)) >= sizeof(luser)) {
		syslog(LOG_ERR, "username too long: %s", pw->pw_name);
		goto die;
	}

	/* Make our entry in /var/authpf as /var/authpf/ipaddr */
	n = snprintf(pidfile, sizeof(pidfile), "%s/%s", PATH_PIDFILE, ipsrc);
	if (n < 0 || (u_int)n >= sizeof(pidfile)) {
		syslog(LOG_ERR, "path to pidfile too long");
		goto die;
	}

	/*
	 * If someone else is already using this ip, then this person
	 * wants to switch users - so kill the old process and exit
	 * as well.
	 *
	 * Note, we could print a message and tell them to log out, but the
	 * usual case of this is that someone has left themselves logged in,
	 * with the authenticated connection iconized and someone else walks
	 * up to use and automatically logs in before using. If this just
	 * gets rid of the old one silently, the new user never knows they
	 * could have used someone else's old authentication. If we
	 * tell them to log out before switching users it is an invitation
	 * for abuse.
	 */

	do {
		int	save_errno, otherpid = -1;
		char	otherluser[MAXLOGNAME];

		if ((pidfd = open(pidfile, O_RDWR|O_CREAT, 0644)) == -1 ||
		    (pidfp = fdopen(pidfd, "r+")) == NULL) {
			if (pidfd != -1)
				close(pidfd);
			syslog(LOG_ERR, "can't open or create %s: %s", pidfile,
			    strerror(errno));
			goto die;
		}

		if (flock(fileno(pidfp), LOCK_EX|LOCK_NB) == 0)
			break;
		save_errno = errno;

		/* Mark our pid, and username to our file. */

		rewind(pidfp);
		/* 31 == MAXLOGNAME - 1 */
		if (fscanf(pidfp, "%d\n%31s\n", &otherpid, otherluser) != 2)
			otherpid = -1;
		syslog(LOG_DEBUG, "Tried to lock %s, in use by pid %d: %s",
		    pidfile, otherpid, strerror(save_errno));

		if (otherpid > 0) {
			syslog(LOG_INFO,
			    "killing prior auth (pid %d) of %s by user %s",
			    otherpid, ipsrc, otherluser);
			if (kill((pid_t) otherpid, SIGTERM) == -1) {
				syslog(LOG_INFO,
				    "Couldn't kill process %d: (%m)",
				    otherpid);
			}
		}

		/*
		 * we try to kill the previous process and acquire the lock
		 * for 10 seconds, trying once a second. if we can't after
		 * 10 attempts we log an error and give up
		 */
		if (++lockcnt > 10) {
			syslog(LOG_ERR, "Can't kill previous authpf (pid %d)",
			    otherpid);
			goto dogdeath;
		}
		sleep(1);

		/* re-open, and try again. The previous authpf process
		 * we killed above should unlink the file and release
		 * it's lock, giving us a chance to get it now
		 */
		fclose(pidfp);
	} while (1);

	/* revoke privs */
	seteuid(getuid());
	setuid(getuid());

	if (!check_luser(PATH_BAN_DIR, luser) || !allowed_luser(luser))
		do_death(0);

	if (read_config(config))
		do_death(0);

	/* We appear to be making headway, so actually mark our pid */
	rewind(pidfp);
	fprintf(pidfp, "%ld\n%s\n", (long)getpid(), luser);
	fflush(pidfp);
	(void) ftruncate(fileno(pidfp), ftell(pidfp));

	if (changefilter(1, luser, ipsrc) == -1) {
		printf("Unable to modify filters\r\n");
		do_death(1);
	}

	signal(SIGTERM, need_death);
	signal(SIGINT, need_death);
	signal(SIGALRM, need_death);
	signal(SIGPIPE, need_death);
	signal(SIGHUP, need_death);
	signal(SIGSTOP, need_death);
	signal(SIGTSTP, need_death);
	while (1) {
		printf("\r\nHello %s, ", luser);
		printf("You are authenticated from host \"%s\"\r\n", ipsrc);
		print_message(PATH_MESSAGE);
		while (1) {
			sleep(10);
			if (want_death)
				do_death(1);
		}
	}

	/* NOTREACHED */
dogdeath:
	printf("\r\n\r\nSorry, this service is currently unavailable due to ");
	printf("technical difficulties\r\n\r\n");
	print_message(PATH_PROBLEM);
	printf("\r\nYour authentication process (pid %d) was unable to run\n",
	    getpid());
	sleep(180); /* them lusers read reaaaaal slow */
die:
	do_death(0);
}

/*
 * reads config file in PATH_CONFFILE to set optional behaviours up
 */
static int
read_config(FILE *f)
{
	char	buf[1024];
	int	i = 0;

	openlog("authpf", LOG_PID | LOG_NDELAY, LOG_DAEMON);

	do {
		char	**ap;
		char	 *pair[4], *cp, *tp;
		int	  len;

		if (fgets(buf, sizeof(buf), f) == NULL) {
			fclose(f);
			return (0);
		}
		i++;
		len = strlen(buf);
		if (buf[len - 1] != '\n' && !feof(f)) {
			syslog(LOG_ERR, "line %d too long in %s", i,
			    PATH_CONFFILE);
			return (1);
		}
		buf[len - 1] = '\0';

		for (cp = buf; *cp == ' ' || *cp == '\t'; cp++)
			; /* nothing */

		if (!*cp || *cp == '#' || *cp == '\n')
			continue;

		for (ap = pair; ap < &pair[3] &&
		    (*ap = strsep(&cp, "=")) != NULL; ) {
			if (**ap != '\0')
				ap++;
		}
		if (ap != &pair[2])
			goto parse_error;

		tp = pair[1]+strlen(pair[1]);
		while ((*tp == ' ' || *tp == '\t') && tp >= pair[1])
			*tp-- = '\0';

		if (strcasecmp(pair[0], "rule_action") == 0) {
			if (strcasecmp(pair[1], "head") == 0)
				Rule_Action = PF_CHANGE_ADD_HEAD;
			else if (strcasecmp(pair[1], "tail") == 0)
				Rule_Action = PF_CHANGE_ADD_TAIL;
			else
				goto parse_error;
		} else if (strcasecmp(pair[0], "nat_action") == 0) {
			if (strcasecmp(pair[1], "head") == 0)
				Nat_Action = PF_CHANGE_ADD_HEAD;
			else if (strcasecmp(pair[1], "tail") == 0)
				Nat_Action = PF_CHANGE_ADD_TAIL;
			else
				goto parse_error;
		} else if (strcasecmp(pair[0], "rdr_action") == 0)  {
			if (strcasecmp(pair[1], "head") == 0)
				Rdr_Action = PF_CHANGE_ADD_HEAD;
			else if (strcasecmp(pair[1], "tail") == 0)
				Rdr_Action = PF_CHANGE_ADD_TAIL;
			else
				goto parse_error;
		}
	} while (!feof(f) && !ferror(f));
	fclose(f);
	return (0);

parse_error:
	fclose(f);
	syslog(LOG_ERR, "parse error, line %d of %s", i, PATH_CONFFILE);
	return (1);
}


/*
 * splatter a file to stdout - max line length of 1024,
 * used for spitting message files at users to tell them
 * they've been bad or we're unavailable.
 */
static void
print_message(char *filename)
{
	char	 buf[1024];
	FILE	*f;

	if ((f = fopen(filename, "r")) == NULL)
		return; /* fail silently, we don't care if it isn't there */

	do {
		if (fgets(buf, sizeof(buf), f) == NULL) {
			fflush(stdout);
			fclose(f);
			return;
		}
	} while (fputs(buf, stdout) != EOF && !feof(f));
	fflush(stdout);
	fclose(f);
}

/*
 * allowed_luser checks to see if user "luser" is allowed to
 * use this gateway by virtue of being listed in an allowed
 * users file, namely /etc/authpf.allow .
 *
 * If /etc/authpf.allow does not exist, then we assume that
 * all users who are allowed in by sshd(8) are permitted to
 * use this gateway. If /etc/authpf.allow does exist, then a
 * user must be listed if the connection is to continue, else
 * the session terminates in the same manner as being banned.
 */
static int
allowed_luser(char *luser)
{
	char	*buf, *lbuf;
	int	 matched;
	size_t	 len;
	FILE	*f;

	if ((f = fopen(PATH_ALLOWFILE, "r")) == NULL) {
		if (errno == ENOENT) {
			/*
			 * allowfile doesn't exist, thus this gateway
			 * isn't restricted to certain users...
			 */
			return (1);
		}

		/*
		 * luser may in fact be allowed, but we can't open
		 * the file even though it's there. probably a config
		 * problem.
		 */
		syslog(LOG_ERR, "Can't open allowed users file %s (%s)",
		    PATH_ALLOWFILE, strerror(errno));
		return (0);
	} else {
		/*
		 * /etc/authpf.allow exists, thus we do a linear
		 * search to see if they are allowed.
		 * also, if username "*" exists, then this is a
		 * "public" gateway, such as it is, so let
		 * everyone use it.
		 */
		lbuf = NULL;
		while ((buf = fgetln(f, &len))) {
			if (buf[len - 1] == '\n')
				buf[len - 1] = '\0';
			else {
				if ((lbuf = (char *)malloc(len + 1)) == NULL)
					err(1, NULL);
				memcpy(lbuf, buf, len);
				lbuf[len] = '\0';
				buf = lbuf;
			}

			matched = strcmp(luser, buf) == 0 || strcmp("*", buf) == 0;

			if (lbuf != NULL) {
				free(lbuf);
				lbuf = NULL;
			}

			if (matched)
				return (1); /* matched an allowed username */
		}
		syslog(LOG_INFO, "Denied access to %s: not listed in %s",
		    luser, PATH_ALLOWFILE);

		/* reuse buf */
		buf = "\n\nSorry, you aren't allowed to use this facility!\n";
		fputs(buf, stdout);
	}
	fflush(stdout);
	return (0);
}

/*
 * check_luser checks to see if user "luser" has been banned
 * from using us by virtue of having an file of the same name
 * in the "luserdir" directory.
 *
 * If the user has been banned, we copy the contents of the file
 * to the user's screen. (useful for telling the user what to
 * do to get un-banned, or just to tell them they aren't
 * going to be un-banned.)
 */
static int
check_luser(char *luserdir, char *luser)
{
	FILE	*f;
	int	 n;
	char	 tmp[MAXPATHLEN];

	n = snprintf(tmp, sizeof(tmp), "%s/%s", luserdir, luser);
	if (n < 0 || (u_int)n >= sizeof(tmp)) {
		syslog(LOG_ERR, "Provided banned directory line too long (%s)",
		    luserdir);
		return (0);
	}
	if ((f = fopen(tmp, "r")) == NULL) {
		if (errno == ENOENT) {
			/*
			 * file or dir doesn't exist, so therefore
			 * this luser isn't banned..  all is well
			 */
			return (1);
		} else {
			/*
			 * luser may in fact be banned, but we can't open the
			 * file even though it's there. probably a config
			 * problem.
			 */
			syslog(LOG_ERR, "Can't open banned file %s (%s)",
			    tmp, strerror(errno));
			return (0);
		}
	} else {
		/*
		 * luser is banned - spit the file at them to
		 * tell what they can do and where they can go.
		 */
		syslog(LOG_INFO, "Denied access to %s: %s exists",
		    luser, tmp);

		/* reuse tmp */
		strlcpy(tmp, "\n\n-**- Sorry, you have been banned! -**-\n\n",
		    sizeof(tmp));
		while ((fputs(tmp, stdout) != EOF) && !feof(f)) {
			if (fgets(tmp, sizeof(tmp), f) == NULL) {
				fflush(stdout);
				return (0);
			}
		}
	}
	fflush(stdout);
	return (0);
}


/*
 * Add/remove filter entries for user "luser" from ip "ipsrc"
 */
static int
changefilter(int add, char *luser, char *ipsrc)
{
	char			 rulesfile[MAXPATHLEN], buf[1024];
	char			 template[] = "/tmp/authpfrules.XXXXXXX";
	int			 tmpfile = -1, from_fd = -1, ret = -1;
	struct pfioc_rule	 pr;
	struct pfctl		 pf;
	int			 n, rcount, wcount;
	FILE			*fin = NULL;

	memset(&pf, 0, sizeof(pf));
	memset(&pr, 0, sizeof(pr));

	syslog(LOG_DEBUG, "%s filter for ip=%s, user %s",
	    add ? "Adding" : "Removing", ipsrc, luser);

	/* add filter rules */
	if (add)
		Delete_Rules = 0;
	else
		Delete_Rules = 1;

	tmpfile = mkstemp(template);
	if (tmpfile == -1) {
		syslog(LOG_ERR, "Can't open temp file %s (%m)",
		    template);
		goto error;
	}

	fin = fdopen(tmpfile, "r+");
	if (fin == NULL) {
		syslog(LOG_ERR, "can't open %s (%m)", template);
		goto error;
	}

	/* write the variable to the start of the file */
	fprintf(fin, "user_ip = \"%s\"\n", ipsrc);

	fflush(fin);

	n = snprintf(rulesfile, sizeof(rulesfile), "%s/%s/authpf.rules",
	    PATH_USER_DIR, luser);
	if (n < 0 || (u_int)n >= sizeof(rulesfile)) {
		syslog(LOG_ERR, "user path too long, exiting");
		goto error;
	}
	if ((from_fd = open(rulesfile, O_RDONLY, 0)) == -1) {
		/* if user dir rules do not exist, we try PATH_PFRULES */
		if (errno != ENOENT) {
			syslog(LOG_ERR, "can't open %s (%m)", rulesfile);
			if (unlink(template) == -1)
				syslog(LOG_ERR, "can't unlink %s", template);
			goto error;
		}
	}
	if (from_fd == -1) {
		snprintf(rulesfile, sizeof(rulesfile), PATH_PFRULES);
		if  ((from_fd = open(rulesfile, O_RDONLY, 0)) == -1) {
			syslog(LOG_ERR, "can't open %s (%m)", rulesfile);
			if (unlink(template) == -1)
				syslog(LOG_ERR, "can't unlink %s", template);
			goto error;
		}
	}

	while ((rcount = read(from_fd, buf, sizeof(buf))) > 0) {
		wcount = write(tmpfile, buf, rcount);
		if (rcount != wcount || wcount == -1) {
			syslog(LOG_ERR, "rules template copy failed");
			if (unlink(template) == -1)
				syslog(LOG_ERR, "can't unlink %s", template);
			goto error;
		}
	}
	if (rcount == -1) {
		syslog(LOG_ERR, "read of rules template failed");
		if (unlink(template) == -1)
			syslog(LOG_ERR, "can't unlink %s", template);
		goto error;
	}

	fclose(fin);
	fin = NULL;
	close(tmpfile);
	tmpfile = -1;
	close(from_fd);
	from_fd = -1;

	fin = fopen(template, "r");
	if (fin == NULL) {
		syslog(LOG_ERR, "can't open %s (%m)", template);
		if (unlink(template) == -1)
			syslog(LOG_ERR, "can't unlink %s", template);
		goto error;
	}

	infile = template;

	if (unlink(template) == -1) {
		syslog(LOG_ERR, "can't unlink %s", template);
		goto error;
	}

	/* add/delete rules, using parse_rule */
	pf.dev = dev;
	pf.prule[PF_RULESET_RULE] = &pr;
	if (parse_rules(fin, &pf, 0) < 0) {
		syslog(LOG_ERR,
		    "syntax error in rule file: authpf rules not loaded");
		goto error;
	}
	ret = 0;
	goto out;
 error:
	ret = -1;
 out:
	if (fin != NULL)
		fclose(fin);
	if (tmpfile != -1)
		close(tmpfile);
	if (from_fd != -1)
		close(from_fd);
	if (add) {
		(void)gettimeofday(&Tstart, NULL);
		syslog(LOG_INFO, "Allowing %s, user %s", ipsrc, luser);
	} else {
		(void)gettimeofday(&Tend, NULL);
		syslog(LOG_INFO, "Removed %s, user %s - duration %ld seconds",
		    ipsrc, luser, Tend.tv_sec - Tstart.tv_sec);
	}
	return (ret);
}

/*
 * This is to kill off states that would otherwise be left behind stateful
 * rules. This means we don't need to allow in more traffic than we really
 * want to, since we don't have to worry about any luser sessions lasting
 * longer than their ssh session. This function is based on
 * pfctl_kill_states from pfctl.
 */
static void
authpf_kill_states()
{
	struct pfioc_state_kill	psk;
	struct in_addr		target;

	memset(&psk, 0, sizeof(psk));
	psk.psk_af = AF_INET;

	inet_pton(AF_INET, ipsrc, &target);

	/* Kill all states from ipsrc */
	psk.psk_src.addr.addr.v4 = target;
	memset(&psk.psk_src.addr.mask, 0xff, sizeof(psk.psk_src.addr.mask));
	if (ioctl(dev, DIOCKILLSTATES, &psk))
		syslog(LOG_ERR, "DIOCKILLSTATES failed (%m)");

	/* Kill all states to ipsrc */
	memset(&psk.psk_src, 0, sizeof(psk.psk_src));
	psk.psk_dst.addr.addr.v4 = target;
	memset(&psk.psk_dst.addr.mask, 0xff, sizeof(psk.psk_dst.addr.mask));
	if (ioctl(dev, DIOCKILLSTATES, &psk))
		syslog(LOG_ERR, "DIOCKILLSTATES failed (%m)");
}

/* signal handler that makes us go away properly */
static void
need_death(int signo)
{
	want_death = 1;
}

/*
 * function that removes our stuff when we go away.
 */
static __dead void
do_death(int active)
{
	int	ret = 0;

	if (active) {
		changefilter(0, luser, ipsrc);
		authpf_kill_states();
	}
	if (pidfp)
		ftruncate(fileno(pidfp), 0);
	if (pidfile[0])
		if (unlink(pidfile) == -1)
			syslog(LOG_ERR, "can't unlink %s (%m)", pidfile);
	exit(ret);
}

int
pfctl_add_pool(struct pfctl *pf, struct pf_pool *p, sa_family_t af)
{
	struct pf_pooladdr	*pa;

	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(pf->dev, DIOCBEGINADDRS, &pf->paddr))
			err(1, "DIOCBEGINADDRS");
	}

	pf->paddr.af = af;
	TAILQ_FOREACH(pa, &p->list, entries) {
		memcpy(&pf->paddr.addr, pa, sizeof(struct pf_pooladdr));
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			if (ioctl(pf->dev, DIOCADDADDR, &pf->paddr))
				err(1, "DIOCADDADDR");
		}
	}
	return (0);
}

int
pfctl_compare_addr_wraps(struct pf_addr_wrap *a, struct pf_addr_wrap *b,
    sa_family_t af)
{
	if (a->addr_dyn != NULL && b->addr_dyn != NULL) {
		if (strcmp(a->addr_dyn->ifname, b->addr_dyn->ifname))
			return (1);
	} else {
		if (a->addr_dyn != NULL || b->addr_dyn != NULL)
			return (1);
		if (PF_ANEQ(&a->addr, &b->addr, af))
			return (1);
	}
	if (PF_ANEQ(&a->mask, &b->mask, af))
		return (1);
	return (0);
}

int
pfctl_compare_pooladdrs(struct pf_pooladdr *a, struct pf_pooladdr *b,
    sa_family_t af)
{
	if (pfctl_compare_addr_wraps(&a->addr.addr, &b->addr.addr, af))
		return (1);
	if (a->addr.port[0] != b->addr.port[0] ||
	    a->addr.port[1] != b->addr.port[1] ||
	    a->addr.not != b->addr.not ||
	    a->addr.port_op != b->addr.port_op)
		return (1);
	if (strcmp(a->ifname, b->ifname))
		return (1);
	return (0);
}

int
pfctl_compare_pools(struct pf_pool *a, struct pf_pool *b, sa_family_t af)
{
	struct pf_pooladdr	*pa_a, *pa_b;

	if (a->key.key32[0] != b->key.key32[0] ||
	    a->key.key32[1] != b->key.key32[1] ||
	    a->key.key32[2] != b->key.key32[2] ||
	    a->key.key32[3] != b->key.key32[3] ||
	    a->proxy_port[0] != b->proxy_port[0] ||
	    a->proxy_port[1] != b->proxy_port[1] ||
	    a->port_op != b->port_op ||
	    a->opts != b->opts)
		return (1);
	pa_a = TAILQ_FIRST(&a->list);
	pa_b = TAILQ_FIRST(&b->list);
	while (pa_a != NULL && pa_b != NULL) {
		pfctl_compare_pooladdrs(pa_a, pa_b, af);
		pa_a = TAILQ_NEXT(pa_a, entries);
		pa_b = TAILQ_NEXT(pa_b, entries);
	}
	return (0);
}

int
pfctl_compare_rules(struct pf_rule *a, struct pf_rule *b)
{
	if (a->return_icmp != b->return_icmp ||
	    a->return_icmp6 != b->return_icmp6 ||
	    a->action != b->action ||
	    a->direction != b->direction ||
	    a->log != b->log ||
	    a->quick != b->quick ||
	    a->keep_state != b->keep_state ||
	    a->af != b->af ||
	    a->proto != b->proto ||
	    a->type != b->type ||
	    a->code != b->code ||
	    a->flags != b->flags ||
	    a->flagset != b->flagset ||
	    a->rule_flag != b->rule_flag ||
	    a->min_ttl != b->min_ttl ||
	    a->tos != b->tos ||
	    a->allow_opts != b->allow_opts ||
	    a->ifnot != b->ifnot)
		return (1);
	if (pfctl_compare_addr_wraps(&a->src.addr, &b->src.addr, a->af))
		return (1);
	if (a->src.port[0] != b->src.port[0] ||
	    a->src.port[1] != b->src.port[1] ||
	    a->src.not != b->src.not ||
	    a->src.port_op != b->src.port_op)
		return (1);
	if (pfctl_compare_addr_wraps(&a->dst.addr, &b->dst.addr, a->af))
		return (1);
	if (a->dst.port[0] != b->dst.port[0] ||
	    a->dst.port[1] != b->dst.port[1] ||
	    a->dst.not != b->dst.not ||
	    a->dst.port_op != b->dst.port_op)
		return (1);
	if (pfctl_compare_pools(&a->rpool, &b->rpool, a->af))
		return (1);
	if (strcmp(a->ifname, b->ifname) ||
	    strcmp(a->anchorname, b->anchorname) ||
	    strcmp(a->label, b->label))
		return (1);
	return (0);
}


/*
 * callback for rule add, used by parser in parse_rules
 */
int
pfctl_add_rule(struct pfctl *pf, struct pf_rule *r)
{
	struct pfioc_rule	pcr;
	u_int32_t		mnr, nr, match = 0;

	memset(&pcr, 0, sizeof(pcr));
	pcr.rule.action = r->action;
	pcr.action = PF_CHANGE_GET_TICKET;
	if (ioctl(pf->dev, DIOCCHANGERULE, &pcr))
		syslog(LOG_INFO, "DIOCCHANGERULE %m");

	switch(r->action) {
	case PF_SCRUB:
	case PF_DROP:
	case PF_PASS:
	default:
		pcr.action = Rule_Action;
		break;
	case PF_NAT:
	case PF_NONAT:
		pcr.action = Nat_Action;
		break;
	case PF_RDR:
	case PF_NORDR:
		pcr.action = Rdr_Action;
		break;
	case PF_BINAT:
	case PF_NOBINAT:
		/* binat is not supported */
		return (0);
		break;
	}

	if (Delete_Rules) {
		pcr.rule.action = r->action;
		if (ioctl(pf->dev, DIOCGETRULES, &pcr))
			syslog(LOG_INFO, "DIOCGETRULES %m");
		mnr = pcr.nr;
		for (nr = 0; nr < mnr; ++nr) {
			if (pcr.action == PF_CHANGE_ADD_HEAD)
				pcr.nr = nr;
			else
				pcr.nr = (mnr - 1) - nr;
			if (ioctl(dev, DIOCGETRULE, &pcr)) {
				syslog(LOG_INFO, "DIOCGETRULE %m");
				return (-1);
			}
			if (! pfctl_compare_rules(&pcr.rule, r)) {
				match++;
				break;
			}
		}
		if (match && ((pf->opts & PF_OPT_NOACTION) == 0)) {
			pcr.action = PF_CHANGE_REMOVE;
			if (ioctl(pf->dev, DIOCCHANGERULE, &pcr))
				syslog(LOG_INFO, "DIOCCHANGERULE %m");
		}
	} else {
		memcpy(&pcr.rule, r, sizeof(pcr.rule));
		if (pfctl_add_pool(pf, &r->rpool, r->af))
			return (1);
		pcr.pool_ticket = pf->paddr.ticket;
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			if (ioctl(pf->dev, DIOCCHANGERULE, &pcr))
				syslog(LOG_INFO, "DIOCCHANGERULE %m");
		}
	}

	return (0);
}

int
pfctl_set_timeout(struct pfctl *pf, const char *opt, int seconds)
{
	fprintf(stderr, "set timeout not supported in authpf\n");
	return (1);
}

int
pfctl_set_optimization(struct pfctl *pf, const char *opt)
{
	fprintf(stderr, "set optimization not supported in authpf\n");
	return (1);
}

int
pfctl_set_limit(struct pfctl *pf, const char *opt, unsigned int limit)
{
	fprintf(stderr, "set limit not supported in authpf\n");
	return (1);
}

int
pfctl_set_logif(struct pfctl *pf, char *ifname)
{
	fprintf(stderr, "set loginterface not supported in authpf\n");
	return (1);
}

int
pfctl_add_altq(struct pfctl *pf, struct pf_altq *a)
{
	fprintf(stderr, "altq and queue not supported in authpf\n");
	return (1);
}
