/*	$OpenBSD: cmpci.c,v 1.9 2002/11/19 18:40:17 jason Exp $	*/

/*
 * Copyright (c) 2000 Takuya SHIOZAKI
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * C-Media CMI8x38 Audio Chip Support.
 *
 * TODO:
 *   - Legacy MPU, OPL and Joystick support (but, I have no interest...)
 *   - SPDIF support
 *
 */

#undef CMPCI_SPDIF_SUPPORT  /* XXX: not working */

#if defined(AUDIO_DEBUG) || defined(DEBUG)
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/pci/cmpcireg.h>
#include <dev/pci/cmpcivar.h>

#include <machine/bus.h>
#include <machine/intr.h>

/*
 * Low-level HW interface
 */
static __inline uint8_t cmpci_mixerreg_read(struct cmpci_softc *,
                                                 uint8_t);
static __inline void cmpci_mixerreg_write(struct cmpci_softc *,
                                               uint8_t, uint8_t);
static __inline void cmpci_reg_partial_write_4(struct cmpci_softc *,
                                                    int, int,
                                                    uint32_t, uint32_t);
static __inline void cmpci_reg_set_4(struct cmpci_softc *,
                                          int, uint32_t);
static __inline void cmpci_reg_clear_4(struct cmpci_softc *,
                                            int, uint32_t);
static int cmpci_rate_to_index(int);
static __inline int cmpci_index_to_rate(int);
static __inline int cmpci_index_to_divider(int);

static int cmpci_adjust(int, int);
static void cmpci_set_mixer_gain(struct cmpci_softc *, int);
static int cmpci_set_in_ports(struct cmpci_softc *, int);


/*
 * autoconf interface
 */
int cmpci_match(struct device *, void *, void *);
void cmpci_attach(struct device *, struct device *, void *);

struct cfdriver cmpci_cd = {
	NULL, "cmpci", DV_DULL
};

struct cfattach cmpci_ca = {
	sizeof (struct cmpci_softc), cmpci_match, cmpci_attach
};

struct audio_device cmpci_device = {
	"CMI PCI Audio",
	"",
	"cmpci"
};

/* interrupt */
int cmpci_intr(void *);


/*
 * DMA stuff
 */
int cmpci_alloc_dmamem(struct cmpci_softc *,
                            size_t, int, int, caddr_t *);
int cmpci_free_dmamem(struct cmpci_softc *, caddr_t, int);
struct cmpci_dmanode * cmpci_find_dmamem(struct cmpci_softc *,
                                                     caddr_t);

/*
 * Interface to machine independent layer
 */
int cmpci_open(void *, int);
void cmpci_close(void *);
int cmpci_query_encoding(void *, struct audio_encoding *);
int cmpci_set_params(void *, int, int,
                          struct audio_params *,
                          struct audio_params *);
int cmpci_round_blocksize(void *, int);
int cmpci_halt_output(void *);
int cmpci_halt_input(void *);
int cmpci_getdev(void *, struct audio_device *);
int cmpci_set_port(void *, mixer_ctrl_t *);
int cmpci_get_port(void *, mixer_ctrl_t *);
int cmpci_query_devinfo(void *, mixer_devinfo_t *);
void *cmpci_malloc(void *, int, size_t, int, int);
void cmpci_free(void *, void *, int);
size_t cmpci_round_buffersize(void *, int, size_t);
paddr_t cmpci_mappage(void *, void *, off_t, int);
int cmpci_get_props(void *);
int cmpci_trigger_output(void *, void *, void *, int,
                         void (*)(void *), void *,
                         struct audio_params *);
int cmpci_trigger_input(void *, void *, void *, int,
                        void (*)(void *), void *,
                        struct audio_params *);

struct audio_hw_if cmpci_hw_if = {
	cmpci_open,			/* open */
	cmpci_close,			/* close */
	NULL,				/* drain */
	cmpci_query_encoding,		/* query_encoding */
	cmpci_set_params,		/* set_params */
	cmpci_round_blocksize,		/* round_blocksize */
	NULL,				/* commit_settings */
	NULL,				/* init_output */
	NULL,				/* init_input */
	NULL,				/* start_output */
	NULL,				/* start_input */
	cmpci_halt_output,		/* halt_output */
	cmpci_halt_input,		/* halt_input */
	NULL,				/* speaker_ctl */
	cmpci_getdev,			/* getdev */
	NULL,				/* setfd */
	cmpci_set_port,			/* set_port */
	cmpci_get_port,			/* get_port */
	cmpci_query_devinfo,		/* query_devinfo */
	cmpci_malloc,			/* malloc */
	cmpci_free,			/* free */
	cmpci_round_buffersize,		/* round_buffersize */
	cmpci_mappage,			/* mappage */
	cmpci_get_props,		/* get_props */
	cmpci_trigger_output,		/* trigger_output */
	cmpci_trigger_input		/* trigger_input */
};


/*
 * Low-level HW interface
 */

/* mixer register read/write */
static __inline uint8_t
cmpci_mixerreg_read(sc, no)
	struct cmpci_softc *sc;
	uint8_t no;
{
	uint8_t ret;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CMPCI_REG_SBADDR, no);
	delay(10);
	ret = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CMPCI_REG_SBDATA);
	delay(10);
	return ret;
}

static __inline void
cmpci_mixerreg_write(sc, no, val)
	struct cmpci_softc *sc;
	uint8_t no, val;
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CMPCI_REG_SBADDR, no);
	delay(10);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CMPCI_REG_SBDATA, val);
	delay(10);
}

