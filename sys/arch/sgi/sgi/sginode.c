/*	$OpenBSD: sginode.c,v 1.3 2009/05/02 21:26:05 miod Exp $	*/
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

int nextcpu = 0;

void	kl_do_boardinfo(lboard_t *);
void	kl_add_memory(int16_t *, unsigned int);

#ifdef DEBUG
#define	DB_PRF(x)	bios_printf x 
#else
#define	DB_PRF(x)
#endif

void
kl_scan_config(int node)
{
	lboard_t *boardinfo;
	kl_config_hdr_t *cfghdr;
	u_int64_t val;

	if (node == 0)
		physmem = 0;

	cfghdr = IP27_KLCONFIG_HDR(0);
	DB_PRF(("config @%p\n", cfghdr));
	DB_PRF(("magic %p version %x\n", cfghdr->magic, cfghdr->version));
	DB_PRF(("console %p baud %d\n", cfghdr->cons_info.uart_base,
	    cfghdr->cons_info.baud));

	val = IP27_LHUB_L(NI_STATUS_REV_ID);
	kl_n_mode = (val & NSRI_MORENODES_MASK) != 0;
        bios_printf("Machine is in %c mode.\n", kl_n_mode + 'M');

	val = IP27_LHUB_L(PI_REGION_PRESENT);
        DB_PRF(("Region present %p.\n", val));
	val = IP27_LHUB_L(PI_CALIAS_SIZE);
        DB_PRF(("Calias size %p.\n", val));

	for (boardinfo = IP27_KLFIRST_BOARD(0); boardinfo != NULL;
	    boardinfo = IP27_KLNEXT_BOARD(0, boardinfo)) {
		kl_do_boardinfo(boardinfo);
		if (boardinfo->brd_next == NULL)
			break;
	}

	if (nextcpu > MAX_CPUS) {
		bios_printf("%u processors found, increase MAX_CPUS\n",
		    nextcpu);
	}
}

void
kl_do_boardinfo(lboard_t *boardinfo)
{
	klinfo_t *comp;
	klcpu_t *cpucomp;
	klhub_t *hubcomp;
	klmembnk_m_t *memcomp_m;
	klmembnk_n_t *memcomp_n;
	klxbow_t *xbowcomp;
	struct cpuinfo *cpu;
	int i, j;

	DB_PRF(("board type %x slot %x nasid %x nic %p components %d\n",
	    boardinfo->brd_type, boardinfo->brd_slot, boardinfo->brd_nasid,
	    boardinfo->brd_nic, boardinfo->brd_numcompts));

	for (i = 0; i < boardinfo->brd_numcompts; i++) {
		comp = IP27_UNCAC_ADDR(klinfo_t *, 0, boardinfo->brd_compts[i]);

		switch(comp->struct_type) {
		case KLSTRUCT_CPU:
			cpucomp = (klcpu_t *)comp;
			DB_PRF(("\tcpu type %x/%x %dMhz cache %dMB speed %dMhz\n",
			    cpucomp->cpu_prid, cpucomp->cpu_fpirr,
			    cpucomp->cpu_speed,
			    cpucomp->cpu_scachesz, cpucomp->cpu_scachespeed));

			if (nextcpu < MAX_CPUS) {
				cpu = &sys_config.cpu[nextcpu];
				cpu->clock = cpucomp->cpu_speed * 1000000;
				cpu->type = (cpucomp->cpu_prid >> 8) & 0xff;
				cpu->vers_maj = (cpucomp->cpu_prid >> 4) & 0x0f;
				cpu->vers_min = cpucomp->cpu_prid & 0x0f;
				cpu->fptype = (cpucomp->cpu_fpirr >> 8) & 0xff;
				cpu->fpvers_maj =
				    (cpucomp->cpu_fpirr >> 4) & 0x0f;
				cpu->fpvers_min = cpucomp->cpu_fpirr & 0x0f;
				cpu->tlbsize = 64;
			}
			nextcpu++;
			break;

		case KLSTRUCT_HUB:
			hubcomp = (klhub_t *)comp;
			DB_PRF(("\thub widget %d port %d flag %d speed %dMHz\n",
			    hubcomp->hub_info.widid,
			    hubcomp->hub_port.port_nasid,
			    hubcomp->hub_port.port_flag,
			    hubcomp->hub_speed / 1000000));
			break;
			
		case KLSTRUCT_MEMBNK:
			memcomp_m = (klmembnk_m_t *)comp;
			memcomp_n = (klmembnk_n_t *)comp;
			DB_PRF(("\tmemory %dMB, select %x\n",
			    memcomp_m->membnk_memsz,
			    memcomp_m->membnk_dimm_select));

			if (kl_n_mode) {
				for (j = 0; j < MD_MEM_BANKS_N; j++) {
					if (memcomp_n->membnk_bnksz[j] == 0)
						continue;
					DB_PRF(("\t\tbank %d %dMB\n",
					    j + 1,
					    memcomp_n->membnk_bnksz[j]));
				}
			} else {
				for (j = 0; j < MD_MEM_BANKS_M; j++) {
					if (memcomp_m->membnk_bnksz[j] == 0)
						continue;
					DB_PRF(("\t\tbank %d %dMB\n",
					    j + 1,
					    memcomp_m->membnk_bnksz[j]));
				}
			}

			kl_add_memory(memcomp_m->membnk_bnksz,
			     kl_n_mode ? MD_MEM_BANKS_N : MD_MEM_BANKS_M);

			break;

		case KLSTRUCT_XBOW:
			xbowcomp = (klxbow_t *)comp;
			DB_PRF(("\txbow hub master link %d\n",
			    xbowcomp->xbow_hub_master_link));
			for (j = 0; j < MAX_XBOW_LINKS; j++) {
				if (xbowcomp->xbow_port_info[j].port_flag &
				    XBOW_PORT_ENABLE)
					DB_PRF(("\t\twidget %d nasid %d flg %u\n",
					    j,
					    xbowcomp->xbow_port_info[j].port_nasid,
					    xbowcomp->xbow_port_info[j].port_flag));
			}
			break;

		default:
			DB_PRF(("\tcomponent widget %d type %d\n",
			    comp->widid, comp->struct_type));
		}
	}

}

