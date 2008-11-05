/*	$OpenBSD: azalia_codec.c,v 1.57 2008/11/05 01:14:01 jakemsr Exp $	*/
/*	$NetBSD: azalia_codec.c,v 1.8 2006/05/10 11:17:27 kent Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by TAMURA Kent
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <uvm/uvm_param.h>
#include <dev/pci/azalia.h>

#define XNAME(co)	(((struct device *)co->az)->dv_xname)
#define MIXER_DELTA(n)	(AUDIO_MAX_GAIN / (n))

#define AZ_CLASS_INPUT	0
#define AZ_CLASS_OUTPUT	1
#define AZ_CLASS_RECORD	2
#define ENUM_OFFON	.un.e={2, {{{AudioNoff}, 0}, {{AudioNon}, 1}}}
#define ENUM_IO		.un.e={2, {{{"input"}, 0}, {{"output"}, 1}}}
#define AzaliaNfront	"front"
#define AzaliaNclfe	"clfe"
#define AzaliaNside	"side"

#define ALC260_FUJITSU_ID	0x132610cf
#define REALTEK_ALC660		0x10ec0660
#define ALC660_ASUS_G2K		0x13391043
#define REALTEK_ALC880		0x10ec0880
#define ALC880_ASUS_M5200	0x19931043
#define ALC880_ASUS_A7M		0x13231043
#define ALC880_MEDION_MD95257	0x203d161f
#define REALTEK_ALC882		0x10ec0882
#define ALC882_ASUS_A7T		0x13c21043
#define ALC882_ASUS_W2J		0x19711043
#define REALTEK_ALC883		0x10ec0883
#define ALC883_ACER_ID		0x00981025
#define REALTEK_ALC885		0x10ec0885
#define ALC885_APPLE_MB3	0x00a1106b
#define ALC885_APPLE_MB4	0x00a3106b
#define SIGMATEL_STAC9221	0x83847680
#define STAC9221_APPLE_ID	0x76808384
#define SIGMATEL_STAC9205	0x838476a0
#define STAC9205_DELL_D630	0x01f91028
#define STAC9205_DELL_V1500	0x02281028

int	azalia_generic_codec_init_dacgroup(codec_t *);
int	azalia_generic_codec_add_dacgroup(codec_t *, int, uint32_t);
int	azalia_generic_codec_find_pin(const codec_t *, int, int, uint32_t);
int	azalia_generic_codec_find_dac(const codec_t *, int, int);

int	azalia_generic_mixer_init(codec_t *);
int	azalia_generic_mixer_autoinit(codec_t *);

int	azalia_generic_mixer_fix_indexes(codec_t *);
int	azalia_generic_mixer_default(codec_t *);
int	azalia_generic_mixer_pin_sense(codec_t *);
int	azalia_generic_mixer_create_virtual(codec_t *);
int	azalia_generic_mixer_delete(codec_t *);
int	azalia_generic_mixer_ensure_capacity(codec_t *, size_t);
int	azalia_generic_mixer_get(const codec_t *, nid_t, int, mixer_ctrl_t *);
int	azalia_generic_mixer_set(codec_t *, nid_t, int, const mixer_ctrl_t *);
u_char	azalia_generic_mixer_from_device_value
	(const codec_t *, nid_t, int, uint32_t );
uint32_t azalia_generic_mixer_to_device_value
	(const codec_t *, nid_t, int, u_char);
int	azalia_generic_set_port(codec_t *, mixer_ctrl_t *);
int	azalia_generic_get_port(codec_t *, mixer_ctrl_t *);
int	azalia_gpio_unmute(codec_t *, int);

int	azalia_alc260_init_dacgroup(codec_t *);
int	azalia_alc260_mixer_init(codec_t *);
int	azalia_alc260_set_port(codec_t *, mixer_ctrl_t *);
int	azalia_alc662_init_dacgroup(codec_t *);
int	azalia_alc861_init_dacgroup(codec_t *);
int	azalia_alc880_init_dacgroup(codec_t *);
int	azalia_alc882_init_dacgroup(codec_t *);
int	azalia_alc882_mixer_init(codec_t *);
int	azalia_alc882_set_port(codec_t *, mixer_ctrl_t *);
int	azalia_alc882_get_port(codec_t *, mixer_ctrl_t *);
int	azalia_alc883_init_dacgroup(codec_t *);
int	azalia_alc883_mixer_init(codec_t *);
int	azalia_alc885_init_dacgroup(codec_t *);
int	azalia_alc888_init_dacgroup(codec_t *);
int	azalia_ad1984_init_dacgroup(codec_t *);
int	azalia_ad1984_mixer_init(codec_t *);
int	azalia_ad1984_set_port(codec_t *, mixer_ctrl_t *);
int	azalia_ad1984_get_port(codec_t *, mixer_ctrl_t *);
int	azalia_ad1988_init_dacgroup(codec_t *);
int	azalia_cmi9880_init_dacgroup(codec_t *);
int	azalia_cmi9880_mixer_init(codec_t *);
int	azalia_stac9200_mixer_init(codec_t *);
int	azalia_stac9221_mixer_init(codec_t *);
int	azalia_stac9221_init_dacgroup(codec_t *);
int	azalia_stac9221_set_port(codec_t *, mixer_ctrl_t *);
int	azalia_stac9221_get_port(codec_t *, mixer_ctrl_t *);
int	azalia_stac7661_init_dacgroup(codec_t *);
int	azalia_stac7661_mixer_init(codec_t *);
int	azalia_stac7661_set_port(codec_t *, mixer_ctrl_t *);
int	azalia_stac7661_get_port(codec_t *, mixer_ctrl_t *);

int
azalia_codec_init_vtbl(codec_t *this)
{
	/**
	 * We can refer this->vid and this->subid.
	 */
	this->name = NULL;
	this->init_dacgroup = azalia_generic_codec_init_dacgroup;
	this->mixer_init = azalia_generic_mixer_autoinit;
	this->mixer_delete = azalia_generic_mixer_delete;
	this->set_port = azalia_generic_set_port;
	this->get_port = azalia_generic_get_port;
	this->unsol_event = NULL;
	switch (this->vid) {
	case 0x10ec0260:
		this->name = "Realtek ALC260";
		this->mixer_init = azalia_alc260_mixer_init;
		this->init_dacgroup = azalia_alc260_init_dacgroup;
		this->set_port = azalia_alc260_set_port;
		break;
	case 0x10ec0268:
		this->name = "Realtek ALC268";
		break;
	case 0x10ec0269:
		this->name = "Realtek ALC269";
		break;
	case 0x10ec0662:
		this->name = "Realtek ALC662-GR";
		this->init_dacgroup = azalia_alc662_init_dacgroup;
		break;
	case 0x10ec0861:
		this->name = "Realtek ALC861";
		this->init_dacgroup = azalia_alc861_init_dacgroup;
		break;
	case 0x10ec0880:
		this->name = "Realtek ALC880";
		this->init_dacgroup = azalia_alc880_init_dacgroup;
		break;
	case 0x10ec0882:
		this->name = "Realtek ALC882";
		this->init_dacgroup = azalia_alc882_init_dacgroup;
		this->mixer_init = azalia_alc882_mixer_init;
		this->get_port = azalia_alc882_get_port;
		this->set_port = azalia_alc882_set_port;
		break;
	case 0x10ec0883:
		/* ftp://209.216.61.149/pc/audio/ALC883_DataSheet_1.3.pdf */
		this->name = "Realtek ALC883";
		this->init_dacgroup = azalia_alc883_init_dacgroup;
		this->mixer_init = azalia_alc883_mixer_init;
		this->get_port = azalia_alc882_get_port;
		this->set_port = azalia_alc882_set_port;
		break;
	case 0x10ec0885:
		this->name = "Realtek ALC885";
		this->init_dacgroup = azalia_alc885_init_dacgroup;
		break;
	case 0x10ec0888:
		this->name = "Realtek ALC888";
		this->init_dacgroup = azalia_alc888_init_dacgroup;
		break;
	case 0x11d41983:
		/* http://www.analog.com/en/prod/0,2877,AD1983,00.html */
		this->name = "Analog Devices AD1983";
		break;
	case 0x11d41984:
		/* http://www.analog.com/en/prod/0,2877,AD1984,00.html */
		this->name = "Analog Devices AD1984";
		this->init_dacgroup = azalia_ad1984_init_dacgroup;
		this->mixer_init = azalia_ad1984_mixer_init;
		this->get_port = azalia_ad1984_get_port;
		this->set_port = azalia_ad1984_set_port;
		break;
	case 0x11d41988:
		/* http://www.analog.com/en/prod/0,2877,AD1988A,00.html */
		this->name = "Analog Devices AD1988A";
		this->init_dacgroup = azalia_ad1988_init_dacgroup;
		break;
	case 0x11d4198b:
		/* http://www.analog.com/en/prod/0,2877,AD1988B,00.html */
		this->name = "Analog Devices AD1988B";
		this->init_dacgroup = azalia_ad1988_init_dacgroup;
		break;
	case 0x434d4980:
		this->name = "CMedia CMI9880";
		this->init_dacgroup = azalia_cmi9880_init_dacgroup;
		this->mixer_init = azalia_cmi9880_mixer_init;
		break;
	case 0x83847680:
		this->name = "Sigmatel STAC9221";
		this->init_dacgroup = azalia_stac9221_init_dacgroup;
		this->mixer_init = azalia_stac9221_mixer_init;
		this->set_port = azalia_stac9221_set_port;
		this->get_port = azalia_stac9221_get_port;
		break;
	case 0x83847683:
		this->name = "Sigmatel STAC9221D";
		this->init_dacgroup = azalia_stac9221_init_dacgroup;
		break;
	case 0x83847690:
		/* http://www.idt.com/products/getDoc.cfm?docID=17812077 */
		this->name = "Sigmatel STAC9200";
		this->mixer_init = azalia_stac9200_mixer_init;
		break;
	case 0x83847691:
		this->name = "Sigmatel STAC9200D";
		break;
	case 0x83847661:
	case 0x83847662:
		this->name = "Sigmatel STAC9872AK";
		this->init_dacgroup = azalia_stac7661_init_dacgroup;
		this->mixer_init = azalia_stac7661_mixer_init;
		this->get_port = azalia_stac7661_get_port;
		this->set_port = azalia_stac7661_set_port;
		break;
	}
	return 0;
}

/* ----------------------------------------------------------------
 * functions for generic codecs
 * ---------------------------------------------------------------- */

int
azalia_generic_codec_init_dacgroup(codec_t *this)
{
	int i, j, assoc, group;

	/*
	 * grouping DACs
	 *   [0] the lowest assoc DACs
	 *   [1] the lowest assoc digital outputs
	 *   [2] the 2nd assoc DACs
	 *      :
	 */
	this->dacs.ngroups = 0;
	for (assoc = 0; assoc < CORB_CD_ASSOCIATION_MAX; assoc++) {
		azalia_generic_codec_add_dacgroup(this, assoc, 0);
		azalia_generic_codec_add_dacgroup(this, assoc, COP_AWCAP_DIGITAL);
	}

	/* find DACs which do not connect with any pins by default */
	FOR_EACH_WIDGET(this, i) {
		boolean_t found;

		if (this->w[i].type != COP_AWTYPE_AUDIO_OUTPUT)
			continue;
		found = FALSE;
		for (group = 0; group < this->dacs.ngroups; group++) {
			for (j = 0; j < this->dacs.groups[group].nconv; j++) {
				if (i == this->dacs.groups[group].conv[j]) {
					found = TRUE;
					group = this->dacs.ngroups;
					break;
				}
			}
		}
		if (found)
			continue;
		if (this->dacs.ngroups >= 32)
			break;
		this->dacs.groups[this->dacs.ngroups].nconv = 1;
		this->dacs.groups[this->dacs.ngroups].conv[0] = i;
		this->dacs.ngroups++;
	}
	this->dacs.cur = 0;

	/* enumerate ADCs */
	this->adcs.ngroups = 0;
	FOR_EACH_WIDGET(this, i) {
		if (this->w[i].type != COP_AWTYPE_AUDIO_INPUT)
			continue;
		this->adcs.groups[this->adcs.ngroups].nconv = 1;
		this->adcs.groups[this->adcs.ngroups].conv[0] = i;
		this->adcs.ngroups++;
		if (this->adcs.ngroups >= 32)
			break;
	}
	this->adcs.cur = 0;
	return 0;
}

