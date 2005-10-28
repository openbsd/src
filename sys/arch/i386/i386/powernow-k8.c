/*	$OpenBSD: powernow-k8.c,v 1.1 2005/10/28 07:03:41 tedu Exp $ */
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
/* AMD POWERNOW K8 driver */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <dev/isa/isareg.h>
#include <i386/isa/isa_machdep.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/bus.h>

#define BIOS_START		0xe0000
#define	BIOS_LEN		0x20000

/*
 * MSRs and bits used by Powernow technology
 */
#define MSR_AMDK7_FIDVID_CTL		0xc0010041
#define MSR_AMDK7_FIDVID_STATUS		0xc0010042

/* Bitfields used by K8 */

#define PN8_CTR_FID(x)			((x) & 0x3f)
#define PN8_CTR_VID(x)			(((x) & 0x1f) << 8)
#define PN8_CTR_PENDING(x)		(((x) & 1) << 32)

#define PN8_STA_CFID(x)			((x) & 0x3f)
#define PN8_STA_SFID(x)			(((x) >> 8) & 0x3f)
#define PN8_STA_MFID(x)			(((x) >> 16) & 0x3f)
#define PN8_STA_PENDING(x)		(((x) >> 31) & 0x01)
#define PN8_STA_CVID(x)			(((x) >> 32) & 0x1f)
#define PN8_STA_SVID(x)			(((x) >> 40) & 0x1f)
#define PN8_STA_MVID(x)			(((x) >> 48) & 0x1f)

/* Reserved1 to powernow k8 configuration */
#define PN8_PSB_TO_RVO(x)		((x) & 0x03)
#define PN8_PSB_TO_IRT(x)		(((x) >> 2) & 0x03)
#define PN8_PSB_TO_MVS(x)		(((x) >> 4) & 0x03)
#define PN8_PSB_TO_BATT(x)		(((x) >> 6) & 0x03)

/* ACPI ctr_val status register to powernow k8 configuration */
#define ACPI_PN8_CTRL_TO_FID(x)		((x) & 0x3f)
#define ACPI_PN8_CTRL_TO_VID(x)		(((x) >> 6) & 0x1f)
#define ACPI_PN8_CTRL_TO_VST(x)		(((x) >> 11) & 0x1f)
#define ACPI_PN8_CTRL_TO_MVS(x)		(((x) >> 18) & 0x03)
#define ACPI_PN8_CTRL_TO_PLL(x)		(((x) >> 20) & 0x7f)
#define ACPI_PN8_CTRL_TO_RVO(x)		(((x) >> 28) & 0x03)
#define ACPI_PN8_CTRL_TO_IRT(x)		(((x) >> 30) & 0x03)

#define WRITE_FIDVID(fid, vid, ctrl)	\
	wrmsr(MSR_AMDK7_FIDVID_CTL,	\
	    (((ctrl) << 32) | (1ULL << 16) | ((vid) << 8) | (fid)))


#define COUNT_OFF_IRT(irt)	DELAY(10 * (1 << (irt)))
#define COUNT_OFF_VST(vst)	DELAY(20 * (vst))

#define FID_TO_VCO_FID(fid)	\
	(((fid) < 8) ? (8 + ((fid) << 1)) : (fid))

/*
 * Divide each value by 10 to get the processor multiplier.
 */

static int pn8_fid_to_mult[32] = {
	40, 50, 60, 70, 80, 90, 100, 110,
	120, 130, 140, 150, 160, 170, 180, 190,
	220, 230, 240, 250, 260, 270, 280, 290,
	300, 310, 320, 330, 340, 350,
};

/*
 * Units are in mV.
 */

/* Desktop and Mobile VRM (K8) */
static int pn8_vid_to_volts[] = {
	1550, 1525, 1500, 1475, 1450, 1425, 1400, 1375,
	1350, 1325, 1300, 1275, 1250, 1225, 1200, 1175,
	1150, 1125, 1100, 1075, 1050, 1025, 1000, 975,
	950, 925, 900, 875, 850, 825, 800, 0,
};

#define POWERNOW_MAX_STATES		16

struct k8pnow_state {
	int freq;
	int fid;
	int vid;
};