/* register partial write */
static __inline void
cmpci_reg_partial_write_4(sc, no, shift, mask, val)
	struct cmpci_softc *sc;
	int no, shift;
	uint32_t mask, val;
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, no,
	    (val<<shift) |
	    (bus_space_read_4(sc->sc_iot, sc->sc_ioh, no) & ~(mask<<shift)));
	delay(10);
}

/* register set/clear bit */
static __inline void
cmpci_reg_set_4(sc, no, mask)
	struct cmpci_softc *sc;
	int no;
	uint32_t mask;
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, no,
	    (bus_space_read_4(sc->sc_iot, sc->sc_ioh, no) | mask));
	delay(10);
}

static __inline void
cmpci_reg_clear_4(sc, no, mask)
	struct cmpci_softc *sc;
	int no;
	uint32_t mask;
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, no,
	    (bus_space_read_4(sc->sc_iot, sc->sc_ioh, no) & ~mask));
	delay(10);
}


/* rate */
struct {
      int rate;
      int divider;
} cmpci_rate_table[CMPCI_REG_NUMRATE] = {
#define _RATE(n) { n, CMPCI_REG_RATE_ ## n }
	_RATE(5512),
	_RATE(8000),
	_RATE(11025),
	_RATE(16000),
	_RATE(22050),
	_RATE(32000),
	_RATE(44100),
	_RATE(48000)
#undef  _RATE
};

int
cmpci_rate_to_index(rate)
	int rate;
{
	int i;
	for (i=0; i<CMPCI_REG_NUMRATE-2; i++)
		if (rate <=
		    (cmpci_rate_table[i].rate + cmpci_rate_table[i+1].rate) / 2)
			return i;
	return i;  /* 48000 */
}

static __inline int
cmpci_index_to_rate(index)
	int index;
{

	return cmpci_rate_table[index].rate;
}

static __inline int
cmpci_index_to_divider(index)
	int index;
{
	return cmpci_rate_table[index].divider;
}

const struct pci_matchid cmpci_devices[] = {
	{ PCI_VENDOR_CMI, PCI_PRODUCT_CMI_CMI8338A },
	{ PCI_VENDOR_CMI, PCI_PRODUCT_CMI_CMI8338B },
	{ PCI_VENDOR_CMI, PCI_PRODUCT_CMI_CMI8738 },
};

/*
 * interface to configure the device.
 */
int
cmpci_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	return (pci_matchbyid((struct pci_attach_args *)aux, cmpci_devices,
	    sizeof(cmpci_devices)/sizeof(cmpci_devices[0])));
}

void
cmpci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct cmpci_softc *sc = (struct cmpci_softc *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	pci_intr_handle_t ih;
	char const *intrstr;
	int i, v;

	/* map I/O space */
	if (pci_mapreg_map(pa, CMPCI_PCI_IOBASEREG, PCI_MAPREG_TYPE_IO, 0,
			   &sc->sc_iot, &sc->sc_ioh, NULL, NULL, 0)) {
		printf("\n%s: failed to map I/O space\n", sc->sc_dev.dv_xname);
		return;
	}

	/* interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf("\n%s: failed to map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_AUDIO, cmpci_intr,
				       sc, sc->sc_dev.dv_xname);

	if (sc->sc_ih == NULL) {
		printf("\n%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s\n", intrstr);

	sc->sc_dmat = pa->pa_dmat;

	audio_attach_mi(&cmpci_hw_if, sc, &sc->sc_dev);

	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_RESET, 0);
	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_ADCMIX_L, 0);
	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_ADCMIX_R, 0);
	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_OUTMIX,
			     CMPCI_SB16_SW_CD|CMPCI_SB16_SW_MIC|
			     CMPCI_SB16_SW_LINE);
	for (i = 0; i < CMPCI_NDEVS; i++) {
		switch(i) {
		case CMPCI_MIC_VOL:
		case CMPCI_LINE_IN_VOL:
			v = 0;
			break;
		case CMPCI_BASS:
		case CMPCI_TREBLE:
			v = CMPCI_ADJUST_GAIN(sc, AUDIO_MAX_GAIN / 2);
			break;
		case CMPCI_CD_IN_MUTE:
		case CMPCI_MIC_IN_MUTE:
		case CMPCI_LINE_IN_MUTE:
		case CMPCI_FM_IN_MUTE:
		case CMPCI_CD_SWAP:
		case CMPCI_MIC_SWAP:
		case CMPCI_LINE_SWAP:
		case CMPCI_FM_SWAP:
			v = 0;
			break;
		case CMPCI_CD_OUT_MUTE:
		case CMPCI_MIC_OUT_MUTE:
		case CMPCI_LINE_OUT_MUTE:
			v = 1;
			break;
		default:
			v = CMPCI_ADJUST_GAIN(sc, AUDIO_MAX_GAIN / 2);
		}
		sc->gain[i][CMPCI_LEFT] = sc->gain[i][CMPCI_RIGHT] = v;
		cmpci_set_mixer_gain(sc, i);
	}
}

int
cmpci_intr(handle)
	void *handle;
{
	struct cmpci_softc *sc = handle;
	uint32_t intrstat;
	int s;

	intrstat = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    CMPCI_REG_INTR_STATUS);
	delay(10);

	if (!(intrstat & CMPCI_REG_ANY_INTR))
		return 0;

	/* disable and reset intr */
	s = splaudio();
	if (intrstat & CMPCI_REG_CH0_INTR)
		cmpci_reg_clear_4(sc, CMPCI_REG_INTR_CTRL,
		    CMPCI_REG_CH0_INTR_ENABLE);
	if (intrstat&CMPCI_REG_CH1_INTR)
		cmpci_reg_clear_4(sc, CMPCI_REG_INTR_CTRL,
		    CMPCI_REG_CH1_INTR_ENABLE);
	splx(s);

	if (intrstat & CMPCI_REG_CH0_INTR) {
		if (sc->sc_play.intr)
			(*sc->sc_play.intr)(sc->sc_play.intr_arg);
	}
	if (intrstat & CMPCI_REG_CH1_INTR) {
	    if (sc->sc_rec.intr)
		    (*sc->sc_rec.intr)(sc->sc_rec.intr_arg);
	}

	/* enable intr */
	s = splaudio();
	if ( intrstat & CMPCI_REG_CH0_INTR )
		cmpci_reg_set_4(sc, CMPCI_REG_INTR_CTRL,
		    CMPCI_REG_CH0_INTR_ENABLE);
	if (intrstat & CMPCI_REG_CH1_INTR)
		cmpci_reg_set_4(sc, CMPCI_REG_INTR_CTRL,
		    CMPCI_REG_CH1_INTR_ENABLE);
	splx(s);

	return 1;
}

