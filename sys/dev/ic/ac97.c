/*      $OpenBSD: ac97.c,v 1.1 1999/09/19 06:45:12 csapuntz Exp $ */

/*
 * Copyright (c) 1999 Constantine Sapuntzakis
 *
 * Author:        Constantine Sapuntzakis <csapuntz@stanford.edu>
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
 * THIS SOFTWARE IS PROVIDED BY CONSTANTINE SAPUNTZAKIS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Partially inspired by FreeBSD's sys/dev/pcm/ac97.c. It came with
   the following copyright */

/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/ic/ac97.h>

#define AC97_REG_RESET                0x00
#define AC97_SOUND_ENHANCEMENT(reg)   (((reg) >> 10) & 0x1f)
#define AC97_REG_MASTER_VOLUME        0x02
#define AC97_REG_HEADPHONE_VOLUME     0x04
#define AC97_REG_MASTER_VOLUME_MONO   0x06
#define AC97_REG_MASTER_TONE          0x08
#define AC97_REG_PCBEEP_VOLUME        0x0a
#define AC97_REG_PHONE_VOLUME         0x0c
#define AC97_REG_MIC_VOLUME           0x0e
#define AC97_REG_LINEIN_VOLUME        0x10
#define AC97_REG_CD_VOLUME            0x12
#define AC97_REG_VIDEO_VOLUME         0x14
#define AC97_REG_AUX_VOLUME           0x16
#define AC97_REG_PCMOUT_VOLUME        0x18
#define AC97_REG_RECORD_SELECT        0x1a
#define AC97_REG_RECORD_GAIN          0x1c
#define AC97_REG_RECORD_GAIN_MIC      0x1e
#define AC97_REG_GP                   0x20
#define AC97_REG_3D_CONTROL           0x22
#define AC97_REG_POWER                0x26
#define AC97_REG_VENDOR_ID1           0x7c
#define AC97_REG_VENDOR_ID2           0x7e

static struct audio_mixer_enum ac97_on_off = { 2,
					       { { { AudioNoff } , 0 },
					         { { AudioNon }  , 1 } }};


static struct audio_mixer_enum ac97_mic_select = { 2,
					       { { { AudioNmicrophone "0" }, 
						   0 },
					         { { AudioNmicrophone "1" }, 
						   1 } }};

static struct audio_mixer_enum ac97_mono_select = { 2,
					       { { { AudioNmixerout },
						   0 },
					         { { AudioNmicrophone }, 
						   1 } }};

static struct audio_mixer_enum ac97_source = { 8,
					       { { { AudioNmicrophone } , 0 },
						 { { AudioNcd }, 1 },
						 { { "video" }, 2 },
						 { { AudioNaux }, 3 },
						 { { AudioNline }, 4 },
						 { { AudioNmixerout }, 5 },
						 { { AudioNmixerout AudioNmono }, 6 },
						 { { "phone" }, 7 }}};

static struct audio_mixer_value ac97_volume_stereo = { { AudioNvolume }, 
						       2 };


static struct audio_mixer_value ac97_volume_mono = { { AudioNvolume }, 
						     1 };

#define WRAP(a)  &a, sizeof(a)

struct ac97_source_info {
	char *class;
	char *device;
	char *qualifier;
	int  type;

	void *info;
	int  info_size;

	u_int8_t  reg;
	u_int8_t  bits:3;
	u_int8_t  ofs:4;
	u_int8_t  mute:1;
	u_int8_t  polarity:1;   /* Does 0 == MAX or MIN */

