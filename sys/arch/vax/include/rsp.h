/*	$OpenBSD: rsp.h,v 1.2 1997/05/29 00:04:50 niklas Exp $ */
/*	$NetBSD: rsp.h,v 1.1 1996/02/17 18:14:50 ragge Exp $ */
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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


/*
 * The Radial Serial Protocol (RSP) that TU58 (DECtape II) uses
 * is a strange animal that is sent over serial lines.
 * Most packet types can match the struct rsp, but some can't (i.e. 
 * data packets). 
 * More about RSP can be read in Digital Peripherals Handbook, p. 92.
 */

struct rsp {
	char	rsp_typ;	/* Packet type */
	char	rsp_sz;		/* Packet size */
	char	rsp_op;		/* Operation */
	char	rsp_mod;	/* Modifier */
	char	rsp_drv;	/* Tape drive number */
	char	rsp_sw;		/* Switches */
	char	rsp_xx1;	/* Unused, always zero */
	char	rsp_xx2;	/* Unused, always zero */
	short	rsp_cnt;	/* Byte count to transfer */
	short	rsp_blk;	/* Block number */
	short	rsp_sum;	/* Checksum of packet */
};

/* Types of packets */
#define	RSP_TYP_DATA		001	/* DATA packet */
#define	RSP_TYP_COMMAND		002	/* COMMAND packet */
#define	RSP_TYP_INIT		004	/* INITIALIZE packet */
#define	RSP_TYP_BOOT		010	/* BOOTSTRAP packet (PDP11) */
#define	RSP_TYP_CONTINUE	020	/* CONTINUE packet */
#define	RSP_TYP_XOFF		023	/* XOFF packet */

/* Operation types */
#define	RSP_OP_NOP		000	/* No operation */
#define	RSP_OP_RESET		001	/* Reset */
#define	RSP_OP_READ		002	/* Read data */
#define	RSP_OP_WRITE		003	/* Write data */
#define	RSP_OP_POS		005	/* Position tape */
#define	RSP_OP_DIAG		007	/* internal diagnose */
#define	RSP_OP_GSTAT		010	/* Get status */
#define	RSP_OP_SSTAT		011	/* Set status */
#define	RSP_OP_END		100	/* End packet */

/* Modifier */
#define	RSP_MOD_VERIFY		001	/* Verify read data */
#define	RSP_MOD_OK		000	/* Success */
#define	RSP_MOD_RETR		001	/* Success w/ retries */
#define	RSP_MOD_FAIL		-1	/* Failed self-test */
#define	RSP_MOD_PART		-2	/* Partial operation */
#define	RSP_MOD_NET		-8	/* Non-existent tape drive */
#define	RSP_MOD_NOC		-9	/* No cartridge */
#define	RSP_MOD_WP		-11	/* Write protected */
#define	RSP_MOD_DERR		-17	/* Data error */
#define	RSP_MOD_SERR		-32	/* Seek error */
#define	RSP_MOD_STOP		-33	/* Motor stopped */
#define	RSP_MOD_INVAL		-48	/* Invalid opcode */
#define	RSP_MOD_INVBLK		-55	/* Invalid bloch number */

