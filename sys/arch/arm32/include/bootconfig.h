/* $NetBSD: bootconfig.h,v 1.2 1996/03/14 23:11:06 mark Exp $ */

/*
 * Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * bootconfig.h
 *
 * boot configuration structure
 *
 * Created      : 12/09/94
 *
 * Based on kate/boot/bootconfig.h
 */

typedef struct _PhysMem {
	u_int address;
	u_int pages;
} PhysMem;

typedef struct _BootConfig {
	u_int kernvirtualbase;
	u_int kernphysicalbase;
	u_int kernsize;
	u_int argvirtualbase;
	u_int argphysicalbase;
	u_int argsize;
	u_int scratchvirtualbase;
	u_int scratchphysicalbase;
	u_int scratchsize;

	u_int display_start;
	u_int display_size;
	u_int width;
	u_int height;
	u_int bitsperpixel;

	PhysMem dram[4];
	PhysMem vram[1];

	u_int dramblocks;
	u_int vramblocks;
	u_int pagesize;
	u_int drampages;
	u_int vrampages;

	char kernelname[80];

	u_int framerate;
	u_char machine_id[4];
} BootConfig;

#ifdef _KERNEL
extern BootConfig bootconfig;
#endif

/* End of bootconfig.h */
