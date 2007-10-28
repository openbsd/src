/*	$OpenBSD: envy.c,v 1.2 2007/10/28 18:25:21 fgsch Exp $	*/
/*
 * Copyright (c) 2007 Alexandre Ratchov <alex@caoua.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <sys/malloc.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/envyvar.h>
#include <dev/pci/envyreg.h>
#include <dev/audio_if.h>
#include <machine/bus.h>

#ifdef ENVY_DEBUG
#define DPRINTF(...) do { if (envydebug) printf(__VA_ARGS__); } while(0)
#define DPRINTFN(n, ...) do { if (envydebug > (n)) printf(__VA_ARGS__); } while(0)
int envydebug = 1;
#else
#define DPRINTF(...) do {} while(0)
#define DPRINTFN(n, ...) do {} while(0)
#endif
#define DEVNAME(sc) ((sc)->dev.dv_xname)

int  envymatch(struct device *, void *, void *);
void envyattach(struct device *, struct device *, void *);
int  envydetach(struct device *, int);

int  envy_ccs_read(struct envy_softc *, int);
void envy_ccs_write(struct envy_softc *, int, int);
int  envy_cci_read(struct envy_softc *, int);
void envy_cci_write(struct envy_softc *, int, int);
void envy_i2c_wait(struct envy_softc *);
int  envy_i2c_read(struct envy_softc *, int, int);
void envy_i2c_write(struct envy_softc *, int, int, int);
int  envy_gpio_read(struct envy_softc *);
void envy_gpio_write(struct envy_softc *, int);
void envy_eeprom_read(struct envy_softc *, unsigned char *);
void envy_reset(struct envy_softc *);
int  envy_ak_read(struct envy_softc *, int, int);
void envy_ak_write(struct envy_softc *, int, int, int);
int  envy_intr(void *);

int envy_lineout_getsrc(struct envy_softc *, int);
void envy_lineout_setsrc(struct envy_softc *, int, int);
int envy_spdout_getsrc(struct envy_softc *, int);
void envy_spdout_setsrc(struct envy_softc *, int, int);
void envy_mon_getvol(struct envy_softc *, int, int *, int *);
void envy_mon_setvol(struct envy_softc *, int, int, int);

int envy_open(void *, int);
void envy_close(void *);
void *envy_allocm(void *, int, size_t, int, int);
void envy_freem(void *, void *, int);
int envy_query_encoding(void *, struct audio_encoding *);
int envy_set_params(void *, int, int, struct audio_params *, 
    struct audio_params *);
int envy_round_blocksize(void *, int);
size_t envy_round_buffersize(void *, int, size_t);
int envy_trigger_output(void *, void *, void *, int,
    void (*)(void *), void *, struct audio_params *);
int envy_trigger_input(void *, void *, void *, int,
    void (*)(void *), void *, struct audio_params *);
int envy_halt_output(void *);
int envy_halt_input(void *);
int envy_getdev(void *, struct audio_device *);
int envy_query_devinfo(void *, struct mixer_devinfo *);
int envy_get_port(void *, struct mixer_ctrl *);
int envy_set_port(void *, struct mixer_ctrl *);
int envy_get_props(void *);

struct cfattach envy_ca = {
	sizeof(struct envy_softc), envymatch, envyattach, envydetach
};

struct cfdriver envy_cd = {
	NULL, "envy", DV_DULL
};

struct audio_hw_if envy_hw_if = {
	envy_open,		/* open */
	envy_close,		/* close */
	NULL,			/* drain */
	envy_query_encoding,	/* query_encoding */
	envy_set_params,	/* set_params */
	envy_round_blocksize,	/* round_blocksize */
	NULL,			/* commit_settings */
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	envy_halt_output,	/* halt_output */
	envy_halt_input,	/* halt_input */
	NULL,			/* speaker_ctl */
	envy_getdev,		/* getdev */
	NULL,			/* setfd */
	envy_set_port,		/* set_port */
	envy_get_port,		/* get_port */
	envy_query_devinfo,	/* query_devinfo */
	envy_allocm,		/* malloc */
	envy_freem,		/* free */
	envy_round_buffersize,	/* round_buffersize */
	NULL,			/* mappage */
	envy_get_props,		/* get_props */
	envy_trigger_output,	/* trigger_output */
	envy_trigger_input,	/* trigger_input */
};

/*
 * correspondence between rates (in frames per second)
 * and values of rate register
 */
