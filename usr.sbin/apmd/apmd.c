/*	$OpenBSD: apmd.c,v 1.30 2004/05/21 19:00:05 deraadt Exp $	*/

/*
 *  Copyright (c) 1995, 1996 John T. Kohl
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/event.h>
#include <sys/time.h>
#include <stdio.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <err.h>
#include <machine/apmvar.h>
#include "pathnames.h"
#include "apm-proto.h"

#define TRUE 1
#define FALSE 0

const char apmdev[] = _PATH_APM_CTLDEV;
const char sockfile[] = _PATH_APM_SOCKET;

int debug = 0;

extern char *__progname;

void usage(void);
int power_status(int fd, int force, struct apm_power_info *pinfo);
int bind_socket(const char *sn);
enum apm_state handle_client(int sock_fd, int ctl_fd);
void suspend(int ctl_fd);
void stand_by(int ctl_fd);
void sigexit(int signo);
void make_noise(int howmany);
void do_etc_file(const char *file);
void sockunlink(void);

void
sigexit(int signo)
{
	sockunlink();
	_exit(1);
}

void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-adempqs] [-f devfile] [-S sockfile] [-t timo]\n",
	    __progname);
	exit(1);
}

void
error(const char *fmt, const char *arg)
{
	char buf[128];

	if (debug)
		err(1, fmt, arg);
	else {
		strlcpy(buf, fmt, sizeof(buf));
		strlcat(buf, ": %m", sizeof(buf));
		syslog(LOG_ERR, buf, arg);
		exit(1);
	}
}


/*
 * tell the driver if it should display messages or not.
 */
void
set_driver_messages(int fd, int mode)
{
	if (ioctl(fd, APM_IOC_PRN_CTL, &mode) == -1)
		syslog(LOG_DEBUG, "can't disable driver messages, error: %m");
}

int
power_status(int fd, int force, struct apm_power_info *pinfo)
{
	struct apm_power_info bstate;
	static struct apm_power_info last;
	int acon = 0;

	if (ioctl(fd, APM_IOC_GETPOWER, &bstate) == 0) {
	/* various conditions under which we report status:  something changed
	 * enough since last report, or asked to force a print */
		if (bstate.ac_state == APM_AC_ON)
			acon = 1;
		if (force ||
		    bstate.ac_state != last.ac_state ||
		    bstate.battery_state != last.battery_state ||
		    (bstate.minutes_left && bstate.minutes_left < 15) ||
		    abs(bstate.battery_life - last.battery_life) > 20) {
#ifdef __powerpc__
			/*
			 * When the battery is charging, the estimated life
			 * time is in fact the estimated remaining charge time
			 * on Apple machines, so lie in the stats.
			 * We still want an useful message if the battery or
			 * ac status changes, however.
			 */
			if (bstate.minutes_left != 0 &&
			    bstate.battery_state != APM_BATT_CHARGING)
#else
			if (bstate.minutes_left)
#endif
				syslog(LOG_NOTICE, "battery status: %s. "
				    "external power status: %s. "
				    "estimated battery life %d%% (%u minutes)",
				    battstate(bstate.battery_state),
				    ac_state(bstate.ac_state),
				    bstate.battery_life,
				    bstate.minutes_left);
			else
				syslog(LOG_NOTICE, "battery status: %s. "
				    "external power status: %s. "
				    "estimated battery life %d%%",
				    battstate(bstate.battery_state),
				    ac_state(bstate.ac_state),
				    bstate.battery_life);
			last = bstate;
		}
		if (pinfo)
			*pinfo = bstate;
	} else
		syslog(LOG_ERR, "cannot fetch power status: %m");

	return acon;
}

char socketname[MAXPATHLEN];

void
sockunlink(void)
{
	if (socketname[0])
		remove(socketname);
}

int
bind_socket(const char *sockname)
{
	struct sockaddr_un s_un;
	int sock;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1)
		error("cannot create local socket", NULL);

	s_un.sun_family = AF_UNIX;
	strncpy(s_un.sun_path, sockname, sizeof(s_un.sun_path));
	s_un.sun_len = SUN_LEN(&s_un);

	/* remove it if present, we're moving in */
	(void) remove(sockname);
	umask(077);

	if (bind(sock, (struct sockaddr *)&s_un, s_un.sun_len) == -1)
		error("cannot connect to APM socket", NULL);
	if (chmod(sockname, 0660) == -1 || chown(sockname, 0, 0) == -1)
		error("cannot set socket mode/owner/group to 660/0/0", NULL);

	listen(sock, 1);
	strlcpy(socketname, sockname, sizeof socketname);
	atexit(sockunlink);

	return sock;
}

