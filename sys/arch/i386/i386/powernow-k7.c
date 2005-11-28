/* $OpenBSD: powernow-k7.c,v 1.9 2005/11/28 17:48:02 mickey Exp $ */
/*
 * Copyright (c) 2004 Martin Végiard.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * Copyright (c) 2004-2005 Bruno Ducrot
 * Copyright (c) 2004 FUKUDA Nobuhiko <nfukuda@spa.is.uec.ac.jp>
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

/* AMD POWERNOW K7 driver */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <i386/isa/isa_machdep.h>

#define BIOS_START		0xe0000
#define	BIOS_LEN		0x20000

/*
 * MSRs and bits used by Powernow technology
 */
#define MSR_AMDK7_FIDVID_CTL		0xc0010041
#define MSR_AMDK7_FIDVID_STATUS		0xc0010042

/* Bitfields used by K7 */

#define PN7_CTR_FID(x)			((x) & 0x1f)
#define PN7_CTR_VID(x)			(((x) & 0x1f) << 8)
#define PN7_CTR_FIDC			0x00010000
#define PN7_CTR_VIDC			0x00020000
#define PN7_CTR_FIDCHRATIO		0x00100000
#define PN7_CTR_SGTC(x)			(((uint64_t)(x) & 0x000fffff) << 32)

#define PN7_STA_CFID(x)			((x) & 0x1f)
#define PN7_STA_SFID(x)			(((x) >> 8) & 0x1f)
#define PN7_STA_MFID(x)			(((x) >> 16) & 0x1f)
#define PN7_STA_CVID(x)			(((x) >> 32) & 0x1f)
#define PN7_STA_SVID(x)			(((x) >> 40) & 0x1f)
#define PN7_STA_MVID(x)			(((x) >> 48) & 0x1f)

/*
 * ACPI ctr_val status register to powernow k7 configuration
 */
#define ACPI_PN7_CTRL_TO_VID(x)		(((x) >> 5) & 0x1f)
#define ACPI_PN7_CTRL_TO_SGTC(x)	(((x) >> 10) & 0xffff)

#define WRITE_FIDVID(fid, vid, ctrl)	\
	wrmsr(MSR_AMDK7_FIDVID_CTL,	\
	    (((ctrl) << 32) | (1ULL << 16) | ((vid) << 8) | (fid)))

/*
 * Divide each value by 10 to get the processor multiplier.
 * Taken from powernow-k7.c/Linux by Dave Jones
 */
static int k7pnow_fid_to_mult[32] = {
	110, 115, 120, 125, 50, 55, 60, 65,
	70, 75, 80, 85, 90, 95, 100, 105,
	30, 190, 40, 200, 130, 135, 140, 210,
	150, 225, 160, 165, 170, 180, -1, -1
};

/*
 * Units are in mV.
 */

/*
 * Mobile VRM (K7)
 */
static int k7pnow_mobile_vid_to_volts[] = {
	2000, 1950, 1900, 1850, 1800, 1750, 1700, 1650,
	1600, 1550, 1500, 1450, 1400, 1350, 1300, 0,
	1275, 1250, 1225, 1200, 1175, 1150, 1125, 1100,
	1075, 1050, 1025, 1000, 975, 950, 925, 0,
};

/*
 * Desktop VRM (K7)
 */

static int k7pnow_desktop_vid_to_volts[] = {
	2000, 1950, 1900, 1850, 1800, 1750, 1700, 1650,
	1600, 1550, 1500, 1450, 1400, 1350, 1300, 0,
	1275, 1250, 1225, 1200, 1175, 1150, 1125, 1100,
	1075, 1050, 1025, 1000, 975, 950, 925, 0,
};

#define POWERNOW_MAX_STATES		16

struct k7pnow_state {
	int freq;
	int fid;
	int vid;
};

struct k7pnow_cpu_state {
	unsigned int fsb;
	unsigned int sgtc;
	struct k7pnow_state state_table[POWERNOW_MAX_STATES];
	unsigned int n_states;
	int errata_a0;
	int *vid_to_volts;
};