struct {
	int rate, reg;
} envy_rates[] = {
	{ 8000, 0x6}, { 9600, 0x3}, {11025, 0xa}, {12000, 2}, {16000, 5},
	{22050, 0x9}, {24000, 0x1}, {32000, 0x4}, {44100, 8}, {48000, 0},
	{64000, 0xf}, {88200, 0xb}, {96000, 0x7}, {-1, -1}
};

int
envy_ccs_read(struct envy_softc *sc, int reg) 
{
	return bus_space_read_1(sc->ccs_iot, sc->ccs_ioh, reg);
}

void
envy_ccs_write(struct envy_softc *sc, int reg, int val)
{
	bus_space_write_1(sc->ccs_iot, sc->ccs_ioh, reg, val);
}

int
envy_cci_read(struct envy_softc *sc, int index)
{
	int val;
	envy_ccs_write(sc, ENVY_CCI_INDEX, index);
	val = envy_ccs_read(sc, ENVY_CCI_DATA);
	return val;
}

void
envy_cci_write(struct envy_softc *sc, int index, int data)
{
	envy_ccs_write(sc, ENVY_CCI_INDEX, index);
	envy_ccs_write(sc, ENVY_CCI_DATA, data);
}

void
envy_i2c_wait(struct envy_softc *sc)
{
	int timeout = 50, st;

        for (;;) {
		st = envy_ccs_read(sc, ENVY_I2C_CTL);
		if (!(st & ENVY_I2C_CTL_BUSY)) 
			break;
		if (timeout == 0) {
			printf("%s: i2c busy timeout\n", DEVNAME(sc));
			break;
		}
		delay(50);
		timeout--;
	}
}

int
envy_i2c_read(struct envy_softc *sc, int dev, int addr)
{
	envy_i2c_wait(sc);
	envy_ccs_write(sc, ENVY_I2C_ADDR, addr);
	envy_i2c_wait(sc);
	envy_ccs_write(sc, ENVY_I2C_DEV, dev << 1);
	envy_i2c_wait(sc);
	return envy_ccs_read(sc, ENVY_I2C_DATA);
}

void
envy_i2c_write(struct envy_softc *sc, int dev, int addr, int data)
{
	if (dev == 0x50) {
		printf("%s: writing on eeprom is evil...\n", DEVNAME(sc));
		return;
	}
	envy_i2c_wait(sc);
	envy_ccs_write(sc, ENVY_I2C_ADDR, addr);
	envy_i2c_wait(sc);
	envy_ccs_write(sc, ENVY_I2C_DATA, data);
	envy_i2c_wait(sc);
	envy_ccs_write(sc, ENVY_I2C_DEV, (dev << 1) | 1);
}

void
envy_eeprom_read(struct envy_softc *sc, unsigned char *eeprom)
{
	int i;

	for (i = 0; i < ENVY_EEPROM_MAXSZ; i++) {
		eeprom[i] = envy_i2c_read(sc, ENVY_I2C_DEV_EEPROM, i);
	}
#ifdef ENVY_DEBUG
	printf("%s: eeprom: ", DEVNAME(sc));
	for (i = 0; i < ENVY_EEPROM_MAXSZ; i++) {
		printf(" %02x", (unsigned)eeprom[i]);
	}
	printf("\n");
#endif
}

int
envy_ak_read(struct envy_softc *sc, int dev, int addr) {
	return sc->ak[dev].reg[addr];
}

void
envy_ak_write(struct envy_softc *sc, int dev, int addr, int data)
{
	int bits, i, reg;

	sc->ak[dev].reg[addr] = data;

	reg = envy_cci_read(sc, ENVY_GPIO_DATA);
	reg &= ~ENVY_GPIO_CSMASK;
	reg |=  ENVY_GPIO_CS(dev);
	envy_cci_write(sc, ENVY_GPIO_DATA, reg);
	delay(1);

	bits  = 0xa000 | (addr << 8) | data;
	for (i = 0; i < 16; i++) {
		reg &= ~(ENVY_GPIO_CLK | ENVY_GPIO_DOUT);
		reg |= (bits & 0x8000) ? ENVY_GPIO_DOUT : 0;
		envy_cci_write(sc, ENVY_GPIO_DATA, reg);
		delay(1);

		reg |= ENVY_GPIO_CLK;
		envy_cci_write(sc, ENVY_GPIO_DATA, reg);
		delay(1);
		bits <<= 1;
	}

	reg |= ENVY_GPIO_CSMASK;
	envy_cci_write(sc, ENVY_GPIO_DATA, reg);
	delay(1);
}

