/* $OpenBSD: ym.c,v 1.1 1998/05/08 18:37:25 csapuntz Exp $ */


/*
 * Copyright (c) 1998 Constantine Sapuntzakis. All rights reserved.
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



#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/pio.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ad1848reg.h>
#include <dev/isa/ad1848var.h>
#include <dev/ic/opl3sa3.h>
#include <dev/isa/ymvar.h>


int	ym_getdev __P((void *, struct audio_device *));
int	ym_mixer_set_port __P((void *, mixer_ctrl_t *));
int	ym_mixer_get_port __P((void *, mixer_ctrl_t *));
int	ym_query_devinfo __P((void *, mixer_devinfo_t *));

static void ym_mute __P((struct ym_softc *, int, int));
static void ym_set_master_gain __P((struct ym_softc *, struct ad1848_volume *));
static void ym_set_mic_gain __P((struct ym_softc *, struct ad1848_volume *));



struct audio_hw_if ym_hw_if = {
	ad1848_open,
	ad1848_close,
	NULL,
	ad1848_query_encoding,
	ad1848_set_params,
	ad1848_round_blocksize,
	ad1848_commit_settings,
	ad1848_dma_init_output,
	ad1848_dma_init_input,
	ad1848_dma_output,
	ad1848_dma_input,
	ad1848_halt_out_dma,
	ad1848_halt_in_dma,
	NULL,
	ym_getdev,
	NULL,
	ym_mixer_set_port,
	ym_mixer_get_port,
	ym_query_devinfo,
	ad1848_malloc,
	ad1848_free,
	ad1848_round,
	ad1848_mappage,
	ad1848_get_props,
};


struct cfdriver ym_cd = {
        NULL, "ym", DV_DULL
};

struct audio_device ym_device = {
	"ym,ad1848",
	"",
	"ym"
};

static __inline int ym_read __P((struct ym_softc *, int));
static __inline void ym_write __P((struct ym_softc *, int, int));

void
ym_attach(sc)
    struct ym_softc *sc;

{
  struct ad1848_volume vol_mid = {220, 220};
  struct ad1848_volume vol_0   = {0, 0};
  
  sc->sc_ih = isa_intr_establish(sc->sc_ic, sc->ym_irq, IST_EDGE, IPL_AUDIO,
			    ad1848_intr, &sc->sc_ad1848, sc->sc_dev.dv_xname);

  ad1848_attach(&sc->sc_ad1848);
  sc->sc_ad1848.parent = sc;

  /* Establish chip in well known mode */
  ym_set_master_gain(sc, &vol_mid);
  ym_set_mic_gain(sc, &vol_0);
  sc->master_mute = 0;
  ym_mute(sc, SA3_LCH, sc->master_mute);
  ym_mute(sc, SA3_RCH, sc->master_mute);

  sc->mic_mute = 1;
  ym_mute(sc, SA3_MIC, sc->mic_mute);

  audio_attach_mi(&ym_hw_if, 0, &sc->sc_ad1848, &sc->sc_dev);
}

static __inline int
ym_read(sc, reg)
    struct ym_softc *sc;
    int reg;
{
  bus_space_write_1(sc->sc_iot, sc->sc_controlioh, 0, 0x1d);
  bus_space_write_1(sc->sc_iot, sc->sc_controlioh, 0, (reg & 0xff));
  return (bus_space_read_1(sc->sc_iot, sc->sc_controlioh, 1));
}

static __inline void
ym_write(sc, reg, data)
    struct ym_softc *sc;
    int reg;
    int data;
{
  bus_space_write_1(sc->sc_iot, sc->sc_controlioh, 0, 0x1d);
  bus_space_write_1(sc->sc_iot, sc->sc_controlioh, 0, (reg & 0xff));
  bus_space_write_1(sc->sc_iot, sc->sc_controlioh, 1, (data & 0xff));
}



int
ym_getdev(addr, retp)
    void *addr;
    struct audio_device *retp;
{
    *retp = ym_device;
    return 0;
}