/* open/close */
int
cmpci_open(handle, flags)
	void *handle;
	int flags;
{
	struct cmpci_softc *sc = handle;
	(void)sc;
	(void)flags;
	
	return 0;
}

void
cmpci_close(handle)
	void *handle;
{
	(void)handle;
}

int
cmpci_query_encoding(handle, fp)
	void *handle;
	struct audio_encoding *fp;
{
	struct cmpci_softc *sc = handle;
	(void)sc;
	
	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	default:
		return EINVAL;
	}
	return 0;
}


int
cmpci_set_params(handle, setmode, usemode, play, rec)
	void *handle;
	int setmode, usemode;
	struct audio_params *play, *rec;
{
	int i;
	struct cmpci_softc *sc = handle;

	for (i=0; i<2; i++) {
		int md_format;
		int md_divide;
		int md_index;
		int mode;
		struct audio_params *p;

		switch (i) {
		case 0:
			mode = AUMODE_PLAY;
			p = play;
			break;
		case 1:
			mode = AUMODE_RECORD;
			p = rec;
			break;
		}

		if (!(setmode & mode))
			continue;

		/* format */
		p->sw_code = NULL;
		switch (p->channels) {
		case 1:
			md_format = CMPCI_REG_FORMAT_MONO;
			break;
		case 2:
			md_format = CMPCI_REG_FORMAT_STEREO;
			break;
		default:
			return (EINVAL);
		}
		switch (p->encoding) {
		case AUDIO_ENCODING_ULAW:
			if (p->precision != 8)
				return (EINVAL);
			if (mode & AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = mulaw_to_slinear16;
				md_format |= CMPCI_REG_FORMAT_16BIT;
			} else
				p->sw_code = ulinear8_to_mulaw;
			md_format |= CMPCI_REG_FORMAT_8BIT;
			break;
		case AUDIO_ENCODING_ALAW:
			if (p->precision != 8)
				return (EINVAL);
			if (mode & AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = alaw_to_slinear16;
				md_format |= CMPCI_REG_FORMAT_16BIT;
			} else
				p->sw_code = ulinear8_to_alaw;
			md_format |= CMPCI_REG_FORMAT_8BIT;
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			switch (p->precision) {
			case 8:
				p->sw_code = change_sign8;
				md_format |= CMPCI_REG_FORMAT_8BIT;
				break;
			case 16:
				md_format |= CMPCI_REG_FORMAT_16BIT;
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			switch (p->precision) {
			case 8:
				md_format |= CMPCI_REG_FORMAT_8BIT;
				p->sw_code = change_sign8;
				break;
			case 16:
				md_format |= CMPCI_REG_FORMAT_16BIT;
				p->sw_code = swap_bytes;
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			switch ( p->precision ) {
			case 8:
				md_format |= CMPCI_REG_FORMAT_8BIT;
				break;
			case 16:
				md_format |= CMPCI_REG_FORMAT_16BIT;
				p->sw_code = change_sign16;
				break;
			default:
				return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			switch (p->precision) {
			case 8:
				md_format |= CMPCI_REG_FORMAT_8BIT;
				break;
			case 16:
				md_format |= CMPCI_REG_FORMAT_16BIT;
				if ( mode&AUMODE_PLAY )
					p->sw_code = swap_bytes_change_sign16;
				else
					p->sw_code = change_sign16_swap_bytes;
				break;
			default:
				return (EINVAL);
			}
			break;
		default:
			return (EINVAL);
		}
		if (mode & AUMODE_PLAY)
			cmpci_reg_partial_write_4(sc,
						  CMPCI_REG_CHANNEL_FORMAT,
						  CMPCI_REG_CH0_FORMAT_SHIFT,
						  CMPCI_REG_CH0_FORMAT_MASK,
						  md_format);
		else
			cmpci_reg_partial_write_4(sc,
						  CMPCI_REG_CHANNEL_FORMAT,
						  CMPCI_REG_CH1_FORMAT_SHIFT,
						  CMPCI_REG_CH1_FORMAT_MASK,
						  md_format);
		/* sample rate */
		md_index = cmpci_rate_to_index(p->sample_rate);
		md_divide = cmpci_index_to_divider(md_index);
		p->sample_rate = cmpci_index_to_rate(md_index);
#if 0
		DPRINTF(("%s: sample:%d, divider=%d\n",
			 sc->sc_dev.dv_xname, (int)p->sample_rate, md_divide));
#endif
		if (mode & AUMODE_PLAY) {
			cmpci_reg_partial_write_4(sc,
						  CMPCI_REG_FUNC_1,
						  CMPCI_REG_DAC_FS_SHIFT,
						  CMPCI_REG_DAC_FS_MASK,
						  md_divide);
#ifdef CMPCI_SPDIF_SUPPORT
			switch (md_divide) {
			case CMPCI_REG_RATE_44100:
				cmpci_reg_clear_4(sc, CMPCI_REG_MISC,
						  CMPCI_REG_SPDIF_48K);
				cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_1,
						  CMPCI_REG_SPDIF_LOOP);
				cmpci_reg_set_4(sc, CMPCI_REG_FUNC_1,
						CMPCI_REG_SPDIF0_ENABLE);
				break;
			case CMPCI_REG_RATE_48000:
				cmpci_reg_set_4(sc, CMPCI_REG_MISC,
						CMPCI_REG_SPDIF_48K);
				cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_1,
						  CMPCI_REG_SPDIF_LOOP);
				cmpci_reg_set_4(sc, CMPCI_REG_FUNC_1,
						CMPCI_REG_SPDIF0_ENABLE);
				break;
			default:
				cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_1,
						  CMPCI_REG_SPDIF0_ENABLE);
			    cmpci_reg_set_4(sc, CMPCI_REG_FUNC_1,
					    CMPCI_REG_SPDIF_LOOP);
			}
#endif
		} else {
			cmpci_reg_partial_write_4(sc,
						  CMPCI_REG_FUNC_1,
						  CMPCI_REG_ADC_FS_SHIFT,
						  CMPCI_REG_ADC_FS_MASK,
						  md_divide);
#ifdef CMPCI_SPDIF_SUPPORT
			if ( sc->in_mask&CMPCI_SPDIF_IN) {
				switch (md_divide) {
				case CMPCI_REG_RATE_44100:
					cmpci_reg_set_4(sc, CMPCI_REG_FUNC_1,
							CMPCI_REG_SPDIF1_ENABLE);
					break;
				default:
					return EINVAL;
				}
			} else
				cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_1,
						  CMPCI_REG_SPDIF1_ENABLE);
#endif
		}
	}
	return 0;
}