struct psb_s {
	char signature[10];     /* AMDK7PNOW! */
	uint8_t version;
	uint8_t flags;
	uint16_t ttime;         /* Min Settling time */
	uint8_t reserved;
	uint8_t n_pst;
};

struct pst_s {
	uint32_t signature;
	uint8_t fsb;		/* Front Side Bus frequency (Mhz) */
	uint8_t fid;		/* Max Frequency code */
	uint8_t vid;		/* Max Voltage code */
	uint8_t n_states;	/* Number of states */
};

struct k7pnow_cpu_state * k7pnow_current_state[I386_MAXPROCS];

/*
 * Prototypes
 */
int k7pnow_decode_pst(struct k7pnow_cpu_state *, uint8_t *, int);
int k7pnow_states(struct k7pnow_cpu_state *, uint32_t, unsigned int, unsigned int);

int
k7_powernow_setperf(int level)
{
	unsigned int i, low, high, freq;
	int cvid, cfid, vid = 0, fid = 0;
	uint64_t status, ctl;
	struct k7pnow_cpu_state * cstate;

	cstate = k7pnow_current_state[cpu_number()];
	high = cstate->state_table[cstate->n_states - 1].freq;
	low = cstate->state_table[0].freq;
	freq = low + (high - low) * level / 100;

	for (i = 0; i < cstate->n_states; i++) {
		if (cstate->state_table[i].freq >= freq) {
			fid = cstate->state_table[i].fid;
			vid = cstate->state_table[i].vid;
			break;
		}
	}

	if (fid == 0 || vid == 0)
		return (0);

	status = rdmsr(MSR_AMDK7_FIDVID_STATUS);
	cfid = PN7_STA_CFID(status);
	cvid = PN7_STA_CVID(status);

	/*
	 * We're already at the requested level.
	 */
	if (fid == cfid && vid == cvid)
		return (0);

	ctl = rdmsr(MSR_AMDK7_FIDVID_CTL) & PN7_CTR_FIDCHRATIO;

	ctl |= PN7_CTR_FID(fid);
	ctl |= PN7_CTR_VID(vid);
	ctl |= PN7_CTR_SGTC(cstate->sgtc);

	if (cstate->errata_a0)
		disable_intr();

	if (k7pnow_fid_to_mult[fid] < k7pnow_fid_to_mult[cfid]) {
		wrmsr(MSR_AMDK7_FIDVID_CTL, ctl | PN7_CTR_FIDC);
		if (vid != cvid)
			wrmsr(MSR_AMDK7_FIDVID_CTL, ctl | PN7_CTR_VIDC);
	} else {
		wrmsr(MSR_AMDK7_FIDVID_CTL, ctl | PN7_CTR_VIDC);
		if (fid != cfid)
			wrmsr(MSR_AMDK7_FIDVID_CTL, ctl | PN7_CTR_FIDC);
	}

	if (cstate->errata_a0)
		enable_intr();

	pentium_mhz = ((cstate->state_table[i].freq / 100000)+1)*100;

	return 1;
}

/*
 * Given a set of pair of fid/vid, and number of performance states,
 * compute state_table via an insertion sort.
 */
int
k7pnow_decode_pst(struct k7pnow_cpu_state * cstate, uint8_t *p, int npst)
{
	int i, j, n;
	struct k7pnow_state state;

	for (i = 0; i < POWERNOW_MAX_STATES; ++i)
		cstate->state_table[i].freq = -1;

	for (n = 0, i = 0; i < npst; ++i) {
		state.fid = *p++;
		state.vid = *p++;
		state.freq = 100 * k7pnow_fid_to_mult[state.fid] * cstate->fsb;
		if (cstate->errata_a0 &&
		    (k7pnow_fid_to_mult[state.fid] % 10) == 5)
			continue;

		j = n;
		while (j > 0 && cstate->state_table[j - 1].freq > state.freq) {
			memcpy(&cstate->state_table[j],
			    &cstate->state_table[j - 1],
			    sizeof(struct k7pnow_state));
			--j;
		}
		memcpy(&cstate->state_table[j], &state,
		    sizeof(struct k7pnow_state));
		++n;
	}
	/*
	 * Fix powernow_max_states, if errata_a0 give us less states
	 * than expected.
	 */
	cstate->n_states = n;
	return 1;
}

