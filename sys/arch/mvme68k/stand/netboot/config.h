/*	$OpenBSD: config.h,v 1.2 2003/06/04 04:11:37 deraadt Exp $ */

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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* configuration information for base-line code */

#define ETHER_ADDR_147		(0xfffe0778)
#define ETHER_ADDR_16X		(0xfffc0000+7980)
#define ERAM_ADDR		(0xfffe0774)
#define LANCE_REG_ADDR		(0xfffe1800)
#define INTEL_REG_ADDR		(0xfff46000)

#define CPU_147			0x147
#define CPU_162			0x162
#define CPU_167			0x167
#define CPU_172			0x172
#define CPU_177			0x177

struct brdid {
	u_long  eye_catcher;
	u_char  rev;
	u_char  month;
	u_char  day;
	u_char  year;
	u_short size;
	u_short rsv1;
	u_short model;
	u_short suffix;
	u_short options;
	u_char  family;
	u_char  cpu;
	u_short ctrlun;
	u_short devlun;
	u_short devtype;
	u_short devnum;
	u_long  bug;
};