void
envy_reset(struct envy_softc *sc)
{
	char eeprom[ENVY_EEPROM_MAXSZ];
	int dev;

	/*
	 * full reset
	 */
	envy_ccs_write(sc, ENVY_CTL, ENVY_CTL_RESET | ENVY_CTL_NATIVE);
	delay(200);
	envy_ccs_write(sc, ENVY_CTL, ENVY_CTL_NATIVE);
	delay(200);

	/*
	 * read config from eprom and write it to registers
	 */
	envy_eeprom_read(sc, eeprom);
	pci_conf_write(sc->pci_pc, sc->pci_tag, ENVY_CONF, 
	    eeprom[ENVY_EEPROM_CONF] |
	    (eeprom[ENVY_EEPROM_ACLINK] << 8) |
	    (eeprom[ENVY_EEPROM_I2S] << 16) |
	    (eeprom[ENVY_EEPROM_SPDIF] << 24));
	envy_cci_write(sc, ENVY_GPIO_MASK, eeprom[ENVY_EEPROM_GPIOMASK]);
	envy_cci_write(sc, ENVY_GPIO_DIR,  eeprom[ENVY_EEPROM_GPIODIR]);
	envy_cci_write(sc, ENVY_GPIO_DATA, eeprom[ENVY_EEPROM_GPIOST]);

	DPRINTF("%s: gpio_mask = %02x\n", DEVNAME(sc), 
		envy_cci_read(sc, ENVY_GPIO_MASK));
	DPRINTF("%s: gpio_dir = %02x\n", DEVNAME(sc), 
		envy_cci_read(sc, ENVY_GPIO_DIR));
	DPRINTF("%s: gpio_state = %02x\n", DEVNAME(sc), 
		envy_cci_read(sc, ENVY_GPIO_DATA));
	
	/*
	 * reset ak4524 codecs
	 */
	for (dev = 0; dev < 4; dev++) {
		envy_ak_write(sc, dev, AK_RST, 0x0);
		delay(300);
		envy_ak_write(sc, dev, AK_RST, AK_RST_AD | AK_RST_DA);
		envy_ak_write(sc, dev, AK_FMT, AK_FMT_IIS24);
	}

	/*
	 * clear all interrupts and unmask used ones
	 */ 
	envy_ccs_write(sc, ENVY_CCS_INTSTAT, 0xff);
	envy_ccs_write(sc, ENVY_CCS_INTMASK, ~ENVY_CCS_INT_MT);
}

int
envy_intr(void *self)
{
	struct envy_softc *sc = (struct envy_softc *)self;
	int st;

	st = bus_space_read_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_INTR);
	if (!(st & (ENVY_MT_INTR_PACK | ENVY_MT_INTR_RACK))) {
		return 0;
	}
	if (st & ENVY_MT_INTR_PACK) {
		st = ENVY_MT_INTR_PACK;
		bus_space_write_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_INTR, st);
		sc->ointr(sc->oarg);
	}
	if (st & ENVY_MT_INTR_RACK) {
		st = ENVY_MT_INTR_RACK;
		bus_space_write_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_INTR, st);
		sc->iintr(sc->iarg);
	}
	return 1;
}

int
envy_lineout_getsrc(struct envy_softc *sc, int out) {
	int reg, shift, src;

	reg = bus_space_read_2(sc->mt_iot, sc->mt_ioh, ENVY_MT_OUTSRC);
	DPRINTF("%s: outsrc=%x\n", DEVNAME(sc), reg);
	shift = (out  & 1) ? (out & ~1) + 8 : out;
	src = (reg >> shift) & 3;
	if (src == ENVY_MT_OUTSRC_DMA) {
		return ENVY_MIX_OUTSRC_DMA;
	} else if (src == ENVY_MT_OUTSRC_MON) {
		return ENVY_MIX_OUTSRC_MON;
	}
	reg = bus_space_read_4(sc->mt_iot, sc->mt_ioh, ENVY_MT_INSEL);
	DPRINTF("%s: insel=%x\n", DEVNAME(sc), reg);
	reg = (reg >> (out * 4)) & 0xf;
	if (src == ENVY_MT_OUTSRC_LINE)
		return ENVY_MIX_OUTSRC_LINEIN + (reg & 7);
	else
		return ENVY_MIX_OUTSRC_SPDIN + (reg >> 3);
}

