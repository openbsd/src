/*	$OpenBSD: isapnpvar.h,v 1.1 1996/08/14 14:36:16 shawn Exp $	*/

/*
 * Copyright (c) 1996, Shawn Hsiao <shawn@alpha.secc.fju.edu.tw>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _DEV_ISA_ISAPNPVAR_H_
#define _DEV_ISA_ISAPNPVAR_H_

struct irq_format {
	int num;		/* IRQ number */
	int info;		/* IRQ type */
};

struct dma_format {
	int channel;		/* DMA channel */
	int info;		/* DMA type */
};

struct io_descriptor {
	int type;		/* normal or fixed I/O descriptor */
	int info;		/* decoding information */
	int min_base;		/* minimum base I/O address */	
	int max_base;		/* maximum base I/O address */
	int alignment;		/* alignment for base address */
	int size;		/* number of contiguous ports */
};

struct mem_descriptor {
	int type;		/* 24 or 32bit memory descriptor */
	int info;		/* misc. memory information */
	int min_base;		/* minimum base mem address */
	int max_base;		/* maximum base mem address */
	int alignment;		/* aligment for base address */
	int size;		/* number of memory range length */
};

struct confinfo {
	int prio;
	struct irq_format *irq[2];
	struct dma_format *dma[2];
	struct io_descriptor *io[8];
	struct mem_descriptor *mem[4];

	TAILQ_ENTRY(confinfo) conf_link;
};

struct devinfo {
	u_int32_t id;
	u_int32_t comp_id;
	char *id_string;

	int ldn;
	int io_range_check;

	TAILQ_HEAD(, confinfo) q_conf;
	struct confinfo *conf;
	struct confinfo *basic;

	TAILQ_ENTRY(devinfo) dev_link;
};

struct cardinfo {
	char serial[9];
	char pnp_version[2];
	int csn;
	char *id_string;

	int num_ld;

	TAILQ_HEAD(, devinfo) q_dev;
	struct devinfo *dev;
	TAILQ_ENTRY(cardinfo) card_link;
};

#endif /* !_DEV_ISA_ISAPNPVAR_H_ */
