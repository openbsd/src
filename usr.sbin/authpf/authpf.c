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
 *
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
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>
#include <resolv.h>

#include <pfctl_parser.h>

#include "pathnames.h"

int Rule_Action = PF_CHANGE_ADD_TAIL;
int Nat_Action = PF_CHANGE_ADD_HEAD;
int Rdr_Action = PF_CHANGE_ADD_HEAD;
int dev;			/* pf device */
int Delete_Rules;		/* for parse_rules callbacks */

char *infile;			/* infile name needed by parse_[rules|nat] */
char luser[MAXLOGNAME] = "";	/* username */
char ipsrc[256] = "";		/* ip as a string */
char pidfile[MAXPATHLEN];	/* we save pid in this file. */
char userfile[MAXPATHLEN];	/* we save username in this file */
char configfile[] = PATH_CONFFILE;
char allowfile[] = PATH_ALLOWFILE;

struct timeval Tstart, Tend;		/* start and end times of session */
static volatile sig_atomic_t hasta_la_vista;

int	pfctl_add_rule(struct pfctl *, struct pf_rule *);
int	pfctl_add_nat(struct pfctl *, struct pf_nat *);
int	pfctl_add_rdr(struct pfctl *, struct pf_rdr *);
int	pfctl_add_binat(struct pfctl *, struct pf_binat *);

static void	read_config(void);
static void	print_message(char *);
static int	allowed_luser(char *);
static int	check_luser(char *, char *);
static int	changefilter(int, char *, char *);
static void	authpf_kill_states(void);
static void	terminator(int s);
static __dead void	go_away(void);

/*
 * authpf:
 * User shell for authenticating gateways. sole purpose is to allow
 * a user to ssh to a gateway, and have the gateway modify packet
 * filters to allow access, then remove access when the user finishes
 * up. Meant to be used only from ssh(1) connections.
 */
