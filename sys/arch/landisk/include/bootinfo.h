/*	$OpenBSD: bootinfo.h,v 1.1.1.1 2006/10/06 21:16:15 miod Exp $	*/
/*	$NetBSD: bootinfo.h,v 1.1 2006/09/01 21:26:18 uwe Exp $	*/

/*
 * Copyright (c) 1997
 *	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

struct btinfo_common {
	int len;
	int type;
};

#define BTINFO_BOOTPATH		0
#define BTINFO_BOOTDISK		1
#define BTINFO_NETIF		2

struct btinfo_bootpath {
	struct btinfo_common common;
	char bootpath[80];
};

struct btinfo_bootdisk {
	struct btinfo_common common;
	int labelsector; /* label valid if != -1 */
	struct {
		uint16_t type, checksum;
		char packname[16];
	} label;
	int biosdev;
	int partition;
};

struct btinfo_netif {
	struct btinfo_common common;
	char ifname[16];
	unsigned int tag; /* PCI, BIOS format */
};

#define BOOTINFO_MAXSIZE 4096

struct bootinfo {
        int nentries;
	char info[BOOTINFO_MAXSIZE - sizeof(int)];
};

#ifdef _KERNEL
void *lookup_bootinfo(int type);
#endif