int
azalia_generic_codec_add_dacgroup(codec_t *this, int assoc, uint32_t digital)
{
	int i, j, n, dac, seq;

	n = 0;
	for (seq = 0 ; seq < CORB_CD_SEQUENCE_MAX; seq++) {
		i = azalia_generic_codec_find_pin(this, assoc, seq, digital);
		if (i < 0)
			continue;
		dac = azalia_generic_codec_find_dac(this, i, 0);
		if (dac < 0)
			continue;
		/* duplication check */
		for (j = 0; j < n; j++) {
			if (this->dacs.groups[this->dacs.ngroups].conv[j] == dac)
				break;
		}
		if (j < n)	/* this group already has <dac> */
			continue;
		this->dacs.groups[this->dacs.ngroups].conv[n++] = dac;
	}
	if (n <= 0)		/* no such DACs */
		return 0;
	this->dacs.groups[this->dacs.ngroups].nconv = n;

	/* check if the same combination is already registered */
	for (i = 0; i < this->dacs.ngroups; i++) {
		if (n != this->dacs.groups[i].nconv)
			continue;
		for (j = 0; j < n; j++) {
			if (this->dacs.groups[this->dacs.ngroups].conv[j] !=
			    this->dacs.groups[i].conv[j])
				break;
		}
		if (j >= n) /* matched */
			return 0;
	}
	/* found no equivalent group */
	this->dacs.ngroups++;
	return 0;
}

int
azalia_generic_codec_find_pin(const codec_t *this, int assoc, int seq, uint32_t digital)
{
	int i;

	FOR_EACH_WIDGET(this, i) {
		if (this->w[i].type != COP_AWTYPE_PIN_COMPLEX)
			continue;
		if ((this->w[i].d.pin.cap & COP_PINCAP_OUTPUT) == 0)
			continue;
		if ((this->w[i].widgetcap & COP_AWCAP_DIGITAL) != digital)
			continue;
		if (this->w[i].d.pin.association != assoc)
			continue;
		if (this->w[i].d.pin.sequence == seq) {
			return i;
		}
	}
	return -1;
}

int
azalia_generic_codec_find_dac(const codec_t *this, int index, int depth)
{
	const widget_t *w;
	int i, j, ret;

	w = &this->w[index];
	if (w->type == COP_AWTYPE_AUDIO_OUTPUT)
		return index;
	if (++depth > 50) {
		return -1;
	}
	if (w->selected >= 0) {
		j = w->connections[w->selected];
		if (VALID_WIDGET_NID(j, this)) {
			ret = azalia_generic_codec_find_dac(this, j, depth);
			if (ret >= 0)
				return ret;
		}
	}
	for (i = 0; i < w->nconnections; i++) {
		j = w->connections[i];
		if (!VALID_WIDGET_NID(j, this))
			continue;
		ret = azalia_generic_codec_find_dac(this, j, depth);
		if (ret >= 0)
			return ret;
	}
	return -1;
}

/* ----------------------------------------------------------------
 * Generic mixer functions
 * ---------------------------------------------------------------- */