/* ARGSUSED */
int
cmpci_round_blocksize(handle, block)
	void *handle;
	int block;
{
	return (block & -4);
}

int
cmpci_halt_output(handle)
	void *handle;
{
	struct cmpci_softc *sc = handle;
	int s;

	s = splaudio();
	sc->sc_play.intr = NULL;
	cmpci_reg_clear_4(sc, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH0_INTR_ENABLE);
	cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_ENABLE);
	/* wait for reset DMA */
	cmpci_reg_set_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_RESET);
	delay(10);
	cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_RESET);
	splx(s);

	return 0;
}

int
cmpci_halt_input(handle)
	void *handle;
{
	struct cmpci_softc *sc = handle;
	int s;

	s = splaudio();
	sc->sc_rec.intr = NULL;
	cmpci_reg_clear_4(sc, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH1_INTR_ENABLE);
	cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_ENABLE);
	/* wait for reset DMA */
	cmpci_reg_set_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_RESET);
	delay(10);
	cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_RESET);
	splx(s);
	
	return 0;
}

int
cmpci_getdev(handle, retp)
        void *handle;
        struct audio_device *retp;
{
	*retp = cmpci_device;
	return 0;
}


/* mixer device information */
int
cmpci_query_devinfo(handle, dip)
	void *handle;
	mixer_devinfo_t *dip;
{
	struct cmpci_softc *sc = handle;
	(void)sc;

	switch (dip->index) {
	case CMPCI_MASTER_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CMPCI_OUTPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmaster);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case CMPCI_FM_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CMPCI_FM_IN_MUTE;
		strcpy(dip->label.name, AudioNfmsynth);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case CMPCI_CD_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CMPCI_CD_IN_MUTE;
		strcpy(dip->label.name, AudioNcd);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case CMPCI_VOICE_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CMPCI_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNdac);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case CMPCI_OUTPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCoutputs);
		return 0;
	case CMPCI_MIC_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CMPCI_MIC_IN_MUTE;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case CMPCI_LINE_IN_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = CMPCI_LINE_IN_MUTE;
		strcpy(dip->label.name, AudioNline);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case CMPCI_RECORD_SOURCE:
		dip->mixer_class = CMPCI_RECORD_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNsource);
		dip->type = AUDIO_MIXER_SET;
#ifdef CMPCI_SPDIF_SUPPORT
		dip->un.s.num_mem = 5;
#else
		dip->un.s.num_mem = 4;
#endif
		strcpy(dip->un.s.member[0].label.name, AudioNmicrophone);
		dip->un.s.member[0].mask = 1 << CMPCI_MIC_VOL;
		strcpy(dip->un.s.member[1].label.name, AudioNcd);
		dip->un.s.member[1].mask = 1 << CMPCI_CD_VOL;
		strcpy(dip->un.s.member[2].label.name, AudioNline);
		dip->un.s.member[2].mask = 1 << CMPCI_LINE_IN_VOL;
		strcpy(dip->un.s.member[3].label.name, AudioNfmsynth);
		dip->un.s.member[3].mask = 1 << CMPCI_FM_VOL;
#ifdef CMPCI_SPDIF_SUPPORT
		strcpy(dip->un.s.member[4].label.name, CmpciNspdif);
		dip->un.s.member[4].mask = 1 << CMPCI_SPDIF_IN;