	int  prev;
	int  next;	
	int  mixer_class;
} source_info[] = {
	{ AudioCinputs ,            NULL,           NULL,    AUDIO_MIXER_CLASS,
	},
	{ AudioCoutputs,            NULL,           NULL,    AUDIO_MIXER_CLASS,
	},
	{ AudioCrecord ,            NULL,           NULL,    AUDIO_MIXER_CLASS,
	},
	/* Stereo master volume*/
	{ AudioCoutputs,     AudioNmaster,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo), 
	  AC97_REG_MASTER_VOLUME, 5, 0, 1,
	},
	/* Mono volume */
	{ AudioCoutputs,       AudioNmono,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono),
	  AC97_REG_MASTER_VOLUME_MONO, 6, 0, 1,
	},
	{ AudioCoutputs,       AudioNmono,AudioNsource,   AUDIO_MIXER_ENUM,
	  WRAP(ac97_mono_select),
	  AC97_REG_GP, 1, 9, 0,
	},
	/* Headphone volume */
	{ AudioCoutputs,  AudioNheadphone,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_HEADPHONE_VOLUME, 6, 0, 1,
	},
	/* Tone */
	{ AudioCoutputs,           "tone",        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_MASTER_TONE, 4, 0, 0,
	},
	/* PC Beep Volume */
	{ AudioCinputs,     AudioNspeaker,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono), 
	  AC97_REG_PCBEEP_VOLUME, 4, 1, 1,
	},
	/* Phone */
	{ AudioCinputs,           "phone",        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono), 
	  AC97_REG_PHONE_VOLUME, 5, 0, 1,
	},
	/* Mic Volume */
	{ AudioCinputs,  AudioNmicrophone,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono), 
	  AC97_REG_MIC_VOLUME, 5, 0, 1,
	},
	{ AudioCinputs,  AudioNmicrophone, AudioNpreamp,   AUDIO_MIXER_ENUM,
	  WRAP(ac97_on_off),
	  AC97_REG_MIC_VOLUME, 1, 6, 0,
	},
	{ AudioCinputs,  AudioNmicrophone, AudioNsource,   AUDIO_MIXER_ENUM,
	  WRAP(ac97_mic_select),
	  AC97_REG_GP, 1, 8, 0,
	},
	/* Line in Volume */
	{ AudioCinputs,        AudioNline,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_LINEIN_VOLUME, 5, 0, 1,
	},
	/* CD Volume */
	{ AudioCinputs,          AudioNcd,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_CD_VOLUME, 5, 0, 1,
	},
	/* Video Volume */
	{ AudioCinputs,           "video",        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_VIDEO_VOLUME, 5, 0, 1,
	},
	/* AUX volume */
	{ AudioCinputs,         AudioNaux,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_AUX_VOLUME, 5, 0, 1,
	},
	/* PCM out volume */
	{ AudioCinputs,         AudioNdac,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_PCMOUT_VOLUME, 5, 0, 1,
	},
	/* Record Source - some logic for this is hard coded - see below */
	{ AudioCrecord,      AudioNsource,        NULL,    AUDIO_MIXER_ENUM,
	  WRAP(ac97_source),
	  AC97_REG_RECORD_SELECT, 3, 0, 0,
	},
	/* Record Gain */
	{ AudioCrecord,      AudioNvolume,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_stereo),
	  AC97_REG_RECORD_GAIN, 4, 0, 1,
	},
	/* Record Gain mic */
	{ AudioCrecord,  AudioNmicrophone,        NULL,    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono), 
	  AC97_REG_RECORD_GAIN_MIC, 4, 0, 1, 1,
	},
	/* */
	{ AudioCoutputs,   AudioNloudness,        NULL,    AUDIO_MIXER_ENUM,
	  WRAP(ac97_on_off),
	  AC97_REG_GP, 1, 12, 0,
	},
	{ AudioCoutputs,    AudioNspatial,        NULL,    AUDIO_MIXER_ENUM,
	  WRAP(ac97_on_off),
	  AC97_REG_GP, 1, 13, 0,
	},
	{ AudioCoutputs,    AudioNspatial,    "center",    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono), 
	  AC97_REG_3D_CONTROL, 4, 8, 0, 1,
	},
	{ AudioCoutputs,    AudioNspatial,     "depth",    AUDIO_MIXER_VALUE,
	  WRAP(ac97_volume_mono), 
	  AC97_REG_3D_CONTROL, 4, 0, 0, 1,
	},

	/* Missing features: Simulated Stereo, POP, Loopback mode */
} ;

#define SOURCE_INFO_SIZE (sizeof(source_info)/sizeof(source_info[0]))

/*
 * Check out http://developer.intel.com/pc-supp/platform/ac97/ for
 * information on AC-97
 */