/*
 * Return the virtual address of the console device.
 */
vaddr_t
kl_get_console_base()
{
	kl_config_hdr_t *cfghdr = IP27_KLCONFIG_HDR(0);

	return (vaddr_t)cfghdr->cons_info.uart_base;
}

/*
 * Process memory bank information.
 */
void
kl_add_memory(int16_t *sizes, unsigned int cnt)
{
	int16_t nasid = 0;	/* XXX */
	paddr_t basepa;
	uint32_t fp, lp, np;
	unsigned int seg, descno, nmeg;
	struct phys_mem_desc *md;

	/*
	 * Access to each DIMM is interleaved, which cause it to map
	 * to four banks on 128MB boundaries.
	 * DIMMs of 128MB or smaller map everything in the first bank,
	 * though.
	 */
	basepa = nasid << (32 - kl_n_mode);
	while (cnt-- != 0) {
		nmeg = *sizes++;
		for (seg = 0; seg < 4; basepa += (1 << 27), seg++) {
			if (nmeg <= 128)
				np = seg == 0 ? nmeg : 0;
			else
				np = nmeg / 4;
			if (np == 0)
				continue;

			DB_PRF(("memory from %p to %p (%u MB)\n",
			    basepa, basepa + (np << 20), np));

			np = atop(np << 20);	/* MB to pages */
			fp = atop(basepa);
			lp = fp + np;

			/*
			 * ARCBios provided us with information on the
			 * first 32MB, so skip them here if necessary.
			 */
			if (fp < atop(32 << 20)) {
				fp = atop(32 << 20);
				if (fp >= lp)
					continue;
				np = lp - fp;
				physmem += atop(32 << 20);
			}

			/*
			 * Walk the existing segment list to find if we
			 * are adjacent to an existing segment, or the
			 * next free segment to use if not.
			 *
			 * Note that since we do not know in which order
			 * we'll find our nodes, we have to check for
			 * both boundaries, despite adding a given node's
			 * memory in increasing pa order.
			 */
			for (descno = 0, md = mem_layout; descno < MAXMEMSEGS;
			    descno++, md++) {
				if (md->mem_first_page == 0)
					break;

				if (md->mem_first_page == lp) {
					md->mem_first_page = fp;
					physmem += np;
					md = NULL;
					break;
				}

				if (md->mem_last_page == fp) {
					md->mem_last_page = lp;
					physmem += np;
					md = NULL;
					break;
				}
			}
			if (descno != MAXMEMSEGS && md != NULL) {
				md->mem_first_page = fp;
				md->mem_last_page = lp;
				physmem += np;
				md = NULL;
			}

			if (md != NULL) {
				/*
				 * We could hijack the smallest segment here.
				 * But is it really worth doing?
				 */
				bios_printf("%u MB of memory could not be "
				    "managed, increase MAXMEMSEGS\n",
				    atop(np) >> 20);
			}
		}
	}
}