#endif
		return 0;
	case CMPCI_BASS:
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNbass);
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CMPCI_EQUALIZATION_CLASS;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNbass);
		return 0;
	case CMPCI_TREBLE:
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNtreble);
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CMPCI_EQUALIZATION_CLASS;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNtreble);
		return 0;
	case CMPCI_RECORD_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CMPCI_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCrecord);
		return 0;
	case CMPCI_INPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCinputs);
		return 0;
	case CMPCI_PCSPEAKER:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "pc_speaker");
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case CMPCI_INPUT_GAIN:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNinput);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case CMPCI_OUTPUT_GAIN:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = CMPCI_OUTPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNoutput);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case CMPCI_AGC:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "agc");
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		return 0;
	case CMPCI_EQUALIZATION_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = CMPCI_EQUALIZATION_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCequalization);
		return 0;
	case CMPCI_CD_IN_MUTE:
		dip->prev = CMPCI_CD_VOL;
		dip->next = CMPCI_CD_SWAP;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		goto mute;
	case CMPCI_MIC_IN_MUTE:
		dip->prev = CMPCI_MIC_VOL;
		dip->next = CMPCI_MIC_SWAP;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		goto mute;
	case CMPCI_LINE_IN_MUTE:
		dip->prev = CMPCI_LINE_IN_VOL;
		dip->next = CMPCI_LINE_SWAP;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		goto mute;
	case CMPCI_FM_IN_MUTE:
		dip->prev = CMPCI_FM_VOL;
		dip->next = CMPCI_FM_SWAP;
		dip->mixer_class = CMPCI_INPUT_CLASS;
		goto mute;
	case CMPCI_CD_SWAP:
		dip->prev = CMPCI_CD_IN_MUTE;
		dip->next = CMPCI_CD_OUT_MUTE;
		goto swap;
	case CMPCI_MIC_SWAP:
		dip->prev = CMPCI_MIC_IN_MUTE;
		dip->next = CMPCI_MIC_OUT_MUTE;
		goto swap;
	case CMPCI_LINE_SWAP:
		dip->prev = CMPCI_LINE_IN_MUTE;
		dip->next = CMPCI_LINE_OUT_MUTE;
		goto swap;
	case CMPCI_FM_SWAP:
		dip->prev = CMPCI_FM_IN_MUTE;
		dip->next = AUDIO_MIXER_LAST;
	swap:
		dip->mixer_class = CMPCI_INPUT_CLASS;
		strcpy(dip->label.name, AudioNswap);
		goto mute1;
	case CMPCI_CD_OUT_MUTE:
		dip->prev = CMPCI_CD_SWAP;
		dip->next = AUDIO_MIXER_LAST;
		dip->mixer_class = CMPCI_OUTPUT_CLASS;
		goto mute;
	case CMPCI_MIC_OUT_MUTE:
		dip->prev = CMPCI_MIC_SWAP;
		dip->next = AUDIO_MIXER_LAST;
		dip->mixer_class = CMPCI_OUTPUT_CLASS;
		goto mute;
	case CMPCI_LINE_OUT_MUTE:
		dip->prev = CMPCI_LINE_SWAP;
		dip->next = AUDIO_MIXER_LAST;
		dip->mixer_class = CMPCI_OUTPUT_CLASS;
	mute:
		strcpy(dip->label.name, AudioNmute);
	mute1:
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		return 0;
	}

	return ENXIO;
}

int
cmpci_alloc_dmamem(sc, size, type, flags, r_addr)
	struct cmpci_softc *sc;
	size_t size;
	int type, flags;
	caddr_t *r_addr;
{
	int ret = 0;
	struct cmpci_dmanode *n;
	int w;
	
	if ( NULL == (n=malloc(sizeof(struct cmpci_dmanode), type, flags)) ) {
		ret = ENOMEM;
		goto quit;
	}

	w = (flags & M_NOWAIT) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK;
#define CMPCI_DMABUF_ALIGN    0x4
#define CMPCI_DMABUF_BOUNDARY 0x0
	n->cd_tag = sc->sc_dmat;
	n->cd_size = size;
	if ( (ret=bus_dmamem_alloc(n->cd_tag, n->cd_size,
				   CMPCI_DMABUF_ALIGN, CMPCI_DMABUF_BOUNDARY,
				   n->cd_segs,
				   sizeof(n->cd_segs)/sizeof(n->cd_segs[0]),
				   &n->cd_nsegs, w)) )
		goto mfree;
	if ( (ret=bus_dmamem_map(n->cd_tag, n->cd_segs, n->cd_nsegs, n->cd_size,
				 &n->cd_addr, w | BUS_DMA_COHERENT)) )
		goto dmafree;
	if ( (ret=bus_dmamap_create(n->cd_tag, n->cd_size, 1, n->cd_size, 0,
				    w, &n->cd_map)) )
		goto unmap;
	if ( (ret=bus_dmamap_load(n->cd_tag, n->cd_map, n->cd_addr, n->cd_size,
				  NULL, w)) )
		goto destroy;

	n->cd_next = sc->sc_dmap;
	sc->sc_dmap = n;
	*r_addr = KVADDR(n);
	return 0;
	
destroy:
	bus_dmamap_destroy(n->cd_tag, n->cd_map);
unmap:
	bus_dmamem_unmap(n->cd_tag, n->cd_addr, n->cd_size);
dmafree:
	bus_dmamem_free(n->cd_tag,
			n->cd_segs, sizeof(n->cd_segs)/sizeof(n->cd_segs[0]));
mfree:
	free(n, type);
quit:
	return ret;
}