void
envy_lineout_setsrc(struct envy_softc *sc, int out, int src) {
	int reg, shift, mask, sel;
	
	if (src < ENVY_MIX_OUTSRC_DMA) {
		/* 
		 * linein and spdin are used as output source so we
		 * must select the input source channel number
		 */
		if (src < ENVY_MIX_OUTSRC_SPDIN)
			sel = src - ENVY_MIX_OUTSRC_LINEIN;
		else
			sel = (src - ENVY_MIX_OUTSRC_SPDIN) << 3;

		shift = out * ENVY_MT_INSEL_BITS;
		mask = ENVY_MT_INSEL_MASK << shift;
		reg = bus_space_read_4(sc->mt_iot, sc->mt_ioh, ENVY_MT_INSEL);
		reg = (reg & ~mask) | (sel << shift);
		bus_space_write_4(sc->mt_iot, sc->mt_ioh, ENVY_MT_INSEL, reg);
		DPRINTF("%s: insel <- %x\n", DEVNAME(sc), reg);
	}

	/*
	 * set the lineout route register
	 */
	if (src < ENVY_MIX_OUTSRC_SPDIN) {
		sel = ENVY_MT_OUTSRC_LINE;
	} else if (src < ENVY_MIX_OUTSRC_DMA) {
		sel = ENVY_MT_OUTSRC_SPD;
	} else if (src == ENVY_MIX_OUTSRC_DMA) {
		sel = ENVY_MT_OUTSRC_DMA;
	} else {
		sel = ENVY_MT_OUTSRC_MON;
	}
	shift = (out  & 1) ? (out & ~1) + 8 : out;
	mask = ENVY_MT_INSEL_MASK << shift;
	reg = bus_space_read_2(sc->mt_iot, sc->mt_ioh, ENVY_MT_OUTSRC);
	reg = (reg & ~mask) | (sel << shift);
	bus_space_write_2(sc->mt_iot, sc->mt_ioh, ENVY_MT_OUTSRC, reg);
	DPRINTF("%s: outsrc <- %x\n", DEVNAME(sc), reg);
}


int
envy_spdout_getsrc(struct envy_softc *sc, int out) {
	int reg, src, sel;

	reg = bus_space_read_2(sc->mt_iot, sc->mt_ioh, ENVY_MT_SPDROUTE);
	DPRINTF("%s: spdroute=%x\n", DEVNAME(sc), reg);
	src = (out == 0) ? reg : reg >> 2;
	src &= ENVY_MT_SPDSRC_MASK;
	if (src == ENVY_MT_SPDSRC_DMA) {
		return ENVY_MIX_OUTSRC_DMA;
	} else if (src == ENVY_MT_SPDSRC_MON) {
		return ENVY_MIX_OUTSRC_MON;
	}

	sel = (out == 0) ? reg >> 8 : reg >> 12;
	sel &= ENVY_MT_SPDSEL_MASK;
	if (src == ENVY_MT_SPDSRC_LINE)
		return ENVY_MIX_OUTSRC_LINEIN + (sel & 7);
	else
		return ENVY_MIX_OUTSRC_SPDIN + (sel >> 3);
}

void
envy_spdout_setsrc(struct envy_softc *sc, int out, int src) {
	int reg, shift, mask, sel;
	
	reg = bus_space_read_2(sc->mt_iot, sc->mt_ioh, ENVY_MT_SPDROUTE);
	if (src < ENVY_MIX_OUTSRC_DMA) {
		/* 
		 * linein and spdin are used as output source so we
		 * must select the input source channel number
		 */
		if (src < ENVY_MIX_OUTSRC_SPDIN)
			sel = src - ENVY_MIX_OUTSRC_LINEIN;
		else
			sel = (src - ENVY_MIX_OUTSRC_SPDIN) << 3;

		shift = 8 + out * ENVY_MT_SPDSEL_BITS;
		mask = ENVY_MT_SPDSEL_MASK << shift;
		reg = (reg & ~mask) | (sel << shift);
	}

	/*
	 * set the lineout route register
	 */
	if (src < ENVY_MIX_OUTSRC_SPDIN) {
		sel = ENVY_MT_OUTSRC_LINE;
	} else if (src < ENVY_MIX_OUTSRC_DMA) {
		sel = ENVY_MT_OUTSRC_SPD;
	} else if (src == ENVY_MIX_OUTSRC_DMA) {
		sel = ENVY_MT_OUTSRC_DMA;
	} else {
		sel = ENVY_MT_OUTSRC_MON;
	}
	shift = out * 2;
	mask = ENVY_MT_SPDSRC_MASK << shift;
	reg = (reg & ~mask) | (sel << shift);
	bus_space_write_2(sc->mt_iot, sc->mt_ioh, ENVY_MT_SPDROUTE, reg);
	DPRINTF("%s: spdroute <- %x\n", DEVNAME(sc), reg);
}

