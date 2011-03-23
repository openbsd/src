/*	$OpenBSD: autoconf.h,v 1.18 2011/03/23 16:54:36 pirofti Exp $ */

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

#ifndef _MACHINE_AUTOCONF_H_
#define _MACHINE_AUTOCONF_H_

#include <machine/bus.h>

struct confargs {
	bus_space_tag_t	ca_iot;
	bus_dma_tag_t	ca_dmat;	
	int	ca_bustype;
	vaddr_t	ca_vaddr;	/* on-board (pcc,mc,pcctwo) only */
	paddr_t	ca_paddr;
	int	ca_offset;
	int	ca_ipl;
	int	ca_vec;
	char	*ca_name;
};

#define BUS_MAIN	1
#define BUS_PCC		2	/* VME147 PCC chip */
#define BUS_MC		3	/* VME162/172 MC chip */
#define BUS_PCCTWO	4	/* VME166/167/176/177 PCC2 chip */
#define BUS_VMES	5	/* 16 bit VME access */
#define BUS_VMEL	6	/* 32 bit VME access */
#define BUS_IP		7	/* VME162/172 IP module bus */
#define BUS_LRC		8	/* VME165 LRC chip */
#define BUS_OFOBIO	9	/* VME141 */

/* the following are from the prom/bootblocks */
extern paddr_t	bootaddr;	/* PA of boot device */
extern int	bootctrllun;	/* ctrl_lun of boot device */
extern int	bootdevlun;	/* dev_lun of boot device */
extern int	bootpart;	/* boot partition (disk) */
extern int	bootbus;	/* scsi bus (disk) */

vaddr_t	mapiodev(paddr_t, int);
void	unmapiodev(vaddr_t, int);

#endif