int
cmpci_free_dmamem(sc, addr, type)
	struct cmpci_softc *sc;
	caddr_t addr;
	int type;
{
    struct cmpci_dmanode **nnp;

	for (nnp = &sc->sc_dmap; *nnp; nnp = &(*nnp)->cd_next) {
		if ((*nnp)->cd_addr == addr) {
			struct cmpci_dmanode *n = *nnp;

			bus_dmamap_unload(n->cd_tag, n->cd_map);
			bus_dmamap_destroy(n->cd_tag, n->cd_map);
			bus_dmamem_unmap(n->cd_tag, n->cd_addr, n->cd_size);
			bus_dmamem_free(n->cd_tag, n->cd_segs,
			    sizeof(n->cd_segs)/sizeof(n->cd_segs[0]));
			free(n, type);
			return 0;
		}
	}
	return -1;
}

struct cmpci_dmanode *
cmpci_find_dmamem(sc, addr)
	struct cmpci_softc *sc;
	caddr_t addr;
{
	struct cmpci_dmanode *p;
	for (p = sc->sc_dmap; p; p = p->cd_next) {
		if (KVADDR(p) == (void *)addr)
			break;
	}
	return p;
}

#if 0
void
cmpci_print_dmamem(struct cmpci_dmanode *p);
void
cmpci_print_dmamem(p)
	struct cmpci_dmanode *p;
{
	DPRINTF(("DMA at virt:%p, dmaseg:%p, mapseg:%p, size:%p\n",
		 (void *)p->cd_addr, (void *)p->cd_segs[0].ds_addr,
		 (void *)DMAADDR(p), (void *)p->cd_size));
}
#endif /* DEBUG */

void *
cmpci_malloc(handle, direction, size, type, flags)
	void  *handle;
	int direction;
	size_t size;
	int    type, flags;
{
	struct cmpci_softc *sc = handle;
	caddr_t addr;
	
	if ( cmpci_alloc_dmamem(sc, size, type, flags, &addr) )
		return NULL;
	return addr;
}

void
cmpci_free(handle, addr, type)
	void    *handle;
	void    *addr;
	int     type;
{
	struct cmpci_softc *sc = handle;
	
	cmpci_free_dmamem(sc, addr, type);
}

#define MAXVAL 256
int
cmpci_adjust(val, mask)
    int val, mask;
{
	val += (MAXVAL - mask) >> 1;
	if (val >= MAXVAL)
		val = MAXVAL-1;
	return val & mask;
}

void
cmpci_set_mixer_gain(sc, port)
	struct cmpci_softc *sc;
	int port;
{
	int src;

	switch (port) {
	case CMPCI_MIC_VOL:
		src = CMPCI_SB16_MIXER_MIC;
		break;
	case CMPCI_MASTER_VOL:
		src = CMPCI_SB16_MIXER_MASTER_L;
		break;
	case CMPCI_LINE_IN_VOL:
		src = CMPCI_SB16_MIXER_LINE_L;
		break;
	case CMPCI_VOICE_VOL:
		src = CMPCI_SB16_MIXER_VOICE_L;
		break;
	case CMPCI_FM_VOL:
		src = CMPCI_SB16_MIXER_FM_L;
		break;
	case CMPCI_CD_VOL:
		src = CMPCI_SB16_MIXER_CDDA_L;
		break;
	case CMPCI_INPUT_GAIN:
		src = CMPCI_SB16_MIXER_INGAIN_L;
		break;
	case CMPCI_OUTPUT_GAIN:
		src = CMPCI_SB16_MIXER_OUTGAIN_L;
		break;
	case CMPCI_TREBLE:
		src = CMPCI_SB16_MIXER_TREBLE_L;
		break;
	case CMPCI_BASS:
		src = CMPCI_SB16_MIXER_BASS_L;
		break;
	case CMPCI_PCSPEAKER:
		cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_SPEAKER,
				     sc->gain[port][CMPCI_LEFT]);
		return;
	default:
		return;
	}
	cmpci_mixerreg_write(sc, src, sc->gain[port][CMPCI_LEFT]);
	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_L_TO_R(src),
			     sc->gain[port][CMPCI_RIGHT]);
}

int
cmpci_set_in_ports(sc, mask)
	struct cmpci_softc *sc;
	int mask;
{
	int bitsl, bitsr;

	if (mask & ~((1<<CMPCI_FM_VOL) | (1<<CMPCI_LINE_IN_VOL) |
		     (1<<CMPCI_CD_VOL) | (1<<CMPCI_MIC_VOL)
#ifdef CMPCI_SPDIF_SUPPORT
		     | (1<<CMPCI_SPDIF_IN)
#endif
		     ))
		return EINVAL;
	bitsr = 0;
	if (mask & (1<<CMPCI_FM_VOL))
		bitsr |= CMPCI_SB16_MIXER_FM_SRC_R;
	if (mask & (1<<CMPCI_LINE_IN_VOL))
		bitsr |= CMPCI_SB16_MIXER_LINE_SRC_R;
	if (mask & (1<<CMPCI_CD_VOL))
		bitsr |= CMPCI_SB16_MIXER_CD_SRC_R;
	bitsl = CMPCI_SB16_MIXER_SRC_R_TO_L(bitsr);
	if (mask & (1<<CMPCI_MIC_VOL)) {
		bitsl |= CMPCI_SB16_MIXER_MIC_SRC;
		bitsr |= CMPCI_SB16_MIXER_MIC_SRC;
	}
	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_ADCMIX_L, bitsl);
	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_ADCMIX_R, bitsr);

	sc->in_mask = mask;

	return 0;
}

