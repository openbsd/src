/*	$NetBSD$	*/
/*
 * Copyright (c) 1995 John T. Kohl.  All rights reserved.
 * Copyright (c) 1993, 1994 Stefan Grefen.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following dipclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Stefan Grefen.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/wait.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmcia_ioctl.h>
#include "pathnames.h"

#define PCMCIABUS_UNIT(a)    (minor(a))
#define PCMCIABUS_SLOT(a)    (a&0x7)

#define HAS_POWER(a) ISSET(a,PCMCIA_POWER)

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

extern const char *__progname;

extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

const char conffile[] = _PATH_PCMCIA_CONF;

enum speaker_tones {
    SINGLE_HIGH,
    SHORT_HIGH,
    LOW_HIGH,
    HIGH_LOW,
    SINGLE_LOW
};


void make_noise __P((enum speaker_tones));
void child_death __P((int));
void usage __P((void));
void handle_fd __P((int fd));

extern int read_conf(u_char *buf,
		     int blen,
		     int cfidx,
		     struct pcmcia_conf *pc_cf);

void
usage(void)
{
	fprintf(stderr,"usage: %s [-d] [-c configfile]\n", __progname);
	exit(1);
}

dev_t devices_opened[64];		/* XXX fixed size */
int slot_status[64];			/* XXX fixed size */
 
int speaker_ok = 1;