int
main(int argc, char *argv[])
{
	int pidfd, ufd, namelen;
	int lockcnt = 0;
	char *foo, *cp;
	FILE *fp = NULL;
	struct sockaddr *namep;
	struct sockaddr_in peer;
	char bannedir[] = PATH_BAN_DIR;

	namep = (struct sockaddr *)&peer;
	namelen = sizeof(peer);

	read_config();

	memset(namep, 0, namelen);

	if ((foo = getenv("LOGNAME")) != NULL)
		strlcpy(luser, foo, sizeof(luser));
	else if ((foo = getenv("USER")) != NULL)
		strlcpy(luser, foo, sizeof(luser));
	else {
		syslog(LOG_ERR, "No user given!");
		exit(1);
	}

	if ((foo = getenv("SSH_CLIENT")) != NULL) {
		strlcpy(ipsrc, foo, sizeof(ipsrc));
		cp = ipsrc;
		while (*cp != '\0') {
			if (*cp == ' ')
				*cp = '\0';
			else
				cp++;
		}
	} else {
		syslog(LOG_ERR, "Can't determine connection source");
		exit(1);
	}
	if (!check_luser(bannedir, luser) || !allowed_luser(luser)) {
		/* give the luser time to read our nastygram on the screen */
		sleep(180);
		exit(1);
	}

	/*
	 * make ourselves an entry in /var/run as /var/run/authpf-ipaddr,
	 * so that things may find us easily to kill us if necessary.
	 */

	if (snprintf(pidfile, sizeof pidfile, "%s-%s", PATH_PIDFILE, ipsrc) >=
	    sizeof pidfile) {
		fprintf(stderr, "Sorry, host too long for me to handle..\n");
		syslog(LOG_ERR, "snprintf pidfile bogosity  - exiting");
		goto dogdeath;
	}

	if ((pidfd = open(pidfile, O_RDWR|O_CREAT, 0644)) == -1 ||
	    (fp = fdopen(pidfd, "r+")) == NULL) {
		syslog(LOG_ERR, "can't open or create %s: %s",
		    pidfile, strerror(errno));
		goto dogdeath;
	}

	/*
	 * If someone else is already using this ip, then this person
	 * wants to switch users - so kill the old process and we
	 * exit as well. Of course, this will only work if we are
	 * running with priviledge.
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

	while (flock(pidfd, LOCK_EX|LOCK_NB) == -1 && errno != EBADF) {
		int otherpid = -1;
		int save_errno = errno;

		lockcnt++;
		fscanf(fp, "%d", &otherpid);
		syslog (LOG_DEBUG, "Tried to lock %s, in use by pid %d: %s",
		    pidfile, otherpid, strerror(save_errno));
		fclose(fp);

		close(pidfd);
		if (otherpid > 0) {
			syslog(LOG_INFO,
			    "killing prior auth (pid %d) of %s by user %s",
			    otherpid,  ipsrc, luser);
			if (kill((pid_t) otherpid, SIGTERM) == -1) {
				syslog (LOG_INFO,
				    "Couldn't kill process %d: (%m)",
				    otherpid);
			}
		}

		/* we try to kill the previous process and aquire the lock
		 * for 10 seconds, trying once a second. if we can't after
		 * 10 attempts we log an error and give up
		 */

		if (lockcnt > 10) {
			syslog(LOG_ERR, "Can't kill previous authpf (pid %d)",
			    otherpid);
			goto dogdeath;
		}
		sleep(1);
	}

	fp = fopen(pidfile, "w+");
	rewind(fp);
	fprintf(fp, "%d\n", getpid());
	fflush(fp);
	(void) ftruncate(fileno(fp), ftell(fp));

	/* open the pf device */

	dev = open (PATH_DEVFILE, O_RDWR);
	if (dev == -1) {
		syslog(LOG_ERR, "Can't open filter device (%m)");
		goto dogdeath;
	}

	/*
	 * make an entry in file /var/authpf/ipaddr, containing the username.
	 * this lets external applications check for authentication by looking
	 * for the ipaddress in that directory, and retrieving the username
	 * from it.
	 */

	snprintf(userfile, sizeof(userfile), "%s/%s", PATH_USERFILE, ipsrc);
	if ((ufd = open(userfile, O_CREAT|O_WRONLY, 0640)) == -1) {
		syslog(LOG_ERR, "Can't open \"%s\" ! (%m)", userfile);
		goto dogdeath;
	}

	write(ufd, luser, strlen(luser));
	write(ufd, "\n", 1);
	close(ufd);

	if (changefilter(1, luser, ipsrc) == -1) {
		/* XXX */
	}

	signal(SIGTERM, terminator);
	signal(SIGINT, terminator);
	signal(SIGALRM, terminator);
	signal(SIGPIPE, terminator);
	signal(SIGHUP, terminator);
	signal(SIGSTOP, terminator);
	signal(SIGTSTP, terminator);
	while(1) {
		printf("\r\nHello %s, ", luser);
		printf("You are authenticated from host \"%s\"\r\n", ipsrc);
		print_message(PATH_MESSAGE);
		while (1) {
			sleep(10);
			if (hasta_la_vista)
				go_away();
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
	if (pidfile[0] != '\0')
		unlink(pidfile); /* fail silently */
	if (userfile[0] != '\0')
		unlink(userfile); /* fail silently */
	exit(1);
}

/* read_config:
 * reads config file in PATH_CONFILE to set optional behaviours up
 */

static void
read_config(void)
{
	char buf[1024];
	int i = 0;
	FILE *f;

	f = fopen(configfile, "r");
	if (f == NULL) 
		exit(1); /* exit silently if we have no config file */

	openlog("authpf", LOG_PID | LOG_NDELAY, LOG_DAEMON);
	
	do {
		char **ap, *pair[4], *cp, *tp;
		int len;

		if (fgets(buf, sizeof(buf), f) == NULL) {
			fclose(f);
			return;
		}
		i++;
		len = strlen(buf);
		if (buf[len - 1] != '\n' && !feof(f)) {
			syslog(LOG_ERR, "line %d too long in %s", i,
			    configfile);
			exit(1);
		}
		buf[len - 1] = '\0';

		for (cp = buf; *cp == ' ' || *cp == '\t'; cp++)
			;

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
	return;
 parse_error:
	fclose(f);
	syslog(LOG_ERR, "parse error, line %d of %s", i, configfile);
	exit(1);
}


/*
 * print_message:
 * splatter a file to stdout - max line length of 1024,
 * used for spitting message files at users to tell them
 * they've been bad or we're unavailable.
 */
static void
print_message(char *filename)
{
	char buf[1024];
	FILE *f;

	if ((f = fopen(filename, "r")) == NULL)
		return; /* fail silently, we don't care if it isn't there */

	do {
		if (fgets(buf, sizeof(buf), f) == NULL) {
			fflush(stdout);
			fclose(f);
			return;
		}
	} while (fputs(buf, stdout) != EOF && !feof(f));
	fclose(f);
}

/*
 * allowed_luser:
 * allowed_luser checks to see if user "luser" is allowed to
 * use this gateway by virtue of being listed in an allowed
 * users file, namely /etc/authpf.allow .
 *
 * If /etc/authpf.allow does not exist, then we assume that
 * all users who are allowed in by sshd(8) are permitted to
 * use this gateway. If /etc/authpf.allow does exist, then a
 * user must be listed if the connection is to continue, else
 * the session terminates in the same manner as being banned.
 *
 */
static int
allowed_luser(char *luser)
{
	char *buf, *lbuf;
	size_t len;
	FILE *f;

	if ((f = fopen(allowfile, "r")) == NULL) {
		if (errno == ENOENT) {
			/*
			 * allowfile doesn't exist, this this gateway
			 * isn't restricted to certain users...
			 */
			return(1);
		}

		/*
		 * luser may in fact be allowed, but we can't open
		 * the file even though it's there. probably a config
		 * problem.
		 */
		syslog(LOG_ERR, "Can't open allowed users file %s (%s)",
		    allowfile, strerror(errno));
		return(0);
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

			if (strcmp(luser, buf) == 0 || strcmp("*", buf) == 0)
				return(1); /* matched an allowed username */

			if (lbuf != NULL) {
				free(lbuf);
				lbuf = NULL;
			}
		}
		syslog(LOG_INFO, "Denied access to %s: not listed in %s",
		    luser, allowfile);

		/* reuse buf */
		buf = "\n\nSorry, you aren't allowed to use this facility!\n";
		fputs(buf, stdout);
	}
	fflush(stdout);
	return(0);
}

/*
 * check_luser:
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
	char tmp[1024];
	FILE *f;

	if (snprintf(tmp, sizeof(tmp), "%s/%s", luserdir, luser) >=
	    sizeof(tmp)) {
		syslog(LOG_ERR, "Provided banned directory line too long (%s)",
		    luserdir);
		return(0);
	}
	if ((f = fopen(tmp, "r")) == NULL) {
		if (errno == ENOENT) {
			/*
			 * file or dir doesn't exist, so therefore
			 * this luser isn't banned..  all is well
			 */
			return(1);
		} else {
			/*
			 * luser may in fact be banned, but we can't open the
			 * file even though it's there. probably a config
			 * problem.
			 */
			syslog (LOG_ERR, "Can't open banned file %s (%s)",
			    tmp, strerror(errno));
			return(0);
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
		while((fputs(tmp, stdout) != EOF) && !feof(f)) {
			if (fgets(tmp, sizeof(tmp), f) == NULL) {
				fflush(stdout);
				return(0);
			}
		}
	}
	fflush(stdout);
	return(0);
}