int
azalia_generic_mixer_init(codec_t *this)
{
	/*
	 * pin		"<color>%2.2x"
	 * audio output	"dac%2.2x"
	 * audio input	"adc%2.2x"
	 * mixer	"mixer%2.2x"
	 * selector	"sel%2.2x"
	 */
	mixer_item_t *m;
	int err, i, j, k;

	this->maxmixers = 10;
	this->nmixers = 0;
	this->mixers = malloc(sizeof(mixer_item_t) * this->maxmixers,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (this->mixers == NULL) {
		printf("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}

	/* register classes */
	m = &this->mixers[AZ_CLASS_INPUT];
	m->devinfo.index = AZ_CLASS_INPUT;
	strlcpy(m->devinfo.label.name, AudioCinputs,
	    sizeof(m->devinfo.label.name));
	m->devinfo.type = AUDIO_MIXER_CLASS;
	m->devinfo.mixer_class = AZ_CLASS_INPUT;
	m->devinfo.next = AUDIO_MIXER_LAST;
	m->devinfo.prev = AUDIO_MIXER_LAST;
	m->nid = 0;

	m = &this->mixers[AZ_CLASS_OUTPUT];
	m->devinfo.index = AZ_CLASS_OUTPUT;
	strlcpy(m->devinfo.label.name, AudioCoutputs,
	    sizeof(m->devinfo.label.name));
	m->devinfo.type = AUDIO_MIXER_CLASS;
	m->devinfo.mixer_class = AZ_CLASS_OUTPUT;
	m->devinfo.next = AUDIO_MIXER_LAST;
	m->devinfo.prev = AUDIO_MIXER_LAST;
	m->nid = 0;

	m = &this->mixers[AZ_CLASS_RECORD];
	m->devinfo.index = AZ_CLASS_RECORD;
	strlcpy(m->devinfo.label.name, AudioCrecord,
	    sizeof(m->devinfo.label.name));
	m->devinfo.type = AUDIO_MIXER_CLASS;
	m->devinfo.mixer_class = AZ_CLASS_RECORD;
	m->devinfo.next = AUDIO_MIXER_LAST;
	m->devinfo.prev = AUDIO_MIXER_LAST;
	m->nid = 0;

	this->nmixers = AZ_CLASS_RECORD + 1;

#define MIXER_REG_PROLOG	\
	mixer_devinfo_t *d; \
	err = azalia_generic_mixer_ensure_capacity(this, this->nmixers + 1); \
	if (err) \
		return err; \
	m = &this->mixers[this->nmixers]; \
	d = &m->devinfo; \
	m->nid = i

	FOR_EACH_WIDGET(this, i) {
		const widget_t *w;

		w = &this->w[i];

		/* skip unconnected pins */
		if (w->type == COP_AWTYPE_PIN_COMPLEX) {
			uint8_t conn =
			    (w->d.pin.config & CORB_CD_PORT_MASK) >> 30;
			if (conn == 1)	/* no physical connection */
				continue;
		}

		/* selector */
		if (w->type != COP_AWTYPE_AUDIO_MIXER &&
		    w->type != COP_AWTYPE_POWER && w->nconnections >= 2) {
			MIXER_REG_PROLOG;
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.source", w->name);
			d->type = AUDIO_MIXER_ENUM;
			if (w->type == COP_AWTYPE_AUDIO_MIXER)
				d->mixer_class = AZ_CLASS_RECORD;
			else if (w->type == COP_AWTYPE_AUDIO_SELECTOR)
				d->mixer_class = AZ_CLASS_INPUT;
			else
				d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_CONNLIST;
			for (j = 0, k = 0; j < w->nconnections && k < 32; j++) {
				uint8_t conn;

				if (!VALID_WIDGET_NID(w->connections[j], this))
					continue;
				/* skip unconnected pins */
				PIN_STATUS(&this->w[w->connections[j]],
				    conn);
				if (conn == 1)
					continue;
				d->un.e.member[k].ord = j;
				strlcpy(d->un.e.member[k].label.name,
				    this->w[w->connections[j]].name,
				    MAX_AUDIO_DEV_LEN);
				k++;
			}
			d->un.e.num_mem = k;
			this->nmixers++;
		}

		/* output mute */
		if (w->widgetcap & COP_AWCAP_OUTAMP &&
		    w->outamp_cap & COP_AMPCAP_MUTE) {
			MIXER_REG_PROLOG;
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.mute", w->name);
			d->type = AUDIO_MIXER_ENUM;
			if (w->type == COP_AWTYPE_AUDIO_MIXER)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else if (w->type == COP_AWTYPE_AUDIO_SELECTOR)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else if (w->type == COP_AWTYPE_PIN_COMPLEX)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else
				d->mixer_class = AZ_CLASS_INPUT;
			m->target = MI_TARGET_OUTAMP;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNoff,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNon,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		/* output gain */
		if (w->widgetcap & COP_AWCAP_OUTAMP
		    && COP_AMPCAP_NUMSTEPS(w->outamp_cap)) {
			MIXER_REG_PROLOG;
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s", w->name);
			d->type = AUDIO_MIXER_VALUE;
			if (w->type == COP_AWTYPE_AUDIO_MIXER)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else if (w->type == COP_AWTYPE_AUDIO_SELECTOR)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else if (w->type == COP_AWTYPE_PIN_COMPLEX)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else
				d->mixer_class = AZ_CLASS_INPUT;
			m->target = MI_TARGET_OUTAMP;
			d->un.v.num_channels = WIDGET_CHANNELS(w);
			d->un.v.units.name[0] = 0;
			d->un.v.delta =
			    MIXER_DELTA(COP_AMPCAP_NUMSTEPS(w->outamp_cap));
			this->nmixers++;
		}

		/* input mute */
		if (w->widgetcap & COP_AWCAP_INAMP &&
		    w->inamp_cap & COP_AMPCAP_MUTE) {
			if (w->type != COP_AWTYPE_AUDIO_SELECTOR &&
			    w->type != COP_AWTYPE_AUDIO_MIXER) {
				MIXER_REG_PROLOG;
				snprintf(d->label.name, sizeof(d->label.name),
				    "%s.mute", w->name);
				d->type = AUDIO_MIXER_ENUM;
				if (w->type == COP_AWTYPE_AUDIO_INPUT)
					d->mixer_class = AZ_CLASS_RECORD;
				else
					d->mixer_class = AZ_CLASS_INPUT;
				m->target = 0;
				d->un.e.num_mem = 2;
				d->un.e.member[0].ord = 0;
				strlcpy(d->un.e.member[0].label.name,
				    AudioNoff, MAX_AUDIO_DEV_LEN);
				d->un.e.member[1].ord = 1;
				strlcpy(d->un.e.member[1].label.name,
				    AudioNon, MAX_AUDIO_DEV_LEN);
				this->nmixers++;
			} else {
				uint8_t conn;

				for (j = 0; j < w->nconnections; j++) {
					MIXER_REG_PROLOG;
					if (!VALID_WIDGET_NID(w->connections[j], this))
						continue;
					/* skip unconnected pins */
					PIN_STATUS(&this->w[w->connections[j]],
					    conn);
					if (conn == 1)
						continue;
					snprintf(d->label.name, sizeof(d->label.name),
					    "%s.%s.mute", w->name,
					    this->w[w->connections[j]].name);
					d->type = AUDIO_MIXER_ENUM;
					if (w->type == COP_AWTYPE_AUDIO_INPUT)
						d->mixer_class = AZ_CLASS_RECORD;
					else
						d->mixer_class = AZ_CLASS_INPUT;
					m->target = j;
					d->un.e.num_mem = 2;
					d->un.e.member[0].ord = 0;
					strlcpy(d->un.e.member[0].label.name,
					    AudioNoff, MAX_AUDIO_DEV_LEN);
					d->un.e.member[1].ord = 1;
					strlcpy(d->un.e.member[1].label.name,
					    AudioNon, MAX_AUDIO_DEV_LEN);
					this->nmixers++;
				}
			}
		}

		/* input gain */
		if (w->widgetcap & COP_AWCAP_INAMP
		    && COP_AMPCAP_NUMSTEPS(w->inamp_cap)) {
			if (w->type != COP_AWTYPE_AUDIO_SELECTOR &&
			    w->type != COP_AWTYPE_AUDIO_MIXER) {
				MIXER_REG_PROLOG;
				snprintf(d->label.name, sizeof(d->label.name),
				    "%s", w->name);
				d->type = AUDIO_MIXER_VALUE;
				if (w->type == COP_AWTYPE_AUDIO_INPUT)
					d->mixer_class = AZ_CLASS_RECORD;
				else
					d->mixer_class = AZ_CLASS_INPUT;
				m->target = 0;
				d->un.v.num_channels = WIDGET_CHANNELS(w);
				d->un.v.units.name[0] = 0;
				d->un.v.delta =
				    MIXER_DELTA(COP_AMPCAP_NUMSTEPS(w->inamp_cap));
				this->nmixers++;
			} else {
				uint8_t conn;

				for (j = 0; j < w->nconnections; j++) {
					MIXER_REG_PROLOG;
					if (!VALID_WIDGET_NID(w->connections[j], this))
						continue;
					/* skip unconnected pins */
					PIN_STATUS(&this->w[w->connections[j]],
					    conn);
					if (conn == 1)
						continue;
					snprintf(d->label.name, sizeof(d->label.name),
					    "%s.%s", w->name,
					    this->w[w->connections[j]].name);
					d->type = AUDIO_MIXER_VALUE;
					if (w->type == COP_AWTYPE_AUDIO_INPUT)
						d->mixer_class = AZ_CLASS_RECORD;
					else
						d->mixer_class = AZ_CLASS_INPUT;
					m->target = j;
					d->un.v.num_channels = WIDGET_CHANNELS(w);
					d->un.v.units.name[0] = 0;
					d->un.v.delta =
					    MIXER_DELTA(COP_AMPCAP_NUMSTEPS(w->inamp_cap));
					this->nmixers++;
				}
			}
		}

		/* pin direction */
		if (w->type == COP_AWTYPE_PIN_COMPLEX &&
		    w->d.pin.cap & COP_PINCAP_OUTPUT &&
		    w->d.pin.cap & COP_PINCAP_INPUT) {
			MIXER_REG_PROLOG;
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.dir", w->name);
			d->type = AUDIO_MIXER_ENUM;
			d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_PINDIR;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNinput,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNoutput,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		/* pin headphone-boost */
		if (w->type == COP_AWTYPE_PIN_COMPLEX &&
		    w->d.pin.cap & COP_PINCAP_HEADPHONE) {
			MIXER_REG_PROLOG;
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.boost", w->name);
			d->type = AUDIO_MIXER_ENUM;
			d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_PINBOOST;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNoff,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNon,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		if (w->type == COP_AWTYPE_PIN_COMPLEX &&
		    w->d.pin.cap & COP_PINCAP_EAPD) {
			MIXER_REG_PROLOG;
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.eapd", w->name);
			d->type = AUDIO_MIXER_ENUM;
			d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_EAPD;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNoff,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNon,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		/* volume knob */
		if (w->type == COP_AWTYPE_VOLUME_KNOB &&
		    w->d.volume.cap & COP_VKCAP_DELTA) {
			MIXER_REG_PROLOG;
			strlcpy(d->label.name, w->name, sizeof(d->label.name));
			d->type = AUDIO_MIXER_VALUE;
			d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_VOLUME;
			d->un.v.num_channels = 1;
			d->un.v.units.name[0] = 0;
			d->un.v.delta =
			    MIXER_DELTA(COP_VKCAP_NUMSTEPS(w->d.volume.cap));
			this->nmixers++;
		}
	}

	/* if the codec has multiple DAC groups, create "inputs.usingdac" */
	if (this->dacs.ngroups > 1) {
		MIXER_REG_PROLOG;
		strlcpy(d->label.name, "usingdac", sizeof(d->label.name));
		d->type = AUDIO_MIXER_ENUM;
		d->mixer_class = AZ_CLASS_INPUT;
		m->target = MI_TARGET_DAC;
		for (i = 0; i < this->dacs.ngroups && i < 32; i++) {
			d->un.e.member[i].ord = i;
			for (j = 0; j < this->dacs.groups[i].nconv; j++) {
				if (j * 2 >= MAX_AUDIO_DEV_LEN)
					break;
				snprintf(d->un.e.member[i].label.name + j*2,
				    MAX_AUDIO_DEV_LEN - j*2, "%2.2x",
				    this->dacs.groups[i].conv[j]);
			}
		}
		d->un.e.num_mem = i;
		this->nmixers++;
	}

	/* if the codec has multiple ADC groups, create "record.usingadc" */
	if (this->adcs.ngroups > 1) {
		MIXER_REG_PROLOG;
		strlcpy(d->label.name, "usingadc", sizeof(d->label.name));
		d->type = AUDIO_MIXER_ENUM;
		d->mixer_class = AZ_CLASS_RECORD;
		m->target = MI_TARGET_ADC;
		for (i = 0; i < this->adcs.ngroups && i < 32; i++) {
			d->un.e.member[i].ord = i;
			for (j = 0; j < this->adcs.groups[i].nconv; j++) {
				if (j * 2 >= MAX_AUDIO_DEV_LEN)
					break;
				snprintf(d->un.e.member[i].label.name + j*2,
				    MAX_AUDIO_DEV_LEN - j*2, "%2.2x",
				    this->adcs.groups[i].conv[j]);
			}
		}
		d->un.e.num_mem = i;
		this->nmixers++;
	}

	azalia_generic_mixer_fix_indexes(this);
	azalia_generic_mixer_default(this);
	return 0;
}

int
azalia_generic_mixer_ensure_capacity(codec_t *this, size_t newsize)
{
	size_t newmax;
	void *newbuf;

	if (this->maxmixers >= newsize)
		return 0;
	newmax = this->maxmixers + 10;
	if (newmax < newsize)
		newmax = newsize;
	newbuf = malloc(sizeof(mixer_item_t) * newmax, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (newbuf == NULL) {
		printf("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	bcopy(this->mixers, newbuf, this->maxmixers * sizeof(mixer_item_t));
	free(this->mixers, M_DEVBUF);
	this->mixers = newbuf;
	this->maxmixers = newmax;
	return 0;
}

int
azalia_generic_mixer_fix_indexes(codec_t *this)
{
	int i;
	mixer_devinfo_t *d;

	for (i = 0; i < this->nmixers; i++) {
		d = &this->mixers[i].devinfo;
#ifdef DIAGNOSTIC
		if (d->index != 0 && d->index != i)
			printf("%s: index mismatch %d %d\n", __func__,
			    d->index, i);
#endif
		d->index = i;
		if (d->prev == 0)
			d->prev = AUDIO_MIXER_LAST;
		if (d->next == 0)
			d->next = AUDIO_MIXER_LAST;
	}
	return 0;
}

int
azalia_generic_mixer_default(codec_t *this)
{
	int i;
	mixer_item_t *m;
	/* unmute all */
	for (i = 0; i < this->nmixers; i++) {
		mixer_ctrl_t mc;

		m = &this->mixers[i];
		if (!IS_MI_TARGET_INAMP(m->target) &&
		    m->target != MI_TARGET_OUTAMP)
			continue;
		if (m->devinfo.type != AUDIO_MIXER_ENUM)
			continue;
		mc.dev = i;
		mc.type = AUDIO_MIXER_ENUM;
		mc.un.ord = 0;
		azalia_generic_mixer_set(this, m->nid, m->target, &mc);
	}

	/* set unextreme volume */
	for (i = 0; i < this->nmixers; i++) {
		mixer_ctrl_t mc;

		m = &this->mixers[i];
		if (!IS_MI_TARGET_INAMP(m->target) &&
		    m->target != MI_TARGET_OUTAMP &&
		    m->target != MI_TARGET_VOLUME)
			continue;
		if (m->devinfo.type != AUDIO_MIXER_VALUE)
			continue;
		mc.dev = i;
		mc.type = AUDIO_MIXER_VALUE;
		mc.un.value.num_channels = 1;
		mc.un.value.level[0] = AUDIO_MAX_GAIN / 2;
		if (m->target != MI_TARGET_VOLUME &&
		    WIDGET_CHANNELS(&this->w[m->nid]) == 2) {
			mc.un.value.num_channels = 2;
			mc.un.value.level[1] = mc.un.value.level[0];
		}
		azalia_generic_mixer_set(this, m->nid, m->target, &mc);
	}

	return 0;
}

int
azalia_generic_mixer_pin_sense(codec_t *this)
{
	typedef enum {
		PIN_DIR_IN,
		PIN_DIR_OUT,
		PIN_DIR_MIC
	} pintype_t;
	const widget_t *w;
	int i;

	FOR_EACH_WIDGET(this, i) {
		pintype_t pintype = PIN_DIR_IN;

		w = &this->w[i];
		if (w->type != COP_AWTYPE_PIN_COMPLEX)
			continue;
		if (!(w->d.pin.cap & COP_PINCAP_INPUT))
			pintype = PIN_DIR_OUT;
		if (!(w->d.pin.cap & COP_PINCAP_OUTPUT))
			pintype = PIN_DIR_IN;

		switch (w->d.pin.device) {
		case CORB_CD_LINEOUT:
		case CORB_CD_SPEAKER:
		case CORB_CD_HEADPHONE:
		case CORB_CD_SPDIFOUT:
		case CORB_CD_DIGITALOUT:
			pintype = PIN_DIR_OUT;
			break;
		case CORB_CD_CD:
		case CORB_CD_LINEIN:
			pintype = PIN_DIR_IN;
			break;
		case CORB_CD_MICIN:
			pintype = PIN_DIR_MIC;
			break;
		}

		switch (pintype) {
		case PIN_DIR_IN:
			this->comresp(this, w->nid,
			    CORB_SET_PIN_WIDGET_CONTROL,
			    CORB_PWC_INPUT, NULL);
			break;
		case PIN_DIR_OUT:
			this->comresp(this, w->nid,
			    CORB_SET_PIN_WIDGET_CONTROL,
			    CORB_PWC_OUTPUT, NULL);
			break;
		case PIN_DIR_MIC:
			this->comresp(this, w->nid,
			    CORB_SET_PIN_WIDGET_CONTROL,
			    CORB_PWC_INPUT|CORB_PWC_VREF_80, NULL);
			break;
		}

		if (w->d.pin.cap & COP_PINCAP_EAPD) {
			uint32_t result;
			int err;

			err = this->comresp(this, w->nid,
			    CORB_GET_EAPD_BTL_ENABLE, 0, &result);
			if (err)
				continue;
			result &= 0xff;
			result |= CORB_EAPD_EAPD;
			err = this->comresp(this, w->nid,
			    CORB_SET_EAPD_BTL_ENABLE, result, &result);
			if (err)
				continue;
		}
	}

	/* GPIO quirks */

	if (this->vid == SIGMATEL_STAC9221 && this->subid == STAC9221_APPLE_ID) {
		this->comresp(this, this->audiofunc, 0x7e7, 0, NULL);
		azalia_gpio_unmute(this, 0);
		azalia_gpio_unmute(this, 1);
	}
	if (this->vid == REALTEK_ALC883 && this->subid == ALC883_ACER_ID) {
		azalia_gpio_unmute(this, 0);
		azalia_gpio_unmute(this, 1);
	}
	if ((this->vid == REALTEK_ALC660 && this->subid == ALC660_ASUS_G2K) ||
	    (this->vid == REALTEK_ALC880 && this->subid == ALC880_ASUS_M5200) ||
	    (this->vid == REALTEK_ALC880 && this->subid == ALC880_ASUS_A7M) ||
	    (this->vid == REALTEK_ALC882 && this->subid == ALC882_ASUS_A7T) ||
	    (this->vid == REALTEK_ALC882 && this->subid == ALC882_ASUS_W2J) ||
	    (this->vid == REALTEK_ALC885 && this->subid == ALC885_APPLE_MB3) ||
	    (this->vid == REALTEK_ALC885 && this->subid == ALC885_APPLE_MB4) ||
	    (this->vid == SIGMATEL_STAC9205 && this->subid == STAC9205_DELL_D630) ||
	    (this->vid == SIGMATEL_STAC9205 && this->subid == STAC9205_DELL_V1500)) {
		azalia_gpio_unmute(this, 0);
	}

	if (this->vid == REALTEK_ALC880 && this->subid == ALC880_MEDION_MD95257) {
		azalia_gpio_unmute(this, 1);
	}

	return 0;
}

int
azalia_generic_mixer_create_virtual(codec_t *this)
{
	mixer_item_t *m;
	mixer_devinfo_t *d;
	convgroup_t *cgdac = &this->dacs.groups[0];
	convgroup_t *cgadc = &this->adcs.groups[0];
	int i, err, mdac, madc, mmaster;

	/* Clear mixer indexes, to make generic_mixer_fix_index happy */
	for (i = 0; i < this->nmixers; i++) {
		d = &this->mixers[i].devinfo;
		d->index = d->prev = d->next = 0;
	}

	mdac = madc = mmaster = -1;
	for (i = 0; i < this->nmixers; i++) {
		if (this->mixers[i].devinfo.type != AUDIO_MIXER_VALUE)
			continue;
		if (mdac < 0 && this->dacs.ngroups > 0 && cgdac->nconv > 0) {
			if (this->mixers[i].nid == cgdac->conv[0])
				mdac = mmaster = i;
		}
		if (madc < 0 && this->adcs.ngroups > 0 && cgadc->nconv > 0) {
			if (this->mixers[i].nid == cgadc->conv[0])
				madc = i;
		}
	}

	if (mdac >= 0) {
		err = azalia_generic_mixer_ensure_capacity(this, this->nmixers + 1);
		if (err)
			return err;
		m = &this->mixers[this->nmixers];
		d = &m->devinfo;
		memcpy(m, &this->mixers[mmaster], sizeof(*m));
		d->mixer_class = AZ_CLASS_OUTPUT;
		snprintf(d->label.name, sizeof(d->label.name), AudioNmaster);
		this->nmixers++;

		err = azalia_generic_mixer_ensure_capacity(this, this->nmixers + 1);
		if (err)
			return err;
		m = &this->mixers[this->nmixers];
		d = &m->devinfo;
		memcpy(m, &this->mixers[mdac], sizeof(*m));
		d->mixer_class = AZ_CLASS_INPUT;
		snprintf(d->label.name, sizeof(d->label.name), AudioNdac);
		this->nmixers++;
	}

	if (madc >= 0) {
		err = azalia_generic_mixer_ensure_capacity(this, this->nmixers + 1);
		if (err)
			return err;
		m = &this->mixers[this->nmixers];
		d = &m->devinfo;
		memcpy(m, &this->mixers[madc], sizeof(*m));
		d->mixer_class = AZ_CLASS_RECORD;
		snprintf(d->label.name, sizeof(d->label.name), AudioNvolume);
		this->nmixers++;
	}

	azalia_generic_mixer_fix_indexes(this);

	return 0;
}

int
azalia_generic_mixer_autoinit(codec_t *this)
{
	azalia_generic_mixer_init(this);
	azalia_generic_mixer_create_virtual(this);
	azalia_generic_mixer_pin_sense(this);

	return 0;
}

int
azalia_generic_mixer_delete(codec_t *this)
{
	if (this->mixers == NULL)
		return 0;
	free(this->mixers, M_DEVBUF);
	this->mixers = NULL;
	return 0;
}

/**
 * @param mc	mc->type must be set by the caller before the call
 */
int
azalia_generic_mixer_get(const codec_t *this, nid_t nid, int target, mixer_ctrl_t *mc)
{
	uint32_t result;
	nid_t n;
	int err;

	/* inamp mute */
	if (IS_MI_TARGET_INAMP(target) && mc->type == AUDIO_MIXER_ENUM) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		    CORB_GAGM_INPUT | CORB_GAGM_LEFT |
		    MI_TARGET_INAMP(target), &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_GAGM_MUTE ? 1 : 0;
	}

	/* inamp gain */
	else if (IS_MI_TARGET_INAMP(target) && mc->type == AUDIO_MIXER_VALUE) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		      CORB_GAGM_INPUT | CORB_GAGM_LEFT |
		      MI_TARGET_INAMP(target), &result);
		if (err)
			return err;
		mc->un.value.level[0] = azalia_generic_mixer_from_device_value(this,
		    nid, target, CORB_GAGM_GAIN(result));
		if (this->w[nid].type == COP_AWTYPE_AUDIO_SELECTOR ||
		    this->w[nid].type == COP_AWTYPE_AUDIO_MIXER) {
			n = this->w[nid].connections[MI_TARGET_INAMP(target)];
#ifdef AZALIA_DEBUG
			if (!VALID_WIDGET_NID(n, this)) {
				DPRINTF(("%s: invalid target: nid=%d nconn=%d index=%d\n",
				   __func__, nid, this->w[nid].nconnections,
				   MI_TARGET_INAMP(target)));
				return EINVAL;
			}
#endif
		} else
			n = nid;
		mc->un.value.num_channels = WIDGET_CHANNELS(&this->w[n]);
		if (mc->un.value.num_channels == 2) {
			err = this->comresp(this, nid,
			    CORB_GET_AMPLIFIER_GAIN_MUTE, CORB_GAGM_INPUT |
			    CORB_GAGM_RIGHT | MI_TARGET_INAMP(target),
			    &result);
			if (err)
				return err;
			mc->un.value.level[1] = azalia_generic_mixer_from_device_value
			    (this, nid, target, CORB_GAGM_GAIN(result));
		}
	}

	/* outamp mute */
	else if (target == MI_TARGET_OUTAMP && mc->type == AUDIO_MIXER_ENUM) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		    CORB_GAGM_OUTPUT | CORB_GAGM_LEFT | 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_GAGM_MUTE ? 1 : 0;
	}

	/* outamp gain */
	else if (target == MI_TARGET_OUTAMP && mc->type == AUDIO_MIXER_VALUE) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		      CORB_GAGM_OUTPUT | CORB_GAGM_LEFT | 0, &result);
		if (err)
			return err;
		mc->un.value.level[0] = azalia_generic_mixer_from_device_value(this,
		    nid, target, CORB_GAGM_GAIN(result));
		mc->un.value.num_channels = WIDGET_CHANNELS(&this->w[nid]);
		if (mc->un.value.num_channels == 2) {
			err = this->comresp(this, nid,
			    CORB_GET_AMPLIFIER_GAIN_MUTE,
			    CORB_GAGM_OUTPUT | CORB_GAGM_RIGHT | 0, &result);
			if (err)
				return err;
			mc->un.value.level[1] = azalia_generic_mixer_from_device_value
			    (this, nid, target, CORB_GAGM_GAIN(result));
		}
	}

	/* selection */
	else if (target == MI_TARGET_CONNLIST) {
		err = this->comresp(this, nid,
		    CORB_GET_CONNECTION_SELECT_CONTROL, 0, &result);
		if (err)
			return err;
		result = CORB_CSC_INDEX(result);
		if (!VALID_WIDGET_NID(this->w[nid].connections[result], this))
			mc->un.ord = -1;
		else
			mc->un.ord = result;
	}

	/* pin I/O */
	else if (target == MI_TARGET_PINDIR) {
		err = this->comresp(this, nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_PWC_OUTPUT ? 1 : 0;
	}

	/* pin headphone-boost */
	else if (target == MI_TARGET_PINBOOST) {
		err = this->comresp(this, nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_PWC_HEADPHONE ? 1 : 0;
	}

	/* DAC group selection */
	else if (target == MI_TARGET_DAC) {
		mc->un.ord = this->dacs.cur;
	}

	/* ADC selection */
	else if (target == MI_TARGET_ADC) {
		mc->un.ord = this->adcs.cur;
	}

	/* Volume knob */
	else if (target == MI_TARGET_VOLUME) {
		err = this->comresp(this, nid, CORB_GET_VOLUME_KNOB,
		    0, &result);
		if (err)
			return err;
		mc->un.value.level[0] = azalia_generic_mixer_from_device_value(this,
		    nid, target, CORB_VKNOB_VOLUME(result));
		mc->un.value.num_channels = 1;
	}

	/* S/PDIF */
	else if (target == MI_TARGET_SPDIF) {
		err = this->comresp(this, nid, CORB_GET_DIGITAL_CONTROL,
		    0, &result);
		if (err)
			return err;
		mc->un.mask = result & 0xff & ~(CORB_DCC_DIGEN | CORB_DCC_NAUDIO);
	} else if (target == MI_TARGET_SPDIF_CC) {
		err = this->comresp(this, nid, CORB_GET_DIGITAL_CONTROL,
		    0, &result);
		if (err)
			return err;
		mc->un.value.num_channels = 1;
		mc->un.value.level[0] = CORB_DCC_CC(result);
	}

	/* EAPD */
	else if (target == MI_TARGET_EAPD) {
		err = this->comresp(this, nid,
		    CORB_GET_EAPD_BTL_ENABLE, 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_EAPD_EAPD ? 1 : 0;
	}

	else {
		printf("%s: internal error in %s: target=%x\n",
		    XNAME(this), __func__, target);
		return -1;
	}
	return 0;
}

int
azalia_generic_mixer_set(codec_t *this, nid_t nid, int target, const mixer_ctrl_t *mc)
{
	uint32_t result, value;
	int err;

	/* inamp mute */
	if (IS_MI_TARGET_INAMP(target) && mc->type == AUDIO_MIXER_ENUM) {
		/* We have to set stereo mute separately to keep each gain value. */
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		    CORB_GAGM_INPUT | CORB_GAGM_LEFT |
		    MI_TARGET_INAMP(target), &result);
		if (err)
			return err;
		value = CORB_AGM_INPUT | CORB_AGM_LEFT |
		    (target << CORB_AGM_INDEX_SHIFT) |
		    CORB_GAGM_GAIN(result);
		if (mc->un.ord)
			value |= CORB_AGM_MUTE;
		err = this->comresp(this, nid, CORB_SET_AMPLIFIER_GAIN_MUTE,
		    value, &result);
		if (err)
			return err;
		if (WIDGET_CHANNELS(&this->w[nid]) == 2) {
			err = this->comresp(this, nid,
			    CORB_GET_AMPLIFIER_GAIN_MUTE, CORB_GAGM_INPUT |
			    CORB_GAGM_RIGHT | MI_TARGET_INAMP(target),
			    &result);
			if (err)
				return err;
			value = CORB_AGM_INPUT | CORB_AGM_RIGHT |
			    (target << CORB_AGM_INDEX_SHIFT) |
			    CORB_GAGM_GAIN(result);
			if (mc->un.ord)
				value |= CORB_AGM_MUTE;
			err = this->comresp(this, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, value, &result);
			if (err)
				return err;
		}
	}

	/* inamp gain */
	else if (IS_MI_TARGET_INAMP(target) && mc->type == AUDIO_MIXER_VALUE) {
		if (mc->un.value.num_channels < 1)
			return EINVAL;
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		      CORB_GAGM_INPUT | CORB_GAGM_LEFT |
		      MI_TARGET_INAMP(target), &result);
		if (err)
			return err;
		value = azalia_generic_mixer_to_device_value(this, nid, target,
		    mc->un.value.level[0]);
		value = CORB_AGM_INPUT | CORB_AGM_LEFT |
		    (target << CORB_AGM_INDEX_SHIFT) |
		    (result & CORB_GAGM_MUTE ? CORB_AGM_MUTE : 0) |
		    (value & CORB_AGM_GAIN_MASK);
		err = this->comresp(this, nid, CORB_SET_AMPLIFIER_GAIN_MUTE,
		    value, &result);
		if (err)
			return err;
		if (mc->un.value.num_channels >= 2 &&
		    WIDGET_CHANNELS(&this->w[nid]) == 2) {
			err = this->comresp(this, nid,
			      CORB_GET_AMPLIFIER_GAIN_MUTE, CORB_GAGM_INPUT |
			      CORB_GAGM_RIGHT | MI_TARGET_INAMP(target),
			      &result);
			if (err)
				return err;
			value = azalia_generic_mixer_to_device_value(this, nid, target,
			    mc->un.value.level[1]);
			value = CORB_AGM_INPUT | CORB_AGM_RIGHT |
			    (target << CORB_AGM_INDEX_SHIFT) |
			    (result & CORB_GAGM_MUTE ? CORB_AGM_MUTE : 0) |
			    (value & CORB_AGM_GAIN_MASK);
			err = this->comresp(this, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, value, &result);
			if (err)
				return err;
		}
	}

	/* outamp mute */
	else if (target == MI_TARGET_OUTAMP && mc->type == AUDIO_MIXER_ENUM) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		    CORB_GAGM_OUTPUT | CORB_GAGM_LEFT, &result);
		if (err)
			return err;
		value = CORB_AGM_OUTPUT | CORB_AGM_LEFT | CORB_GAGM_GAIN(result);
		if (mc->un.ord)
			value |= CORB_AGM_MUTE;
		err = this->comresp(this, nid, CORB_SET_AMPLIFIER_GAIN_MUTE,
		    value, &result);
		if (err)
			return err;
		if (WIDGET_CHANNELS(&this->w[nid]) == 2) {
			err = this->comresp(this, nid,
			    CORB_GET_AMPLIFIER_GAIN_MUTE,
			    CORB_GAGM_OUTPUT | CORB_GAGM_RIGHT, &result);
			if (err)
				return err;
			value = CORB_AGM_OUTPUT | CORB_AGM_RIGHT |
			    CORB_GAGM_GAIN(result);
			if (mc->un.ord)
				value |= CORB_AGM_MUTE;
			err = this->comresp(this, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, value, &result);
			if (err)
				return err;
		}
	}

	/* outamp gain */
	else if (target == MI_TARGET_OUTAMP && mc->type == AUDIO_MIXER_VALUE) {
		if (mc->un.value.num_channels < 1)
			return EINVAL;
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		      CORB_GAGM_OUTPUT | CORB_GAGM_LEFT, &result);
		if (err)
			return err;
		value = azalia_generic_mixer_to_device_value(this, nid, target,
		    mc->un.value.level[0]);
		value = CORB_AGM_OUTPUT | CORB_AGM_LEFT |
		    (result & CORB_GAGM_MUTE ? CORB_AGM_MUTE : 0) |
		    (value & CORB_AGM_GAIN_MASK);
		err = this->comresp(this, nid, CORB_SET_AMPLIFIER_GAIN_MUTE,
		    value, &result);
		if (err)
			return err;
		if (mc->un.value.num_channels >= 2 &&
		    WIDGET_CHANNELS(&this->w[nid]) == 2) {
			err = this->comresp(this, nid,
			      CORB_GET_AMPLIFIER_GAIN_MUTE, CORB_GAGM_OUTPUT |
			      CORB_GAGM_RIGHT, &result);
			if (err)
				return err;
			value = azalia_generic_mixer_to_device_value(this, nid, target,
			    mc->un.value.level[1]);
			value = CORB_AGM_OUTPUT | CORB_AGM_RIGHT |
			    (result & CORB_GAGM_MUTE ? CORB_AGM_MUTE : 0) |
			    (value & CORB_AGM_GAIN_MASK);
			err = this->comresp(this, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, value, &result);
			if (err)
				return err;
		}
	}

	/* selection */
	else if (target == MI_TARGET_CONNLIST) {
		if (mc->un.ord < 0 ||
		    mc->un.ord >= this->w[nid].nconnections ||
		    !VALID_WIDGET_NID(this->w[nid].connections[mc->un.ord], this))
			return EINVAL;
		err = this->comresp(this, nid,
		    CORB_SET_CONNECTION_SELECT_CONTROL, mc->un.ord, &result);
		if (err)
			return err;
	}

	/* pin I/O */
	else if (target == MI_TARGET_PINDIR) {
		if (mc->un.ord >= 2)
			return EINVAL;
		err = this->comresp(this, nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err)
			return err;
		if (mc->un.ord == 0) {
			result &= ~CORB_PWC_OUTPUT;
			result |= CORB_PWC_INPUT;
		} else {
			result &= ~CORB_PWC_INPUT;
			result |= CORB_PWC_OUTPUT;
		}
		err = this->comresp(this, nid,
		    CORB_SET_PIN_WIDGET_CONTROL, result, &result);
		if (err)
			return err;
	}

	/* pin headphone-boost */
	else if (target == MI_TARGET_PINBOOST) {
		if (mc->un.ord >= 2)
			return EINVAL;
		err = this->comresp(this, nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err)
			return err;
		if (mc->un.ord == 0) {
			result &= ~CORB_PWC_HEADPHONE;
		} else {
			result |= CORB_PWC_HEADPHONE;
		}
		err = this->comresp(this, nid,
		    CORB_SET_PIN_WIDGET_CONTROL, result, &result);
		if (err)
			return err;
	}

	/* DAC group selection */
	else if (target == MI_TARGET_DAC) {
		if (this->running)
			return EBUSY;
		if (mc->un.ord >= this->dacs.ngroups)
			return EINVAL;
		return azalia_codec_construct_format(this,
		    mc->un.ord, this->adcs.cur);
	}

	/* ADC selection */
	else if (target == MI_TARGET_ADC) {
		if (this->running)
			return EBUSY;
		if (mc->un.ord >= this->adcs.ngroups)
			return EINVAL;
		return azalia_codec_construct_format(this,
		    this->dacs.cur, mc->un.ord);
	}

	/* Volume knob */
	else if (target == MI_TARGET_VOLUME) {
		if (mc->un.value.num_channels != 1)
			return EINVAL;
		value = azalia_generic_mixer_to_device_value(this, nid, target,
		     mc->un.value.level[0]) | CORB_VKNOB_DIRECT;
		err = this->comresp(this, nid, CORB_SET_VOLUME_KNOB,
		   value, &result);
		if (err)
			return err;
	}

	/* S/PDIF */
	else if (target == MI_TARGET_SPDIF) {
		err = this->comresp(this, nid, CORB_GET_DIGITAL_CONTROL,
		    0, &result);
		result &= CORB_DCC_DIGEN | CORB_DCC_NAUDIO;
		result |= mc->un.mask & 0xff & ~CORB_DCC_DIGEN;
		err = this->comresp(this, nid, CORB_SET_DIGITAL_CONTROL_L,
		    result, NULL);
		if (err)
			return err;
	} else if (target == MI_TARGET_SPDIF_CC) {
		if (mc->un.value.num_channels != 1)
			return EINVAL;
		if (mc->un.value.level[0] > 127)
			return EINVAL;
		err = this->comresp(this, nid, CORB_SET_DIGITAL_CONTROL_H,
		    mc->un.value.level[0], NULL);
		if (err)
			return err;
	}

	/* EAPD */
	else if (target == MI_TARGET_EAPD) {
		if (mc->un.ord >= 2)
			return EINVAL;
		err = this->comresp(this, nid,
		    CORB_GET_EAPD_BTL_ENABLE, 0, &result);
		if (err)
			return err;
		result &= 0xff;
		if (mc->un.ord == 0) {
			result &= ~CORB_EAPD_EAPD;
		} else {
			result |= CORB_EAPD_EAPD;
		}
		err = this->comresp(this, nid,
		    CORB_SET_EAPD_BTL_ENABLE, result, &result);
		if (err)
			return err;
	}

	else {
		printf("%s: internal error in %s: target=%x\n",
		    XNAME(this), __func__, target);
		return -1;
	}
	return 0;
}

u_char
azalia_generic_mixer_from_device_value(const codec_t *this, nid_t nid, int target,
    uint32_t dv)
{
	uint32_t dmax;

	if (IS_MI_TARGET_INAMP(target))
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].inamp_cap);
	else if (target == MI_TARGET_OUTAMP)
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].outamp_cap);
	else if (target == MI_TARGET_VOLUME)
		dmax = COP_VKCAP_NUMSTEPS(this->w[nid].d.volume.cap);
	else {
		printf("unknown target: %d\n", target);
		dmax = 255;
	}
	if (dv <= 0 || dmax == 0)
		return AUDIO_MIN_GAIN;
	if (dv >= dmax)
		return AUDIO_MAX_GAIN - AUDIO_MAX_GAIN % dmax;
	return dv * (AUDIO_MAX_GAIN - AUDIO_MAX_GAIN % dmax) / dmax;
}

