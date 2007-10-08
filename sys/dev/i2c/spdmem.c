/*	$OpenBSD: spdmem.c,v 1.3 2007/10/08 01:40:09 jsg Exp $	*/
/* $NetBSD: spdmem.c,v 1.3 2007/09/20 23:09:59 xtraeme Exp $ */

/*
 * Copyright (c) 2007 Nicolas Joly
 * Copyright (c) 2007 Paul Goyette
 * Copyright (c) 2007 Tobias Nygren
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Serial Presence Detect (SPD) memory identification
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/i2c/i2cvar.h>

/* possible values for the memory type */
#define	SPDMEM_MEMTYPE_FPM		0x01
#define	SPDMEM_MEMTYPE_EDO		0x02
#define	SPDMEM_MEMTYPE_PIPE_NIBBLE	0x03
#define	SPDMEM_MEMTYPE_SDRAM		0x04
#define	SPDMEM_MEMTYPE_ROM		0x05
#define	SPDMEM_MEMTYPE_DDRSGRAM		0x06
#define	SPDMEM_MEMTYPE_DDRSDRAM		0x07
#define	SPDMEM_MEMTYPE_DDR2SDRAM	0x08

/* possible values for the supply voltage */
#define	SPDMEM_VOLTAGE_TTL_5V		0x00
#define	SPDMEM_VOLTAGE_TTL_LV		0x01
#define	SPDMEM_VOLTAGE_HSTTL_1_5V	0x02
#define	SPDMEM_VOLTAGE_SSTL_3_3V	0x03
#define	SPDMEM_VOLTAGE_SSTL_2_5V	0x04
#define	SPDMEM_VOLTAGE_SSTL_1_8V	0x05

/* possible values for module configuration */
#define	SPDMEM_MODCONFIG_PARITY		0x01
#define	SPDMEM_MODCONFIG_ECC		0x02

/* for DDR2, module configuration is a bit-mask field */
#define	SPDMEM_MODCONFIG_HAS_DATA_PARITY	0x01
#define	SPDMEM_MODCONFIG_HAS_DATA_ECC		0x02
#define	SPDMEM_MODCONFIG_HAS_ADDR_CMD_PARITY	0x04

/* possible values for the refresh field */
#define	SPDMEM_REFRESH_STD		0x00
#define	SPDMEM_REFRESH_QUARTER		0x01
#define	SPDMEM_REFRESH_HALF		0x02
#define	SPDMEM_REFRESH_TWOX		0x03
#define	SPDMEM_REFRESH_FOURX		0x04
#define	SPDMEM_REFRESH_EIGHTX		0x05
#define	SPDMEM_REFRESH_SELFREFRESH	0x80

/* superset types */
#define	SPDMEM_SUPERSET_ESDRAM		0x01
#define	SPDMEM_SUPERSET_DDR_ESDRAM	0x02
#define	SPDMEM_SUPERSET_EDO_PEM		0x03
#define	SPDMEM_SUPERSET_SDR_PEM		0x04

/* FPM and EDO DIMMS */
#define SPDMEM_FPM_ROWS			0x00
#define SPDMEM_FPM_COLS			0x01
#define SPDMEM_FPM_BANKS		0x02
#define SPDMEM_FPM_CONFIG		0x08
#define SPDMEM_FPM_REFRESH		0x09
#define SPDMEM_FPM_SUPERSET		0x0c

/* PC66/PC100/PC133 SDRAM */
#define SPDMEM_SDR_ROWS			0x00
#define SPDMEM_SDR_COLS			0x01
#define SPDMEM_SDR_BANKS		0x02
#define SPDMEM_SDR_BANKS_PER_CHIP	0x0e
#define SPDMEM_SDR_SUPERSET		0x1d

/* Dual Data Rate SDRAM */
#define SPDMEM_DDR_ROWS			0x00
#define SPDMEM_DDR_COLS			0x01
#define SPDMEM_DDR_RANKS		0x02
#define SPDMEM_DDR_DATAWIDTH		0x03
#define SPDMEM_DDR_VOLTAGE		0x05
#define SPDMEM_DDR_CYCLE		0x06
#define SPDMEM_DDR_REFRESH		0x09
#define SPDMEM_DDR_BANKS_PER_CHIP	0x0e
#define SPDMEM_DDR_CAS			0x0f
#define SPDMEM_DDR_MOD_ATTRIB		0x12
#define SPDMEM_DDR_SUPERSET		0x1d

