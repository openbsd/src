/*	$OpenBSD: rc.c,v 1.2 1996/09/21 19:12:06 maja Exp $ */

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
static char rcsid[] = "$OpenBSD: rc.c,v 1.2 1996/09/21 19:12:06 maja Exp $";
#endif

#include "os.h"
#include "common/get.h"
#include "common/print.h"
#include "common/mopdef.h"

void
mopDumpRC(fd, pkt, trans)
	FILE	*fd;
	u_char 	*pkt;
	int	 trans;
{
	int	i,index = 0;
	long	tmpl;
	u_char	tmpc,code,control;
	u_short	len,tmps,moplen;

	len = mopGetLength(pkt, trans);

	switch (trans) {
	case TRANS_8023:
		index = 22;
		moplen = len - 8;
		break;
	default:
		index = 16;
		moplen = len;
	}
	code = mopGetChar(pkt,&index);
	
	switch (code) {
	case MOP_K_CODE_RID:
	  	
		tmpc = mopGetChar(pkt,&index);
		(void)fprintf(fd,"Reserved     :   %02x\n",tmpc);
		
		tmps = mopGetShort(pkt,&index);
		(void)fprintf(fd,"Receipt Nbr  : %04x\n",tmps);
		
		break;
	case MOP_K_CODE_BOT:
		
		if ((moplen == 5)) {
			tmps = mopGetShort(pkt,&index);
			(void)fprintf(fd,"Verification : %04x\n",tmps);
		} else {
			
			tmpl = mopGetLong(pkt,&index);
			(void)fprintf(fd,"Verification : %08x\n",tmpl);

			tmpc = mopGetChar(pkt,&index);	/* Processor */
			(void)fprintf(fd,"Processor    :   %02x ",tmpc);
			mopPrintBPTY(fd, tmpc);  (void)fprintf(fd, "\n");
			
			control = mopGetChar(pkt,&index);	/* Control */
			(void)fprintf(fd,"Control    :   %02x ",control);
			if ((control & (1>>MOP_K_BOT_CNTL_SERVER))) {
				(void)fprintf(fd,
					      "Bootserver Requesting system ");
			} else {
				(void)fprintf(fd,
					      "Bootserver System default ");
			}
			if ((control & (1>>MOP_K_BOT_CNTL_DEVICE))) {
				(void)fprintf(fd,
					      "Bootdevice Specified device");
			} else {
				(void)fprintf(fd,
					      "Bootdevice System default");
			}
			(void)fprintf(fd,"\n");
			
			if ((control & (1>>MOP_K_BOT_CNTL_DEVICE))) {
				tmpc = mopGetChar(pkt,&index);/* Device ID */
				(void)fprintf(fd,
					      "Device ID    :   %02x '",tmpc);
				for (i = 0; i < ((int) tmpc); i++) {
				  (void)fprintf(fd,"%c",
						mopGetChar(pkt,&index));
				}
				(void)fprintf(fd,"'\n");
			}
			
			tmpc = mopGetChar(pkt,&index);      /* Software ID */
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
						     mopGetChar(pkt,&index));
				}
				(void)fprintf(fd,"'");
			}
			(void)fprintf(fd,"'\n");
			
		}
		break;
	case MOP_K_CODE_SID:
		
		tmpc = mopGetChar(pkt,&index);		/* Reserved */
		(void)fprintf(fd, "Reserved     :   %02x\n",tmpc);
		
		tmps = mopGetShort(pkt,&index);		/* Receipt # */
		(void)fprintf(fd, "Receipt Nbr  : %04x\n",tmpc);
		
		mopPrintInfo(fd, pkt, &index, moplen, code, trans);
		
		break;
	case MOP_K_CODE_RQC:
		
		tmps = mopGetShort(pkt,&index);
		(void)fprintf(fd,"Receipt Nbr  : %04x\n",tmps);
		
		break;
	case MOP_K_CODE_CNT:
		
		tmps = mopGetShort(pkt,&index);
		(void)fprintf(fd,"Receipt Nbr  : %04x %d\n",tmps,tmps);
		
		tmps = mopGetShort(pkt,&index);
		(void)fprintf(fd,"Last Zeroed  : %04x %d\n",tmps,tmps);
		
		tmpl = mopGetLong(pkt,&index);
		(void)fprintf(fd,"Bytes rec    : %08x %d\n",tmpl,tmpl);
		
		tmpl = mopGetLong(pkt,&index);
		(void)fprintf(fd,"Bytes snd    : %08x %d\n",tmpl,tmpl);
		
		tmpl = mopGetLong(pkt,&index);
		(void)fprintf(fd,"Frames rec   : %08x %d\n",tmpl,tmpl);
		
		tmpl = mopGetLong(pkt,&index);
		(void)fprintf(fd,"Frames snd   : %08x %d\n",tmpl,tmpl);
		
		tmpl = mopGetLong(pkt,&index);
		(void)fprintf(fd,"Mcst Bytes re: %08x %d\n",tmpl,tmpl);
		
		tmpl = mopGetLong(pkt,&index);
		(void)fprintf(fd,"Mcst Frame re: %08x %d\n",tmpl,tmpl);
		
		tmpl = mopGetLong(pkt,&index);
		(void)fprintf(fd,"Frame snd,def: %08x %d\n",tmpl,tmpl);
		
		tmpl = mopGetLong(pkt,&index);
		(void)fprintf(fd,"Frame snd,col: %08x %d\n",tmpl,tmpl);
		
		tmpl = mopGetLong(pkt,&index);
		(void)fprintf(fd,"Frame snd,mcl: %08x %d\n",tmpl,tmpl);
		
		tmps = mopGetShort(pkt,&index);
		(void)fprintf(fd,"Snd failure  : %04x %d\n",tmps,tmps);
		
		tmps = mopGetShort(pkt,&index);
		(void)fprintf(fd,"Snd fail reas: %04x ",tmps);
		if ((tmps &  1)) (void)fprintf(fd,"Excess col  ");
		if ((tmps &  2)) (void)fprintf(fd,"Carrier chk fail  ");
		if ((tmps &  4)) (void)fprintf(fd,"Short circ  ");
		if ((tmps &  8)) (void)fprintf(fd,"Open circ  ");
		if ((tmps & 16)) (void)fprintf(fd,"Frm to long  ");
		if ((tmps & 32)) (void)fprintf(fd,"Rem fail to defer  ");
		(void)fprintf(fd,"\n");
		
		tmps = mopGetShort(pkt,&index);
		(void)fprintf(fd,"Rec failure  : %04x %d\n",tmps,tmps);
		
		tmps = mopGetShort(pkt,&index);
		(void)fprintf(fd,"Rec fail reas: %04x ",tmps);
		if ((tmps &  1)) (void)fprintf(fd,"Block chk err  ");
		if ((tmps &  2)) (void)fprintf(fd,"Framing err  ");
		if ((tmps &  4)) (void)fprintf(fd,"Frm to long  ");
		(void)fprintf(fd,"\n");
		
		tmps = mopGetShort(pkt,&index);
		(void)fprintf(fd,"Unrec frm dst: %04x %d\n",tmps,tmps);
		
		tmps = mopGetShort(pkt,&index);
		(void)fprintf(fd,"Data overrun : %04x %d\n",tmps,tmps);
		
		tmps = mopGetShort(pkt,&index);
		(void)fprintf(fd,"Sys Buf Unava: %04x %d\n",tmps,tmps);
		
		tmps = mopGetShort(pkt,&index);
		(void)fprintf(fd,"Usr Buf Unava: %04x %d\n",tmps,tmps);
		
		break;
	case MOP_K_CODE_RVC:
		
		tmpl = mopGetLong(pkt,&index);
		(void)fprintf(fd,"Verification : %08x\n",tmpl);
		
		break;
	case MOP_K_CODE_RLC:

		/* Empty message */
	  
		break;
	case MOP_K_CODE_CCP:
		
		tmpc = mopGetChar(pkt,&index);
		(void)fprintf(fd,
			      "Control Flags: %02x Message %d ",tmpc,tmpc & 1);
		if ((tmpc & 2))
			(void)fprintf(fd,"Break");
		(void)fprintf(fd,"\n");
		
		if (moplen > 2) {
#ifndef SHORT_PRINT
			for (i = 0; i < (moplen - 2); i++) {
		  		if ((i % 16) == 0) {
					if ((i / 16) == 0) {
						(void)fprintf(fd,
							"Image Data   : %04x ",
							      moplen-2);
					} else {
						(void)fprintf(fd,
						       "                    ");
				        }
				}
				(void)fprintf(fd,"%02x ",
					      mopGetChar(pkt,&index));
				if ((i % 16) == 15)
					(void)fprintf(fd,"\n");
			}
			if ((i % 16) != 15)
				(void)fprintf(fd,"\n");
#else
			index = index + moplen - 2;
#endif
		}
		
		break;
	case MOP_K_CODE_CRA:
		
		tmpc = mopGetChar(pkt,&index);
		(void)fprintf(fd,
			      "Control Flags: %02x Message %d ",tmpc,tmpc & 1);
		if ((tmpc & 2))
			(void)fprintf(fd,"Cmd Data Lost ");
		if ((tmpc & 4))
			(void)fprintf(fd,"Resp Data Lost ");
		(void)fprintf(fd,"\n");
		
		if (moplen > 2) {
#ifndef SHORT_PRINT
			for (i = 0; i < (moplen - 2); i++) {
				if ((i % 16) == 0) {
					if ((i / 16) == 0) {
						(void)fprintf(fd,
							"Image Data   : %04x ",
							      moplen-2);
					} else {
						(void)fprintf(fd,
						       "                    ");
					}
				}
				(void)fprintf(fd,"%02x ",
					      mopGetChar(pkt,&index));
				if ((i % 16) == 15)
					(void)fprintf(fd,"\n");
			}
			if ((i % 16) != 15)
				(void)fprintf(fd,"\n");
#else
			index = index + moplen - 2;
#endif
		}
		
		break;
	default:
		break;
	}
}

