/*	$OpenBSD: mopprobe.c,v 1.5 1999/03/27 14:31:22 maja Exp $ */

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
static char rcsid[] = "$OpenBSD: mopprobe.c,v 1.5 1999/03/27 14:31:22 maja Exp $";
#endif

/*
 * mopprobe - MOP Probe Utility
 *
 * Usage:	mopprobe -a [ -3 | -4 ] [-v] [-o]
 *		mopprobe [ -3 | -4 ] [-v] [-o] interface
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

struct once {
	u_char	eaddr[6];		/* Ethernet addr */
	struct once *next;		/* Next one */
};

int     AllFlag = 0;		/* listen on "all" interfaces  */
int	Not3Flag = 0;		/* Not MOP V3 messages         */
int	Not4Flag = 0;		/* Not MOP V4 messages         */
int	VerboseFlag = 0;	/* Print All Announces	       */
int     OnceFlag = 0;		/* print only once             */
int	promisc = 1;		/* Need promisc mode           */
char	*Program;
struct once *root = NULL;

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
	while ((op = getopt(argc, argv, "34aov")) != -1) {
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
		case 'o':
			OnceFlag++;
			break;
		case 'v':
			VerboseFlag++;
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
	(void) fprintf(stderr, "usage: %s -a [ -3 | -4 ] [-v] [-o]\n",Program);
	(void) fprintf(stderr, "       %s [ -3 | -4 ] [-v] [-o] interface\n",Program);
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
	u_char	*dst, *src, mopcode, tmpc, device, ilen;
	u_short	 ptype, moplen = 0, tmps, itype;
	int	 index, trans, len, i, hwa = 0;
	struct once *o = NULL;
	
	/* We don't known with transport, Guess! */

	trans = mopGetTrans(pkt, 0);

	/* Ok, return if we don't wan't this message */

	if ((trans == TRANS_ETHER) && Not3Flag) return;
	if ((trans == TRANS_8023) && Not4Flag)	return;

	index = 0;
	mopGetHeader(pkt, &index, &dst, &src, &ptype, &len, trans);

	/* Ignore our own transmissions */

	if (mopCmpEAddr(ii->eaddr,src) == 0)
		return;

	/* Just check multicast */

	if (mopCmpEAddr(rc_mcst,dst) != 0) {
		return;
	}
	
	switch(ptype) {
	case MOP_K_PROTO_RC:
		break;
	default:
		return;
	}
	
	if (OnceFlag) {
		o = root;
		while (o != NULL) {
			if (mopCmpEAddr(o->eaddr,src) == 0)
				return;
			o = o->next;
		}
		o = (struct once *)malloc(sizeof(*o));
		o->eaddr[0] = src[0];
		o->eaddr[1] = src[1];
		o->eaddr[2] = src[2];
		o->eaddr[3] = src[3];
		o->eaddr[4] = src[4];
		o->eaddr[5] = src[5];
		o->next = root;
		root = o;
	}

	moplen  = mopGetLength(pkt, trans);
	mopcode	= mopGetChar(pkt,&index);

	/* Just process System Information */

	if (mopcode != MOP_K_CODE_SID) {
		return;
	}
	
	tmpc = mopGetChar(pkt,&index);		/* Reserved */
	tmps = mopGetShort(pkt,&index);		/* Receipt # */
		
	device = 0;

	switch(trans) {
	case TRANS_ETHER:
		moplen = moplen + 16;
		break;
	case TRANS_8023:
		moplen = moplen + 14;
		break;
	}

	itype = mopGetShort(pkt,&index); 

	while (index < (int)(moplen)) {
		ilen  = mopGetChar(pkt,&index);
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
			hwa = index;
			index = index + 6;
			break;
		case MOP_K_INFO_TIME:
			index = index + 10;
			break;
	        case MOP_K_INFO_SOFD:
			device = mopGetChar(pkt,&index);
			if (VerboseFlag && 
			    (device != NMA_C_SOFD_LCS) &&   /* DECserver 100 */
			    (device != NMA_C_SOFD_DS2) &&   /* DECserver 200 */
			    (device != NMA_C_SOFD_DP2) &&   /* DECserver 250 */
			    (device != NMA_C_SOFD_DS3))     /* DECserver 300 */
			{
				mopPrintHWA(stdout, src);
				(void)fprintf(stdout," # ");
				mopPrintDevice(stdout, device);
				(void)fprintf(stdout," ");
				mopPrintHWA(stdout, &pkt[hwa]);
				(void)fprintf(stdout,"\n");
			}
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
					mopPrintHWA(stdout, src);
					(void)fprintf(stdout," ");
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

