/*	$OpenBSD: nvram.h,v 1.6 2011/03/23 16:54:36 pirofti Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
 * All rights reserved.
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
 */

#ifndef	_MACHINE_NVRAM_H_
#define	_MACHINE_NVRAM_H_

struct nvram_147 {
	u_char	user[0x400];
	u_char	os[0x200];
	u_char	bug[0x174];
	u_long	emem;
	u_char	ether[3];
	u_char	memsizing;
	u_char	other[124];
	/*struct clockreg cl; */
};

struct nvram_16x {
	u_char	user[0x1000];
	u_char	net[0x100];
	u_char	os[1528];
	u_char	bug[2048];
	struct nvram_16x_conf {
		u_char	version[4];
		u_char	serial[12];
		u_char	id[16];
		u_char	pwa[16];
		u_char	speed[4];
		u_char	ether[6];
		u_char	fill[2];
		u_char	lscsiid[2];
		u_char	mem_pwb[8];
		u_char	mem_serial[8];
		u_char	port2_pwb[8];
		u_char	port2_serial[8];
		u_char	ipa_brdid[8];
		u_char	ipa_serial[8];
		u_char	ipa_pwb[8];
		u_char	ipb_brdid[8];
		u_char	ipb_serial[8];
		u_char	ipb_pwb[8];
		u_char	ipc_brdid[8];
		u_char	ipc_serial[8];
		u_char	ipc_pwb[8];
		u_char	ipd_brdid[8];
		u_char	ipd_serial[8];
		u_char	ipd_pwb[8];
		u_char	reserved[65];
		u_char	cksum[1];
	} conf;
	/*struct clockreg cl; */
};

#endif
