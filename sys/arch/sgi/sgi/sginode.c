/*	$OpenBSD: sginode.c,v 1.16 2010/01/09 20:33:16 miod Exp $	*/
/*
 * Copyright (c) 2008, 2009 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2004 Opsycon AB.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/memconf.h>
#include <machine/param.h>
#include <machine/autoconf.h>
#include <mips64/arcbios.h>
#include <mips64/archtype.h>

#include <machine/mnode.h>
#include <sgi/xbow/hub.h>

void	kl_add_memory_ip27(int16_t, int16_t *, unsigned int);
void	kl_add_memory_ip35(int16_t, int16_t *, unsigned int);

int	kl_first_pass_board(lboard_t *, void *);
int	kl_first_pass_comp(klinfo_t *, void *);

#ifdef DEBUG
#define	DB_PRF(x)	bios_printf x
#else
#define	DB_PRF(x)
#endif

int	kl_n_mode = 0;
u_int	kl_n_shift = 32;

void
kl_init(int ip35)
{
	kl_config_hdr_t *cfghdr;
	uint64_t val;
	uint64_t nibase = ip35 ? HUBNIBASE_IP35 : HUBNIBASE_IP27;

	/* will be recomputed when processing memory information */
	physmem = 0;

	cfghdr = IP27_KLCONFIG_HDR(0);
	DB_PRF(("config @%p\n", cfghdr));
	DB_PRF(("magic %p version %x\n", cfghdr->magic, cfghdr->version));
	DB_PRF(("console %p baud %d\n", cfghdr->cons_info.uart_base,
	    cfghdr->cons_info.baud));

	val = IP27_LHUB_L(nibase | HUBNI_STATUS);
	kl_n_mode = (val & NI_MORENODES) != 0;
	kl_n_shift = (ip35 ? 33 : 32) - kl_n_mode;
        bios_printf("Machine is in %c mode.\n", kl_n_mode + 'M');

	val = IP27_LHUB_L(HUBPI_REGION_PRESENT);
        DB_PRF(("Region present %p.\n", val));
	val = IP27_LHUB_L(HUBPI_CALIAS_SIZE);
        DB_PRF(("Calias size %p.\n", val));
}

void
kl_scan_config(int nasid)
{
	kl_scan_node(nasid, KLBRD_ANY, kl_first_pass_board, NULL);
}

/*
 * Callback routine for the initial enumeration (boards).
 */
int
kl_first_pass_board(lboard_t *boardinfo, void *arg)
{
	DB_PRF(("%cboard type %x slot %x nasid %x nic %p components %d\n",
	    boardinfo->struct_type & LBOARD ? 'l' : 'r',
	    boardinfo->brd_type, boardinfo->brd_slot, boardinfo->brd_nasid,
	    boardinfo->brd_nic, boardinfo->brd_numcompts));

	kl_scan_board(boardinfo, KLSTRUCT_ANY, kl_first_pass_comp, NULL);
	return 0;
}

/*
 * Callback routine for the initial enumeration (components).
 * We are interested in cpu and memory information only, but display a few
 * other things if option DEBUG.
 */
