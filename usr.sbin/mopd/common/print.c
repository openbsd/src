/*	$OpenBSD: print.c,v 1.4 2002/09/07 07:58:21 maja Exp $ */

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
static char rcsid[] = "$OpenBSD: print.c,v 1.4 2002/09/07 07:58:21 maja Exp $";
#endif

#include <sys/types.h>
#include <stdio.h>

#include "os.h"
#include "common/mopdef.h"
#include "common/nmadef.h"
#include "common/nma.h"
#include "common/cmp.h"
#include "common/get.h"

#define SHORT_PRINT

void
mopPrintHWA(fd, ap)
	FILE	*fd;
        u_char *ap;
{
	(void)fprintf(fd, "%x:%x:%x:%x:%x:%x",
		      ap[0],ap[1],ap[2],ap[3],ap[4],ap[5]);
	if (ap[0] < 16) (void)fprintf(fd, " ");
	if (ap[1] < 16) (void)fprintf(fd, " ");
	if (ap[2] < 16) (void)fprintf(fd, " ");
	if (ap[3] < 16) (void)fprintf(fd, " ");
	if (ap[4] < 16) (void)fprintf(fd, " ");
	if (ap[5] < 16) (void)fprintf(fd, " ");
}

void
mopPrintBPTY(fd, bpty)
	FILE	*fd;
	u_char 	bpty;
{
	switch(bpty) {
	case MOP_K_BPTY_SYS:
		(void)fprintf(fd, "System Processor");
		break;
	case MOP_K_BPTY_COM:
		(void)fprintf(fd, "Communication Processor");
		break;
	default:
		(void)fprintf(fd, "Unknown");
		break;
	};
}

void
mopPrintPGTY(fd, pgty)
	FILE	*fd;
	u_char 	pgty;
{
	switch(pgty) {
	case MOP_K_PGTY_SECLDR:
		(void)fprintf(fd, "Secondary Loader");
		break;
	case MOP_K_PGTY_TERLDR:
		(void)fprintf(fd, "Tertiary Loader");
		break;
	case MOP_K_PGTY_OPRSYS:
		(void)fprintf(fd, "Operating System");
		break;
	case MOP_K_PGTY_MGNTFL:
		(void)fprintf(fd, "Management File");
		break;
	default:
		(void)fprintf(fd, "Unknown");
		break;
	};
}