/*
 * changefilter:
 * Add/remove filter entries for user "luser" from ip "ipsrc"
 */
static int
changefilter(int add, char *luser, char *ipsrc)
{
	char rulesfile[MAXPATHLEN], natfile[MAXPATHLEN], buf[1024];
	char template[] = "/tmp/authpfrules.XXXXXXX";
	char template2[] = "/tmp/authpfnat.XXXXXXX";
	int tmpfile = -1, from_fd = -1, ret = -1;
	struct pfioc_nat	pn;
	struct pfioc_binat	pb;
	struct pfioc_rdr	pd;
	struct pfioc_rule	pr;
	struct pfctl		pf;
	int rcount, wcount;
	FILE *fin = NULL;
	char *cp;

	memset (&pf, 0, sizeof(pf));
	memset (&pr, 0, sizeof(pr));

	syslog (LOG_DEBUG, "%s filter for ip=%s, user %s",
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

	if ((cp = getenv("HOME")) == NULL) {
		syslog(LOG_ERR, "No Home Directory!");
		goto error;
	}
	if (snprintf(rulesfile, sizeof rulesfile, "%s/.authpf/authpf.rules",
	    cp) >= sizeof rulesfile) {
		syslog(LOG_ERR, "homedir path too long, exiting");
		goto error;
	}
	if ((from_fd = open(rulesfile, O_RDONLY, 0)) == -1) {
		/* if home dir rules do not exist, we try PATH_PFRULES */
		if (errno != ENOENT) {
			syslog(LOG_ERR, "can't open %s (%m)", rulesfile);
			if (unlink(template) == -1)
				syslog(LOG_ERR, "can't unlink %s", template);
			goto error;
		}
	}
	snprintf(rulesfile, sizeof rulesfile, PATH_PFRULES);
	if (from_fd == -1 &&
	    (from_fd = open(rulesfile, O_RDONLY, 0)) == -1) {
		syslog(LOG_ERR, "can't open %s (%m)", rulesfile);
		if (unlink(template) == -1)
			syslog(LOG_ERR, "can't unlink %s", template);
		goto error;
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
	memset(&pf, 0, sizeof(pf));
	pf.dev = dev;
	pf.prule = &pr;
	if (parse_rules(fin, &pf) < 0) {
		syslog(LOG_ERR,
		    "syntax error in rule file: authpf rules not loaded");
		goto error;
	}

	/* now, for NAT, if we have some */

	if ((cp = getenv("HOME")) == NULL) {
		syslog(LOG_ERR, "No Home Directory!");
		goto error;
	}
	if (snprintf(natfile, sizeof natfile, "%s/.authpf/authpf.nat", cp) >=
	    sizeof natfile) {
		syslog(LOG_ERR, "homedir path too long, exiting");
		goto error;
	}
	if ((from_fd = open(natfile, O_RDONLY, 0)) == -1) {
		/* if it doesn't exist, we try /etc */
		if (errno != ENOENT) {
			syslog(LOG_ERR, "can't open %s (%m)", natfile);
			if (unlink(template) == -1)
				syslog(LOG_ERR, "can't unlink %s", template);
			goto error;
		}
	}
	snprintf(natfile, sizeof natfile, PATH_NATRULES);
	if (from_fd == -1 &&
	    (from_fd = open(natfile, O_RDONLY, 0)) == -1) {
		if (errno == ENOENT)
			goto out; /* NAT is optional */
		else {
			syslog(LOG_ERR, "can't open %s (%m)", natfile);
			if (unlink(template) == -1)
				syslog(LOG_ERR, "can't unlink %s", template);
			goto error;
		}
	}
	tmpfile = mkstemp(template2);
	if (tmpfile == -1) {
		syslog(LOG_ERR, "Can't open temp file %s (%m)",
		    template2);
		goto error;
	}

	fin = fdopen(tmpfile, "r+");
	if (fin == NULL) {
		syslog(LOG_ERR, "Can't open %s (%m)", template2);
		goto error;
	}

	/* write the variable to the start of the file */
	fprintf(fin, "user_ip = \"%s\"\n", ipsrc);
	fflush(fin);

	while ((rcount = read(from_fd, buf, sizeof(buf))) > 0) {
		wcount = write(tmpfile, buf, rcount);
		if (rcount != wcount || wcount == -1) {
			syslog(LOG_INFO, "nat copy failed");
			goto error;
		}
	}

	if (rcount == -1) {
		syslog(LOG_INFO, "read for nat copy failed");
		goto error;
	}

	fclose(fin);
	fin = NULL;
	close(tmpfile);
	tmpfile = -1;
	close(from_fd);
	from_fd = -1;

	fin = fopen(template2, "r");

	if (fin == NULL) {
		syslog(LOG_INFO, "can't open %s (%m)", template2);
		goto error;
	}

	infile = template;

	if (unlink(template2) == -1) {
		syslog(LOG_INFO, "can't unlink %s (%m)", template2);
		goto error;
	}
	/* add/delete rules, using parse_nat */

	memset(&pf, 0, sizeof(pf));
	pf.dev = dev;
	pf.pnat = &pn;
	pf.pbinat = &pb;
	pf.prdr = &pd;
	if (parse_nat(fin, &pf) < 0) {
		syslog(LOG_INFO,
		    "syntax error in nat file: nat rules not loaded");
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
		syslog (LOG_INFO, "Allowing %s, user %s", ipsrc, luser);
	} else {
		(void)gettimeofday(&Tend, NULL);
		syslog (LOG_INFO, "Removed %s, user %s - duration %ld seconds",
		    ipsrc, luser, Tend.tv_sec - Tstart.tv_sec);
	}
	return(ret);
}

/*
 * authpf_kill_states:
 * This is to kill off states that would otherwide be left behind stateful
 * rules. This means we don't need to allow in more traffic than we really
 * want to, since we don't have to worry about any luser sessions lasting
 * longer than their ssh session. This function is based on
 * pfctl_kill_states from pfctl.
 */
static void
authpf_kill_states()
{
	struct pfioc_state_kill psk;
	struct in_addr target, temp;

	memset(&psk, 0, sizeof(psk));
	memset(&psk.psk_src.mask, 0xff, sizeof(psk.psk_src.mask));
	memset(&target, 0xff, sizeof(target));
	memset(&temp, 0xff, sizeof(temp));

	inet_pton(AF_INET, "255.255.255.255", &temp);
	inet_pton(AF_INET, ipsrc, &target);

	psk.psk_src.addr.v4 = target;
	psk.psk_dst.addr.v4 = temp;
	if (ioctl(dev, DIOCKILLSTATES, &psk))
		syslog(LOG_ERR, "DIOCKILLSTATES failed (%m)");

	psk.psk_dst.addr.v4 = target;
	psk.psk_src.addr.v4 = temp;
	if (ioctl(dev, DIOCKILLSTATES, &psk))
		syslog(LOG_ERR, "DIOCKILLSTATES failed (%m)");
}

/* signal handler that makes us go away properly */
static void
terminator(int s)
{
	hasta_la_vista = 1;
}

/*
 * go_away:
 * function that removes our stuff when we go away.
 */
static __dead void
go_away(void)
{
	int ret = 0;

	changefilter(0, luser, ipsrc);
	authpf_kill_states();
	if (unlink(pidfile) != 0) {
		syslog(LOG_ERR, "Couldn't unlink %s! (%m)", pidfile);
		ret = 1;
	}
	if (unlink(userfile) != 0) {
		syslog(LOG_ERR, "Couldn't unlink %s! (%m)", userfile);
		ret = 1;
	}
	exit(ret);
}

/*
 * pfctl_add_rules:
 * callback for rule add, used by parser in parse_rules
 */
int
pfctl_add_rule(struct pfctl *pf, struct pf_rule *r)
{
	struct pfioc_changerule pcr;

	memset(&pcr, 0, sizeof(pcr));
	if (Delete_Rules) {
		pcr.action = PF_CHANGE_REMOVE;
		memcpy(&pcr.oldrule, r, sizeof(pcr.oldrule));
	} else {
		pcr.action = Rule_Action;
		memcpy(&pcr.newrule, r, sizeof(pcr.newrule));
	}
	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(pf->dev, DIOCCHANGERULE, &pcr))
			syslog(LOG_INFO, "DIOCCHANGERULE %m");
	}

	return 0;
}