int
kl_first_pass_comp(klinfo_t *comp, void *arg)
{
	klcpu_t *cpucomp;
	klmembnk_m_t *memcomp_m;
#ifdef DEBUG
	klhub_t *hubcomp;
	klmembnk_n_t *memcomp_n;
	klxbow_t *xbowcomp;
	int i;
#endif

	switch (comp->struct_type) {
	case KLSTRUCT_CPU:
		cpucomp = (klcpu_t *)comp;
		DB_PRF(("\tcpu type %x/%x %dMHz cache %dMB speed %dMHz\n",
		    cpucomp->cpu_prid, cpucomp->cpu_fpirr, cpucomp->cpu_speed,
		    cpucomp->cpu_scachesz, cpucomp->cpu_scachespeed));

		/*
		 * XXX this assumes the first cpu encountered is the boot
		 * XXX cpu.
		 */
		if (bootcpu_hwinfo.clock == 0) {
			bootcpu_hwinfo.c0prid = cpucomp->cpu_prid;
#if 0
			bootcpu_hwinfo.c1prid = cpucomp->cpu_fpirr;
#else
			bootcpu_hwinfo.c1prid = cpucomp->cpu_prid;
#endif
			bootcpu_hwinfo.clock = cpucomp->cpu_speed * 1000000;
			bootcpu_hwinfo.tlbsize = 64;
			bootcpu_hwinfo.type = (cpucomp->cpu_prid >> 8) & 0xff;
		} else
			ncpusfound++;
		break;

	case KLSTRUCT_MEMBNK:
		memcomp_m = (klmembnk_m_t *)comp;
#ifdef DEBUG
		memcomp_n = (klmembnk_n_t *)comp;
		DB_PRF(("\tmemory %dMB, select %x flags %x\n",
		    memcomp_m->membnk_memsz, memcomp_m->membnk_dimm_select,
		    kl_n_mode ?
		      memcomp_n->membnk_attr : memcomp_m->membnk_attr));

		if (kl_n_mode) {
			for (i = 0; i < MD_MEM_BANKS_N; i++) {
				if (memcomp_n->membnk_bnksz[i] == 0)
					continue;
				DB_PRF(("\t\tbank %d %dMB\n",
				    i + 1, memcomp_n->membnk_bnksz[i]));
			}
		} else {
			for (i = 0; i < MD_MEM_BANKS_M; i++) {
				if (memcomp_m->membnk_bnksz[i] == 0)
					continue;
				DB_PRF(("\t\tbank %d %dMB\n",
				    i + 1, memcomp_m->membnk_bnksz[i]));
			}
		}
#endif

		if (sys_config.system_type == SGI_IP27)
			kl_add_memory_ip27(comp->nasid, memcomp_m->membnk_bnksz,
			    kl_n_mode ? MD_MEM_BANKS_N : MD_MEM_BANKS_M);
		else
			kl_add_memory_ip35(comp->nasid, memcomp_m->membnk_bnksz,
			    kl_n_mode ? MD_MEM_BANKS_N : MD_MEM_BANKS_M);
		break;

#ifdef DEBUG
	case KLSTRUCT_HUB:
		hubcomp = (klhub_t *)comp;
		DB_PRF(("\thub widget %d port %d flag %d speed %dMHz\n",
		    hubcomp->hub_info.widid, hubcomp->hub_port.port_nasid,
		    hubcomp->hub_port.port_flag, hubcomp->hub_speed / 1000000));
		break;

	case KLSTRUCT_XBOW:
		xbowcomp = (klxbow_t *)comp;
		DB_PRF(("\txbow hub master link %d\n",
		    xbowcomp->xbow_hub_master_link));
		for (i = 0; i < MAX_XBOW_LINKS; i++) {
			if (xbowcomp->xbow_port_info[i].port_flag &
			    XBOW_PORT_ENABLE)
				DB_PRF(("\t\twidget %d nasid %d flg %u\n",
				    8 + i,
				    xbowcomp->xbow_port_info[i].port_nasid,
				    xbowcomp->xbow_port_info[i].port_flag));
		}
		break;

	default:
		DB_PRF(("\tcomponent widget %d type %d\n",
		    comp->widid, comp->struct_type));
		break;
#endif
	}
	return 0;
}

/*
 * Enumerate the boards of a node, and invoke a callback for those matching
 * the given class.
 */
int
kl_scan_node(int nasid, uint clss, int (*cb)(lboard_t *, void *), void *cbarg)
{
	lboard_t *boardinfo;

	for (boardinfo = IP27_KLFIRST_BOARD(nasid); boardinfo != NULL;
	    boardinfo = IP27_KLNEXT_BOARD(nasid, boardinfo)) {
		if (clss == KLBRD_ANY ||
		    (boardinfo->brd_type & IP27_BC_MASK) == clss) {
			if ((*cb)(boardinfo, cbarg) != 0)
				return 1;
		}
		if (boardinfo->brd_next == NULL)
			break;
	}

	return 0;
}

/*
 * Enumerate the components of a board, and invoke a callback for those
 * matching the given type.
 */
int
kl_scan_board(lboard_t *boardinfo, uint type, int (*cb)(klinfo_t *, void *),
    void *cbarg)
{
	klinfo_t *comp;
	int i;

	if (!ISSET(boardinfo->struct_type, LBOARD))
		return 0;

	for (i = 0; i < boardinfo->brd_numcompts; i++) {
		comp = IP27_UNCAC_ADDR(klinfo_t *, boardinfo->brd_nasid,
		    boardinfo->brd_compts[i]);

		if (!ISSET(comp->flags, KLINFO_ENABLED) ||
		    ISSET(comp->flags, KLINFO_FAILED))
			continue;

		if (type != KLSTRUCT_ANY && comp->struct_type != type)
			continue;

		if ((*cb)(comp, cbarg) != 0)
			return 1;
	}

	return 0;
}