int
cmpci_set_port(handle, cp)
	void *handle;
	mixer_ctrl_t *cp;
{
	struct cmpci_softc *sc = handle;
	int lgain, rgain;
	int mask, bits;
	int lmask, rmask, lbits, rbits;
	int mute, swap;
	
	switch (cp->dev) {
	case CMPCI_TREBLE:
	case CMPCI_BASS:
	case CMPCI_PCSPEAKER:
	case CMPCI_INPUT_GAIN:
	case CMPCI_OUTPUT_GAIN:
	case CMPCI_MIC_VOL:
	case CMPCI_LINE_IN_VOL:
	case CMPCI_VOICE_VOL:
	case CMPCI_FM_VOL:
	case CMPCI_CD_VOL:
	case CMPCI_MASTER_VOL:
		if (cp->type != AUDIO_MIXER_VALUE)
			return EINVAL;
		switch (cp->dev) {
		case CMPCI_MIC_VOL:
			if (cp->un.value.num_channels != 1)
				return EINVAL;

			lgain = rgain = CMPCI_ADJUST_MIC_GAIN(sc, 
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
			break;
		case CMPCI_PCSPEAKER:
			if (cp->un.value.num_channels != 1)
			    return EINVAL;
			/* FALLTHROUGH */
		case CMPCI_INPUT_GAIN:
		case CMPCI_OUTPUT_GAIN:
			lgain = rgain = CMPCI_ADJUST_2_GAIN(sc, 
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
			break;
		default:
			switch (cp->un.value.num_channels) {
			case 1:
				lgain = rgain = CMPCI_ADJUST_GAIN(sc, 
				    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
				break;
			case 2:
				lgain = CMPCI_ADJUST_GAIN(sc, 
				    cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT]);
				rgain = CMPCI_ADJUST_GAIN(sc, 
				    cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT]);
				break;
			default:
				return EINVAL;
			}
			break;
		}
		sc->gain[cp->dev][CMPCI_LEFT]  = lgain;
		sc->gain[cp->dev][CMPCI_RIGHT] = rgain;

		cmpci_set_mixer_gain(sc, cp->dev);
		break;

	case CMPCI_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_SET)
			return EINVAL;
#ifdef CMPCI_SPDIF_SUPPORT
		if ( cp->un.mask&(1<<CMPCI_SPDIF_IN) )
			cp->un.mask = 1<<CMPCI_SPDIF_IN;
#endif
		return cmpci_set_in_ports(sc, cp->un.mask);

	case CMPCI_AGC:
		cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_AGC, cp->un.ord & 1);
		break;
	case CMPCI_CD_OUT_MUTE:
		mask = CMPCI_SB16_SW_CD;
		goto omute;
	case CMPCI_MIC_OUT_MUTE:
		mask = CMPCI_SB16_SW_MIC;
		goto omute;
	case CMPCI_LINE_OUT_MUTE:
		mask = CMPCI_SB16_SW_LINE;
	omute:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;
		bits = cmpci_mixerreg_read(sc, CMPCI_SB16_MIXER_OUTMIX);
		sc->gain[cp->dev][CMPCI_LR] = cp->un.ord != 0;
		if (cp->un.ord)
			bits = bits & ~mask;
		else
			bits = bits | mask;
		cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_OUTMIX, bits);
		break;

	case CMPCI_MIC_IN_MUTE:
	case CMPCI_MIC_SWAP:
		lmask = rmask = CMPCI_SB16_SW_MIC;
		goto imute;
	case CMPCI_CD_IN_MUTE:
	case CMPCI_CD_SWAP:
		lmask = CMPCI_SB16_SW_CD_L;
		rmask = CMPCI_SB16_SW_CD_R;
		goto imute;
	case CMPCI_LINE_IN_MUTE:
	case CMPCI_LINE_SWAP:
		lmask = CMPCI_SB16_SW_LINE_L;
		rmask = CMPCI_SB16_SW_LINE_R;
		goto imute;
	case CMPCI_FM_IN_MUTE:
	case CMPCI_FM_SWAP:
		lmask = CMPCI_SB16_SW_FM_L;
		rmask = CMPCI_SB16_SW_FM_R;
	imute:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;
		mask = lmask | rmask;
		lbits = cmpci_mixerreg_read(sc, CMPCI_SB16_MIXER_ADCMIX_L)
		    & ~mask;
		rbits = cmpci_mixerreg_read(sc, CMPCI_SB16_MIXER_ADCMIX_R)
		    & ~mask;
		sc->gain[cp->dev][CMPCI_LR] = cp->un.ord != 0;
		if (CMPCI_IS_IN_MUTE(cp->dev)) {
			mute = cp->dev;
			swap = mute - CMPCI_CD_IN_MUTE + CMPCI_CD_SWAP;
		} else {
			swap = cp->dev;
			mute = swap + CMPCI_CD_IN_MUTE - CMPCI_CD_SWAP;
		}
		if (sc->gain[swap][CMPCI_LR]) {
			mask = lmask;
			lmask = rmask;
			rmask = mask;
		}
		if (!sc->gain[mute][CMPCI_LR]) {
			lbits = lbits | lmask;
			rbits = rbits | rmask;
		}
		cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_ADCMIX_L, lbits);
		cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_ADCMIX_R, rbits);
		break;

	default:
		return EINVAL;
	}

	return 0;
}