void /* XXX */
main(int argc,
     char *argv[])
{
	int fd, ch, maxfd = 0, ready;
	int debug = 0;
	FILE *infile = NULL;
	const char *fname = conffile;
	char confline[128];
	fd_set sockets;
	fd_set selcopy;
	struct stat statb;

	while ((ch = getopt(argc, argv, "qdc:")) != -1)
		switch(ch) {
		case 'q':
			speaker_ok = 0;
			break;
		case 'd':
			debug = 1;
			break;
		case 'c':
			fname = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if ((infile = fopen(fname, "r")) == NULL) {
		(void)err(1, "cannot open config file `%s'", fname);
	} 
	if (debug) {
		openlog(__progname, LOG_CONS, LOG_LOCAL1);
	} else {
		openlog(__progname, LOG_CONS, LOG_DAEMON);
		setlogmask(LOG_UPTO(LOG_NOTICE));
		daemon(0, 0);
	}
	syslog(LOG_DEBUG, "opened config file %s", fname);
	FD_ZERO(&sockets);
	while (fgets(confline, sizeof(confline), infile) != NULL) {
		if (confline[strlen(confline)-1] == '\n')
			confline[strlen(confline)-1] = '\0';
		fd = open(confline, O_RDWR);
		if (fd != -1) {
			struct pcmcia_status stbuf;
			if (ioctl(fd, PCMCIAIO_GET_STATUS, &stbuf) < 0) {
			    syslog(LOG_ERR,"ioctl PCMCIAIO_GET_STATUS %s: %m",
				   confline);
			    close(fd);
			} else {
				FD_SET(fd, &sockets);
				syslog(LOG_DEBUG, "%s is fd %d", confline, fd);
				if (fstat(fd, &statb) == -1) {
					syslog(LOG_ERR, "cannot fstat %s: %m",
					       confline);
					(void) close(fd);
				} else {
					maxfd = MAX(fd, maxfd);
					devices_opened[fd] = statb.st_rdev;
				}
				slot_status[fd] = ISSET(stbuf.status,
							PCMCIA_CARD_PRESENT);
				if (ISSET(stbuf.status, PCMCIA_CARD_INUSE))
					make_noise(LOW_HIGH);
				else if (ISSET(stbuf.status,
					       PCMCIA_CARD_IS_MAPPED))
				    make_noise(SINGLE_LOW);
				else if (ISSET(stbuf.status,
					       PCMCIA_CARD_PRESENT))
				    handle_fd(fd);

			}
		} else
			syslog(LOG_DEBUG, "%s: %m", confline);
	}
	fclose(infile);

	if (maxfd == 0) {
		syslog(LOG_ERR, "no files to monitor");
		exit(1);
	}
	syslog(LOG_DEBUG, "maxfd = %d", maxfd);

	signal(SIGCHLD, child_death);
	while (1) {
	    for (selcopy = sockets; 
		 (ready = select(maxfd+1, 0, 0, &selcopy, 0)) > 0;
		 selcopy = sockets) {
		register int i;
		syslog(LOG_DEBUG, "%d ready descriptors", ready);
		for (i = 0; ready && i <= maxfd; i++) {
		    if (FD_ISSET(i, &selcopy)) {
			syslog(LOG_DEBUG, "fd %d is exceptionally ready", i);
			/* sleep to let it settle */
			sleep(2);
			ready--;
			handle_fd(i);
		    }
		}
	    }
	    if (ready == -1) {
		if (errno != EINTR)
		    syslog(LOG_ERR, "select failed: %m");
		continue;
	    } else {
		syslog(LOG_ERR, "leaving with ready == 0?");
		break;
	    }
	}
}

void
handle_fd(int fd)
{
	struct pcmcia_status stbuf;
	struct pcmcia_info inbuf;
	struct pcmcia_conf pc_cf;
	int status;
	int pw;
	int first=1;
	char manu[MAX_CIS_NAMELEN];
	char model[MAX_CIS_NAMELEN];
	char addinf1[MAX_CIS_NAMELEN];
	char addinf2[MAX_CIS_NAMELEN];
	char cmd[64];

	if (ioctl(fd, PCMCIAIO_GET_STATUS, &stbuf) < 0) {
		syslog(LOG_ERR,"ioctl PCMCIAIO_GET_STATUS: %m");
		return;
	}
	status = ISSET(stbuf.status, PCMCIA_CARD_PRESENT);
	if (!status) {
		syslog(LOG_INFO,"No card in slot %d",stbuf.slot);
		if (ISSET(stbuf.status, PCMCIA_CARD_INUSE) ||
		    ISSET(stbuf.status, PCMCIA_CARD_IS_MAPPED)) {
		    if (ioctl(fd,
			      ISSET(stbuf.status, PCMCIA_CARD_INUSE) ?
			      PCMCIAIO_UNCONFIGURE : PCMCIAIO_UNMAP, 0) < 0)
			syslog(LOG_ERR,
			       "ioctl PCMCIAIO_UNCONFIGURE slot %d: %m",
			       stbuf.slot);
		    else {
			    if (status != slot_status[fd]) {
				    make_noise(HIGH_LOW);
			    }
		    }
		    slot_status[fd] = status;
		    if (ioctl(fd, PCMCIAIO_GET_STATUS, &stbuf) < 0) {
			syslog(LOG_ERR, "ioctl PCMCIAIO_GET_STATUS: %m");
			make_noise(SINGLE_LOW);
			return;
		    }
		} else {
			syslog(LOG_DEBUG,"Card in slot %d is not mapped",
			       stbuf.slot);
			if (status != slot_status[fd]) {
			    make_noise(HIGH_LOW);
			}
			slot_status[fd] = status;
			return;
		}
		if (ISSET(stbuf.status, PCMCIA_POWER)) {
		    pw = PCMCIASIO_POWER_OFF;
		    if (ioctl(fd, PCMCIAIO_SET_POWER, &pw) < 0)
			syslog(LOG_ERR,"ioctl PCMCIAIO_SET_POWER slot %d: %m",
			       stbuf.slot);
		}
#if 0
		sprintf(cmd, "/sbin/pcmcia_cntrl -f %d %d unmapforce", fd,
			PCMCIABUS_SLOT(PCMCIABUS_UNIT(devices_opened[fd])));
#endif
		return;
	} else {
		if (status != slot_status[fd])
			make_noise(SHORT_HIGH);
		if (ISSET(stbuf.status, PCMCIA_CARD_INUSE)) {
			syslog(LOG_INFO,
			       "Card in slot %d is attached, can't probe it",
			       stbuf.slot);
			/* make_noise(SINGLE_LOW); */
			return;
		}
		/* unmap the card to clean up. */
		if (ISSET(stbuf.status, PCMCIA_CARD_IS_MAPPED) &&
		    ioctl(fd, PCMCIAIO_UNMAP, 0) == -1) {
			syslog(LOG_NOTICE,
			       "cannot unmap card in slot %d: %m", stbuf.slot);
			make_noise(SINGLE_LOW);
			return;
		}
			       
tryagain:
		pw = PCMCIASIO_POWER_OFF;
		if (ioctl(fd, PCMCIAIO_SET_POWER, &pw) == -1) {
		    syslog(LOG_ERR,"ioctl PCMCIAIO_SET_POWER slot %d: %m",
			   stbuf.slot);
		    make_noise(SINGLE_LOW);
		    return;
		}	
		pw = PCMCIASIO_POWER_AUTO;
		if (ioctl(fd, PCMCIAIO_SET_POWER, &pw) == -1) {
		    syslog(LOG_ERR,"ioctl PCMCIAIO_SET_POWER slot %d: %m",
			   stbuf.slot);
		    make_noise(SINGLE_LOW);
		    return;
		}
		if (ioctl(fd, PCMCIAIO_GET_INFO, &inbuf) < 0)
			syslog(LOG_ERR, "ioctl PCMCIAIO_GETINFO: %m");
		else {
			syslog(LOG_DEBUG, "config: %s", inbuf.cis_data);
		}
		memset(&pc_cf, 0, sizeof(pc_cf));
		if (pcmcia_get_cf(0, inbuf.cis_data, 512, CFGENTRYMASK,
				  &pc_cf)) {
			syslog(LOG_ERR, "can't interpret config info");
		} else {
			if (pcmcia_get_cisver1(0, (u_char *)&inbuf.cis_data,
					       512, manu, model, addinf1,
					       addinf2) == 0) {
				syslog(LOG_INFO,"<%s, %s, %s, %s>",
				       manu, model, addinf1, addinf2);
			} else {
				syslog(LOG_ERR, "can't get CIS info");
				if (first) {
					first = 0;
					goto tryagain;
				}
			}
		}
#if 0
		sprintf(cmd, "/sbin/pcmcia_cntrl -f %d %d probe", fd,
			PCMCIABUS_SLOT(PCMCIABUS_UNIT(devices_opened[fd])));
#endif
#if 0 || 0
		if (ioctl(fd, PCMCIAIO_CONFIGURE, &pc_cf) == -1) {
		    syslog(LOG_ERR, "ioctl PCMCIAIO_CONFIGURE: %m");
		    return;
		}
#endif
		memset(&pc_cf, 0, sizeof(pc_cf));
		if (ioctl(fd, PCMCIAIO_CONFIGURE, &pc_cf) == -1) {
		    syslog(LOG_ERR, "ioctl PCMCIAIO_CONFIGURE: %m");
		    make_noise(SINGLE_LOW);
		} else
		    if (status != slot_status[fd])
			make_noise(LOW_HIGH);
		slot_status[fd] = status;
		return;
	}
#if 0
	syslog(LOG_DEBUG, "execing `%s'", cmd);
	status = system(cmd);
	if (status == -1)
		syslog(LOG_ERR, "cannot run %s", cmd);
	else if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) != 0)
			syslog(LOG_ERR,
			       "%s returned %d", cmd, WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		syslog(LOG_ERR,
		       "%s died from signal %d", cmd, WTERMSIG(status));
	}
	return;
