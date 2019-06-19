/*	$OpenBSD: apmd.c,v 1.84 2018/12/04 18:00:57 tedu Exp $	*/

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

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <err.h>
#include <limits.h>
#include <machine/apmvar.h>

#include "pathnames.h"
#include "apm-proto.h"

#define AUTO_SUSPEND 1
#define AUTO_HIBERNATE 2

const char apmdev[] = _PATH_APM_CTLDEV;
const char sockfile[] = _PATH_APM_SOCKET;

int debug = 0;

int doperf = PERF_NONE;

extern char *__progname;

void usage(void);
int power_status(int fd, int force, struct apm_power_info *pinfo);
int bind_socket(const char *sn);
enum apm_state handle_client(int sock_fd, int ctl_fd);
int  get_avg_idle_mp(int ncpu);
int  get_avg_idle_up(void);
void perf_status(struct apm_power_info *pinfo, int ncpu);
void suspend(int ctl_fd);
void stand_by(int ctl_fd);
void hibernate(int ctl_fd);
void resumed(int ctl_fd);
void setperfpolicy(char *policy);
void sigexit(int signo);
void do_etc_file(const char *file);
void sockunlink(void);
void error(const char *fmt, const char *arg);
void set_driver_messages(int fd, int mode);

/* ARGSUSED */
void
sigexit(int signo)
{
	sockunlink();
	_exit(1);
}

void
logmsg(int prio, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	if (debug) {
		vfprintf(stdout, msg, ap);
		fprintf(stdout, "\n");
	} else {
		vsyslog(prio, msg, ap);
	}
	va_end(ap);
}

void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-AadHLs] [-f devname] [-S sockname] [-t seconds] "
		"[-Z percent] [-z percent]\n", __progname);
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
		logmsg(LOG_DEBUG, "can't disable driver messages, error: %s",
		    strerror(errno));
}