static ad1848_devmap_t mappings[] = {
{ YM_MIDI_LVL, AD1848_KIND_LVL, AD1848_AUX2_CHANNEL },
{ YM_CD_LVL, AD1848_KIND_LVL, AD1848_AUX1_CHANNEL },
{ YM_DAC_LVL, AD1848_KIND_LVL, AD1848_DAC_CHANNEL },
{ YM_LINE_LVL, AD1848_KIND_LVL, AD1848_LINE_CHANNEL },
{ YM_SPEAKER_LVL, AD1848_KIND_LVL, AD1848_MONO_CHANNEL },
{ YM_MONITOR_LVL, AD1848_KIND_LVL, AD1848_MONITOR_CHANNEL },
{ YM_MIDI_MUTE, AD1848_KIND_MUTE, AD1848_AUX2_CHANNEL },
{ YM_CD_MUTE, AD1848_KIND_MUTE, AD1848_AUX1_CHANNEL },
{ YM_DAC_MUTE, AD1848_KIND_MUTE, AD1848_DAC_CHANNEL },
{ YM_LINE_MUTE, AD1848_KIND_MUTE, AD1848_LINE_CHANNEL },
{ YM_SPEAKER_MUTE, AD1848_KIND_MUTE, AD1848_MONO_CHANNEL },
{ YM_MONITOR_MUTE, AD1848_KIND_MUTE, AD1848_MONITOR_CHANNEL },
{ YM_REC_LVL, AD1848_KIND_RECORDGAIN, -1 },
{ YM_RECORD_SOURCE, AD1848_KIND_RECORDSOURCE, -1}
};

static int nummap = sizeof(mappings) / sizeof(mappings[0]);


static void
ym_mute(sc, left_reg, mute)
  struct ym_softc *sc;
  int left_reg;
  int mute;

{
  u_char reg;

  if (mute) {
    reg = ym_read(sc, left_reg);
    ym_write (sc, left_reg, reg | 0x80);
  } else {
    reg = ym_read(sc, left_reg);
    ym_write (sc, left_reg, reg & ~0x80);
  }
}

#define MIC_ATTEN_BITS 0x1f
#define MASTER_ATTEN_BITS 0x0f


static void
ym_set_master_gain(sc, vol)
  struct ym_softc *sc;
  struct ad1848_volume *vol;
{
  u_char reg;
  u_int  atten;

  sc->master_gain = *vol;

  atten = ((AUDIO_MAX_GAIN - vol->left) * MASTER_ATTEN_BITS)/AUDIO_MAX_GAIN;

  reg = ym_read(sc, SA3_LCH);

  reg &= ~(MASTER_ATTEN_BITS);
  reg |= atten;

  ym_write (sc, SA3_LCH, reg);

  atten = ((AUDIO_MAX_GAIN - vol->right) * MASTER_ATTEN_BITS)/AUDIO_MAX_GAIN;

  reg = ym_read(sc, SA3_RCH) & ~(MASTER_ATTEN_BITS);
  reg |= atten;

  ym_write (sc, SA3_RCH, reg);
}

static void
ym_set_mic_gain(sc, vol)
  struct ym_softc *sc;
  struct ad1848_volume *vol;
{
  u_char reg;
  u_int  atten;

  sc->mic_gain = *vol;

  atten = ((AUDIO_MAX_GAIN - vol->left) * MIC_ATTEN_BITS)/AUDIO_MAX_GAIN;

  reg = ym_read(sc, SA3_MIC) & ~(MIC_ATTEN_BITS);
  reg |= atten;

  ym_write (sc, SA3_MIC, reg);
}

int
ym_mixer_set_port(addr, cp)
    void *addr;
    mixer_ctrl_t *cp;
{
    struct ad1848_softc *ac = addr;
    struct ym_softc *sc = ac->parent;
    struct ad1848_volume vol;
    int error = ad1848_mixer_set_port(ac, mappings, nummap, cp);
    
    if (error != ENXIO)
      return (error);

    error = 0;

    switch (cp->dev) {
    case YM_OUTPUT_LVL:
      ad1848_to_vol(cp, &vol);
      ym_set_master_gain(sc, &vol);
      break;

    case YM_OUTPUT_MUTE:
      sc->master_mute = (cp->un.ord != 0);
      ym_mute(sc, SA3_LCH, sc->master_mute);
      ym_mute(sc, SA3_RCH, sc->master_mute);
      break;

    case YM_MIC_LVL:
      if (cp->un.value.num_channels != 1)
	error = EINVAL;

      ad1848_to_vol(cp, &vol);
      ym_set_mic_gain(sc, &vol);      
      break;

    case YM_MIC_MUTE:
      sc->mic_mute = (cp->un.ord != 0);
      ym_mute(sc, SA3_MIC, sc->mic_mute);
      break;

    default:
	    return ENXIO;
	    /*NOTREACHED*/
    }
    
    return (error);
}

int
ym_mixer_get_port(addr, cp)
    void *addr;
    mixer_ctrl_t *cp;
{
    struct ad1848_softc *ac = addr;
    struct ym_softc *sc = ac->parent;

    int error = ad1848_mixer_get_port(ac, mappings, nummap, cp);

    if (error != ENXIO)
      return (error);

    error = 0;

    switch (cp->dev) {
    case YM_OUTPUT_LVL:
      ad1848_from_vol(cp, &sc->master_gain);
      break;

    case YM_OUTPUT_MUTE:
      cp->un.ord = sc->master_mute;
      break;

    case YM_MIC_LVL:
      if (cp->un.value.num_channels != 1)
	error = EINVAL;

      ad1848_from_vol(cp, &sc->mic_gain);
      break;

    case YM_MIC_MUTE:
      cp->un.ord = sc->mic_mute;
      break;

    default:
	error = ENXIO;
	break;
    }

    return(error);
}

