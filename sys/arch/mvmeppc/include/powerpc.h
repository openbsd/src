/*	$OpenBSD: powerpc.h,v 1.3 2004/01/22 20:45:18 miod Exp $	*/
/*	$NetBSD: powerpc.h,v 1.1 1996/09/30 16:34:30 ws Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_MACHINE_POWERPC_H_
#define	_MACHINE_POWERPC_H_

struct mem_region {
	vm_offset_t start;
	vm_size_t size;
};

void mem_regions(struct mem_region **, struct mem_region **);

/*
 * These two functions get used solely in boot() in machdep.c.
 *
 * Not sure whether boot itself should be implementation dependent instead.	XXX
 */
typedef void (exit_f)(void) /*__attribute__((__noreturn__))*/ ;
typedef void (boot_f)(char *bootspec) /* __attribute__((__noreturn__))*/ ;
typedef void (vmon_f)(void);
typedef unsigned char (nvram_rd_f)(unsigned long offset);
typedef void (nvram_wr_f)(unsigned long offset, unsigned char val);
typedef unsigned long (tps_f)(void);


typedef void (mem_regions_f)(struct mem_region **memp,
	struct mem_region **availp);

typedef int (clock_read_f)(int *sec, int *min, int *hour, int *day,
									int *mon, int *yr);
typedef int (clock_write_f)(int sec, int min, int hour, int day,
									 int mon, int yr);
typedef int (time_read_f)(u_long *sec);
typedef int (time_write_f)(u_long sec);

/* firmware interface.
 * regardless of type of firmware used several items
 * are need from firmware to boot up.
 * these include:
 *	memory information
 *	vmsetup for firmware calls.
 *	default character print mechanism ???
 *	firmware exit (return)
 *	firmware boot (reset)
 *	vmon - tell firmware the bsd vm is active.
 */

struct firmware {
	mem_regions_f	*mem_regions;
	exit_f		*exit;
	boot_f		*boot;
	vmon_f		*vmon;
	nvram_rd_f	*nvram_rd;
	nvram_wr_f	*nvram_wr;
	tps_f			*tps;
   clock_read_f *clock_read;
   clock_write_f *clock_write;
   time_read_f	*time_read;
	time_write_f *time_write;
#ifdef FW_HAS_PUTC
	boot_f		*putc;
#endif
};
extern  struct firmware *fw;

#define ppc_exit() if (fw->exit != NULL) (fw->exit)()
#define ppc_boot(x) if (fw->boot != NULL) (fw->boot)(x)
#define ppc_nvram_rd(a) ({unsigned char val;		\
		if (fw->nvram_rd !=NULL)		\
			val = (fw->nvram_rd)(a);	\
		else					\
			val = 0;			\
		val;})

#define ppc_nvram_wr(a, v) if (fw->nvram_wr !=NULL) (fw->nvram_wr)(a, v)

#define ppc_tps() ({unsigned long val;			\
		if (fw->tps != NULL)			\
			val = (fw->tps)();		\
		else					\
			val = 0;			\
		val;}) 

#define SPR_XER		"1"
#define SPR_LR		"8"
#define SPR_CTR		"9"
#define SPR_DSISR	"18"
#define SPR_DAR		"19"
#define SPR_DEC		"22"
#define SPR_SDR1	"25"
#define SPR_SRR0	"26"
#define SPR_SRR1	"27"

#define ppc_get_spr(reg) ({u_int32_t val; \
		__asm__ volatile("mfspr %0," reg : "=r"(val)); \
		val;})
#define ppc_set_spr(reg, val) ({ \
		__asm__ volatile("mtspr " reg ",%0" :: "r"(val));})

#endif	/* _MACHINE_POWERPC_H_ */