void
mopPrintOneline(fd, pkt, trans)
	FILE	*fd;
	u_char	*pkt;
	int	 trans;
{
	int	 index = 0;
	u_char	*dst, *src, code;
	u_short	 proto;
	int	 len;

	trans = mopGetTrans(pkt, trans);
	mopGetHeader(pkt, &index, &dst, &src, &proto, &len, trans);
	code = mopGetChar(pkt, &index);

	switch (proto) {
	case MOP_K_PROTO_DL:
		(void)fprintf(fd, "MOP DL ");
		break;
	case MOP_K_PROTO_RC:
		(void)fprintf(fd, "MOP RC ");
		break;
	case MOP_K_PROTO_LP:
		(void)fprintf(fd, "MOP LP ");
		break;
	default:
		switch((proto % 256)*256 + (proto / 256)) {
		case MOP_K_PROTO_DL:
			(void)fprintf(fd, "MOP DL ");
			proto = MOP_K_PROTO_DL;
			break;
		case MOP_K_PROTO_RC:
			(void)fprintf(fd, "MOP RC ");
			proto = MOP_K_PROTO_RC;
			break;
		case MOP_K_PROTO_LP:
			(void)fprintf(fd, "MOP LP ");
			proto = MOP_K_PROTO_LP;
			break;
		default:
			(void)fprintf(fd, "MOP ?? ");
			break;
		}
	}

	if (trans == TRANS_8023) {
		(void)fprintf(fd, "802.3 ");
	}

	mopPrintHWA(fd, src); (void)fprintf(fd," > ");
	mopPrintHWA(fd, dst);
	if (len < 1600) {
        	(void)fprintf(fd, " len %4d code %02x ",len,code);
	} else {
		(void)fprintf(fd, " len %4d code %02x ",
			      (len % 256)*256 + (len /256), code);
	}

	switch (proto) {
	case MOP_K_PROTO_DL:
        	switch (code) {
		case MOP_K_CODE_MLT:
			(void)fprintf(fd, "MLT ");
			break;
		case MOP_K_CODE_DCM:
			(void)fprintf(fd, "DCM ");
			break;
		case MOP_K_CODE_MLD:
			(void)fprintf(fd, "MLD ");
			break;
		case MOP_K_CODE_ASV:
			(void)fprintf(fd, "ASV ");
			break;
		case MOP_K_CODE_RMD:
			(void)fprintf(fd, "RMD ");
			break;
		case MOP_K_CODE_RPR:
			(void)fprintf(fd, "RPR ");
			break;
		case MOP_K_CODE_RML:
			(void)fprintf(fd, "RML ");
			break;
	        case MOP_K_CODE_RDS:
			(void)fprintf(fd, "RDS ");
			break;
		case MOP_K_CODE_MDD:
			(void)fprintf(fd, "MDD ");
			break;
		case MOP_K_CODE_PLT:
			(void)fprintf(fd, "PLT ");
			break;
	        default:
			(void)fprintf(fd, "??? ");
			break;
		}
		break;
	case MOP_K_PROTO_RC:
		switch (code) {
		case MOP_K_CODE_RID:
			(void)fprintf(fd, "RID ");
			break;
		case MOP_K_CODE_BOT:
			(void)fprintf(fd, "BOT ");
			break;
		case MOP_K_CODE_SID:
			(void)fprintf(fd, "SID ");
			break;
		case MOP_K_CODE_RQC:
			(void)fprintf(fd, "RQC ");
			break;
		case MOP_K_CODE_CNT:
			(void)fprintf(fd, "CNT ");
			break;
		case MOP_K_CODE_RVC:
			(void)fprintf(fd, "RVC ");
			break;
		case MOP_K_CODE_RLC:
			(void)fprintf(fd, "RLC ");
			break;
		case MOP_K_CODE_CCP:
			(void)fprintf(fd, "CCP ");
			break;
		case MOP_K_CODE_CRA:
			(void)fprintf(fd, "CRA ");
			break;
		default:
			(void)fprintf(fd, "??? ");
			break;
		}
		break;
	case MOP_K_PROTO_LP:
		switch (code) {
		case MOP_K_CODE_ALD:
			(void)fprintf(fd, "ALD ");
			break;
		case MOP_K_CODE_PLD:
			(void)fprintf(fd, "PLD ");
			break;
		default:
			(void)fprintf(fd, "??? ");
			break;
		}
		break;
	default:
		(void)fprintf(fd, "??? ");
		break;
	}
	(void)fprintf(fd, "\n");
}

void
mopPrintHeader(fd, pkt, trans)
	FILE	*fd;
	u_char	*pkt;
	int	 trans;
{
	u_char	*dst, *src;
	u_short	 proto;
	int	 len, index = 0;

	trans = mopGetTrans(pkt, trans);
	mopGetHeader(pkt, &index, &dst, &src, &proto, &len, trans);
	
	(void)fprintf(fd,"\nDst          : ");
	mopPrintHWA(fd, dst);
	if (mopCmpEAddr(dl_mcst,dst) == 0) {
		(void)fprintf(fd," MOP Dump/Load Multicast");
	};
	if (mopCmpEAddr(rc_mcst,dst) == 0) {
		(void)fprintf(fd," MOP Remote Console Multicast");
	};
	(void)fprintf(fd,"\n");

	(void)fprintf(fd,"Src          : ");
	mopPrintHWA(fd, src);
	(void)fprintf(fd,"\n");
	(void)fprintf(fd,"Proto        : %04x ",proto);
	switch (proto) {
	case MOP_K_PROTO_DL:
		switch (trans) {
		case TRANS_8023:
			(void)fprintf(fd, "MOP Dump/Load (802.3)\n");
			break;
		default:
			(void)fprintf(fd, "MOP Dump/Load\n");
		}
		break;
	case MOP_K_PROTO_RC:
		switch (trans) {
		case TRANS_8023:
			(void)fprintf(fd, "MOP Remote Console (802.3)\n");
			break;
		default:
			(void)fprintf(fd, "MOP Remote Console\n");
		}
		break;
	case MOP_K_PROTO_LP:
		switch (trans) {
		case TRANS_8023:
			(void)fprintf(fd, "MOP Loopback (802.3)\n");
			break;
		default:
			(void)fprintf(fd, "MOP Loopback\n");
		}
		break;
	default:
		(void)fprintf(fd, "\n");
		break;
	}

	
        (void)fprintf(fd,"Length       : %04x (%d)\n",len,len);
}