void
envy_mon_getvol(struct envy_softc *sc, int idx, int *l, int *r) {
	int reg;

	bus_space_write_2(sc->mt_iot, sc->mt_ioh, ENVY_MT_MONIDX, idx);
	reg = bus_space_read_2(sc->mt_iot, sc->mt_ioh, ENVY_MT_MONDATA);
	*l = 0x7f - ((reg) & 0x7f);
	*r = 0x7f - ((reg >> 8) & 0x7f);
}

void
envy_mon_setvol(struct envy_softc *sc, int idx, int l, int r) {
	int reg;

	bus_space_write_2(sc->mt_iot, sc->mt_ioh, ENVY_MT_MONIDX, idx);
	reg = (0x7f - l) | ((0x7f - r) << 8);
	DPRINTF("%s: mon=%d <- %d,%d\n", DEVNAME(sc), reg, l, r);
	bus_space_write_2(sc->mt_iot, sc->mt_ioh, ENVY_MT_MONDATA, reg);
}

int
envymatch(struct device *parent, void *match, void *aux) {
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ICENSEMBLE &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ICENSEMBLE_ICE1712) {
		return 1;
	}
	return 0;
}

void
envyattach(struct device *parent, struct device *self, void *aux)
{
	struct envy_softc *sc = (struct envy_softc *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	pci_intr_handle_t ih;
	const char *intrstr;

	sc->pci_tag = pa->pa_tag;
	sc->pci_pc = pa->pa_pc;
	sc->pci_dmat = pa->pa_dmat;
	sc->pci_ih = NULL;
	sc->ibuf.addr = sc->obuf.addr = NULL;
	sc->ccs_iosz = 0;
	sc->mt_iosz = 0;

	if (pci_mapreg_map(pa, ENVY_CTL_BAR, PCI_MAPREG_TYPE_IO, 0, 
			   &sc->ccs_iot, &sc->ccs_ioh, NULL, &sc->ccs_iosz, 0)) {
		printf(": failed to map ctl i/o space\n");
		sc->ccs_iosz = 0;
		return;
        }
	if (pci_mapreg_map(pa, ENVY_MT_BAR, PCI_MAPREG_TYPE_IO, 0, 
			   &sc->mt_iot, &sc->mt_ioh, NULL, &sc->mt_iosz, 0)) {
		printf(": failed to map mt i/o space\n");
		sc->mt_iosz = 0;
		return;
        }
	if (pci_intr_map(pa, &ih)) {
		printf(": can't map interrupt\n");
	}
	intrstr = pci_intr_string(sc->pci_pc, ih);
	sc->pci_ih = pci_intr_establish(sc->pci_pc, ih, IPL_AUDIO,
	    envy_intr, sc, sc->dev.dv_xname);
	if (sc->pci_ih == NULL) {
		printf(": can't establish interrupt");
		if (intrstr)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s\n", intrstr);
	envy_reset(sc);
	sc->audio = audio_attach_mi(&envy_hw_if, sc, &sc->dev);
}

int
envydetach(struct device *self, int flags)
{
	struct envy_softc *sc = (struct envy_softc *)self;

	if (sc->pci_ih != NULL) {
		pci_intr_disestablish(sc->pci_pc, sc->pci_ih);
		sc->pci_ih = NULL;
	}
	if (sc->ccs_iosz) {
		bus_space_unmap(sc->ccs_iot, sc->ccs_ioh, sc->ccs_iosz);
	}
	if (sc->mt_iosz) {
		bus_space_unmap(sc->ccs_iot, sc->mt_ioh, sc->mt_iosz);
	}
	return 0;
}

int
envy_open(void *self, int flags)
{
	return 0;
}

void
envy_close(void *self)
{
}

void *
envy_allocm(void *self, int dir, size_t size, int type, int flags)
{
	struct envy_softc *sc = (struct envy_softc *)self;
	int err, rsegs, basereg, wait;
	struct envy_buf *buf;

	if (dir == AUMODE_RECORD) {
		buf = &sc->ibuf;
		basereg = ENVY_MT_RADDR;
	} else {
		buf = &sc->obuf;
		basereg = ENVY_MT_PADDR;
	}
	if (buf->addr != NULL) {
		DPRINTF("%s: multiple alloc, dir = %d\n", DEVNAME(sc), dir);
		return NULL;
	}
	buf->size = size;
	wait = (flags & M_NOWAIT) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK;

#define ENVY_ALIGN	4
#define ENVY_BOUNDARY	0

	err = bus_dmamem_alloc(sc->pci_dmat, buf->size, ENVY_ALIGN, 
	    ENVY_BOUNDARY, &buf->seg, 1, &rsegs, wait);
	if (err) {
		DPRINTF("%s: dmamem_alloc: failed %d\n", DEVNAME(sc), err);
		goto err_ret;
	}

	err = bus_dmamem_map(sc->pci_dmat, &buf->seg, rsegs, buf->size, 
            &buf->addr, wait | BUS_DMA_COHERENT);
	if (err) {
		DPRINTF("%s: dmamem_map: failed %d\n", DEVNAME(sc), err);
		goto err_free;
	}
	
	err = bus_dmamap_create(sc->pci_dmat, buf->size, 1, buf->size, 0,
	    wait, &buf->map);
	if (err) {
		DPRINTF("%s: dmamap_create: failed %d\n", DEVNAME(sc), err);
		goto err_unmap;
	}
	
	err = bus_dmamap_load(sc->pci_dmat, buf->map, buf->addr, 
            buf->size, NULL, wait);
	if (err) {
		DPRINTF("%s: dmamap_load: failed %d\n", DEVNAME(sc), err);
		goto err_destroy;
	}
	bus_space_write_4(sc->mt_iot, sc->mt_ioh, basereg, buf->seg.ds_addr);
	DPRINTF("%s: allocated %d bytes dir=%d, ka=%p, da=%p\n", 
		DEVNAME(sc), buf->size, dir, buf->addr, buf->seg.ds_addr);
	return buf->addr;

 err_destroy:
	bus_dmamap_destroy(sc->pci_dmat, buf->map);	
 err_unmap:
	bus_dmamem_unmap(sc->pci_dmat, buf->addr, buf->size);
 err_free:
	bus_dmamem_free(sc->pci_dmat, &buf->seg, 1);
 err_ret:
	return NULL;	
}

void
envy_freem(void *self, void *addr, int type)
{
	struct envy_buf *buf;
	struct envy_softc *sc = (struct envy_softc *)self;
	int dir;

	if (sc->ibuf.addr == addr) {
		buf = &sc->ibuf;
		dir = AUMODE_RECORD;
	} else if (sc->obuf.addr == addr) {
		buf = &sc->obuf;
		dir = AUMODE_PLAY;
	} else {
		DPRINTF("%s: no buf to free\n", DEVNAME(sc));
		return;
	}
	bus_dmamap_destroy(sc->pci_dmat, buf->map);	
	bus_dmamem_unmap(sc->pci_dmat, buf->addr, buf->size);
	bus_dmamem_free(sc->pci_dmat, &buf->seg, 1);
	buf->addr = NULL;
	DPRINTF("%s: freed buffer (mode=%d)\n", DEVNAME(sc), dir);
}

int
envy_query_encoding(void *self, struct audio_encoding *enc)
{
	if (enc->index == 0) {
		strlcpy(enc->name, AudioEslinear_le, sizeof(enc->name));
		enc->encoding = AUDIO_ENCODING_SLINEAR_LE;
		enc->precision = 32;
		enc->flags = 0;
		return 0;
	}
	return EINVAL;
}

int
envy_set_params(void *self, int setmode, int usemode,
    struct audio_params *p, struct audio_params *r)
{
	struct envy_softc *sc = (struct envy_softc *)self;
	int i, rate, reg;

	if (setmode == 0) {
		DPRINTF("%s: no params to set\n", DEVNAME(sc));
		return 0;
	}
	if (setmode == (AUMODE_PLAY | AUMODE_RECORD) &&
	    p->sample_rate != r->sample_rate) {
		DPRINTF("%s: play/rec rates mismatch\n", DEVNAME(sc));
		return EINVAL;
	}
	rate = (setmode & AUMODE_PLAY) ? p->sample_rate : r->sample_rate;
	for (i = 0; envy_rates[i].rate < rate; i++) {
		if (envy_rates[i].rate == -1) {
			i--;
			DPRINTF("%s: rate: %d -> %d\n", DEVNAME(sc), rate, i);
			break;
		}
	}
	reg = bus_space_read_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_RATE);
	reg &= ~ENVY_MT_RATEMASK;
	reg |= envy_rates[i].reg;
	bus_space_write_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_RATE, reg);
	if (setmode & AUMODE_PLAY) {
		p->encoding = AUDIO_ENCODING_SLINEAR;
		p->precision = 32;
		p->channels = ENVY_PCHANS;
	}
	if (setmode & AUMODE_RECORD) {
		r->encoding = AUDIO_ENCODING_SLINEAR;
		r->precision = 32;
		r->channels = ENVY_RCHANS;
	}
	return 0;
}

