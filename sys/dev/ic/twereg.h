/*	$OpenBSD: twereg.h,v 1.3 2000/11/06 23:56:18 mickey Exp $	*/

/*
 * Copyright (c) 2000 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * most of the meaning for registers were taken from
 * freebsd driver, which in turn got 'em from linux.
 * it seems those got 'em from windows driver, in it's turn.
 */


/* general parameters */
#define	TWE_MAX_UNITS		16
#define	TWE_MAXOFFSETS		62
#define	TWE_MAXCMDS		255
#define	TWE_SECTOR_SIZE		512
#define	TWE_ALIGN		512
#define	TWE_MAXFER		(TWE_MAXOFFSETS * PAGE_SIZE)

/* registers */
#define	TWE_CONTROL		0x00
#define		TWE_CTRL_CHOSTI	0x00080000	/* clear host int */
#define		TWE_CTRL_CATTNI	0x00040000	/* clear attention int */
#define		TWE_CTRL_MCMDI	0x00020000	/* mask cmd int */
#define		TWE_CTRL_MRDYI	0x00010000	/* mask ready int */
#define		TWE_CTRL_ECMDI	0x00008000	/* enable cmd int */
#define		TWE_CTRL_ERDYI	0x00004000	/* enable ready int */
#define		TWE_CTRL_CERR	0x00000200	/* clear error status */
#define		TWE_CTRL_SRST	0x00000100	/* soft reset */
#define		TWE_CTRL_EINT	0x00000080	/* enable ints */
#define		TWE_CTRL_MINT	0x00000040	/* mask ints */
#define		TWE_CTRL_HOSTI	0x00000020	/* generate host int */
#define	TWE_STATUS		0x04
#define		TWE_STAT_MAJV	0xf0000000
#define		TWE_MAJV(st)	(((st) >> 28) & 0xf)
#define		TWE_STAT_MINV	0x0f000000
#define		TWE_MINV(st)	(((st) >> 24) & 0xf)
#define		TWE_STAT_PCIPAR	0x00800000
#define		TWE_STAT_QUEUEE	0x00400000
#define		TWE_STAT_CPUERR	0x00200000
#define		TWE_STAT_PCIABR	0x00100000
#define		TWE_STAT_HOSTI	0x00080000
#define		TWE_STAT_ATTNI	0x00040000
#define		TWE_STAT_CMDI	0x00020000
#define		TWE_STAT_RDYI	0x00010000
#define		TWE_STAT_CQF	0x00008000	/* cmd queue full */
#define		TWE_STAT_RQE	0x00004000	/* ready queue empty */
#define		TWE_STAT_CPURDY	0x00002000	/* cpu ready */
#define		TWE_STAT_CQR	0x00001000	/* cmd queue ready */
#define		TWE_STAT_FLAGS	0x00fff000	/* mask out other stuff */
#define		TWE_STAT_BITS	"\020\015cqr\016cpurdy\017rqe\20cqf"	\
    "\021rdyi\022cmdi\023attni\024hosti\025pciabr\026cpuerr\027queuee\030pcipar"
#define	TWE_COMMANDQUEUE	0x08
	/*
	 * the segs offset is encoded into upper 3 bits of the opcode.
	 * i bet other bits mean something too
	 * upper 8 bits is the command size in 32bit words.
	 */
#define		TWE_CMD_NOP	0x0200
#define		TWE_CMD_INIT	0x0301
#define		TWE_CMD_READ	0x0362
#define		TWE_CMD_WRITE	0x0363
#define		TWE_CMD_VERIFY	0x0364
#define		TWE_CMD_GPARAM	0x0252
#define		TWE_CMD_SPARAM	0x0253
#define		TWE_CMD_SECINF	0x021a
#define		TWE_CMD_AEN	0x021c
#define	TWE_READYQUEUE		0x0c
#define		TWE_READYID(u)	(((u) >> 4) & 0xff)

/* get/set param table ids */
#define	TWE_PARAM_UC	0x003	/* unit config */
#define	TWE_PARAM_UI	0x300	/* + 16 -- unit information */
#define	TWE_PARAM_AEN	0x401

#define	TWE_AEN_QEMPTY	0x0000
#define	TWE_AEN_SRST	0x0001
#define	TWE_AEN_DMIRROR	0x0002	/* degraded mirror */
#define	TWE_AEN_CERROR	0x0003	/* controller error */
#define	TWE_AEN_RBFAIL	0x0004	/* rebuild failed */
#define	TWE_AEN_RBDONE	0x0005
/*	TWE_AEN_	0x0009	 * dunno what this is (yet) */
#define	TWE_AEN_QFULL	0x00ff
#define	TWE_AEN_TUN	0x0015	/* table undefined */

/* struct definitions */
struct twe_param {
	u_int16_t	table_id;
	u_int8_t	param_id;
	u_int8_t	param_size;
	u_int8_t	data[1];
} __attribute__ ((packed));

struct twe_segs {
	u_int32_t twes_addr;
	u_int32_t twes_len;
};

struct twe_cmd {
	u_int16_t	cmd_op;
	u_int8_t	cmd_index;
	u_int8_t	cmd_unit_host;
#define	TWE_UNITHOST(u, h)	(((u) & 0xf) | ((h) << 4))
	u_int8_t	cmd_status;
	u_int8_t	cmd_flags;
#define	TWE_FLAGS_CACHEDISABLE		0x01
	u_int16_t	cmd_count;
	union {
		struct {
			u_int32_t	lba;
			struct twe_segs	segs[TWE_MAXOFFSETS];
			u_int32_t	pad;
		} _cmd_io;
#define	cmd_io		_._cmd_io
		struct {
			struct twe_segs	segs[TWE_MAXOFFSETS];
		} _cmd_param;
#define	cmd_param	_._cmd_param
		struct {
			u_int32_t rdy_q_ptr;
		} _cmd_init;
#define	cmd_init	_._cmd_init
	} _;
} __attribute__ ((packed));	/* 512 bytes */