static char *mixer_classes[] = { AudioCinputs, AudioCrecord, AudioCoutputs,
				 AudioCmonitor };

int
ym_query_devinfo(addr, dip)
    void *addr;
    mixer_devinfo_t *dip;
{
  static char *mixer_port_names[] = { AudioNmidi, AudioNcd, AudioNdac,
				       AudioNline, AudioNspeaker, 
				       AudioNmicrophone, 
                                       AudioNmonitor};

    dip->next = dip->prev = AUDIO_MIXER_LAST;

    switch(dip->index) {
    case YM_INPUT_CLASS:			/* input class descriptor */
    case YM_OUTPUT_CLASS:
    case YM_MONITOR_CLASS:
    case YM_RECORD_CLASS:
	dip->type = AUDIO_MIXER_CLASS;
	dip->mixer_class = dip->index;
	strcpy(dip->label.name, 
	       mixer_classes[dip->index - YM_INPUT_CLASS]);
	break;

    case YM_MIDI_LVL:
    case YM_CD_LVL:
    case YM_DAC_LVL:
    case YM_LINE_LVL: 
    case YM_SPEAKER_LVL:
    case YM_MIC_LVL:
    case YM_MONITOR_LVL:
	dip->type = AUDIO_MIXER_VALUE;
	if (dip->index == YM_MONITOR_LVL)
	  dip->mixer_class = YM_MONITOR_CLASS;
	else
	  dip->mixer_class = YM_INPUT_CLASS;

	dip->next = dip->index + 7;

	strcpy(dip->label.name,mixer_port_names[dip->index - YM_MIDI_LVL]);

	if (dip->index == YM_SPEAKER_LVL ||
	    dip->index == YM_MIC_LVL)
	  dip->un.v.num_channels = 1;
	else
	  dip->un.v.num_channels = 2;

	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case YM_MIDI_MUTE:
    case YM_CD_MUTE:
    case YM_DAC_MUTE:
    case YM_LINE_MUTE:
    case YM_SPEAKER_MUTE:
    case YM_MIC_MUTE:
    case YM_MONITOR_MUTE:
	if (dip->index == YM_MONITOR_MUTE)
	  dip->mixer_class = YM_MONITOR_CLASS;
	else
	  dip->mixer_class = YM_INPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = dip->index - 7;
mute:
	strcpy(dip->label.name, AudioNmute);
	dip->un.e.num_mem = 2;
	strcpy(dip->un.e.member[0].label.name, AudioNoff);
	dip->un.e.member[0].ord = 0;
	strcpy(dip->un.e.member[1].label.name, AudioNon);
	dip->un.e.member[1].ord = 1;
	break;


    case YM_OUTPUT_LVL:
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = YM_OUTPUT_CLASS;
	dip->next = YM_OUTPUT_MUTE;
	strcpy(dip->label.name, AudioNmaster);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;

    case YM_OUTPUT_MUTE:
	dip->mixer_class = YM_OUTPUT_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = YM_OUTPUT_LVL;
	goto mute;
      
    case YM_REC_LVL:	/* record level */
	dip->type = AUDIO_MIXER_VALUE;
	dip->mixer_class = YM_RECORD_CLASS;
	dip->next = YM_RECORD_SOURCE;
	strcpy(dip->label.name, AudioNrecord);
	dip->un.v.num_channels = 2;
	strcpy(dip->un.v.units.name, AudioNvolume);
	break;
	

    case YM_RECORD_SOURCE:
	dip->mixer_class = YM_RECORD_CLASS;
	dip->type = AUDIO_MIXER_ENUM;
	dip->prev = YM_REC_LVL;
	strcpy(dip->label.name, AudioNsource);
	dip->un.e.num_mem = 4;
	strcpy(dip->un.e.member[0].label.name, AudioNmicrophone);
	dip->un.e.member[0].ord = MIC_IN_PORT;
	strcpy(dip->un.e.member[1].label.name, AudioNline);
	dip->un.e.member[1].ord = LINE_IN_PORT;
	strcpy(dip->un.e.member[2].label.name, AudioNdac);
	dip->un.e.member[2].ord = DAC_IN_PORT;
	strcpy(dip->un.e.member[3].label.name, AudioNcd);
	dip->un.e.member[3].ord = AUX1_IN_PORT;
	break;

    default:
	return ENXIO;
	/*NOTREACHED*/
    }

    return 0;
}