#define SPDMEM_DDR_ATTRIB_REG		(1 << 1)

/* Dual Data Rate 2 SDRAM */
#define SPDMEM_DDR2_ROWS		0x00
#define SPDMEM_DDR2_COLS		0x01
#define SPDMEM_DDR2_RANKS		0x02
#define SPDMEM_DDR2_DATAWIDTH		0x03
#define SPDMEM_DDR2_VOLTAGE		0x05
#define SPDMEM_DDR2_CYCLE		0x06
#define SPDMEM_DDR2_BANKS_PER_CHIP	0x0e
#define SPDMEM_DDR2_DIMMTYPE		0x11

#define SPDMEM_DDR2_TYPE_REGMASK	((1 << 4) | (1 << 0))

/* Direct Rambus DRAM */
#define SPDMEM_RDR_ROWS_COLS		0x00

struct spdmem {
	uint8_t sm_len;
	uint8_t sm_size;
	uint8_t sm_type;
	uint8_t sm_data[60];
	uint8_t	sm_cksum;
} __packed;

#define SPDMEM_TYPE_MAXLEN 16
struct spdmem_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;
	struct spdmem	sc_spd_data;
	char		sc_type[SPDMEM_TYPE_MAXLEN];
};

int		 spdmem_match(struct device *, void *, void *);
void		 spdmem_attach(struct device *, struct device *, void *);
uint8_t		 spdmem_read(struct spdmem_softc *, uint8_t);
void		 spdmem_hexdump(struct spdmem_softc *, int, int);

struct cfattach spdmem_ca = {
	sizeof(struct spdmem_softc), spdmem_match, spdmem_attach
};

struct cfdriver spdmem_cd = {
	NULL, "spdmem", DV_DULL
};

#define IS_RAMBUS_TYPE (s->sm_len < 4)

static const char* spdmem_basic_types[] = {
	"unknown",
	"FPM",
	"EDO",
	"Pipelined Nibble",
	"SDRAM",
	"ROM",
	"DDR SGRAM",
	"DDR SDRAM",
	"DDR2 SDRAM",
	"DDR2 SDRAM FB",
	"DDR2 SDRAM FB Probe"
};

static const char* spdmem_superset_types[] = {
	"unknown",
	"ESDRAM",
	"DDR ESDRAM",
	"PEM EDO",
	"PEM SDRAM"
};

static const char* spdmem_parity_types[] = {
	"non-parity",
	"data parity",
	"ECC",
	"data parity and ECC",
	"cmd/addr parity",
	"cmd/addr/data parity",
	"cmd/addr parity, data ECC",
	"cmd/addr/data parity, data ECC"
};

int
spdmem_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	
	if (strcmp(ia->ia_name, "spd") == 0)
		return (1);
	return (0);
}