int
envy_round_blocksize(void *self, int blksz)
{
	/*
	 * XXX: sizes depend on the mode but we don't have 
	 * access to the mode here; So we use the greatest 
	 * common divisor of input and output blocksizes, until 
	 * upper layer is fixed
	 */
#define ENVY_GCD (6 * 5 * 4)
	return (blksz / ENVY_GCD) * ENVY_GCD;
}

size_t
envy_round_buffersize(void *self, int dir, size_t bufsz)
{
	/*
	 * XXX: same remark as above
	 */
	return (bufsz / ENVY_GCD) * ENVY_GCD;
}

int
envy_trigger_output(void *self, void *start, void *end, int blksz,
    void (*intr)(void *), void *arg, struct audio_params *param)
{
	struct envy_softc *sc = (struct envy_softc *)self;
	size_t bufsz;
	int st;

	bufsz = end - start;
	if (bufsz % (ENVY_PCHANS * 4) != 0) {
		DPRINTF("%s: %d: bad output bufsz\n", DEVNAME(sc), bufsz);
		return EINVAL;
	}
	if (blksz % (ENVY_PCHANS * 4) != 0) {
		DPRINTF("%s: %d: bad output blksz\n", DEVNAME(sc), blksz);
		return EINVAL;
	}
	bus_space_write_2(sc->mt_iot, sc->mt_ioh, 
	    ENVY_MT_PBUFSZ, bufsz / 4 - 1);
	bus_space_write_2(sc->mt_iot, sc->mt_ioh, 
	    ENVY_MT_PBLKSZ, blksz / 4 - 1);

	sc->ointr = intr;
	sc->oarg = arg;

	st = ENVY_MT_INTR_PACK;
	bus_space_write_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_INTR, st);

	st = bus_space_read_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_CTL);
	st |= ENVY_MT_CTL_PSTART;
	bus_space_write_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_CTL, st);
	return 0;
}