int
k7pnow_states(struct k7pnow_cpu_state *cstate, uint32_t cpusig,
    unsigned int fid, unsigned int vid)
{
	int maxpst;
	struct psb_s *psb;
	struct pst_s *pst;
	uint8_t *p;

	/*
	 * Look in the 0xe0000 - 0x100000 physical address
	 * range for the pst tables; 16 byte blocks
	 */
	for (p = (u_int8_t *)ISA_HOLE_VADDR(BIOS_START);
	    p < (u_int8_t *)ISA_HOLE_VADDR(BIOS_START + BIOS_LEN); p+= 16) {
		if (memcmp(p, "AMDK7PNOW!", 10) == 0) {
			psb = (struct psb_s *)p;
			if (psb->version != 0x12)
				return 0;

			cstate->sgtc = psb->ttime * cstate->fsb;
			if (cstate->sgtc < 100 * cstate->fsb)
				cstate->sgtc = 100 * cstate->fsb;

			p += sizeof(struct psb_s);

			for (maxpst = 0; maxpst < 200; maxpst++) {
				pst = (struct pst_s*) p;

				if (cpusig == pst->signature && fid == pst->fid
				    && vid == pst->vid) {
					switch(pst->signature) {
					case 0x760:
					case 0x761:
					case 0x762:
					case 0x770:
					case 0x771:
					case 0x780:
					case 0x781:
					case 0x7a0:
						break;
					default:
						return 0;
					}

					if (abs(cstate->fsb - pst->fsb) > 5)
						continue;
					cstate->n_states = pst->n_states;
					return (k7pnow_decode_pst(cstate,
					     p + sizeof(struct pst_s),
					     cstate->n_states));
				}
				p += sizeof(struct pst_s) + (2 * pst->n_states);
			}
		}
	}

	return 0;
}

void
k7_powernow_init(void)
{
	u_int regs[4];
	uint64_t status;
	u_int maxfid, startvid, currentfid;
	struct k7pnow_cpu_state *cstate;
	struct k7pnow_state *state;
	struct cpu_info *ci;
	char *techname = NULL;
	int i;

	ci = curcpu();

	cstate = malloc(sizeof(struct k7pnow_cpu_state), M_DEVBUF, M_NOWAIT);
	if (!cstate)
		return;

	cpuid(0x80000001, regs);
	if ((regs[0] & 0xfff) == 0x760)
		cstate->errata_a0 = TRUE;
	else
		cstate->errata_a0 = FALSE;

	status = rdmsr(MSR_AMDK7_FIDVID_STATUS);
	maxfid = PN7_STA_MFID(status);
	startvid = PN7_STA_SVID(status);
	currentfid = PN7_STA_CFID(status);

	CPU_CLOCKUPDATE();
	cstate->fsb = pentium_base_tsc / 100000 / k7pnow_fid_to_mult[currentfid];
	/*
	 * If start FID is different to max FID, then it is a
	 * mobile processor.  If not, it is a low powered desktop
	 * processor.
	 */
	if (maxfid != currentfid) {
		cstate->vid_to_volts = k7pnow_mobile_vid_to_volts;
		techname = "PowerNow! K7";
	} else {
		cstate->vid_to_volts = k7pnow_desktop_vid_to_volts;
		techname = "Cool`n'Quiet K7";
	}
	if (k7pnow_states(cstate, ci->ci_signature, maxfid, startvid)) {
		if (cstate->n_states) {
			printf("%s: AMD %s: available states ", 
			    ci->ci_dev.dv_xname, techname);
			for(i = 0; i < cstate->n_states; i++) {
				state = &cstate->state_table[i];
				printf("%c%d", i==0 ? '(' : ',',
				    ((state->freq / 100000)+1)*100);
			}
			printf(")\n");	
			k7pnow_current_state[cpu_number()] = cstate;
			cpu_setperf = k7_powernow_setperf;
		}
	}
}