int
cmpci_get_port(handle, cp)
	void *handle;
	mixer_ctrl_t *cp;
{
	struct cmpci_softc *sc = handle;
	
	switch (cp->dev) {
	case CMPCI_MIC_VOL:
	case CMPCI_LINE_IN_VOL:
		if (cp->un.value.num_channels != 1)
		    return EINVAL;
		/* FALLTHROUGH */
	case CMPCI_TREBLE:
	case CMPCI_BASS:
	case CMPCI_PCSPEAKER:
	case CMPCI_INPUT_GAIN:
	case CMPCI_OUTPUT_GAIN:
	case CMPCI_VOICE_VOL:
	case CMPCI_FM_VOL:
	case CMPCI_CD_VOL:
	case CMPCI_MASTER_VOL:
		switch (cp->un.value.num_channels) {
		case 1:
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = 
			    sc->gain[cp->dev][CMPCI_LEFT];
			break;
		case 2:
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = 
			    sc->gain[cp->dev][CMPCI_LEFT];
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = 
			    sc->gain[cp->dev][CMPCI_RIGHT];
			break;
		default:
			return EINVAL;
		}
		break;

	case CMPCI_RECORD_SOURCE:
		cp->un.mask = sc->in_mask;
		break;

	case CMPCI_AGC:
		cp->un.ord = cmpci_mixerreg_read(sc, CMPCI_SB16_MIXER_AGC);
		break;

	case CMPCI_CD_IN_MUTE:
	case CMPCI_MIC_IN_MUTE:
	case CMPCI_LINE_IN_MUTE:
	case CMPCI_FM_IN_MUTE:
	case CMPCI_CD_SWAP:
	case CMPCI_MIC_SWAP:
	case CMPCI_LINE_SWAP:
	case CMPCI_FM_SWAP:
	case CMPCI_CD_OUT_MUTE:
	case CMPCI_MIC_OUT_MUTE:
	case CMPCI_LINE_OUT_MUTE:
		cp->un.ord = sc->gain[cp->dev][CMPCI_LR];
		break;

	default:
		return EINVAL;
	}

	return 0;
}

/* ARGSUSED */
size_t
cmpci_round_buffersize(handle, direction, bufsize)
	void *handle;
	int direction;
	size_t bufsize;
{
	if (bufsize > 0x10000)
	    bufsize = 0x10000;
    
	return bufsize;
}

paddr_t
cmpci_mappage(handle, addr, offset, prot)
	void *handle;
	void *addr;
	off_t offset;
	int   prot;
{
	struct cmpci_softc *sc = handle;
	struct cmpci_dmanode *p;

	if ( offset < 0 || (p = cmpci_find_dmamem(sc, addr)) == NULL)
		return -1;

	return bus_dmamem_mmap(p->cd_tag, p->cd_segs,
			       sizeof(p->cd_segs)/sizeof(p->cd_segs[0]),
			       offset, prot, BUS_DMA_WAITOK);
}

/* ARGSUSED */
int
cmpci_get_props(handle)
	void *handle;
{
	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}


int
cmpci_trigger_output(handle, start, end, blksize, intr, arg, param)
        void *handle;
        void *start, *end;
        int blksize;
        void (*intr)(void *);
        void *arg;
        struct audio_params *param;
{
	struct cmpci_softc *sc = handle;
	struct cmpci_dmanode *p;
	int bps;

	sc->sc_play.intr = intr;
	sc->sc_play.intr_arg = arg;
	bps = param->channels * param->precision * param->factor / 8;
	if (!bps)
		return EINVAL;

	/* set DMA frame */
	if (!(p = cmpci_find_dmamem(sc, start)))
		return EINVAL;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CMPCI_REG_DMA0_BASE,
			  DMAADDR(p));
	delay(10);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CMPCI_REG_DMA0_BYTES,
			  ((caddr_t)end-(caddr_t)start+1)/bps-1);
	delay(10);

	/* set interrupt count */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CMPCI_REG_DMA0_SAMPLES,
			  (blksize+bps-1)/bps-1);
	delay(10);

	/* start DMA */
	cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_DIR); /* PLAY */
	cmpci_reg_set_4(sc, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH0_INTR_ENABLE);
	cmpci_reg_set_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_ENABLE);
	
	return 0;
}

int
cmpci_trigger_input(handle, start, end, blksize, intr, arg, param)
        void *handle;
        void *start, *end;
        int blksize;
        void (*intr)(void *);
        void *arg;
        struct audio_params *param;
{
	struct cmpci_softc *sc = handle;
	struct cmpci_dmanode *p;
	int bps;

	sc->sc_rec.intr = intr;
	sc->sc_rec.intr_arg = arg;
	bps = param->channels*param->precision*param->factor/8;
	if (!bps)
		return EINVAL;

	/* set DMA frame */
	if (!(p = cmpci_find_dmamem(sc, start)))
		return EINVAL;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CMPCI_REG_DMA1_BASE,
			  DMAADDR(p));
	delay(10);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CMPCI_REG_DMA1_BYTES,
			  ((caddr_t)end-(caddr_t)start+1)/bps-1);
	delay(10);

	/* set interrupt count */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CMPCI_REG_DMA1_SAMPLES,
			  (blksize+bps-1)/bps-1);
	delay(10);

	/* start DMA */
	cmpci_reg_set_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_DIR); /* REC */
	cmpci_reg_set_4(sc, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH1_INTR_ENABLE);
	cmpci_reg_set_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_ENABLE);
	
	return 0;
}