struct ac97_softc {
	struct ac97_codec_if codecIf;

	struct ac97_host_if *hostIf;

	struct ac97_source_info source_info[2 * SOURCE_INFO_SIZE];
	int num_source_info;
};

int ac97_mixer_get_port __P((struct ac97_codec_if *self, mixer_ctrl_t *cp));
int ac97_mixer_set_port __P((struct ac97_codec_if *self, mixer_ctrl_t *));
int ac97_query_devinfo __P((struct ac97_codec_if *self, mixer_devinfo_t *));

struct ac97_codec_if_vtbl ac97civ = {
	ac97_mixer_get_port, 
	ac97_mixer_set_port,
	ac97_query_devinfo
};

static struct ac97_codecid {
	u_int32_t id;
	char *name;
} ac97codecid[] = {
	{ 0x414B4D00, "Asahi Kasei AK4540" 	},
	{ 0x43525900, "Cirrus Logic CS4297" 	},
	{ 0x83847600, "SigmaTel STAC????" 	},
	{ 0x83847604, "SigmaTel STAC9701/3/4/5" },
	{ 0x83847605, "SigmaTel STAC9704" 	},
	{ 0x83847608, "SigmaTel STAC9708" 	},
	{ 0x83847609, "SigmaTel STAC9721" 	},
	{ 0, 	      NULL			}
};

static char *ac97enhancement[] = {
	"No 3D Stereo",
	"Analog Devices Phat Stereo",
	"Creative"
	"National Semi 3D",
	"Yamaha Ymersion",
	"BBE 3D",
	"Crystal Semi 3D"
	"Qsound QXpander",
	"Spatializer 3D",
	"SRS 3D",
	"Platform Tech 3D",
	"AKM 3D",
	"Aureal",
	"AZTECH 3D",
	"Binaura 3D",
	"ESS Technology",
	"Harman International VMAx",
	"Nvidea 3D",
	"Philips Incredible Sound",
	"Texas Instruments' 3D",
	"VLSI Technology 3D",
	"TriTech 3D",
	"Realtek 3D",
	"Samsung 3D",
	"Wolfson Microelectronics 3D",
	"Delta Integration 3D",
	"SigmaTel 3D",
	"Unknown 3D",
	"Rockwell 3D",
	"Unknown 3D",
	"Unknown 3D",
	"Unknown 3D",
};

static char *ac97feature[] = {
	"mic channel",
	"reserved",
	"tone",
	"simulated stereo",
	"headphone",
	"bass boost",
	"18 bit DAC",
	"20 bit DAC",
	"18 bit ADC",
	"20 bit ADC"
};


int ac97_str_equal __P((char *, char *));
void ac97_setup_source_info __P((struct ac97_softc *));

#define AUDIO_DEBUG
#define AC97_DEBUG 0

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (ac97debug) printf x
#define DPRINTFN(n,x)	if (ac97debug>(n)) printf x
#ifdef AC97_DEBUG
int	ac97debug = AC97_DEBUG;
#else
int	ac97debug = 0;
#endif
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif


int
ac97_str_equal(a, b)
	char *a, *b;
{
	return ((a == b) || (a && b && (!strcmp(a, b))));
}

void
ac97_setup_source_info(as)
	struct ac97_softc *as;
{
	int idx, ouridx;
	struct ac97_source_info *si, *si2; 

	for (idx = 0, ouridx = 0; idx < SOURCE_INFO_SIZE; idx++) {
		si = &as->source_info[ouridx];

		bcopy(&source_info[idx], si, sizeof(*si));

		switch (si->type) {
		case AUDIO_MIXER_CLASS:
		        si->mixer_class = ouridx;
			ouridx++;
			break;
		case AUDIO_MIXER_VALUE:
			/* Todo - Test to see if it works */
			ouridx++;

			/* Add an entry for mute, if necessary */
			if (si->mute) {
				si = &as->source_info[ouridx];
				bcopy(&source_info[idx], si, sizeof(*si));
				si->qualifier = AudioNmute;
				si->type = AUDIO_MIXER_ENUM;
				si->info = &ac97_on_off;
				si->info_size = sizeof(ac97_on_off);
				si->bits = 1;
				si->ofs = 15;
				si->mute = 0;
				si->polarity = 0;
				ouridx++;
			}
			break;
		case AUDIO_MIXER_ENUM:
			/* Todo - Test to see if it works */
			ouridx++;
			break;
		default:
			printf ("ac97: shouldn't get here\n");
			break;
		}
	}