uint32_t
azalia_generic_mixer_to_device_value(const codec_t *this, nid_t nid, int target,
    u_char uv)
{
	uint32_t dmax;

	if (IS_MI_TARGET_INAMP(target))
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].inamp_cap);
	else if (target == MI_TARGET_OUTAMP)
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].outamp_cap);
	else if (target == MI_TARGET_VOLUME)
		dmax = COP_VKCAP_NUMSTEPS(this->w[nid].d.volume.cap);
	else {
		printf("unknown target: %d\n", target);
		dmax = 255;
	}
	if (uv <= AUDIO_MIN_GAIN || dmax == 0)
		return 0;
	if (uv >= AUDIO_MAX_GAIN - AUDIO_MAX_GAIN % dmax)
		return dmax;
	return uv * dmax / (AUDIO_MAX_GAIN - AUDIO_MAX_GAIN % dmax);
}

int
azalia_generic_set_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	if (mc->type != m->devinfo.type)
		return EINVAL;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;	/* nothing to do */
	return azalia_generic_mixer_set(this, m->nid, m->target, mc);
}

int
azalia_generic_get_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	mc->type = m->devinfo.type;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;	/* nothing to do */
	return azalia_generic_mixer_get(this, m->nid, m->target, mc);
}

int
azalia_gpio_unmute(codec_t *this, int pin)
{
	uint32_t data, mask, dir;

	this->comresp(this, this->audiofunc, CORB_GET_GPIO_DATA, 0, &data);
	this->comresp(this, this->audiofunc, CORB_GET_GPIO_ENABLE_MASK, 0, &mask);
	this->comresp(this, this->audiofunc, CORB_GET_GPIO_DIRECTION, 0, &dir);

	data |= 1 << pin;
	mask |= 1 << pin;
	dir |= 1 << pin;

	this->comresp(this, this->audiofunc, CORB_SET_GPIO_ENABLE_MASK, mask, NULL);
	this->comresp(this, this->audiofunc, CORB_SET_GPIO_DIRECTION, dir, NULL);
	DELAY(1000);
	this->comresp(this, this->audiofunc, CORB_SET_GPIO_DATA, data, NULL);

	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC260
 *
 * Fujitsu LOOX T70M/T
 *	Internal Speaker: 0x10
 *	Front Headphone: 0x14
 *	Front mic: 0x12
 * ---------------------------------------------------------------- */

static const mixer_item_t alc260_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},

	{{0, {AudioNmaster}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, 3}}, 0x08, MI_TARGET_OUTAMP}, /* and 0x09, 0x0a(mono) */
	{{0, {AudioNmaster".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_PINBOOST},
	{{0, {AudioNmono".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x11, MI_TARGET_OUTAMP},
	{{0, {"mic1.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x12, MI_TARGET_OUTAMP},
	{{0, {"mic1"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x12, MI_TARGET_PINDIR},
	{{0, {"mic2.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x13, MI_TARGET_OUTAMP},
	{{0, {"mic2"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x13, MI_TARGET_PINDIR},
	{{0, {"line1.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {"line1"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x14, MI_TARGET_PINDIR},
	{{0, {"line2.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_OUTAMP},
	{{0, {"line2"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x15, MI_TARGET_PINDIR},

	{{0, {AudioNdac".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)}, /* and 0x09, 0x0a(mono) */
	{{0, {"mic1.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {"mic1"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {"mic2.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(1)},
	{{0, {"mic2"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(1)},
	{{0, {"line1.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(2)},
	{{0, {"line1"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, 3}}, 0x07, MI_TARGET_INAMP(2)},
	{{0, {"line2.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(3)},
	{{0, {"line2"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(3)},
	{{0, {AudioNcd".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(4)},
	{{0, {AudioNcd}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(4)},
	{{0, {AudioNspeaker".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(5)},
	{{0, {AudioNspeaker}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(5)},

	{{0, {"adc04.source"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={5, {{{"mic1"}, 0}, {{"mic2"}, 1}, {{"line1"}, 2},
		     {{"line2"}, 3}, {{AudioNcd}, 4}}}},
	 0x04, MI_TARGET_CONNLIST},
	{{0, {"adc04.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  ENUM_OFFON}, 0x04, MI_TARGET_INAMP(0)},
	{{0, {"adc04"}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	  .un.v={{""}, 2, MIXER_DELTA(35)}}, 0x04, MI_TARGET_INAMP(0)},
	{{0, {"adc05.source"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={6, {{{"mic1"}, 0}, {{"mic2"}, 1}, {{"line1"}, 2},
		     {{"line2"}, 3}, {{AudioNcd}, 4}, {{AudioNmixerout}, 5}}}},
	 0x05, MI_TARGET_CONNLIST},
	{{0, {"adc05.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  ENUM_OFFON}, 0x05, MI_TARGET_INAMP(0)},
	{{0, {"adc05"}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	  .un.v={{""}, 2, MIXER_DELTA(35)}}, 0x05, MI_TARGET_INAMP(0)},

	{{0, {"usingdac"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT, 0, 0,
	  .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}}, 0, MI_TARGET_DAC},
	{{0, {"usingadc"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={3, {{{"adc04"}, 0}, {{"adc05"}, 1}, {{"digital"}, 2}}}}, 0, MI_TARGET_ADC},
};

static const mixer_item_t alc260_loox_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},

	{{0, {AudioNmaster}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, 3}}, 0x08, MI_TARGET_OUTAMP}, /* and 0x09, 0x0a(mono) */
	{{0, {AudioNmaster".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_PINBOOST},
	{{0, {AudioNheadphone".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_PINBOOST},

	{{0, {AudioNdac".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)}, /* and 0x09, 0x0a(mono) */
	{{0, {AudioNmicrophone".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {AudioNmicrophone}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {AudioNcd".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(4)},
	{{0, {AudioNcd}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(4)},
	{{0, {AudioNspeaker".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(5)},
	{{0, {AudioNspeaker}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(5)},

	{{0, {"adc04.source"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={2, {{{AudioNmicrophone}, 0}, {{AudioNcd}, 4}}}}, 0x04, MI_TARGET_CONNLIST},
	{{0, {"adc04.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  ENUM_OFFON}, 0x04, MI_TARGET_INAMP(0)},
	{{0, {"adc04"}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	  .un.v={{""}, 2, MIXER_DELTA(35)}}, 0x04, MI_TARGET_INAMP(0)},
	{{0, {"adc05.source"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={3, {{{AudioNmicrophone}, 0}, {{AudioNcd}, 4}, {{AudioNmixerout}, 5}}}},
	 0x05, MI_TARGET_CONNLIST},
	{{0, {"adc05.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  ENUM_OFFON}, 0x05, MI_TARGET_INAMP(0)},
	{{0, {"adc05"}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	  .un.v={{""}, 2, MIXER_DELTA(35)}}, 0x05, MI_TARGET_INAMP(0)},

	{{0, {"usingdac"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT, 0, 0,
	  .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}}, 0, MI_TARGET_DAC},
	{{0, {"usingadc"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={3, {{{"adc04"}, 0}, {{"adc05"}, 1}, {{"digital"}, 2}}}}, 0, MI_TARGET_ADC},
};

int
azalia_alc260_mixer_init(codec_t *this)
{
	const mixer_item_t *mi;
	mixer_ctrl_t mc;

	switch (this->subid) {
	case ALC260_FUJITSU_ID:
		this->nmixers = sizeof(alc260_loox_mixer_items) / sizeof(mixer_item_t);
		mi = alc260_loox_mixer_items;
		break;
	default:
		this->nmixers = sizeof(alc260_mixer_items) / sizeof(mixer_item_t);
		mi = alc260_mixer_items;
	}
	this->mixers = malloc(sizeof(mixer_item_t) * this->nmixers,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (this->mixers == NULL) {
		printf("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, mi, sizeof(mixer_item_t) * this->nmixers);
	azalia_generic_mixer_fix_indexes(this);
	azalia_generic_mixer_default(this);

	azalia_generic_mixer_pin_sense(this);

	mc.dev = -1;		/* no need for generic_mixer_set() */
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 0;		/* mute: off */
	azalia_generic_mixer_set(this, 0x08, MI_TARGET_INAMP(0), &mc);
	azalia_generic_mixer_set(this, 0x08, MI_TARGET_INAMP(1), &mc);
	azalia_generic_mixer_set(this, 0x09, MI_TARGET_INAMP(0), &mc);
	azalia_generic_mixer_set(this, 0x09, MI_TARGET_INAMP(1), &mc);
	azalia_generic_mixer_set(this, 0x0a, MI_TARGET_INAMP(0), &mc);
	azalia_generic_mixer_set(this, 0x0a, MI_TARGET_INAMP(1), &mc);
	if (this->subid == ALC260_FUJITSU_ID) {
		mc.un.ord = 4;	/* connlist: cd */
		azalia_generic_mixer_set(this, 0x05, MI_TARGET_CONNLIST, &mc);
	}
	return 0;
}

int
azalia_alc260_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{1, {0x02}},	/* analog 2ch */
		 {1, {0x03}}}};	/* digital */
	static const convgroupset_t adcs = {
		-1, 3,
		{{1, {0x04}},	/* analog 2ch */
		 {1, {0x05}},	/* analog 2ch */
		 {1, {0x06}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

int
azalia_alc260_set_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;
	mixer_ctrl_t mc2;
	int err;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	if (mc->type != m->devinfo.type)
		return EINVAL;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if (m->nid == 0x08 && m->target == MI_TARGET_OUTAMP) {
		DPRINTF(("%s: hook for outputs.master\n", __func__));
		err = azalia_generic_mixer_set(this, m->nid, m->target, mc);
		if (!err) {
			azalia_generic_mixer_set(this, 0x09, m->target, mc);
			mc2 = *mc;
			mc2.un.value.num_channels = 1;
			mc2.un.value.level[0] = (mc2.un.value.level[0]
			    + mc2.un.value.level[1]) / 2;
			azalia_generic_mixer_set(this, 0x0a, m->target, &mc2);
		}
		return err;
	} else if (m->nid == 0x08 && m->target == MI_TARGET_INAMP(0)) {
		DPRINTF(("%s: hook for inputs.dac.mute\n", __func__));
		err = azalia_generic_mixer_set(this, m->nid, m->target, mc);
		if (!err) {
			azalia_generic_mixer_set(this, 0x09, m->target, mc);
			azalia_generic_mixer_set(this, 0x0a, m->target, mc);
		}
		return err;
	} else if (m->nid == 0x04 &&
		   m->target == MI_TARGET_CONNLIST &&
		   m->devinfo.un.e.num_mem == 2) {
		if (1 <= mc->un.ord && mc->un.ord <= 3)
			return EINVAL;
	} else if (m->nid == 0x05 &&
		   m->target == MI_TARGET_CONNLIST &&
		   m->devinfo.un.e.num_mem == 3) {
		if (1 <= mc->un.ord && mc->un.ord <= 3)
			return EINVAL;
	}
	return azalia_generic_mixer_set(this, m->nid, m->target, mc);
}

/* ----------------------------------------------------------------
 * Realtek ALC662-GR
 * ---------------------------------------------------------------- */

int
azalia_alc662_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 1,
		{{3, {0x02, 0x03, 0x04}}}}; /* analog 6ch */
	static const convgroupset_t adcs = {
		-1, 1,
		{{2, {0x09, 0x08}}}};	/* analog 4ch */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC861
 * ---------------------------------------------------------------- */

int
azalia_alc861_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x03, 0x04, 0x05, 0x06}}, /* analog 8ch */
		 {1, {0x07}}}};	/* digital */
	static const convgroupset_t adcs = {
		-1, 1,
		{{1, {0x08}}}};	/* analog 2ch */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC880
 * ---------------------------------------------------------------- */

int
azalia_alc880_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}};	/* digital */
	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x08, 0x09}}, /* analog 4ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC882
 * ---------------------------------------------------------------- */

static const mixer_item_t alc882_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},

	/* 0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x14,0x15,0x16,0x17 */
	{{0, {"mic1."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(0)},
	{{0, {"mic1"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(0)},
	{{0, {"mic2."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(1)},
	{{0, {"mic2"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(1)},
	{{0, {AudioNline"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(2)},
	{{0, {AudioNline}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(2)},
	{{0, {AudioNcd"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(4)},
	{{0, {AudioNcd}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(4)},
	{{0, {AudioNspeaker"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(5)},
	{{0, {AudioNspeaker}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 1, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(5)},

	{{0, {AudioNmaster}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0c, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_PINBOOST},
	{{0, {AudioNheadphone"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_PINBOOST},
	{{0, {AzaliaNfront".dac.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront".mixer.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_INAMP(1)},

	{{0, {AudioNsurround}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0d, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_PINBOOST},
	{{0, {AudioNsurround".dac.mut"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround".mixer.m"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_INAMP(1)},

	{{0, {AzaliaNclfe}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0e, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_PINBOOST},
	{{0, {AzaliaNclfe".dac.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe".mixer.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_INAMP(1)},

	{{0, {AzaliaNside}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0f, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_PINBOOST},
	{{0, {AzaliaNside".dac.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNside".mixer.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_INAMP(1)},

	/* 0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x14,0x15,0x16,0x17,0xb */
#define ALC882_MIC1	0x001
#define ALC882_MIC2	0x002
#define ALC882_LINE	0x004
#define ALC882_CD	0x010
#define ALC882_BEEP	0x020
#define ALC882_MIX	0x400
#define ALC882_MASK	0x437
	{{0, {AzaliaNfront"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront"."AudioNsource}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{"mic1"}, ALC882_MIC1}, {{"mic2"}, ALC882_MIC2},
			   {{AudioNline}, ALC882_LINE}, {{AudioNcd}, ALC882_CD},
			   {{AudioNspeaker}, ALC882_BEEP},
			   {{AudioNmixerout}, ALC882_MIX}}}}, 0x24, -1},
	{{0, {AudioNsurround"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround"."AudioNsource}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{"mic1"}, ALC882_MIC1}, {{"mic2"}, ALC882_MIC2},
			   {{AudioNline}, ALC882_LINE}, {{AudioNcd}, ALC882_CD},
			   {{AudioNspeaker}, ALC882_BEEP},
			   {{AudioNmixerout}, ALC882_MIX}}}}, 0x23, -1},
	{{0, {AzaliaNclfe"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe"."AudioNsource}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{"mic1"}, ALC882_MIC1}, {{"mic2"}, ALC882_MIC2},
			   {{AudioNline}, ALC882_LINE}, {{AudioNcd}, ALC882_CD},
			   {{AudioNspeaker}, ALC882_BEEP},
			   {{AudioNmixerout}, ALC882_MIX}}}}, 0x22, -1},

	{{0, {"usingdac"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT, 0, 0,
	  .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}}, 0, MI_TARGET_DAC},
	{{0, {"usingadc"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}}, 0, MI_TARGET_ADC},
};

int
azalia_alc882_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = sizeof(alc882_mixer_items) / sizeof(mixer_item_t);
	this->mixers = malloc(sizeof(mixer_item_t) * this->nmixers,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (this->mixers == NULL) {
		printf("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, alc882_mixer_items,
	    sizeof(mixer_item_t) * this->nmixers);
	azalia_generic_mixer_fix_indexes(this);
	azalia_generic_mixer_default(this);

	azalia_generic_mixer_pin_sense(this);

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 0;		/* [0] 0x0c */
	azalia_generic_mixer_set(this, 0x14, MI_TARGET_CONNLIST, &mc);
	azalia_generic_mixer_set(this, 0x1b, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 1;		/* [1] 0x0d */
	azalia_generic_mixer_set(this, 0x15, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 2;		/* [2] 0x0e */
	azalia_generic_mixer_set(this, 0x16, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 2;		/* [3] 0x0fb */
	azalia_generic_mixer_set(this, 0x17, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 0;		/* unmute */
	azalia_generic_mixer_set(this, 0x24, MI_TARGET_INAMP(0), &mc);
	azalia_generic_mixer_set(this, 0x23, MI_TARGET_INAMP(1), &mc);
	azalia_generic_mixer_set(this, 0x22, MI_TARGET_INAMP(2), &mc);
	return 0;
}

int
azalia_alc882_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}};	/* digital */
		/* don't support for 0x25 dac */
	static const convgroupset_t adcs = {
		-1, 2,
		{{3, {0x07, 0x08, 0x09}}, /* analog 6ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

int
azalia_alc882_set_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;
	mixer_ctrl_t mc2;
	uint32_t mask, bit;
	int i, err;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	if (mc->type != m->devinfo.type)
		return EINVAL;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if ((m->nid == 0x22 || m->nid == 0x23 || m->nid == 0x24)
	    && m->target == -1) {
		DPRINTF(("%s: hook for record.*.source\n", __func__));
		mc2.dev = -1;
		mc2.type = AUDIO_MIXER_ENUM;
		bit = 1;
		mask = mc->un.mask & ALC882_MASK;
		for (i = 0; i < this->w[m->nid].nconnections && i < 32; i++) {
			mc2.un.ord = (mask & bit) ? 0 : 1;
			err = azalia_generic_mixer_set(this, m->nid,
			    MI_TARGET_INAMP(i), &mc2);
			if (err)
				return err;
			bit = bit << 1;
		}
		return 0;
	}
	return azalia_generic_mixer_set(this, m->nid, m->target, mc);
}

int
azalia_alc882_get_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;
	uint32_t mask, bit, result;
	int i, err;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	mc->type = m->devinfo.type;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if ((m->nid == 0x22 || m->nid == 0x23 || m->nid == 0x24)
	    && m->target == -1) {
		DPRINTF(("%s: hook for record.*.source\n", __func__));
		mask = 0;
		bit = 1;
		for (i = 0; i < this->w[m->nid].nconnections && i < 32; i++) {
			err = this->comresp(this, m->nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
				      CORB_GAGM_INPUT | CORB_GAGM_LEFT |
				      i, &result);
			if (err)
				return err;
			if ((result & CORB_GAGM_MUTE) == 0)
				mask |= bit;
			bit = bit << 1;
		}
		mc->un.mask = mask & ALC882_MASK;
		return 0;
	}
	return azalia_generic_mixer_get(this, m->nid, m->target, mc);
}

/* ----------------------------------------------------------------
 * Realtek ALC883
 * ALC882 without adc07 and mix24.
 * ---------------------------------------------------------------- */

int
azalia_alc883_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}}; /* digital */

	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x08, 0x09}}, /* analog 4ch */
		 {1, {0x0a}}}}; /* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

static const mixer_item_t alc883_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},

	{{0, {AudioNmaster}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  4, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0c, MI_TARGET_OUTAMP},
	{{0, {AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 3, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},

	/* 0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x14,0x15,0x16,0x17 */
	{{0, {AudioNmicrophone"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(0)},
	{{0, {AudioNmicrophone}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(0)},
	{{0, {"mic2."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(1)},
	{{0, {"mic2"}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(1)},
	{{0, {AudioNline"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(2)},
	{{0, {AudioNline}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(2)},
	{{0, {AudioNcd"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(4)},
	{{0, {AudioNcd}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(4)},
	{{0, {AudioNspeaker"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(5)},
	{{0, {AudioNspeaker}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 1, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(5)},
	{{0, {AudioNmaster".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_PINBOOST},
	{{0, {AudioNheadphone"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_PINBOOST},
	{{0, {AzaliaNfront".dac.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront".mixer.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_INAMP(1)},
	{{0, {AudioNsurround}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0d, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_PINBOOST},
	{{0, {AudioNsurround".dac.mut"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround".mixer.m"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_INAMP(1)},

	{{0, {AzaliaNclfe}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0e, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_PINBOOST},
	{{0, {AzaliaNclfe".dac.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe".mixer.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_INAMP(1)},

	{{0, {AzaliaNside}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0f, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_PINBOOST},
	{{0, {AzaliaNside".dac.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNside".mixer.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_INAMP(1)},

	{{0, {AudioNvolume"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AudioNvolume}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AudioNsource}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{AudioNmicrophone}, ALC882_MIC1}, {{"mic2"}, ALC882_MIC2},
			   {{AudioNline}, ALC882_LINE}, {{AudioNcd}, ALC882_CD},
			   {{AudioNspeaker}, ALC882_BEEP},
			   {{AudioNmixerout}, ALC882_MIX}}}}, 0x23, -1},

	{{0, {AudioNsource"2."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AudioNvolume"2"}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AudioNsource"2"}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{AudioNmicrophone}, ALC882_MIC1}, {{"mic2"}, ALC882_MIC2},
			   {{AudioNline}, ALC882_LINE}, {{AudioNcd}, ALC882_CD},
			   {{AudioNspeaker}, ALC882_BEEP},
			   {{AudioNmixerout}, ALC882_MIX}}}}, 0x22, -1},

	{{0, {"usingdac"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT, 0, 0,
	  .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}}, 0, MI_TARGET_DAC},
	{{0, {"usingadc"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}}, 0, MI_TARGET_ADC},
};

int
azalia_alc883_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = sizeof(alc883_mixer_items) / sizeof(mixer_item_t);
	this->mixers = malloc(sizeof(mixer_item_t) * this->nmixers,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (this->mixers == NULL) {
		printf("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, alc883_mixer_items,
	    sizeof(mixer_item_t) * this->nmixers);
	azalia_generic_mixer_fix_indexes(this);
	azalia_generic_mixer_default(this);

	azalia_generic_mixer_pin_sense(this);

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 0;		/* [0] 0x0c */
	azalia_generic_mixer_set(this, 0x14, MI_TARGET_CONNLIST, &mc);
	azalia_generic_mixer_set(this, 0x1b, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 1;		/* [1] 0x0d */
	azalia_generic_mixer_set(this, 0x15, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 2;		/* [2] 0x0e */
	azalia_generic_mixer_set(this, 0x16, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 2;		/* [3] 0x0fb */
	azalia_generic_mixer_set(this, 0x17, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 0;		/* unmute */
	azalia_generic_mixer_set(this, 0x23, MI_TARGET_INAMP(1), &mc);
	azalia_generic_mixer_set(this, 0x22, MI_TARGET_INAMP(2), &mc);
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC885
 * ---------------------------------------------------------------- */

int
azalia_alc885_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}};	/* digital */
		/* don't support for 0x25 dac */
	static const convgroupset_t adcs = {
		-1, 2,
		{{3, {0x07, 0x08, 0x09}},	/* analog 6ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC888
 * ---------------------------------------------------------------- */

int
azalia_alc888_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}};	/* digital */
		/* don't support for 0x25 dac */
		/* ALC888S has another SPDIF-out 0x10 */
	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x08, 0x09}},	/* analog 4ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Analog Devices AD1984
 * ---------------------------------------------------------------- */

int
azalia_ad1984_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{2, {0x04, 0x03}},	/* analog 4ch */
		 {1, {0x02}}}};		/* digital */
	static const convgroupset_t adcs = {
		-1, 3,
		{{2, {0x08, 0x09}},	/* analog 4ch */
		 {1, {0x06}},		/* digital */
		 {1, {0x05}}}}; 	/* digital */
	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

static const mixer_item_t ad1984_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},
#define AD1984_DAC_HP        0x03
#define AD1984_DAC_SPEAKER   0x04
#define AD1984_TARGET_MASTER -1
#define AD1984_TARGET_MASTER_MUTE -2
	{{0, {AudioNmaster}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  4, 0, .un.v={{""}, 2, MIXER_DELTA(39)}}, 0x03, AD1984_TARGET_MASTER},
	{{0, {AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 3, ENUM_OFFON}, 0x11, AD1984_TARGET_MASTER_MUTE},
	{{0, {AudioNvolume}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(54)}}, 0x0c, MI_TARGET_OUTAMP},
	{{0, {AudioNvolume"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_OUTAMP},
	{{0, {AudioNsource}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={1, {{{AudioNmicrophone}, 0}}}},
	 0x0c, MI_TARGET_CONNLIST},
	{{0, {AudioNmicrophone"."AudioNpreamp}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(3)}}, 0x14, MI_TARGET_INAMP(0)},
	{{0, {AudioNmicrophone}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x20, MI_TARGET_INAMP(0)},
	{{0, {AudioNmicrophone"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x20, MI_TARGET_INAMP(0)},
	{{0, {AudioNheadphone}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(39)}}, 0x03, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x11, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x11, MI_TARGET_PINBOOST},
	{{0, {AudioNspeaker}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(39)}}, 0x04, MI_TARGET_OUTAMP},
	{{0, {AudioNspeaker"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x12, MI_TARGET_OUTAMP},
	{{0, {AudioNspeaker".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x12, MI_TARGET_PINBOOST},
	{{0, {AudioNmono}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 1, MIXER_DELTA(31)}}, 0x13, MI_TARGET_OUTAMP},
	{{0, {AudioNmono"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x13, MI_TARGET_OUTAMP}
};

int
azalia_ad1984_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = sizeof(ad1984_mixer_items) / sizeof(mixer_item_t);
	this->mixers = malloc(sizeof(mixer_item_t) * this->nmixers,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (this->mixers == NULL) {
		printf("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, ad1984_mixer_items,
	    sizeof(mixer_item_t) * this->nmixers);
	azalia_generic_mixer_fix_indexes(this);
	azalia_generic_mixer_default(this);

	azalia_generic_mixer_pin_sense(this);

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 1;		/* front DAC -> headphones */
	azalia_generic_mixer_set(this, 0x22, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 0;          /* unmute */
	azalia_generic_mixer_set(this, 0x07, MI_TARGET_INAMP(0), &mc);
	azalia_generic_mixer_set(this, 0x07, MI_TARGET_INAMP(1), &mc);
	azalia_generic_mixer_set(this, 0x0a, MI_TARGET_INAMP(0), &mc);
	azalia_generic_mixer_set(this, 0x0a, MI_TARGET_INAMP(1), &mc);
	azalia_generic_mixer_set(this, 0x0b, MI_TARGET_INAMP(0), &mc);
	azalia_generic_mixer_set(this, 0x0b, MI_TARGET_INAMP(1), &mc);
	azalia_generic_mixer_set(this, 0x1e, MI_TARGET_INAMP(0), &mc);
	azalia_generic_mixer_set(this, 0x1e, MI_TARGET_INAMP(1), &mc);
	azalia_generic_mixer_set(this, 0x24, MI_TARGET_INAMP(0), &mc);
	azalia_generic_mixer_set(this, 0x24, MI_TARGET_INAMP(1), &mc);
	return 0;
}

int
azalia_ad1984_set_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;
	int err;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	if (mc->type != m->devinfo.type)
		return EINVAL;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if (m->target == AD1984_TARGET_MASTER) {
		err = azalia_generic_mixer_set(this, AD1984_DAC_HP,
		    MI_TARGET_OUTAMP, mc);
		err = azalia_generic_mixer_set(this, AD1984_DAC_SPEAKER,
		    MI_TARGET_OUTAMP, mc);
		return err;
	}
	if (m->target == AD1984_TARGET_MASTER_MUTE) {
		err = azalia_generic_mixer_set(this, 0x11, MI_TARGET_OUTAMP, mc);
		err = azalia_generic_mixer_set(this, 0x12, MI_TARGET_OUTAMP, mc);
		err = azalia_generic_mixer_set(this, 0x13, MI_TARGET_OUTAMP, mc);
		return err;
	}
	return azalia_generic_mixer_set(this, m->nid, m->target, mc);
}

int
azalia_ad1984_get_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	mc->type = m->devinfo.type;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if (m->target == AD1984_TARGET_MASTER || 
	    m->target == AD1984_TARGET_MASTER_MUTE)
		return azalia_generic_mixer_get(this, m->nid,
		    MI_TARGET_OUTAMP, mc);
	return azalia_generic_mixer_get(this, m->nid, m->target, mc);
}

/* ----------------------------------------------------------------
 * Analog Devices AD1988A/AD1988B
 * ---------------------------------------------------------------- */

int
azalia_ad1988_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 3,
		{{4, {0x04, 0x05, 0x06, 0x0a}},	/* analog 8ch */
		 {1, {0x02}},	/* digital */
		 {1, {0x03}}}};	/* another analog */
	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x08, 0x09, 0x0f}}, /* analog 6ch */
		 {1, {0x07}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * CMedia CMI9880
 * ---------------------------------------------------------------- */

static const mixer_item_t cmi9880_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},

	{{0, {AudioNmaster"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x03, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x04, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x05, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x06, MI_TARGET_OUTAMP},
	{{0, {"digital."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_OUTAMP},

	{{0, {AzaliaNfront"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(30)}}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront"."AudioNsource}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={4, {{{AudioNmicrophone}, 5}, {{AudioNcd}, 6},
			   {{"line1"}, 7}, {{"line2"}, 8}}}},
	 0x08, MI_TARGET_CONNLIST},
	{{0, {AudioNsurround"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(30)}}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround"."AudioNsource}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={4, {{{AudioNmicrophone}, 5}, {{AudioNcd}, 6},
			   {{"line1"}, 7}, {{"line2"}, 8}}}},
	 0x09, MI_TARGET_CONNLIST},

	{{0, {AudioNspeaker"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x23, MI_TARGET_OUTAMP},
	{{0, {AudioNspeaker}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 1, MIXER_DELTA(15)}}, 0x23, MI_TARGET_OUTAMP}
};

int
azalia_cmi9880_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = sizeof(cmi9880_mixer_items) / sizeof(mixer_item_t);
	this->mixers = malloc(sizeof(mixer_item_t) * this->nmixers,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (this->mixers == NULL) {
		printf("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, cmi9880_mixer_items,
	    sizeof(mixer_item_t) * this->nmixers);
	azalia_generic_mixer_fix_indexes(this);
	azalia_generic_mixer_default(this);

	azalia_generic_mixer_pin_sense(this);

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 5;		/* record.front.source=mic */
	azalia_generic_mixer_set(this, 0x08, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 7;		/* record.surround.source=line1 */
	azalia_generic_mixer_set(this, 0x09, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 0;		/* front DAC -> headphones */
	azalia_generic_mixer_set(this, 0x0f, MI_TARGET_CONNLIST, &mc);
	return 0;
}

int
azalia_cmi9880_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x03, 0x04, 0x05, 0x06}}, /* analog 8ch */
		 {1, {0x07}}}};	/* digital */
	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x08, 0x09}}, /* analog 4ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Sigmatel STAC9200
 * ---------------------------------------------------------------- */

static const mixer_item_t stac9200_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},

	{{0, {AudioNmaster}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  4, 0, .un.v={{""}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_OUTAMP},
	{{0, {AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 3, ENUM_OFFON}, 0x0b, MI_TARGET_OUTAMP},
	{{0, {AudioNvolume}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(15)}}, 0x0a, MI_TARGET_OUTAMP},
	{{0, {AudioNvolume"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x0a, MI_TARGET_OUTAMP},
	{{0, {AudioNsource}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={5, {{{AudioNline}, 0}, {{AudioNmicrophone}, 1},
			   {{AudioNline"2"}, 2}, {{AudioNline"3"}, 3},
			   {{AudioNcd}, 4}}}},
	 0x0c, MI_TARGET_CONNLIST},
	{{0, {AudioNmicrophone}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(4)}}, 0x0c, MI_TARGET_OUTAMP},
	{{0, {AudioNmicrophone"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_OUTAMP},
	{{0, {AudioNsource}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={3, {{{AudioNdac}, 0}, {{"digital-in"}, 1}, {{"selector"}, 2}}}},
	 0x07, MI_TARGET_CONNLIST},
	{{0, {"digital."AudioNsource}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{AudioNdac}, 0}, {{"selector"}, 1}}}},
	 0x09, MI_TARGET_CONNLIST}, /* AudioNdac is not accurate name */
	{{0, {AudioNheadphone".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_PINBOOST},
	{{0, {AudioNspeaker".boost"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_PINBOOST},
	{{0, {AudioNmono"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x11, MI_TARGET_OUTAMP},
	{{0, {AudioNmono}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 1, MIXER_DELTA(31)}}, 0x11, MI_TARGET_OUTAMP},
	{{0, {"beep."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {"beep"}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 1, MIXER_DELTA(3)}}, 0x14, MI_TARGET_OUTAMP},
	{{0, {"usingdac"}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}},
	 0, MI_TARGET_DAC},
	{{0, {"usingadc"}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={2, {{{"analog"}, 0}, {{"digital"}, 1}}}},
	 0, MI_TARGET_ADC},
};

int
azalia_stac9200_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = sizeof(stac9200_mixer_items) / sizeof(mixer_item_t);
	this->mixers = malloc(sizeof(mixer_item_t) * this->nmixers,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (this->mixers == NULL) {
		printf("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, stac9200_mixer_items,
	    sizeof(mixer_item_t) * this->nmixers);
	azalia_generic_mixer_fix_indexes(this);
	azalia_generic_mixer_default(this);

	azalia_generic_mixer_pin_sense(this);

	mc.dev = -1;		/* no need for generic_mixer_set() */
	mc.type = AUDIO_MIXER_VALUE;
	mc.un.value.num_channels = 2;
	mc.un.value.level[0] = AUDIO_MAX_GAIN;
	mc.un.value.level[1] = mc.un.value.level[0];
	azalia_generic_mixer_set(this, 0x0c, MI_TARGET_OUTAMP, &mc);
	return 0;
}

int
azalia_stac9221_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 1,
		{{4, {0x02, 0x03, 0x04, 0x05}}}};

	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x06, 0x07}},
		 {1, {0x09}}}};

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

static const mixer_item_t stac9221_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},
#define STAC9221_TARGET_MASTER -1
	{{0, {AudioNmaster}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  4, 0, .un.v={{""}, 2, MIXER_DELTA(127)}}, 0x02, STAC9221_TARGET_MASTER},
	{{0, {AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 3, ENUM_OFFON}, 0x02, STAC9221_TARGET_MASTER},

	{{0, {AudioNheadphone}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(15)}}, 0x02, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x02, MI_TARGET_OUTAMP},

	{{0, {AudioNspeaker}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{""}, 2, MIXER_DELTA(15)}}, 0x03, MI_TARGET_OUTAMP},
	{{0, {AudioNspeaker".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x03, MI_TARGET_OUTAMP},

        {{0, {"line"}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
          0, 0, .un.v={{""}, 2, MIXER_DELTA(15)}}, 0x04, MI_TARGET_OUTAMP},
        {{0, {"line.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
          0, 0, ENUM_OFFON}, 0x04, MI_TARGET_OUTAMP},

        {{0, {"line2"}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
          0, 0, .un.v={{""}, 2, MIXER_DELTA(15)}}, 0x05, MI_TARGET_OUTAMP},
        {{0, {"line2.mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
          0, 0, ENUM_OFFON}, 0x05, MI_TARGET_OUTAMP},
};

int
azalia_stac9221_mixer_init(codec_t *this)
{
	this->nmixers = sizeof(stac9221_mixer_items) / sizeof(mixer_item_t);
	this->mixers = malloc(sizeof(mixer_item_t) * this->nmixers,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (this->mixers == NULL) {
		printf("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, stac9221_mixer_items,
	    sizeof(mixer_item_t) * this->nmixers);
	azalia_generic_mixer_fix_indexes(this);
	azalia_generic_mixer_default(this);

	azalia_generic_mixer_pin_sense(this);

	return 0;
}

int
azalia_stac9221_set_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;
	int err;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	if (mc->type != m->devinfo.type)
		return EINVAL;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if (m->target == STAC9221_TARGET_MASTER) {
		err = azalia_generic_mixer_set(this, 0x02,
		    MI_TARGET_OUTAMP, mc);
		err = azalia_generic_mixer_set(this, 0x03,
		    MI_TARGET_OUTAMP, mc);
		err = azalia_generic_mixer_set(this, 0x04,
		    MI_TARGET_OUTAMP, mc);
		err = azalia_generic_mixer_set(this, 0x05,
		    MI_TARGET_OUTAMP, mc);
		return err;
	}
	return azalia_generic_mixer_set(this, m->nid, m->target, mc);
}

int
azalia_stac9221_get_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	mc->type = m->devinfo.type;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if (m->target == STAC9221_TARGET_MASTER)
		return azalia_generic_mixer_get(this, m->nid,
		    MI_TARGET_OUTAMP, mc);
	return azalia_generic_mixer_get(this, m->nid, m->target, mc);
}

/* ----------------------------------------------------------------
 * Sony VAIO FE and SZ
 * ---------------------------------------------------------------- */

int
azalia_stac7661_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 1,
		{{2, {0x02, 0x05}}}};

	static const convgroupset_t adcs = {
		-1, 1,
		{{1, {0x08}}}};

	this->dacs = dacs;
	this->adcs = adcs;

	return 0;
}

static const mixer_item_t stac7661_mixer_items[] = {
	{{AZ_CLASS_INPUT, {AudioCinputs}, AUDIO_MIXER_CLASS, AZ_CLASS_INPUT, 0, 0}, 0},
	{{AZ_CLASS_OUTPUT, {AudioCoutputs}, AUDIO_MIXER_CLASS, AZ_CLASS_OUTPUT, 0, 0}, 0},
	{{AZ_CLASS_RECORD, {AudioCrecord}, AUDIO_MIXER_CLASS, AZ_CLASS_RECORD, 0, 0}, 0},

#define STAC7661_DAC_HP        0x02
#define STAC7661_DAC_SPEAKER   0x05
#define STAC7661_TARGET_MASTER -1

	{{0, {AudioNmaster}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  4, 0, .un.v={{""}, 2, MIXER_DELTA(127)}}, 0x02, STAC7661_TARGET_MASTER},
	{{0, {AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 3, ENUM_OFFON}, 0x02, STAC7661_TARGET_MASTER},
	{{0, {AudioNvolume"."AudioNmute}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	    ENUM_OFFON}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AudioNvolume}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	    .un.v={{""}, 2, MIXER_DELTA(15)}}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AudioNsource}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={3, {{{AudioNmicrophone}, 1}, {{AudioNmicrophone"2"}, 2},
			   {{AudioNdac}, 3}}}},
	 0x15, MI_TARGET_CONNLIST},
	{{0, {AudioNmicrophone}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  .un.v={{""}, 2, MIXER_DELTA(4)}}, 0x15, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	    0, 0, ENUM_OFFON}, 0x02, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	    0, 0, .un.v={{""}, 2, MIXER_DELTA(127)}}, 0x02, MI_TARGET_OUTAMP},
	{{0, {AudioNspeaker".mute"}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	    0, 0, ENUM_OFFON}, 0x05, MI_TARGET_OUTAMP},
	{{0, {AudioNspeaker}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	    0, 0, .un.v={{""}, 2, MIXER_DELTA(127)}}, 0x05, MI_TARGET_OUTAMP}
};

int
azalia_stac7661_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = sizeof(stac7661_mixer_items) / sizeof(mixer_item_t);
	this->mixers = malloc(sizeof(mixer_item_t) * this->nmixers,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (this->mixers == NULL) {
		printf("%s: out of memory in %s\n", XNAME(this), __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, stac7661_mixer_items,
	    sizeof(mixer_item_t) * this->nmixers);
	azalia_generic_mixer_fix_indexes(this);
	azalia_generic_mixer_default(this);

	azalia_generic_mixer_pin_sense(this);

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 1;
	azalia_generic_mixer_set(this, 0x09, MI_TARGET_INAMP(0), &mc); /* mute input */
	mc.un.ord = 2;          /* select internal mic for recording */
	azalia_generic_mixer_set(this, 0x15, MI_TARGET_CONNLIST, &mc);
	return 0;
}

int
azalia_stac7661_set_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;
	int err;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	if (mc->type != m->devinfo.type)
		return EINVAL;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if (m->target == STAC7661_TARGET_MASTER) {
		err = azalia_generic_mixer_set(this, STAC7661_DAC_HP,
		    MI_TARGET_OUTAMP, mc);
		err = azalia_generic_mixer_set(this, STAC7661_DAC_SPEAKER,
		    MI_TARGET_OUTAMP, mc);
		return err;
	}
	return azalia_generic_mixer_set(this, m->nid, m->target, mc);
}
int
azalia_stac7661_get_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	mc->type = m->devinfo.type;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if (m->target == STAC7661_TARGET_MASTER)
		return azalia_generic_mixer_get(this, m->nid,
		    MI_TARGET_OUTAMP, mc);
	return azalia_generic_mixer_get(this, m->nid, m->target, mc);
}