enum apm_state
handle_client(int sock_fd, int ctl_fd)
{
	/* accept a handle from the client, process it, then clean up */
	int cli_fd;
	struct sockaddr_un from;
	socklen_t fromlen;
	struct apm_command cmd;
	struct apm_reply reply;

	fromlen = sizeof(from);
	cli_fd = accept(sock_fd, (struct sockaddr *)&from, &fromlen);
	if (cli_fd == -1) {
		syslog(LOG_INFO, "client accept failure: %m");
		return NORMAL;
	}

	if (recv(cli_fd, &cmd, sizeof(cmd), 0) != sizeof(cmd)) {
		(void) close(cli_fd);
		syslog(LOG_INFO, "client size botch");
		return NORMAL;
	}

	if (cmd.vno != APMD_VNO) {
		close(cli_fd);			/* terminate client */
		/* no error message, just drop it. */
		return NORMAL;
	}

	power_status(ctl_fd, 0, &reply.batterystate);
	switch (cmd.action) {
	case SUSPEND:
		reply.newstate = SUSPENDING;
		break;
	case STANDBY:
		reply.newstate = STANDING_BY;
		break;
	default:
		reply.newstate = NORMAL;
		break;
	}

	reply.vno = APMD_VNO;
	if (send(cli_fd, &reply, sizeof(reply), 0) != sizeof(reply))
		syslog(LOG_INFO, "client reply botch");
	close(cli_fd);

	return reply.newstate;
}

int speaker_ok = TRUE;

void
make_noise(int howmany)
{
	int spkrfd = -1;
	int trycnt;

	if (!speaker_ok)		/* don't bother after sticky errors */
		return;

	for (trycnt = 0; trycnt < 3; trycnt++) {
		spkrfd = open(_PATH_DEV_SPEAKER, O_WRONLY);
		if (spkrfd == -1) {
			switch (errno) {
			case EBUSY:
				usleep(500000);
				errno = EBUSY;
				continue;
			case ENOENT:
			case ENODEV:
			case ENXIO:
			case EPERM:
			case EACCES:
				syslog(LOG_WARNING, "speaker device "
				    _PATH_DEV_SPEAKER " unavailable: %m");
				speaker_ok = FALSE;
				return;
			}
		} else
			break;
	}

	if (spkrfd == -1) {
		syslog(LOG_WARNING, "cannot open " _PATH_DEV_SPEAKER ": %m");
		return;
	}

	syslog(LOG_DEBUG, "sending %d tones to speaker", howmany);
	write(spkrfd, "o4cc", 2 + howmany);
	close(spkrfd);
}

void
suspend(int ctl_fd)
{
	do_etc_file(_PATH_APM_ETC_SUSPEND);
	sync();
	make_noise(2);
	sync();
	sync();
	sleep(1);
	ioctl(ctl_fd, APM_IOC_SUSPEND, 0);
}

void
stand_by(int ctl_fd)
{
	do_etc_file(_PATH_APM_ETC_STANDBY);
	sync();
	make_noise(1);
	sync();
	sync();
	sleep(1);
	ioctl(ctl_fd, APM_IOC_STANDBY, 0);
}

#define TIMO (1*60)			/* 10 minutes */