void
mopPrintMopHeader(fd, pkt, trans)
	FILE	*fd;
	u_char	*pkt;
	int	 trans;
{
	u_char	*dst, *src;
	u_short	 proto;
	int	 len, index = 0;
	u_char   code;

	trans = mopGetTrans(pkt, trans);
	mopGetHeader(pkt, &index, &dst, &src, &proto, &len, trans);
	
	code = mopGetChar(pkt, &index);

	(void)fprintf(fd, "Code         :   %02x ",code);

	switch (proto) {
	case MOP_K_PROTO_DL:
		switch (code) {
		case MOP_K_CODE_MLT:
			(void)fprintf(fd,
				      "Memory Load with transfer address\n");
			break;
		case MOP_K_CODE_DCM:
			(void)fprintf(fd, "Dump Complete\n");
			break;
		case MOP_K_CODE_MLD:
			(void)fprintf(fd, "Memory Load\n");
			break;
		case MOP_K_CODE_ASV:
			(void)fprintf(fd, "Assistance volunteer\n");
			break;
		case MOP_K_CODE_RMD:
			(void)fprintf(fd, "Request memory dump\n");
			break;
		case MOP_K_CODE_RPR:
			(void)fprintf(fd, "Request program\n");
			break;
		case MOP_K_CODE_RML:
			(void)fprintf(fd, "Request memory load\n");
			break;
		case MOP_K_CODE_RDS:
			(void)fprintf(fd, "Request Dump Service\n");
			break;
		case MOP_K_CODE_MDD:
			(void)fprintf(fd, "Memory dump data\n");
			break;
		case MOP_K_CODE_PLT:
			(void)fprintf(fd,
				      "Parameter load with transfer addres\n");
			break;
		default:
			(void)fprintf(fd, "(unknown)\n");
			break;
		}
		break;
	case MOP_K_PROTO_RC:
		switch (code) {
		case MOP_K_CODE_RID:
			(void)fprintf(fd, "Request ID\n");
			break;
		case MOP_K_CODE_BOT:
			(void)fprintf(fd, "Boot\n");
			break;
		case MOP_K_CODE_SID:
			(void)fprintf(fd, "System ID\n");
			break;
		case MOP_K_CODE_RQC:
			(void)fprintf(fd, "Request Counters\n");
			break;
		case MOP_K_CODE_CNT:
			(void)fprintf(fd, "Counters\n");
			break;
		case MOP_K_CODE_RVC:
			(void)fprintf(fd, "Reserve Console\n");
			break;
		case MOP_K_CODE_RLC:
			(void)fprintf(fd, "Release Console\n");
			break;
		case MOP_K_CODE_CCP:
			(void)fprintf(fd, "Console Command and Poll\n");
			break;
		case MOP_K_CODE_CRA:
			(void)fprintf(fd,
				      "Console Response and Acknnowledge\n");
			break;
		default:
			(void)fprintf(fd, "(unknown)\n");
			break;
		}
		break;
	case MOP_K_PROTO_LP:
		switch (code) {
		case MOP_K_CODE_ALD:
			(void)fprintf(fd, "Active loop data\n");
			break;
		case MOP_K_CODE_PLD:
			(void)fprintf(fd, "Passive looped data\n");
			break;
		default:
			(void)fprintf(fd, "(unknown)\n");
			break;
		}
		break;
	default:
		(void)fprintf(fd, "(unknown)\n");
		break;
	}
}

void
mopPrintDevice(fd, device)
	FILE	*fd;
        u_char device;
{
	char	*sname, *name;

	sname = nmaGetShort((int) device);
	name  = nmaGetDevice((int) device);
	
        (void)fprintf(fd, "%s '%s'",sname,name);
}

void
mopPrintTime(fd, ap)
	FILE	*fd;
        u_char *ap;
{
	(void)fprintf(fd,
		      "%04d-%02d-%02d %02d:%02d:%02d.%02d %d:%02d",
		      ap[0]*100 + ap[1],
		      ap[2],ap[3],ap[4],ap[5],ap[6],ap[7],ap[8],ap[9]);
}