#endif
}

/*
 * Insert/remove tones.  We do the same tones as Wildboar (BSD/OS kit):
 *
 * single high note: suspend/resume (look at apmd instead)
 * short high note: card insertion noticed
 * low then high note: successful attach
 * high then low note: card eject noticed
 * single low note: unknown card or attach failure
 *
 * we do the sound via /dev/speaker.
 */

const char *tone_string[] = {
/*    SINGLE_HIGH*/"o4c",
/*    SHORT_HIGH,*/"t240o4c",
/*    LOW_HIGH,*/  "t180o2co4c",
/*    HIGH_LOW,*/  "t180o4co2c",
/*    SINGLE_LOW*/ "t180o2c"
};

void
make_noise(tones)
	enum speaker_tones tones;
{
	pid_t pid;
	int spkrfd;
	int trycnt;

	if (!speaker_ok)		/* don't bother after sticky errors */
		return;

	pid = fork();
	switch (pid) {
	case -1:
		syslog(LOG_ERR, "cannot fork for speaker tones: %m");
		return;
	case 0:
		/* child */
		for (trycnt = 0; trycnt < 3; trycnt++) {
			spkrfd = open(_PATH_DEV_SPEAKER, O_WRONLY);
			if (spkrfd == -1) {
				switch (errno) {
				case EBUSY:
					usleep(1000000);
					errno = EBUSY;
					continue;
				case ENOENT:
				case ENODEV:
				case ENXIO:
				case EPERM:
				case EACCES:
					syslog(LOG_INFO,
					       "speaker device " _PATH_DEV_SPEAKER " unavailable: %m");
					exit(2);
					break;
				}
			} else
				break;
		}
		if (spkrfd == -1) {
			syslog(LOG_WARNING,
			       "cannot open " _PATH_DEV_SPEAKER ": %m");
			exit(1);
		}
		syslog(LOG_DEBUG,
		       "sending %s to speaker", tone_string[tones]);
		write (spkrfd, tone_string[tones], strlen(tone_string[tones]));
		exit(0);
	default:
		/* parent */
		return;
	}
}

void
child_death(sig)
	int sig;
{
	int status;
	int save_errno = errno;

	if (wait(&status) == -1) {
		syslog(LOG_ERR, "wait error for signaled child: %m");
		errno = save_errno;
		return;
	}
	if (WEXITSTATUS(status) == 2)
		speaker_ok = 0;
	errno = save_errno;
}