int
main(int argc, char *argv[])
{
	const char *fname = apmdev;
	int ctl_fd, sock_fd, ch, suspends, standbys, resumes;
	int statonly = 0;
	int enableonly = 0;
	int pctonly = 0;
	int powerstatus = 0, powerbak = 0, powerchange = 0;
	int messages = 0;
	int noacsleep = 0;
	struct timespec ts = {TIMO, 0}, sts = {0, 0};
	const char *sockname = sockfile;
	int kq;
	struct kevent ev[2];

	while ((ch = getopt(argc, argv, "qadsepmf:t:S:")) != -1)
		switch(ch) {
		case 'q':
			speaker_ok = FALSE;
			break;
		case 'a':
			noacsleep = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'f':
			fname = optarg;
			break;
		case 'S':
			sockname = optarg;
			break;
		case 't':
			ts.tv_sec = strtoul(optarg, NULL, 0);
			if (ts.tv_sec == 0)
				usage();
			break;
		case 's':	/* status only */
			statonly = 1;
			break;
		case 'e':
			enableonly = 1;
			break;
		case 'p':
			pctonly = 1;
			break;
		case 'm':
			messages = 1;
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (debug)
		openlog(__progname, LOG_CONS, LOG_LOCAL1);
	else {
		daemon(0, 0);
		openlog(__progname, LOG_CONS, LOG_DAEMON);
		setlogmask(LOG_UPTO(LOG_NOTICE));
	}

	(void) signal(SIGTERM, sigexit);
	(void) signal(SIGHUP, sigexit);
	(void) signal(SIGINT, sigexit);

	if ((ctl_fd = open(fname, O_RDWR)) == -1)
		error("cannot open device file `%s'", fname);

	if (fcntl(ctl_fd, F_SETFD, 1) == -1)
		error("cannot set close-on-exec for `%s'", fname);

	sock_fd = bind_socket(sockname);

	if (fcntl(sock_fd, F_SETFD, 1) == -1)
		error("cannot set close-on-exec for the socket", NULL);

	power_status(ctl_fd, 1, 0);

	if (statonly)
		exit(0);

	if (enableonly) {
		set_driver_messages(ctl_fd, APM_PRINT_ON);
		exit(0);
	}

	if (pctonly) {
		set_driver_messages(ctl_fd, APM_PRINT_PCT);
		exit(0);
	}

	if (!messages)
		set_driver_messages(ctl_fd, APM_PRINT_OFF);

	kq = kqueue();
	if (kq <= 0)
		error("kqueue", NULL);

	EV_SET(&ev[0], ctl_fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR,
	    0, 0, NULL);
	EV_SET(&ev[1], sock_fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR,
	    0, 0, NULL);
	if (kevent(kq, ev, 2, NULL, 0, &sts) < 0)
		error("kevent", NULL);

	for (;;) {
		int rv;

		sts = ts;

		if ((rv = kevent(kq, NULL, 0, ev, 1, &sts)) < 0)
			break;

		/* wakeup for timeout: take status */
		powerbak = power_status(ctl_fd, 0, 0);
		if (powerstatus != powerbak) {
			powerstatus = powerbak;
			powerchange = 1;
		}

		if (!rv)
			continue;

		if (ev->ident == ctl_fd) {
			suspends = standbys = resumes = 0;
			syslog(LOG_DEBUG, "apmevent %04x index %d",
			    APM_EVENT_TYPE(ev->data),
			    APM_EVENT_INDEX(ev->data));

			switch (APM_EVENT_TYPE(ev->data)) {
			case APM_SUSPEND_REQ:
			case APM_USER_SUSPEND_REQ:
			case APM_CRIT_SUSPEND_REQ:
			case APM_BATTERY_LOW:
				suspends++;
				break;
			case APM_USER_STANDBY_REQ:
			case APM_STANDBY_REQ:
				standbys++;
				break;
#if 0
			case APM_CANCEL:
				suspends = standbys = 0;
				break;
#endif
			case APM_NORMAL_RESUME:
			case APM_CRIT_RESUME:
			case APM_SYS_STANDBY_RESUME:
				powerbak = power_status(ctl_fd, 0, 0);
				if (powerstatus != powerbak) {
					powerstatus = powerbak;
					powerchange = 1;
				}
				resumes++;
				break;
			case APM_POWER_CHANGE:
				powerbak = power_status(ctl_fd, 0, 0);
				if (powerstatus != powerbak) {
					powerstatus = powerbak;
					powerchange = 1;
				}
				break;
			default:
				;
			}

			if ((standbys || suspends) && noacsleep &&
			    power_status(ctl_fd, 0, 0))
				syslog(LOG_DEBUG, "no! sleep! till brooklyn!");
			else if (suspends)
				suspend(ctl_fd);
			else if (standbys)
				stand_by(ctl_fd);
			else if (resumes) {
				do_etc_file(_PATH_APM_ETC_RESUME);
				syslog(LOG_NOTICE,
				    "system resumed from APM sleep");
			}

			if (powerchange) {
				if (powerstatus)
					do_etc_file(_PATH_APM_ETC_POWERUP);
				else
					do_etc_file(_PATH_APM_ETC_POWERDOWN);
				powerchange = 0;
			}

		} else if (ev->ident == sock_fd)
			switch (handle_client(sock_fd, ctl_fd)) {
			case NORMAL:
				break;
			case SUSPENDING:
				suspend(ctl_fd);
				break;
			case STANDING_BY:
				stand_by(ctl_fd);
				break;
			}
	}
	error("kevent loop", NULL);

	return 1;
}

void
do_etc_file(const char *file)
{
	pid_t pid;
	int status;
	const char *prog;

	/* If file doesn't exist, do nothing. */
	if (access(file, X_OK|R_OK)) {
		syslog(LOG_DEBUG, "do_etc_file(): cannot access file %s", file);
		return;
	}

	prog = strrchr(file, '/');
	if (prog)
		prog++;
	else
		prog = file;

	pid = fork();
	switch (pid) {
	case -1:
		syslog(LOG_ERR, "failed to fork(): %m");
		return;
	case 0:
		/* We are the child. */
		execl(file, prog, (char *)NULL);
		_exit(1);
		/* NOTREACHED */
	default:
		/* We are the parent. */
		wait4(pid, &status, 0, 0);
		if (WIFEXITED(status))
			syslog(LOG_DEBUG, "%s exited with status %d", file,
			    WEXITSTATUS(status));
		else
			syslog(LOG_ERR, "%s exited abnormally.", file);
	}
}
