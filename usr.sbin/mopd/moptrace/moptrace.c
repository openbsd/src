/*	$OpenBSD: moptrace.c,v 1.4 1998/03/19 07:40:13 deraadt Exp $ */

/*
 * Copyright (c) 1993-95 Mats O Jansson.  All rights reserved.
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
static char rcsid[] = "$OpenBSD: moptrace.c,v 1.4 1998/03/19 07:40:13 deraadt Exp $";
#endif

/*
 * moptrace - MOP Trace Utility
 *
 * Usage:	moptrace -a [ -d ] [ -3 | -4 ]
 *		moptrace [ -d ] [ -3 | -4 ] interface
 */

#include "os.h"
#include "common/common.h"
#include "common/mopdef.h"
#include "common/device.h"
#include "common/print.h"
#include "common/pf.h"
#include "common/dl.h"
#include "common/rc.h"
#include "common/get.h"

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

int     AllFlag = 0;		/* listen on "all" interfaces  */
int     DebugFlag = 0;		/* print debugging messages    */
int	Not3Flag = 0;		/* Ignore MOP V3 messages      */
int	Not4Flag = 0;		/* Ignore MOP V4 messages      */ 
int	promisc = 1;		/* Need promisc mode           */
char	*Program;

int
main(argc, argv)
	int     argc;
	char  **argv;
{
	int     op;
	char   *interface;

	extern int optind, opterr;

	if ((Program = strrchr(argv[0], '/')))
		Program++;
	else
		Program = argv[0];
	
	if (*Program == '-')
		Program++;

	/* All error reporting is done through syslogs. */
	openlog(Program, LOG_PID | LOG_CONS, LOG_DAEMON);

	opterr = 0;
	while ((op = getopt(argc, argv, "34ad")) != -1) {
		switch (op) {
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
		default:
			Usage();
			/* NOTREACHED */
		}
	}

	interface = argv[optind++];
	
	if ((AllFlag && interface) ||
	    (!AllFlag && interface == 0) ||
	    (Not3Flag && Not4Flag))
		Usage();

	if (AllFlag)
 		deviceInitAll();
	else
		deviceInitOne(interface);

	Loop();
}

void
Usage()
{
	(void) fprintf(stderr, "usage: %s -a [ -d ] [ -3 | -4 ]\n",Program);
	(void) fprintf(stderr, "       %s [ -d ] [ -3 | -4 ] interface\n",
		       Program);
	exit(1);
}

/*
 * Process incoming packages.
 */
void
mopProcess(ii, pkt)
	struct if_info *ii;
	u_char *pkt;
{
	int	 trans;

	/* We don't known which transport, Guess! */

	trans = mopGetTrans(pkt, 0);

	/* Ok, return if we don't want this message */

	if ((trans == TRANS_ETHER) && Not3Flag) return;
	if ((trans == TRANS_8023) && Not4Flag)	return;

	mopPrintHeader(stdout, pkt, trans);
	mopPrintMopHeader(stdout, pkt, trans);
	
	mopDumpDL(stdout, pkt, trans);
	mopDumpRC(stdout, pkt, trans);

	fprintf(stdout, "\n");
	fflush(stdout);
}


