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
 */

/* AMD POWERNOW K7 driver */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <dev/isa/isareg.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#if 0
/*	WTF?	*/
#define BIOS_START		0xe0000
#define BIOS_END		0x20000
#define BIOS_LEN		BIOS_END - BIOS_START
#endif
#define BIOS_START		0xe0000
#define	BIOS_LEN		0x20000

#define MSR_K7_CTL		0xC0010041
#define CTL_SET_FID		0x0000000000010000ULL
#define CTL_SET_VID		0x0000000000020000ULL

#define cpufreq(x)	k7pnow_fsb * k7pnow_fid_codes[x] / 10

struct psb_s {
	char signature[10];	/* AMDK7PNOW! */
	uint8_t version;
	uint8_t flags;
	uint16_t ttime;		/* Min Settling time */
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

struct state_s {
	uint8_t fid;		/* Frequency code */
	uint8_t vid;		/* Voltage code */
};

struct k7pnow_freq_table_s {
	unsigned int frequency;
	struct state_s *state;
};

/* Taken from powernow-k7.c/Linux by Dave Jones */
int k7pnow_fid_codes[32] = {
	110, 115, 120, 125, 50, 55, 60, 65,
	70, 75, 80, 85, 90, 95, 100, 105,
	30, 190, 40, 200, 130, 135, 140, 210,
	150, 225, 160, 165, 170, 180, -1, -1
};

/* Static variables */
unsigned int k7pnow_fsb;
unsigned int k7pnow_cur_freq;
unsigned int k7pnow_ttime;
unsigned int k7pnow_nstates;
struct k7pnow_freq_table_s *k7pnow_freq_table;


/* Prototypes */
struct state_s *k7_powernow_getstates(uint32_t);

struct state_s *
k7_powernow_getstates(uint32_t signature)
{
	unsigned int i, j;
	struct psb_s *psb;
	struct pst_s *pst;
	char *ptr;

	/*
	 * Look in the 0xe0000 - 0x20000 physical address
	 * range for the pst tables; 16 byte blocks
	 */
	if (bus_space_map(I386_BUS_SPACE_MEM, BIOS_START, BIOS_LEN, 0,
	    (bus_space_handle_t *)&ptr)) {
		printf("k7_powernow: couldn't map BIOS\n");
		return NULL;
	}

	for (i = 0; i < BIOS_LEN; i += 16, ptr += 16) {
		if (memcmp(ptr, "AMDK7PNOW!", 10) == 0) {
			psb = (struct psb_s *) ptr;
			ptr += sizeof(struct psb_s);

			k7pnow_ttime = psb->ttime;

			/* Only this version is supported */
			if (psb->version != 0x12)
				return 0;

			/* Find the right PST */
			for (j = 0; j < psb->n_pst; j++) {
				pst = (struct pst_s *) ptr;
				ptr += sizeof(struct pst_s);

				/* Use the first PST with matching CPUID */
				if (signature == pst->signature) {
					/*
					 * XXX I need more info on this.
					 * For now, let's just ignore it
					 */
					if ((signature & 0xFF) == 0x60)
						return 0;

					k7pnow_fsb = pst->fsb;
					k7pnow_nstates = pst->n_states;
					return (struct state_s *)ptr;
				} else
					ptr += sizeof(struct state_s) *
					    pst->n_states;
			}
			/* printf("No match was found for your CPUID\n"); */
			return 0;
		}
	}
	/* printf("Power state table not found\n"); */
	return 0;
}

int
k7_powernow_setperf(int level)
{
	unsigned int low, high, freq, i;
	uint32_t sgtc, vid = 0, fid = 0;
	uint64_t ctl;

	high = k7pnow_freq_table[k7pnow_nstates - 1].frequency;
	low = k7pnow_freq_table[0].frequency;
	freq = low + (high - low) * level / 100;

	for (i = 0; i < k7pnow_nstates; i++) {
		/* Do we know how to set that frequency? */
		if (k7pnow_freq_table[i].frequency >= freq) {
			fid = k7pnow_freq_table[i].state->fid;
			vid = k7pnow_freq_table[i].state->vid;
			break;
		}
	}

	if (fid == 0 || vid == 0)
		return (-1);

	/* Get CTL and only modify fid/vid/sgtc */
	ctl = rdmsr(MSR_K7_CTL);

	/* FID */
	ctl &= 0xFFFFFFFFFFFFFF00ULL;
	ctl |= fid;

	/* VID */
	ctl &= 0xFFFFFFFFFFFF00FFULL;
	ctl |= vid << 8;

	/* SGTC */
	if ((sgtc = k7pnow_ttime * 100) < 10000) sgtc = 10000;
	ctl &= 0xFFF00000FFFFFFFFULL;
	ctl |= (uint64_t)sgtc << 32;

	if (k7pnow_cur_freq > freq) {
		wrmsr(MSR_K7_CTL, ctl | CTL_SET_FID);
		wrmsr(MSR_K7_CTL, ctl | CTL_SET_VID);
	} else {
		wrmsr(MSR_K7_CTL, ctl | CTL_SET_VID);
		wrmsr(MSR_K7_CTL, ctl | CTL_SET_FID);
	}
	ctl = rdmsr(MSR_K7_CTL);
	return (0);
}

void
k7_powernow_init(uint32_t signature)
{
	unsigned int i, freq_names_len, len = 0;
	struct state_s *s;
	char *freq_names;

	s = k7_powernow_getstates(signature);
	if (s == 0)
		return;

	freq_names_len =  k7pnow_nstates * (sizeof("9999 ")-1) + 1;
	freq_names = malloc(freq_names_len, M_TEMP, M_WAITOK);

	k7pnow_freq_table = malloc(sizeof(struct k7pnow_freq_table_s) *
	    k7pnow_nstates, M_TEMP, M_WAITOK);

	for (i = 0; i < k7pnow_nstates; i++, s++) {
		k7pnow_freq_table[i].frequency = cpufreq(s->fid);
		k7pnow_freq_table[i].state = s;

		/* XXX len += snprintf is an illegal idiom */ 
		len += snprintf(freq_names + len, freq_names_len - len, "%d%s",
		    k7pnow_freq_table[i].frequency,
		    i < k7pnow_nstates - 1 ? " " : "");
	}

	/* On bootup the frequency should be at it's max */
	k7pnow_cur_freq = k7pnow_freq_table[i-1].frequency;

	printf("cpu0: AMD POWERNOW Available frequencies (Mhz): %s\n",
	    freq_names);
	cpu_setperf = k7_powernow_setperf;
}
