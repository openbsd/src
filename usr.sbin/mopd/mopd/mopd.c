/*	$OpenBSD: mopd.c,v 1.4 1998/03/04 20:21:59 deraadt Exp $ */

/*
 * Copyright (c) 1993-96 Mats O Jansson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mats O Jansson.
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
 */

#ifndef LINT
static char rcsid[] = "$OpenBSD: mopd.c,v 1.4 1998/03/04 20:21:59 deraadt Exp $";
#endif

/*
 * mopd - MOP Dump/Load Daemon
 *
 * Usage:	mopd -a [ -d -f -v ] [ -3 | -4 ]
 *		mopd [ -d -f -v ] [ -3 | -4 ] interface
 */

#include "os.h"
#include "common/common.h"
#include "common/mopdef.h"
#include "common/device.h"
#include "common/print.h"
#include "common/pf.h"
#include "common/cmp.h"
#include "common/get.h"
#include "common/dl.h"
#include "common/rc.h"
#include "process.h"

/*
 * The list of all interfaces that are being listened to. 
 * "selects" on the descriptors in this list.
 */
struct if_info *iflist;

#ifdef NO__P
void   Loop	     (/* void */);
void   Usage         (/* void */);
void   mopProcess    (/* struct if_info *, u_char * */);
#else
void   Loop	     __P((void));
void   Usage         __P((void));
void   mopProcess    __P((struct if_info *, u_char *));
#endif

int     AllFlag = 0;		/* listen on "all" interfaces */
int     DebugFlag = 0;		/* print debugging messages   */
int	ForegroundFlag = 0;	/* run in foreground          */
int	VersionFlag = 0;	/* print version              */
int	Not3Flag = 0;		/* Not MOP V3 messages.       */
int	Not4Flag = 0;		/* Not MOP V4 messages.       */
int	promisc = 1;		/* Need promisc mode    */
char    *Program;

int
main(argc, argv)
	int     argc;
	char  **argv;
{
	int	c, pid, devnull, f;
	char   *interface;

	extern int optind;
	extern char version[];

	if ((Program = strrchr(argv[0], '/')))
		Program++;
	else
		Program = argv[0];

	if (*Program == '-')
		Program++;

	while ((c = getopt(argc, argv, "34adfv")) != -1)
		switch (c) {
			case '3':
				Not3Flag++;
				break;
			case '4':
				Not4Flag++;
				break;
			case 'a':
				AllFlag++;
				break;
			case 'd':
				DebugFlag++;
				break;
			case 'f':
				ForegroundFlag++;
				break;
			case 'v':
				VersionFlag++;
				break;
			default:
				Usage();
				/* NOTREACHED */
		}
	
	if (VersionFlag) {
		fprintf(stdout,"%s: version %s\n", Program, version);
		exit(0);
	}

	interface = argv[optind++];

	if ((AllFlag && interface) ||
	    (!AllFlag && interface == 0) ||
	    (argc > optind) ||
	    (Not3Flag && Not4Flag))  
		Usage();

	/* All error reporting is done through syslogs. */
	openlog(Program, LOG_PID | LOG_CONS, LOG_DAEMON);

	if ((!ForegroundFlag) && DebugFlag) {
		fprintf(stdout,
			"%s: not running as daemon, -d given.\n",
			Program);
	}

	if ((!ForegroundFlag) && (!DebugFlag)) {

		pid = fork();
		if (pid > 0)
			/* Parent exits, leaving child in background. */
			exit(0);
		else
			if (pid == -1) {
				syslog(LOG_ERR, "cannot fork");
				exit(0);
			}

		/* Fade into the background */
		f = open("/dev/tty", O_RDWR);
		if (f >= 0) {
			if (ioctl(f, TIOCNOTTY, 0) < 0) {
				syslog(LOG_ERR, "TIOCNOTTY: %m");
				exit(0);
			}
			(void) close(f);
		}
		
		(void) chdir("/");
#ifdef SETPGRP_NOPARAM
		(void) setpgrp();
#else
		(void) setpgrp(0, getpid());
#endif
		devnull = open("/dev/null", O_RDWR);
		if (devnull >= 0) {
			(void) dup2(devnull, 0);
			(void) dup2(devnull, 1);
			(void) dup2(devnull, 2);
			if (devnull > 2)
				(void) close(devnull);
		}
	}

	syslog(LOG_INFO, "%s %s started.", Program, version);

	if (AllFlag)
 		deviceInitAll();
	else
		deviceInitOne(interface);

	Loop();
}

void
Usage()
{
	(void) fprintf(stderr, "usage: %s -a [ -d -f -v ] [ -3 | -4 ]\n",Program);
	(void) fprintf(stderr, "       %s [ -d -f -v ] [ -3 | -4 ] interface\n",Program);
	exit(1);
}

/*
 * Process incomming packages.
 */
void
mopProcess(ii, pkt)
	struct if_info *ii;
	u_char *pkt;
{
	u_char	*dst, *src;
	u_short  ptype;
	int	 index, trans, len;

	/* We don't known with transport, Guess! */

	trans = mopGetTrans(pkt, 0);

	/* Ok, return if we don't wan't this message */

	if ((trans == TRANS_ETHER) && Not3Flag) return;
	if ((trans == TRANS_8023) && Not4Flag)	return;

	index = 0;
	mopGetHeader(pkt, &index, &dst, &src, &ptype, &len, trans);

	/*
	 * Ignore our own transmissions
	 *
	 */	
	if (mopCmpEAddr(ii->eaddr,src) == 0)
		return;

	switch(ptype) {
	case MOP_K_PROTO_DL:
		mopProcessDL(stdout, ii, pkt, &index, dst, src, trans, len);
		break;
	case MOP_K_PROTO_RC:
		mopProcessRC(stdout, ii, pkt, &index, dst, src, trans, len);
		break;
	default:
		break;
	}
}