/*
 * pfctl_add_nat:
 * callback for nat add, used by parser in parse_nat
 */
int
pfctl_add_nat(struct pfctl *pf, struct pf_nat *n)
{
	struct pfioc_changenat pcr;

	memset(&pcr, 0, sizeof(pcr));
	if (Delete_Rules) {
		pcr.action = PF_CHANGE_REMOVE;
		memcpy(&pcr.oldnat, n, sizeof(pcr.oldnat));
	} else {
		pcr.action = Nat_Action;
		memcpy(&pcr.newnat, n, sizeof(pcr.newnat));
	}
	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(pf->dev, DIOCCHANGENAT, &pcr))
			syslog(LOG_INFO, "DIOCCHANGENAT %m");
	}
	return 0;
}

/*
 * pfctl_add_rdr:
 * callback for rdr add, used by parser in parse_nat
 */
int
pfctl_add_rdr(struct pfctl *pf, struct pf_rdr *r)
{
	struct pfioc_changerdr pcr;

	memset(&pcr, 0, sizeof(pcr));
	if (Delete_Rules) {
		pcr.action = PF_CHANGE_REMOVE;
		memcpy(&pcr.oldrdr, r, sizeof(pcr.oldrdr));
	} else {
		pcr.action = Rdr_Action;
		memcpy(&pcr.newrdr, r, sizeof(pcr.newrdr));
	}
	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(pf->dev, DIOCCHANGERDR, &pcr))
			syslog(LOG_INFO, "DIOCCHANGERDR %m");
	}
	return 0;
}

/*
 * pfctl_add_binat:
 * We don't support adding binat's, since pf doesn't,
 * and I can't for the life of me think of a sane situation where it
 * might be useful.  This is here only because the pfctl parse
 * routines need this defined.
 */
int
pfctl_add_binat(struct pfctl *pf, struct pf_binat *b)
{
	return 0;
}
