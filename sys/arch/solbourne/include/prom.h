/*	$OpenBSD: prom.h,v 1.1 2005/04/19 21:30:18 miod Exp $	*/
/*
 * Copyright (c) 2005, Miodrag Vallat
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_SOLBOURNE_PROM_H_
#define	_SOLBOURNE_PROM_H_

/*
 * The following describes the PROM communication structure,
 * which appears at the beginning of physical memory.
 */

#define	PROM_CODE_PA		0x00000000
#define	PROM_CODE_VA		PTW0_BASE
#define	PROM_DATA_PA		PHYSMEM_BASE
#define	PROM_DATA_VA		PTW1_BASE

struct sb_prom {
	int	sp_interface;			/* interface version */
	int	(*sp_interp)(const char *);	/* prom commands */
	char	sp_version[128];		/* prom version */
	int	(*sp_eval)(const char *);	/* forth commands */
	int	sp_ramdisk;			/* ramdisk size if any in MB */
	int	sp_promend;			/* first available va */
	int	sp_memsize;			/* memory size in pages... */
	int	sp_memsize_mb;			/* ...and in MB */
	int	sp_reserve_start;		/* reserved area (in pages) */
	int	sp_reserve_len;			/* and length (in pages) */
	vaddr_t	sp_msgbufp;			/* PROM msgbuf pointer */
	int	sp_sash_usrtrap;
	int	sp_rootnode;
	int	sp_validregs;			/* nonzero if registers... */
	int	sp_regs[100];			/* ...array is valid */
	int	sp_revision;			/* prom revision */
};

#define	PROM_INTERFACE		4

/*
 * Reset strings
 */

#define	PROM_RESET_COLD		"cold"
#define	PROM_RESET_WARM		"warm"
#define	PROM_RESET_HALT		"halt"

/*
 * Environment variables (all upper-case)
 */

#define	ENV_ETHERADDR		"ENETADDR"
#define	ENV_INPUTDEVICE		"INPUT-DEVICE"
#define	ENV_MODEL		"MODEL"
#define	ENV_OUTPUTDEVICE	"OUTPUT-DEVICE"
#define	ENV_TTYA		"TTYA_MODE"
#define	ENV_TTYB		"TTYB_MODE"

/*
 * Node structures
 */

struct prom_node {
	int	pn_sibling;
	int	pn_child;
	vaddr_t	pn_props;
	char	*pn_name;
};

struct prom_prop {
	struct prom_prop *pp_next;
	size_t	pp_size;
	char	pp_data[0];
};

/*
 * System model
 */

extern int sysmodel;

#define	SYS_S4000	0xf4
#define	SYS_S4100	0xf5

const char *prom_getenv(const char *);

#endif	/* _SOLBOURNE_PROM_H_ */
