/*	$Id: pcmcia_ioctl.h,v 1.2 1996/04/29 14:17:25 hvozda Exp $	*/
/*
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
 */
struct pcmcia_info {
	int slot;
	u_char cis_data[CIS_MAXSIZE];
};

struct pcmcia_status {
	int slot;
	int status;
};

struct pcmcia_regs {
	int chip;
	int chiptype;
#define PCMCIA_CHIP_UNKNOWN	0
#define PCMCIA_PCIC		1
	u_char chip_data[CIS_MAXSIZE];
};

#define PCMCIAIO_GET_STATUS	   _IOR('s', 128, struct pcmcia_status)
#define PCMCIAIO_GET_INFO	   _IOR('s', 129, struct pcmcia_info)
#define PCMCIAIO_SET_POWER	   _IOW('s', 139, int)
#define PCMCIASIO_POWER_5V	    0x3
#define PCMCIASIO_POWER_3V	    0x5
#define PCMCIASIO_POWER_AUTO	    0x7
#define PCMCIASIO_POWER_OFF	    0x0
#define PCMCIAIO_CONFIGURE	   _IOW('s', 140, struct pcmcia_conf)
#define PCMCIAIO_UNMAP		   _IO('s', 141)
#define PCMCIAIO_UNCONFIGURE	   _IO('s', 142)
#define PCMCIAIO_READ_COR	   _IOR('s', 143, struct pcmcia_info)
#define PCMCIAIO_READ_REGS	   _IOWR('s', 160, struct pcmcia_regs)