	as->num_source_info = ouridx;

	for (idx = 0; idx < as->num_source_info; idx++) {
		int idx2, previdx;

		si = &as->source_info[idx];

		/* Find mixer class */
		for (idx2 = 0; idx2 < as->num_source_info; idx2++) {
			si2 = &as->source_info[idx2];

			if (si2->type == AUDIO_MIXER_CLASS && 
			    ac97_str_equal(si->class,
					   si2->class)) {
				si->mixer_class = idx2;
			}
		}


		/* Setup prev and next pointers */
		if (si->prev != 0)
			continue;

		if (si->qualifier)
			continue;

		si->prev = AUDIO_MIXER_LAST;
		previdx = idx;

		for (idx2 = 0; idx2 < as->num_source_info; idx2++) {
			if (idx2 == idx)
				continue;

			si2 = &as->source_info[idx2];

			if (!si2->prev &&
			    ac97_str_equal(si->class, si2->class) &&
			    ac97_str_equal(si->device, si2->device)) {
				as->source_info[previdx].next = idx2;
				as->source_info[idx2].prev = previdx;
				
				previdx = idx2;
			}
		}

		as->source_info[previdx].next = AUDIO_MIXER_LAST;
	}
}

int 
ac97_attach(hostIf)
	struct ac97_host_if *hostIf;
{
	struct ac97_softc *as;
	int error, i, j;
	u_int16_t id1, id2, caps;
	u_int32_t id;
	
	as = malloc(sizeof(struct ac97_softc), M_DEVBUF, M_WAITOK);

	if (!as) return (ENOMEM);

	as->codecIf.vtbl = &ac97civ;
	as->hostIf = hostIf;

	if ((error = hostIf->attach(hostIf->arg, &as->codecIf))) {
		free (as, M_DEVBUF);
		return (error);
	}

	hostIf->reset(hostIf->arg);
	DELAY(1000);

	hostIf->write(hostIf->arg, AC97_REG_POWER, 0);
	hostIf->write(hostIf->arg, AC97_REG_RESET, 0);
	DELAY(10000);

	if ((error = hostIf->read(hostIf->arg, AC97_REG_VENDOR_ID1, &id1)))
		return (error);

	if ((error = hostIf->read(hostIf->arg, AC97_REG_VENDOR_ID2, &id2)))
		return (error);

	if ((error = hostIf->read(hostIf->arg, AC97_REG_RESET, &caps)))
		return (error);

	id = (id1 << 16) | id2;

	printf("ac97: codec id 0x%8x", id);
	for (i = 0; ac97codecid[i].id; i++) {
		if (ac97codecid[i].id == id) 
			printf(" (%s)", ac97codecid[i].name);
	}
	printf("\nac97: codec features ");
	for (i = j = 0; i < 10; i++) {
		if (caps & (1 << i)) {
			printf("%s%s", j? ", " : "", ac97feature[i]);
			j++;
		}
	}

	printf("%s%s\n", j? ", " : "", ac97enhancement[(caps >> 10) & 0x1f]);

	ac97_setup_source_info(as);

	return (0);
}


int 
ac97_query_devinfo(codec_if, dip)
	struct ac97_codec_if *codec_if;
	mixer_devinfo_t *dip;
{
	struct ac97_softc *as = (struct ac97_softc *)codec_if;

	if (dip->index < as->num_source_info) {
		struct ac97_source_info *si = &as->source_info[dip->index];
		char *name;

		dip->type = si->type;
		dip->mixer_class = si->mixer_class;
		dip->prev = si->prev;
		dip->next = si->next;
		
		if (si->qualifier)
			name = si->qualifier;
		else if (si->device)
			name = si->device;
		else if (si->class)
			name = si->class;
		
		if (name)
			strcpy(dip->label.name, name);

		bcopy(si->info, &dip->un, si->info_size);
		return (0);
	}

	return (ENXIO);
}