void
mopPrintInfo(fd, pkt, index, moplen, mopcode, trans)
	FILE	*fd;
	u_char  *pkt, mopcode;
	int     *index, trans;
	u_short moplen;
{
        u_short itype,tmps;
	u_char  ilen ,tmpc,device;
	u_char  uc1,uc2,uc3,*ucp;
	int     i;
	
	device = 0;

	switch(trans) {
	case TRANS_ETHER:
		moplen = moplen + 16;
		break;
	case TRANS_8023:
		moplen = moplen + 14;
		break;
	}

	itype = mopGetShort(pkt,index); 

	while (*index < (int)(moplen + 2)) {
		ilen  = mopGetChar(pkt,index);
		switch (itype) {
		case 0:
			tmpc  = mopGetChar(pkt,index);
			*index = *index + tmpc;
			break;
		case MOP_K_INFO_VER:
			uc1 = mopGetChar(pkt,index);
			uc2 = mopGetChar(pkt,index);
			uc3 = mopGetChar(pkt,index);
			(void)fprintf(fd,"Maint Version: %d.%d.%d\n",
				      uc1,uc2,uc3);
			break;
		case MOP_K_INFO_MFCT:
			tmps = mopGetShort(pkt,index);
			(void)fprintf(fd,"Maint Funcion: %04x ( ",tmps);
			if (tmps &   1) (void)fprintf(fd, "Loop ");
			if (tmps &   2) (void)fprintf(fd, "Dump ");
			if (tmps &   4) (void)fprintf(fd, "Pldr ");
			if (tmps &   8) (void)fprintf(fd, "MLdr ");
			if (tmps &  16) (void)fprintf(fd, "Boot ");
			if (tmps &  32) (void)fprintf(fd, "CC ");
			if (tmps &  64) (void)fprintf(fd, "DLC ");
			if (tmps & 128) (void)fprintf(fd, "CCR ");
			(void)fprintf(fd, ")\n");
			break;
		case MOP_K_INFO_CNU:
			ucp = pkt + *index; *index = *index + 6;
			(void)fprintf(fd,"Console User : ");
			mopPrintHWA(fd, ucp);
			(void)fprintf(fd, "\n");
			break;
		case MOP_K_INFO_RTM:
			tmps = mopGetShort(pkt,index);
			(void)fprintf(fd,"Reserv Timer : %04x (%d)\n",
				      tmps,tmps); 
			break;
		case MOP_K_INFO_CSZ:
			tmps = mopGetShort(pkt,index);
			(void)fprintf(fd,"Cons Cmd Size: %04x (%d)\n",
				      tmps,tmps);
			break;
		case MOP_K_INFO_RSZ:
			tmps = mopGetShort(pkt,index);
			(void)fprintf(fd,"Cons Res Size: %04x (%d)\n",
				      tmps,tmps);
			break;
		case MOP_K_INFO_HWA:
			ucp = pkt + *index; *index = *index + 6;
			(void)fprintf(fd,"Hardware Addr: ");
			mopPrintHWA(fd, ucp);
			(void)fprintf(fd, "\n");
			break;
		case MOP_K_INFO_TIME:
			ucp = pkt + *index; *index = *index + 10;
			(void)fprintf(fd,"System Time: ");
			mopPrintTime(fd, ucp);
			(void)fprintf(fd,"\n");
			break;
		case MOP_K_INFO_SOFD:
			device = mopGetChar(pkt,index);
			(void)fprintf(fd,"Comm Device  :   %02x ",device);
			mopPrintDevice(fd, device);
			(void)fprintf(fd, "\n");
			break;
		case MOP_K_INFO_SFID:
			tmpc = mopGetChar(pkt,index);
			(void)fprintf(fd,"Software ID  :   %02x ",tmpc);
			if ((tmpc == 0)) {
				(void)fprintf(fd,"No software id");
			}
			if ((tmpc == 254)) {
				(void)fprintf(fd,"Maintenance system");
				tmpc = 0;
			}
			if ((tmpc == 255)) {
				(void)fprintf(fd,"Standard operating system");
				tmpc = 0;
			}
			if ((tmpc > 0)) {
				(void)fprintf(fd,"'");
				for (i = 0; i < ((int) tmpc); i++) {
					(void)fprintf(fd,"%c",
						     mopGetChar(pkt,index));
				}
				(void)fprintf(fd,"'");
			}
			(void)fprintf(fd,"\n");
			break;
		case MOP_K_INFO_PRTY:
			tmpc = mopGetChar(pkt,index);
			(void)fprintf(fd,"System Proc  :   %02x ",tmpc);
			switch (tmpc) { 
			case MOP_K_PRTY_11:
				(void)fprintf(fd, "PDP-11\n");
				break;
			case MOP_K_PRTY_CMSV:
				(void)fprintf(fd,
					      "Communication Server\n");
				break;
			case MOP_K_PRTY_PRO:
				(void)fprintf(fd, "Professional\n");
				break;
			case MOP_K_PRTY_SCO:
				(void)fprintf(fd, "Scorpio\n");
				break;
			case MOP_K_PRTY_AMB:
				(void)fprintf(fd, "Amber\n");
				break;
			case MOP_K_PRTY_BRI:
				(void)fprintf(fd, "XLII Bridge\n");
				break;
			default:
				(void)fprintf(fd, "Unknown\n");
				break;
			};
			break;
		case MOP_K_INFO_DLTY:
			tmpc = mopGetChar(pkt,index);
			(void)fprintf(fd,"Data Link Typ:   %02x ",tmpc);
			switch (tmpc) { 
			case MOP_K_DLTY_NI:
				(void)fprintf(fd, "Ethernet\n");
				break;
			case MOP_K_DLTY_DDCMP:
				(void)fprintf(fd, "DDCMP\n");
				break;
			case MOP_K_DLTY_LAPB:
				(void)fprintf(fd, "LAPB (X.25)\n");
				break;
			default:
				(void)fprintf(fd, "Unknown\n");
				break;
			};
			break;
		case MOP_K_INFO_DLBSZ:
			tmps = mopGetShort(pkt,index);
			(void)fprintf(fd,"DL Buff Size : %04x (%d)\n",
				      tmps,tmps);
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
					ucp = pkt + *index;
					*index = *index + ilen;
					(void)fprintf(fd,
						     "ROM Sftwr Ver:   %02x '",
						      ilen);
					for (i = 0; i < ilen; i++) {
						(void)fprintf(fd,"%c",ucp[i]);
					}
					(void)fprintf(fd, "'\n");
					break;
				case 103:
					ucp = pkt + *index;
					*index = *index + ilen;
					(void)fprintf(fd,
						     "Software Ver :   %02x '",
						      ilen);
					for (i = 0; i < ilen; i++) {
						(void)fprintf(fd, "%c",ucp[i]);
					}
					(void)fprintf(fd, "'\n");
					break;
				case 104:
					tmps = mopGetShort(pkt,index);
					(void)fprintf(fd,
						"DECnet Addr  : %d.%d (%d)\n",
						      tmps / 1024,
						      tmps % 1024,
						      tmps);
					break;
				case 105:
					ucp = pkt + *index;
					*index = *index + ilen;
					(void)fprintf(fd,
						     "Node Name    :   %02x '",
						      ilen);
					for (i = 0; i < ilen; i++) {
						(void)fprintf(fd, "%c",ucp[i]);
					}
					(void)fprintf(fd, "'\n");
					break;
				case 106:
					ucp = pkt + *index;
					*index = *index + ilen;
					(void)fprintf(fd,
						     "Node Ident   :   %02x '",
						      ilen);
					for (i = 0; i < ilen; i++) {
						(void)fprintf(fd, "%c",ucp[i]);
					}
					(void)fprintf(fd, "'\n");
					break;
				};
			} else {
				ucp = pkt + *index; *index = *index + ilen;
				(void)fprintf(fd, "Info Type    : %04x (%d)\n",
					      itype,
					      itype);
				(void)fprintf(fd, "Info Data    :   %02x ",
					      ilen);
				for (i = 0; i < ilen; i++) {
					if ((i % 16) == 0) {
						if ((i / 16) == 0) { 
						} else { 
							(void)fprintf(fd,
						     "\n                    ");
						};
					};
					(void)fprintf(fd, "%02x ",ucp[i]); 
				}
				(void)fprintf(fd, "\n");
			};
		}
		itype = mopGetShort(pkt,index); 
        }
}