int
envy_trigger_input(void *self, void *start, void *end, int blksz,
    void (*intr)(void *), void *arg, struct audio_params *param)
{
	struct envy_softc *sc = (struct envy_softc *)self;
	size_t bufsz;
	int st;
	
	bufsz = end - start;
	if (bufsz % (ENVY_RCHANS * 4) != 0) {
		DPRINTF("%s: %d: bad input bufsz\n", DEVNAME(sc), bufsz);
		return EINVAL;
	}
	if (blksz % (ENVY_RCHANS * 4) != 0) {
		DPRINTF("%s: %d: bad input blksz\n", DEVNAME(sc), blksz);
		return EINVAL;
	}
	bus_space_write_2(sc->mt_iot, sc->mt_ioh, 
	    ENVY_MT_RBUFSZ, bufsz / 4 - 1);
	bus_space_write_2(sc->mt_iot, sc->mt_ioh, 
	    ENVY_MT_RBLKSZ, blksz / 4 - 1);

	sc->iintr = intr;
	sc->iarg = arg;

	st = ENVY_MT_INTR_RACK;
	bus_space_write_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_INTR, st);

	st = bus_space_read_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_CTL);
	st |= ENVY_MT_CTL_RSTART;
	bus_space_write_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_CTL, st);
	return 0;
}

int
envy_halt_output(void *self)
{
	struct envy_softc *sc = (struct envy_softc *)self;
	int st;

	st = bus_space_read_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_CTL);
	st &= ~ENVY_MT_CTL_PSTART;
	bus_space_write_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_CTL, 0);
	return 0;
}

int
envy_halt_input(void *self)
{
	struct envy_softc *sc = (struct envy_softc *)self;
	int st;

	st = bus_space_read_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_CTL);
	st &= ~ENVY_MT_CTL_RSTART;
	bus_space_write_1(sc->mt_iot, sc->mt_ioh, ENVY_MT_CTL, 0);
	return 0;
}

int
envy_getdev(void *self, struct audio_device *dev)
{
	strlcpy(dev->name, "Envy24", MAX_AUDIO_DEV_LEN);
	strlcpy(dev->version, "-", MAX_AUDIO_DEV_LEN);	/* XXX eeprom version */
	strlcpy(dev->config, "envy", MAX_AUDIO_DEV_LEN);
	return 0;
}