void
spdmem_attach(struct device *parent, struct device *self, void *aux)
{
	struct spdmem_softc *sc = (struct spdmem_softc *)self;
	struct i2c_attach_args *ia = aux;
	struct spdmem *s = &(sc->sc_spd_data);
	const char *type;
	const char *ddr_type_string = NULL;
	int num_banks = 0;
	int per_chip = 0;
	int dimm_size, cycle_time, d_clk, p_clk, bits;
	int i;
	uint8_t config, rows, cols, cl;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	/* All SPD have at least 64 bytes of data including checksum */
	for (i = 0; i < 64; i++) {
		((uint8_t *)s)[i] = spdmem_read(sc, i);
	}

#if 0
	for (i = 0; i < 64;  i += 16) {
		int j;
		printf("\n%s: 0x%02x:", self->dv_xname, i);
		for(j = 0; j < 16; j++)
			printf(" %02x", ((uint8_t *)s)[i + j]);
	}
	printf("\n%s", self->dv_xname);
#endif

	/*
	 * Decode and print SPD contents
	 */
	if (IS_RAMBUS_TYPE)
		type = "Rambus";
	else {
		if (s->sm_type <= 10)
			type = spdmem_basic_types[s->sm_type];
		else
			type = "unknown";

		if (s->sm_type == SPDMEM_MEMTYPE_EDO &&
		    s->sm_data[SPDMEM_FPM_SUPERSET] == SPDMEM_SUPERSET_EDO_PEM)
			type = spdmem_superset_types[SPDMEM_SUPERSET_EDO_PEM];
		if (s->sm_type == SPDMEM_MEMTYPE_SDRAM &&
		    s->sm_data[SPDMEM_SDR_SUPERSET] == SPDMEM_SUPERSET_SDR_PEM)
			type = spdmem_superset_types[SPDMEM_SUPERSET_SDR_PEM];
		if (s->sm_type == SPDMEM_MEMTYPE_DDRSDRAM &&
		    s->sm_data[SPDMEM_DDR_SUPERSET] ==
		    SPDMEM_SUPERSET_DDR_ESDRAM)
			type =
			    spdmem_superset_types[SPDMEM_SUPERSET_DDR_ESDRAM];
		if (s->sm_type == SPDMEM_MEMTYPE_SDRAM &&
		    s->sm_data[SPDMEM_SDR_SUPERSET] == SPDMEM_SUPERSET_ESDRAM) {
			type = spdmem_superset_types[SPDMEM_SUPERSET_ESDRAM];
		}
	}

	dimm_size = 0;
	if (IS_RAMBUS_TYPE) {
		rows = s->sm_data[SPDMEM_RDR_ROWS_COLS] & 0x0f;
		cols = s->sm_data[SPDMEM_RDR_ROWS_COLS] >> 4;
		printf(": %dMB", 1 << (rows + cols - 13));
	} else if (s->sm_type == SPDMEM_MEMTYPE_SDRAM ||
	    s->sm_type == SPDMEM_MEMTYPE_DDRSDRAM) {
		rows = s->sm_data[SPDMEM_SDR_ROWS] & 0x0f;
		cols = s->sm_data[SPDMEM_SDR_COLS] & 0x0f;
		dimm_size = rows + cols - 17;
		num_banks = s->sm_data[SPDMEM_SDR_BANKS];
		per_chip = s->sm_data[SPDMEM_SDR_BANKS_PER_CHIP];
	} else if (s->sm_type == SPDMEM_MEMTYPE_DDR2SDRAM) {
		rows = s->sm_data[SPDMEM_DDR2_ROWS] & 0x1f;
		cols = s->sm_data[SPDMEM_DDR2_COLS] & 0x0f;
		dimm_size = rows + cols - 17;
		num_banks = s->sm_data[SPDMEM_DDR_RANKS] + 1;
		per_chip = s->sm_data[SPDMEM_DDR2_BANKS_PER_CHIP];
	}
	if (!(IS_RAMBUS_TYPE) && num_banks <= 8 && per_chip <= 8 &&
	    dimm_size > 0 && dimm_size <= 12) {
		dimm_size = (1 << dimm_size) * num_banks * per_chip;
		printf(": %dMB", dimm_size);
	}

	printf(" %s", type);
	strlcpy(sc->sc_type, type, SPDMEM_TYPE_MAXLEN);

	if (((s->sm_type == SPDMEM_MEMTYPE_DDRSDRAM) && 
	     (s->sm_data[SPDMEM_DDR_MOD_ATTRIB] & SPDMEM_DDR_ATTRIB_REG)) ||
	    ((s->sm_type == SPDMEM_MEMTYPE_DDR2SDRAM) &&
	     (s->sm_data[SPDMEM_DDR2_DIMMTYPE] & SPDMEM_DDR2_TYPE_REGMASK)))
		printf(" registered");

	if ((s->sm_type == SPDMEM_MEMTYPE_SDRAM ||
	     s->sm_type == SPDMEM_MEMTYPE_DDRSDRAM ||
	     s->sm_type == SPDMEM_MEMTYPE_DDR2SDRAM ) &&
	    s->sm_data[SPDMEM_FPM_CONFIG] < 8)
		printf(" %s",
		    spdmem_parity_types[s->sm_data[SPDMEM_FPM_CONFIG]]);

	/* cycle_time is expressed in units of 0.01 ns */
	cycle_time = 0;
	if (s->sm_type == SPDMEM_MEMTYPE_DDRSDRAM ||
	    s->sm_type == SPDMEM_MEMTYPE_SDRAM)
		cycle_time = (s->sm_data[SPDMEM_DDR_CYCLE] >> 4) * 100 +
		    (s->sm_data[SPDMEM_DDR_CYCLE] & 0x0f) * 10;
	else if (s->sm_type == SPDMEM_MEMTYPE_DDR2SDRAM) {
		cycle_time = (s->sm_data[SPDMEM_DDR_CYCLE] >> 4) * 100 +
		    (s->sm_data[SPDMEM_DDR_CYCLE] & 0x0f);
	}

	if (cycle_time != 0) {
		/*
		 * cycle time is scaled by a factor of 100 to avoid using
		 * floating point.  Calculate memory speed as the number
		 * of cycles per microsecond.
		 */
		d_clk = 100 * 1000;
		config = s->sm_data[SPDMEM_FPM_CONFIG];
		switch (s->sm_type) {
		case SPDMEM_MEMTYPE_DDR2SDRAM:
			/* DDR2 uses quad-pumped clock */
			d_clk *= 4;
			bits = s->sm_data[SPDMEM_DDR2_DATAWIDTH];
			if ((config & 0x03) != 0)
				bits -= 8;
			ddr_type_string = "PC2";
			break;
		case SPDMEM_MEMTYPE_DDRSDRAM:
			/* DDR uses dual-pumped clock */
			d_clk *= 2;
			/* FALLTHROUGH */
		default:	/* SPDMEM_MEMTYPE_SDRAM */
			bits = s->sm_data[SPDMEM_DDR_DATAWIDTH] |
			    (s->sm_data[SPDMEM_DDR_DATAWIDTH + 1] << 8);
			if (config == 1 || config == 2)
				bits -= 8;
			ddr_type_string = "PC";
		}
		d_clk /= cycle_time;
		if (s->sm_type == SPDMEM_MEMTYPE_DDR2SDRAM)
			d_clk = (d_clk + 1) / 2;
		p_clk = d_clk * bits / 8;
		if ((p_clk % 100) >= 50)
			p_clk += 50;
		p_clk -= p_clk % 100;
		printf(" %s%d", ddr_type_string, p_clk);
	}

	if (s->sm_type == SPDMEM_MEMTYPE_DDRSDRAM) {
		for (i = 6; i >= 0; i--) {
			if (s->sm_data[SPDMEM_DDR_CAS] & (1 << i)) {
				cl = ((i * 10) / 2) + 10;
				printf("CL%d.%d", cl / 10, cl % 10);
				break;
			}
		}
	} else if (s->sm_type == SPDMEM_MEMTYPE_DDR2SDRAM) {
		for (i = 5; i >= 2; i--) {
			if (s->sm_data[SPDMEM_DDR_CAS] & (i << i)) {
				printf("CL%d", i);
				break;
			}
		}
	}

	printf("\n");
}

uint8_t
spdmem_read(struct spdmem_softc *sc, uint8_t reg)
{
	uint8_t val;

	iic_acquire_bus(sc->sc_tag,0);
	iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr, &reg, 1,
		 &val, 1, 0);
	iic_release_bus(sc->sc_tag, 0);

	return val;
}

void
spdmem_hexdump(struct spdmem_softc *sc, int start, int size)
{
	int i;
	
	uint8_t data[size];

	for (i = 0; i < size;  i++) {
		data[i] = spdmem_read(sc, i);
	}

	for (i = 0; i < size ; i += 16) {
		int j;
		printf("\n0x%02x:", start + i);
		for(j = 0; j < 16; j++)
			printf(" %02x", ((uint8_t *)data)[i + j]);
	}
	printf("\n");
}
