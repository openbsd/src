/*	$OpenBSD: mopprobe.c,v 1.4 1998/03/19 07:39:44 deraadt Exp $ */

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
static char rcsid[] = "$OpenBSD: mopprobe.c,v 1.4 1998/03/19 07:39:44 deraadt Exp $";
#endif

/*
 * mopprobe - MOP Probe Utility
 *
 * Usage:	mopprobe -a [ -3 | -4 ]
 *		mopprobe [ -3 | -4 ] interface
 */

#include "os.h"
#include "common/common.h"
#include "common/mopdef.h"
#include "common/device.h"
#include "common/print.h"
#include "common/get.h"
#include "common/cmp.h"
#include "common/pf.h"
#include "common/nmadef.h"

/*
 * The list of all interfaces that are being listened to.  rarp_loop()
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
int	Not3Flag = 0;		/* Not MOP V3 messages         */
int	Not4Flag = 0;		/* Not MOP V4 messages         */
int     oflag = 0;		/* print only once             */
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
	while ((op = getopt(argc, argv, "ado")) != -1) {
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
		case 'o':
			oflag++;
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
	(void) fprintf(stderr, "usage: %s -a [ -3 | -4 ]\n",Program);
	(void) fprintf(stderr, "       %s [ -3 | -4 ] interface\n",Program);
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
	u_char  *dst, *src, *p, mopcode, tmpc, ilen;
	u_short *ptype, moplen, tmps, itype, len;
	int	index, i, device, trans;

	dst	= pkt;
	src	= pkt+6;
	ptype   = (u_short *)(pkt+12);
	index   = 0;
	
	if (*ptype < 1600) {
		len = *ptype;
		trans = TRANS_8023;
		ptype = (u_short *)(pkt+20);
		p = pkt+22;
		if (Not4Flag) return;
	} else {
		len = 0;
		trans = TRANS_ETHER;
		p = pkt+14;
		if (Not3Flag) return;
	}
	
	/* Ignore our own messages */

	if (mopCmpEAddr(ii->eaddr,src) == 0) {
		return;
	}

	/* Just check multicast */

	if (mopCmpEAddr(rc_mcst,dst) != 0) {
		return;
	}
	
	switch (trans) {
	case TRANS_8023:
		moplen = len;
		break;
	default:
		moplen = mopGetShort(pkt,&index);
	}
	mopcode	= mopGetChar(p,&index);

	/* Just process System Information */

	if (mopcode != MOP_K_CODE_SID) {
		return;
	}
	
	tmpc	= mopGetChar(pkt,&index);		/* Reserved  */
	tmps	= mopGetShort(pkt,&index);		/* Receipt # */

	device	= 0;					/* Unknown Device */
	
	itype	= mopGetShort(pkt,&index);

	while (index < (int)(moplen + 2)) {
		ilen	= mopGetChar(pkt,&index);
		switch (itype) {
		case 0:
			tmpc  = mopGetChar(pkt,&index);
			index = index + tmpc;
			break;
	        case MOP_K_INFO_VER:
			index = index + 3;
			break;
		case MOP_K_INFO_MFCT:
			index = index + 2;
			break;
		case MOP_K_INFO_CNU:
			index = index + 6;
			break;
		case MOP_K_INFO_RTM:
			index = index + 2;
			break;
		case MOP_K_INFO_CSZ:
			index = index + 2;
			break;
		case MOP_K_INFO_RSZ:
			index = index + 2;
			break;
		case MOP_K_INFO_HWA:
			index = index + 6;
			break;
		case MOP_K_INFO_TIME:
			index = index + 10;
			break;
	        case MOP_K_INFO_SOFD:
			device = mopGetChar(pkt,&index);
			break;
		case MOP_K_INFO_SFID:
			tmpc = mopGetChar(pkt,&index);
			if ((index > 0) && (index < 17)) 
			  index = index + tmpc;
			break;
		case MOP_K_INFO_PRTY:
			index = index + 1;
			break;
		case MOP_K_INFO_DLTY:
			index = index + 1;
			break;
	        case MOP_K_INFO_DLBSZ:
			index = index + 2;
			break;
		default:
			if (((device = NMA_C_SOFD_LCS) ||   /* DECserver 100 */
			     (device = NMA_C_SOFD_DS2) ||   /* DECserver 200 */
			     (device = NMA_C_SOFD_DP2) ||   /* DECserver 250 */
			     (device = NMA_C_SOFD_DS3)) &&  /* DECserver 300 */
			    ((itype > 101) && (itype < 107)))
			{
				switch (itype) {
				case 102:
					index = index + ilen;
					break;
				case 103:
					index = index + ilen;
					break;
				case 104:
					index = index + 2;
					break;
				case 105:
					(void)fprintf(stdout,"%x:%x:%x:%x:%x:%x\t",
						      src[0],src[1],src[2],src[3],src[4],src[5]);
					for (i = 0; i < ilen; i++) {
					  (void)fprintf(stdout, "%c",pkt[index+i]);
					}
					index = index + ilen;
					(void)fprintf(stdout, "\n");
					break;
				case 106:
					index = index + ilen;
					break;
				};
			} else {
				index = index + ilen;
			};
		}
		itype = mopGetShort(pkt,&index); 
	}

}