/*
 * Return the console device information.
 */
console_t *
kl_get_console()
{
	kl_config_hdr_t *cfghdr = IP27_KLCONFIG_HDR(0);

	return &cfghdr->cons_info;
}

/*
 * Process memory bank information.
 * There are two different routines, because IP27 and IP35 do not
 * layout memory the same way.
 */

void
kl_add_memory_ip27(int16_t nasid, int16_t *sizes, unsigned int cnt)
{
	paddr_t basepa;
	uint64_t fp, lp, np;
	unsigned int seg, nmeg;

	/*
	 * On IP27, access to each DIMM is interleaved, which cause it to
	 * map to four banks on 128MB boundaries.
	 * DIMMs of 128MB or smaller map everything in the first bank,
	 * though.
	 */
	basepa = (paddr_t)nasid << kl_n_shift;
	while (cnt-- != 0) {
		nmeg = *sizes++;
		for (seg = 0; seg < 4; basepa += (1 << 27), seg++) {
			if (nmeg <= 128)
				np = seg == 0 ? nmeg : 0;
			else
				np = nmeg / 4;
			if (np == 0)
				continue;

			DB_PRF(("IP27 memory from %p to %p (%u MB)\n",
			    basepa, basepa + (np << 20), np));

			np = atop(np << 20);	/* MB to pages */
			fp = atop(basepa);
			lp = fp + np;

			/*
			 * We do not manage the first 32MB, so skip them here
			 * if necessary.
			 */
			if (fp < atop(32 << 20)) {
				fp = atop(32 << 20);
				if (fp >= lp)
					continue;
				np = lp - fp;
				physmem += atop(32 << 20);
			}

			/*
			 * XXX Temporary until there is a way to cope with
			 * XXX xbridge ATE shortage.
			 */
			if (fp >= atop(2UL << 30)) {
#if 0
				physmem += lp - fp;
#endif
				continue;
			}

			if (memrange_register(fp, lp,
			    ~(atop(1UL << kl_n_shift) - 1),
			    lp <= atop(2UL << 30) ?
			      VM_FREELIST_DEFAULT : VM_FREELIST_DMA32) != 0) {
				/*
				 * We could hijack the smallest segment here.
				 * But is it really worth doing?
				 */
				bios_printf("%u MB of memory could not be "
				    "managed, increase MAXMEMSEGS\n",
				    ptoa(np) >> 20);
			}
		}
	}
}

void
kl_add_memory_ip35(int16_t nasid, int16_t *sizes, unsigned int cnt)
{
	paddr_t basepa;
	uint32_t fp, lp, np;

	/*
	 * On IP35, the smallest memory DIMMs are 256MB, and the
	 * largest is 1GB. Memory is reported at 1GB intervals.
	 */

	basepa = (paddr_t)nasid << kl_n_shift;
	while (cnt-- != 0) {
		np = *sizes++;
		if (np != 0) {
			DB_PRF(("IP35 memory from %p to %p (%u MB)\n",
			    basepa, basepa + (np << 20), np));

			fp = atop(basepa);
			np = atop(np << 20);	/* MB to pages */
			lp = fp + np;

			/*
			 * We do not manage the first 64MB, so skip them here
			 * if necessary.
			 */
			if (fp < atop(64 << 20)) {
				fp = atop(64 << 20);
				if (fp >= lp)
					continue;
				np = lp - fp;
				physmem += atop(64 << 20);
			}

			/*
			 * XXX Temporary until there is a way to cope with
			 * XXX xbridge ATE shortage.
			 */
			if (fp >= atop(2UL << 30)) {
#if 0
				physmem += lp - fp;
#endif
				continue;
			}

			if (memrange_register(fp, lp,
			    ~(atop(1UL << kl_n_shift) - 1),
			    lp <= atop(2UL << 30) ?
			      VM_FREELIST_DEFAULT : VM_FREELIST_DMA32) != 0) {
				/*
				 * We could hijack the smallest segment here.
				 * But is it really worth doing?
				 */
				bios_printf("%u MB of memory could not be "
				    "managed, increase MAXMEMSEGS\n",
				    ptoa(np) >> 20);
			}
		}
		basepa += 1UL << 30;	/* 1 GB */
	}
}