int
envy_query_devinfo(void *self, struct mixer_devinfo *dev)
{
	int i, n, out;
	char *classes[] = { 
		AudioCinputs, AudioCoutputs, "source", AudioCmonitor 
	};
	/* XXX: define AudioCsource */

	dev->prev = dev->next = AUDIO_MIXER_LAST;
	if (dev->index < ENVY_MIX_OUTSRC) {
		dev->mixer_class = dev->index - ENVY_MIX_CLASSIN;
		strlcpy(dev->label.name, 
		    classes[dev->index - ENVY_MIX_CLASSIN], MAX_AUDIO_DEV_LEN);
		return 0;
	}
	if (dev->index < ENVY_MIX_MONITOR) {
		n = 0;
		out = dev->index - ENVY_MIX_OUTSRC;
		dev->type = AUDIO_MIXER_ENUM;
		dev->mixer_class = ENVY_MIX_CLASSMIX;
		for (i = 0; i < 10; i++) {
			dev->un.e.member[n].ord = n;
			snprintf(dev->un.e.member[n++].label.name,
			    MAX_AUDIO_DEV_LEN, "in%d", i);
		}
		dev->un.e.member[n].ord = n;
		snprintf(dev->un.e.member[n++].label.name, 
			 MAX_AUDIO_DEV_LEN, "play%d", out);
		if (out < 2) {
			dev->un.e.member[n].ord = n;
			snprintf(dev->un.e.member[n++].label.name, 
			    MAX_AUDIO_DEV_LEN, "mon%d", out);
		}
		snprintf(dev->label.name, MAX_AUDIO_DEV_LEN, "out%d", out);
		dev->un.s.num_mem = n;
		return 0;
	}
	if (dev->index < ENVY_MIX_INVAL) {
		out = dev->index - ENVY_MIX_MONITOR;
		dev->type = AUDIO_MIXER_VALUE;
		dev->mixer_class = ENVY_MIX_CLASSMON;
		dev->un.v.delta = 2;
		dev->un.v.num_channels = 2;
		snprintf(dev->label.name, MAX_AUDIO_DEV_LEN, 
			 "%s%d", out < 10 ? "play" : "rec", out % 10);
		strlcpy(dev->un.v.units.name, AudioNvolume, MAX_AUDIO_DEV_LEN);
		return 0;
	}
	return ENXIO;
}

int
envy_get_port(void *self, struct mixer_ctrl *ctl)
{
	struct envy_softc *sc = (struct envy_softc *)self;
	int out, l, r;

	if (ctl->dev < ENVY_MIX_OUTSRC) {
		return EINVAL;
	}
	if (ctl->dev <  ENVY_MIX_OUTSRC + 8) {
		out = ctl->dev - ENVY_MIX_OUTSRC;
		ctl->un.ord = envy_lineout_getsrc(sc, out);
		return 0;
	}
	if (ctl->dev <  ENVY_MIX_MONITOR) {
		out = ctl->dev - (ENVY_MIX_OUTSRC + 8);
		ctl->un.ord = envy_spdout_getsrc(sc, out);
		return 0;
	}
	if (ctl->dev <  ENVY_MIX_INVAL) {
		out = ctl->dev - ENVY_MIX_MONITOR;
		envy_mon_getvol(sc, out, &l, &r);
		ctl->un.value.num_channels = 2;
		ctl->un.value.level[0] = 2 * l;
		ctl->un.value.level[1] = 2 * r;
		return 0;
	}
	return ENXIO;
}

int
envy_set_port(void *self, struct mixer_ctrl *ctl)
{
	struct envy_softc *sc = (struct envy_softc *)self;
	int out, maxsrc, l, r;

	if (ctl->dev < ENVY_MIX_OUTSRC) {
		return EINVAL;
	}
	if (ctl->dev < ENVY_MIX_OUTSRC + 8) {
		out = ctl->dev - ENVY_MIX_OUTSRC;
		maxsrc = (out < 2 || out >= 8) ? 12 : 11;
		if (ctl->un.ord < 0 || ctl->un.ord >= maxsrc)
			return EINVAL;
		envy_lineout_setsrc(sc, out, ctl->un.ord);
		return 0;
	}
	if (ctl->dev <  ENVY_MIX_MONITOR) {
		out = ctl->dev - (ENVY_MIX_OUTSRC + 8);
		if (ctl->un.ord < 0 || ctl->un.ord >= 12)
			return EINVAL;
		envy_spdout_setsrc(sc, out, ctl->un.ord);
		return 0;
	}
	if (ctl->dev <  ENVY_MIX_INVAL) {
		out = ctl->dev - ENVY_MIX_MONITOR;
		if (ctl->un.value.num_channels != 2) {
			return EINVAL;
		}
		l = ctl->un.value.level[0] / 2;
		r = ctl->un.value.level[1] / 2;
		envy_mon_setvol(sc, out, l, r);
		return 0;
	}
	return ENXIO;
}

int
envy_get_props(void *self)
{
	return AUDIO_PROP_FULLDUPLEX;
}