int
ac97_mixer_set_port(codec_if, cp)
	struct ac97_codec_if *codec_if;
	mixer_ctrl_t *cp;
{
	struct ac97_softc *as = (struct ac97_softc *)codec_if;
	struct ac97_source_info *si = &as->source_info[cp->dev];
	u_int16_t mask;
	u_int16_t val, newval;
	int error;

	if (cp->dev < 0 || cp->dev >= as->num_source_info)
		return (EINVAL);

	if (cp->type != si->type)
		return (EINVAL);

	error = as->hostIf->read(as->hostIf->arg, si->reg, &val);
	if (error)
		return (error);

	DPRINTFN(5, ("read(%x) = %x\n", si->reg, val));

	mask = (1 << si->bits) - 1;

	switch (cp->type) {
	case AUDIO_MIXER_ENUM:
		if (cp->un.ord > mask || cp->un.ord < 0)
			return (EINVAL);

		newval = (cp->un.ord << si->ofs);
		if (si->reg == AC97_REG_RECORD_SELECT) {
			newval |= (newval << (8 + si->ofs));
			mask |= (mask << 8);
		}
		break;
	case AUDIO_MIXER_VALUE:
	{
		struct audio_mixer_value *value = si->info;
		u_int16_t  l, r;

		if (cp->un.value.num_channels != 
		    value->num_channels) return (EINVAL);

		if (cp->un.value.num_channels == 1) {
			l = r = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		} else {
			l = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			r = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
		}

		if (!si->polarity) {
			l = 255 - l;
			r = 255 - r;
		}
		
		l = l >> (8 - si->bits);
		r = r >> (8 - si->bits);

		newval = ((l & mask) << si->ofs);
		if (value->num_channels == 2) {
			newval |= ((r & mask) << (si->ofs + 8));
			mask |= (mask << 8);
		}

		break;
	}
	default:
		return (EINVAL);
	}

	mask = mask << si->ofs;
	error = as->hostIf->write(as->hostIf->arg, si->reg, (val & ~mask) | newval);
	if (error)
		return (error);

	return (0);
}

int
ac97_mixer_get_port(codec_if, cp)
	struct ac97_codec_if *codec_if;
	mixer_ctrl_t *cp;
{
	struct ac97_softc *as = (struct ac97_softc *)codec_if;
	struct ac97_source_info *si = &as->source_info[cp->dev];
	u_int16_t mask;
	u_int16_t val;
	int error;

	if (cp->dev < 0 || cp->dev >= as->num_source_info)
		return (EINVAL);

	if (cp->type != si->type)
		return (EINVAL);

	error = as->hostIf->read(as->hostIf->arg, si->reg, &val);
	if (error)
		return (error);

	DPRINTFN(5, ("read(%x) = %x\n", si->reg, val));

	mask = (1 << si->bits) - 1;

	switch (cp->type) {
	case AUDIO_MIXER_ENUM:
		cp->un.ord = (val >> si->ofs) & mask;
		DPRINTFN(4, ("AUDIO_MIXER_ENUM: %x %d %x %d\n", val, si->ofs, mask, cp->un.ord));
		break;
	case AUDIO_MIXER_VALUE:
	{
		struct audio_mixer_value *value = si->info;
		u_int16_t  l, r;

		if (cp->un.value.num_channels != 
		    value->num_channels) return (EINVAL);

		if (value->num_channels == 1) {
			l = r = (val >> si->ofs) & mask;
		} else {
			l = (val >> si->ofs) & mask;
			r = (val >> (si->ofs + 8)) & mask;
		}

		l = (l << (8 - si->bits));
		r = (r << (8 - si->bits));
		if (!si->polarity) {
			l = 255 - l;
			r = 255 - r;
		}

		/* The EAP driver averages l and r for stereo
		   channels that are requested in MONO mode. Does this
		   make sense? */
		if (cp->un.value.num_channels == 1) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = l;
		} else if (cp->un.value.num_channels == 2) {
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = l;
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = r;
		}

		break;
	}
	default:
		return (EINVAL);
	}

	return (0);
}