int
power_status(int fd, int force, struct apm_power_info *pinfo)
{
	struct apm_power_info bstate;
	static struct apm_power_info last;
	int acon = 0;

	if (fd == -1) {
		if (pinfo) {
			bstate.battery_state = 255;
			bstate.ac_state = 255;
			bstate.battery_life = 0;
			bstate.minutes_left = -1;
			*pinfo = bstate;
		}

		return 0;
	}

	if (ioctl(fd, APM_IOC_GETPOWER, &bstate) == 0) {
	/* various conditions under which we report status:  something changed
	 * enough since last report, or asked to force a print */
		if (bstate.ac_state == APM_AC_ON)
			acon = 1;
		if (force ||
		    bstate.ac_state != last.ac_state ||
		    bstate.battery_state != last.battery_state ||
		    (bstate.minutes_left && bstate.minutes_left < 15) ||
		    abs(bstate.battery_life - last.battery_life) >= 10) {
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
			if ((int)bstate.minutes_left > 0)
#endif
				logmsg(LOG_NOTICE, "battery status: %s. "
				    "external power status: %s. "
				    "estimated battery life %d%% (%u minutes)",
				    battstate(bstate.battery_state),
				    ac_state(bstate.ac_state),
				    bstate.battery_life,
				    bstate.minutes_left);
			else
				logmsg(LOG_NOTICE, "battery status: %s. "
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
		logmsg(LOG_ERR, "cannot fetch power status: %m");

	return acon;
}

char socketname[PATH_MAX];

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
	mode_t old_umask;
	int sock;

	sock = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (sock == -1)
		error("cannot create local socket", NULL);

	s_un.sun_family = AF_UNIX;
	strlcpy(s_un.sun_path, sockname, sizeof(s_un.sun_path));

	/* remove it if present, we're moving in */
	(void) remove(sockname);

	old_umask = umask(077);
	if (bind(sock, (struct sockaddr *)&s_un, sizeof(s_un)) == -1)
		error("cannot bind on APM socket", NULL);
	umask(old_umask);
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
	int cpuspeed_mib[] = {CTL_HW, HW_CPUSPEED};
	int cpuspeed = 0;
	size_t cpuspeed_sz = sizeof(cpuspeed);

	fromlen = sizeof(from);
	cli_fd = accept(sock_fd, (struct sockaddr *)&from, &fromlen);
	if (cli_fd == -1) {
		logmsg(LOG_INFO, "client accept failure: %s", strerror(errno));
		return NORMAL;
	}

	if (recv(cli_fd, &cmd, sizeof(cmd), 0) != sizeof(cmd)) {
		(void) close(cli_fd);
		logmsg(LOG_INFO, "client size botch");
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
	case HIBERNATE:
		reply.newstate = HIBERNATING;
		break;
	case SETPERF_LOW:
		doperf = PERF_MANUAL;
		reply.newstate = NORMAL;
		logmsg(LOG_NOTICE, "setting hw.perfpolicy to low");
		setperfpolicy("low");
		break;
	case SETPERF_HIGH:
		doperf = PERF_MANUAL;
		reply.newstate = NORMAL;
		logmsg(LOG_NOTICE, "setting hw.perfpolicy to high");
		setperfpolicy("high");
		break;
	case SETPERF_AUTO:
	case SETPERF_COOL:
		doperf = PERF_AUTO;
		reply.newstate = NORMAL;
		logmsg(LOG_NOTICE, "setting hw.perfpolicy to auto");
		setperfpolicy("auto");
		break;
	default:
		reply.newstate = NORMAL;
		break;
	}

	if (sysctl(cpuspeed_mib, 2, &cpuspeed, &cpuspeed_sz, NULL, 0) < 0)
		logmsg(LOG_INFO, "cannot read hw.cpuspeed");

	reply.cpuspeed = cpuspeed;
	reply.perfmode = doperf;
	reply.vno = APMD_VNO;
	if (send(cli_fd, &reply, sizeof(reply), 0) != sizeof(reply))
		logmsg(LOG_INFO, "reply to client botched");
	close(cli_fd);

	return reply.newstate;
}

void
suspend(int ctl_fd)
{
	logmsg(LOG_NOTICE, "system suspending");
	power_status(ctl_fd, 1, NULL);
	do_etc_file(_PATH_APM_ETC_SUSPEND);
	sync();
	sleep(1);
	ioctl(ctl_fd, APM_IOC_SUSPEND, 0);
}

void
stand_by(int ctl_fd)
{
	logmsg(LOG_NOTICE, "system entering standby");
	power_status(ctl_fd, 1, NULL);
	do_etc_file(_PATH_APM_ETC_STANDBY);
	sync();
	sleep(1);
	ioctl(ctl_fd, APM_IOC_STANDBY, 0);
}

void
hibernate(int ctl_fd)
{
	logmsg(LOG_NOTICE, "system hibernating");
	power_status(ctl_fd, 1, NULL);
	do_etc_file(_PATH_APM_ETC_HIBERNATE);
	sync();
	sleep(1);
	ioctl(ctl_fd, APM_IOC_HIBERNATE, 0);
}

void
resumed(int ctl_fd)
{
	do_etc_file(_PATH_APM_ETC_RESUME);
	logmsg(LOG_NOTICE, "system resumed from sleep");
	power_status(ctl_fd, 1, NULL);
}

#define TIMO (10*60)			/* 10 minutes */

int
main(int argc, char *argv[])
{
	const char *fname = apmdev;
	int ctl_fd, sock_fd, ch, suspends, standbys, hibernates, resumes;
	int autoaction = 0;
	int autolimit = 0;
	int statonly = 0;
	int powerstatus = 0, powerbak = 0, powerchange = 0;
	int noacsleep = 0;
	struct timespec ts = {TIMO, 0}, sts = {0, 0};
	struct apm_power_info pinfo;
	time_t apmtimeout = 0;
	const char *sockname = sockfile;
	const char *errstr;
	int kq, nchanges;
	struct kevent ev[2];
	int ncpu_mib[2] = { CTL_HW, HW_NCPU };
	int ncpu;
	size_t ncpu_sz = sizeof(ncpu);

	while ((ch = getopt(argc, argv, "aACdHLsf:t:S:z:Z:")) != -1)
		switch(ch) {
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
			ts.tv_sec = strtonum(optarg, 1, LLONG_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "number of seconds is %s: %s", errstr,
				    optarg);
			break;
		case 's':	/* status only */
			statonly = 1;
			break;
		case 'A':
		case 'C':
			if (doperf != PERF_NONE)
				usage();
			doperf = PERF_AUTO;
			setperfpolicy("auto");
			break;
		case 'L':
			if (doperf != PERF_NONE)
				usage();
			doperf = PERF_MANUAL;
			setperfpolicy("low");
			break;
		case 'H':
			if (doperf != PERF_NONE)
				usage();
			doperf = PERF_MANUAL;
			setperfpolicy("high");
			break;
		case 'Z':
			autoaction = AUTO_HIBERNATE;
			autolimit = strtonum(optarg, 1, 100, &errstr);
			if (errstr != NULL)
				errx(1, "battery percentage is %s: %s", errstr,
				    optarg);
			break;
		case 'z':
			autoaction = AUTO_SUSPEND;
			autolimit = strtonum(optarg, 1, 100, &errstr);
			if (errstr != NULL)
				errx(1, "battery percentage is %s: %s", errstr,
				    optarg);
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if (doperf == PERF_NONE)
		doperf = PERF_MANUAL;

	if (debug == 0) {
		if (daemon(0, 0) < 0)
			error("failed to daemonize", NULL);
		openlog(__progname, LOG_CONS, LOG_DAEMON);
		setlogmask(LOG_UPTO(LOG_NOTICE));
	}

	(void) signal(SIGTERM, sigexit);
	(void) signal(SIGHUP, sigexit);
	(void) signal(SIGINT, sigexit);

	if ((ctl_fd = open(fname, O_RDWR | O_CLOEXEC)) == -1) {
		if (errno != ENXIO && errno != ENOENT)
			error("cannot open device file `%s'", fname);
	}

	sock_fd = bind_socket(sockname);

	power_status(ctl_fd, 1, &pinfo);

	if (statonly)
		exit(0);

	set_driver_messages(ctl_fd, APM_PRINT_OFF);

	kq = kqueue();
	if (kq <= 0)
		error("kqueue", NULL);

	EV_SET(&ev[0], sock_fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR,
	    0, 0, NULL);
	if (ctl_fd == -1)
		nchanges = 1;
	else {
		EV_SET(&ev[1], ctl_fd, EVFILT_READ, EV_ADD | EV_ENABLE |
		    EV_CLEAR, 0, 0, NULL);
		nchanges = 2;
	}
	if (kevent(kq, ev, nchanges, NULL, 0, &sts) < 0)
		error("kevent", NULL);

	if (sysctl(ncpu_mib, 2, &ncpu, &ncpu_sz, NULL, 0) < 0)
		error("cannot read hw.ncpu", NULL);

	for (;;) {
		int rv;

		sts = ts;

		apmtimeout += 1;
		if ((rv = kevent(kq, NULL, 0, ev, 1, &sts)) < 0)
			break;

		if (apmtimeout >= ts.tv_sec) {
			apmtimeout = 0;

			/* wakeup for timeout: take status */
			powerbak = power_status(ctl_fd, 0, &pinfo);
			if (powerstatus != powerbak) {
				powerstatus = powerbak;
				powerchange = 1;
			}

			if (!powerstatus && autoaction &&
			    autolimit > (int)pinfo.battery_life) {
				logmsg(LOG_NOTICE,
				    "estimated battery life %d%%, "
				    "autoaction limit set to %d%% .",
				    pinfo.battery_life,
				    autolimit
				);

				if (autoaction == AUTO_SUSPEND)
					suspend(ctl_fd);
				else
					hibernate(ctl_fd);
			}
		}

		if (!rv)
			continue;

		if (ev->ident == ctl_fd) {
			suspends = standbys = hibernates = resumes = 0;
			logmsg(LOG_DEBUG, "apmevent %04x index %d",
			    (int)APM_EVENT_TYPE(ev->data),
			    (int)APM_EVENT_INDEX(ev->data));

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
			case APM_USER_HIBERNATE_REQ:
				hibernates++;
				break;
#if 0
			case APM_CANCEL:
				suspends = standbys = 0;
				break;
#endif
			case APM_NORMAL_RESUME:
			case APM_CRIT_RESUME:
			case APM_SYS_STANDBY_RESUME:
				powerbak = power_status(ctl_fd, 0, &pinfo);
				if (powerstatus != powerbak) {
					powerstatus = powerbak;
					powerchange = 1;
				}
				resumes++;
				break;
			case APM_POWER_CHANGE:
				powerbak = power_status(ctl_fd, 0, &pinfo);
				if (powerstatus != powerbak) {
					powerstatus = powerbak;
					powerchange = 1;
				}
				break;
			default:
				;
			}

			if ((standbys || suspends) && noacsleep &&
			    power_status(ctl_fd, 0, &pinfo))
				logmsg(LOG_DEBUG, "no! sleep! till brooklyn!");
			else if (suspends)
				suspend(ctl_fd);
			else if (standbys)
				stand_by(ctl_fd);
			else if (hibernates)
				hibernate(ctl_fd);
			else if (resumes) {
				resumed(ctl_fd);
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
			case HIBERNATING:
				hibernate(ctl_fd);
				break;
			}
	}
	error("kevent loop", NULL);

	return 1;
}

void
setperfpolicy(char *policy)
{
	int hw_perfpol_mib[] = { CTL_HW, HW_PERFPOLICY };
	char oldpolicy[32];
	size_t oldsz = sizeof(oldpolicy);
	int setlo = 0;

	if (strcmp(policy, "low") == 0) {
		policy = "manual";
		setlo = 1;
	}

	if (sysctl(hw_perfpol_mib, 2, oldpolicy, &oldsz, policy, strlen(policy) + 1) < 0)
		logmsg(LOG_INFO, "cannot set hw.perfpolicy");

	if (setlo == 1) {
		int hw_perf_mib[] = {CTL_HW, HW_SETPERF};
		int perf;
		int new_perf = 0;
		size_t perf_sz = sizeof(perf);
		if (sysctl(hw_perf_mib, 2, &perf, &perf_sz, &new_perf, perf_sz) < 0)
			logmsg(LOG_INFO, "cannot set hw.setperf");
	}
}

void
do_etc_file(const char *file)
{
	pid_t pid;
	int status;
	const char *prog;

	/* If file doesn't exist, do nothing. */
	if (access(file, X_OK|R_OK)) {
		logmsg(LOG_DEBUG, "do_etc_file(): cannot access file %s", file);
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
		logmsg(LOG_ERR, "failed to fork(): %s", strerror(errno));
		return;
	case 0:
		/* We are the child. */
		execl(file, prog, (char *)NULL);
		logmsg(LOG_ERR, "failed to exec %s: %m", file, strerror(errno));
		_exit(1);
		/* NOTREACHED */
	default:
		/* We are the parent. */
		wait4(pid, &status, 0, 0);
		if (WIFEXITED(status))
			logmsg(LOG_DEBUG, "%s exited with status %d", file,
			    WEXITSTATUS(status));
		else
			logmsg(LOG_ERR, "%s exited abnormally.", file);
	}
}