struct k8pnow_cpu_state {
	struct k8pnow_state state_table[POWERNOW_MAX_STATES];
	unsigned int n_states;
	unsigned int fsb;
	unsigned int sgtc;
	unsigned int vst;
	unsigned int mvs;
	unsigned int pll;
	unsigned int rvo;
	unsigned int irt;
	int low;
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

struct k8pnow_cpu_state *k8pnow_current_state[I386_MAXPROCS];

/*
 * Prototypes
 */
int k8pnow_read_pending_wait(uint64_t *);
int k8pnow_decode_pst(struct k8pnow_cpu_state *, uint8_t *, int);
int k8pnow_states(struct k8pnow_cpu_state *, uint32_t, unsigned int,
    unsigned int);


int k8pnow_read_pending_wait(uint64_t * status) {
	unsigned int i = 0;
	while(PN8_STA_PENDING(*status)) {
		i++;
		if (i > 0x1000000) {
			printf("k8pnow_read_pending_wait: change pending stuck"
			    ".\n");
			return 1;
		}
		*status = rdmsr(MSR_AMDK7_FIDVID_STATUS);
	}
	return 0;
}

int
k8_powernow_setperf(int level)
{
	unsigned int i, low, high, freq;
	uint64_t status;
	int cfid, cvid, fid = 0, vid = 0;
	int rvo;
	u_int val;
	struct k8pnow_cpu_state *cstate;

	/*
	 * We dont do a k8pnow_read_pending_wait here, need to ensure that the
	 * change pending bit isn't stuck,
	 */
	status = rdmsr(MSR_AMDK7_FIDVID_STATUS);
	if (PN8_STA_PENDING(status))
		return 1;
	cfid = PN8_STA_CFID(status);
	cvid = PN8_STA_CVID(status);

	cstate = k8pnow_current_state[cpu_number()];
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

	if (fid == cfid && vid == cvid)
		return (0);

	/*
	 * Phase 1: Raise core voltage to requested VID if frequency is
	 * going up.
	 */
	while (cvid > vid) {
		val = cvid - (1 << cstate->mvs);
		WRITE_FIDVID(cfid, (val > 0) ? val : 0, 1ULL);
		if (k8pnow_read_pending_wait(&status))
			return 1;
		cvid = PN8_STA_CVID(status);
		COUNT_OFF_VST(cstate->vst);
	}

	/* ... then raise to voltage + RVO (if required) */
	for (rvo = cstate->rvo; rvo > 0 && cvid > 0; --rvo) {
		/* XXX It's not clear from spec if we have to do that
		 * in 0.25 step or in MVS.  Therefore do it as it's done
		 * under Linux */
		WRITE_FIDVID(cfid, cvid - 1, 1ULL);
		if (k8pnow_read_pending_wait(&status))
			return 1;
		cvid = PN8_STA_CVID(status);
		COUNT_OFF_VST(cstate->vst);
	}

	/* Phase 2: change to requested core frequency */
	if (cfid != fid) {
		u_int vco_fid, vco_cfid;

		vco_fid = FID_TO_VCO_FID(fid);
		vco_cfid = FID_TO_VCO_FID(cfid);

		while (abs(vco_fid - vco_cfid) > 2) {
			if (fid > cfid) {
				if (cfid > 6)
					val = cfid + 2;
				else
					val = FID_TO_VCO_FID(cfid) + 2;
			} else
				val = cfid - 2;
			WRITE_FIDVID(val, cvid, cstate->pll *
			    (uint64_t)cstate->fsb);

			if (k8pnow_read_pending_wait(&status))
				return 1;
			cfid = PN8_STA_CFID(status);
			COUNT_OFF_IRT(cstate->irt);

			vco_cfid = FID_TO_VCO_FID(cfid);
		}

		WRITE_FIDVID(fid, cvid, cstate->pll * (uint64_t)cstate->fsb);
		if (k8pnow_read_pending_wait(&status))
			return 1;
		cfid = PN8_STA_CFID(status);
		COUNT_OFF_IRT(cstate->irt);
	}

	/* Phase 3: change to requested voltage */
	if (cvid != vid) {
		WRITE_FIDVID(cfid, vid, 1ULL);
		if (k8pnow_read_pending_wait(&status))
			return 1;
		cvid = PN8_STA_CVID(status);
		COUNT_OFF_VST(cstate->vst);
	}

	/* Check if transition failed. */
	if (cfid != fid || cvid != vid)
		return (1);

	calibrate_cyclecounter();
	return (0);
}

/*
 * Given a set of pair of fid/vid, and number of performance states,
 * compute state_table via an insertion sort.
 */
int
k8pnow_decode_pst(struct k8pnow_cpu_state *cstate, uint8_t *p,
	int npst)
{
	int i, j, n;
	struct k8pnow_state state;

	for (i = 0; i < POWERNOW_MAX_STATES; ++i)
		cstate->state_table[i].freq = -1;

	for (n = 0, i = 0; i < npst; ++i) {
		state.fid = *p++;
		state.vid = *p++;

		state.freq = 100 * pn8_fid_to_mult[state.fid >> 1] *
			cstate->fsb;
		j = n;
		while (j > 0 && cstate->state_table[j - 1].freq < state.freq) {
			memcpy(&cstate->state_table[j],
			    &cstate->state_table[j - 1],
			    sizeof(struct k8pnow_state));
			--j;
		}
		memcpy(&cstate->state_table[j], &state,
		    sizeof(struct k8pnow_state));
		n++;
	}
	return 1;
}

int
k8pnow_states(struct k8pnow_cpu_state *cstate, uint32_t cpusig,
    unsigned int fid, unsigned int vid)
{
	int maxpst;
	struct psb_s *psb;
	struct pst_s *pst;
	uint8_t *p;

	for (p = (u_int8_t *)ISA_HOLE_VADDR(BIOS_START);
	    p < (u_int8_t *)ISA_HOLE_VADDR(BIOS_START + BIOS_LEN); p += 16) {
		if (memcmp(p, "AMDK7PNOW!", 10) == 0) {
			psb = (struct psb_s *)p;
			if (psb->version != 0x14)
				return 0;

			cstate->vst = psb->ttime;
			cstate->rvo = PN8_PSB_TO_RVO(psb->reserved);
			cstate->irt = PN8_PSB_TO_IRT(psb->reserved);
			cstate->mvs = PN8_PSB_TO_MVS(psb->reserved);
			cstate->low = PN8_PSB_TO_BATT(psb->reserved);

			p += sizeof(struct psb_s);
			/*
			 * XXX: FreeBSD completely ignores psb->n_pst
			 * XXX: n_pst might be 2 its not supposed to be
			 * XXX: but why 200?
			 */
			for (maxpst = 0; maxpst < 200; maxpst++) {
				pst = (struct pst_s*) p;
				if (cpusig == pst->signature && fid == pst->fid
				   && vid == pst->vid) {
					cstate->n_states = pst->n_states;
					return (k8pnow_decode_pst(cstate,
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
k8_powernow_init(void)
{
	uint64_t status, rate;
	u_int maxfid, maxvid, currentfid;
	struct k8pnow_cpu_state *cstate;
	struct cpu_info * ci;
	char * techname = NULL;
	ci = curcpu();

	cstate = malloc(sizeof(struct k8pnow_cpu_state), M_TEMP, M_WAITOK);

	rate = pentium_mhz;
	status = rdmsr(MSR_AMDK7_FIDVID_STATUS);
	maxfid = PN8_STA_MFID(status);
	maxvid = PN8_STA_MVID(status);
	currentfid = PN8_STA_CFID(status);

	cstate->fsb = rate / 100000 / pn8_fid_to_mult[currentfid >> 1];
	cstate->vid_to_volts = pn8_vid_to_volts;
	/*
	* If start FID is different to max FID, then it is a
	* mobile processor.  If not, it is a low powered desktop
	* processor.
	*/
	if (PN8_STA_SFID(status) != PN8_STA_MFID(status))
		techname = "PowerNow! K8";
	else
		techname = "Cool`n'Quiet K8";

	if (k8pnow_states(cstate, ci->ci_signature, maxfid, maxvid)) {
		printf("%s: AMD %s: %d available states\n", ci->ci_dev.dv_xname,
		    techname, cstate->n_states);
		if (cstate->n_states) {
			k8pnow_current_state[cpu_number()] = cstate;
			cpu_setperf = k8_powernow_setperf;
		}
	}
}
