/*	$NetBSD: azalia_codec.c,v 1.3 2005/09/29 04:14:03 kent Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include <sys/cdefs.h>
#ifdef NETBSD_GOOP
__KERNEL_RCSID(0, "$NetBSD: azalia_codec.c,v 1.3 2005/09/29 04:14:03 kent Exp $");
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <uvm/uvm_param.h>
#include <dev/pci/azalia.h>


static int	azalia_codec_init_dacgroup(codec_t *);
static int	azalia_codec_add_dacgroup(codec_t *, int, uint32_t);
static int	azalia_codec_find_pin(const codec_t *, int, int, uint32_t);
static int	azalia_codec_find_dac(const codec_t *, int, int);
static int	alc260_init_dacgroup(codec_t *);
static int	alc880_init_dacgroup(codec_t *);
static int	alc882_init_dacgroup(codec_t *);
static int	alc882_init_widget(const codec_t *, widget_t *, nid_t);
static int	stac9221_init_dacgroup(codec_t *);


int
azalia_codec_init_vtbl(codec_t *this, uint32_t vid)
{
	switch (vid) {
	case 0x10ec0260:
		this->name = "Realtek ALC260";
		this->init_dacgroup = alc260_init_dacgroup;
		break;
	case 0x10ec0880:
		this->name = "Realtek ALC880";
		this->init_dacgroup = alc880_init_dacgroup;
		break;
	case 0x10ec0882:
		this->name = "Realtek ALC882";
		this->init_dacgroup = alc882_init_dacgroup;
		this->init_widget = alc882_init_widget;
		break;
	case 0x83847680:
		this->name = "Sigmatel STAC9221";
		this->init_dacgroup = stac9221_init_dacgroup;
	default:
		this->name = NULL;
		this->init_dacgroup = azalia_codec_init_dacgroup;
	}
	return 0;
}

/* ----------------------------------------------------------------
 * functions for generic codecs
 * ---------------------------------------------------------------- */

static int
azalia_codec_init_dacgroup(codec_t *this)
{
	int i, j, assoc, group;

	/*
	 * grouping DACs
	 *   [0] the lowest assoc DACs
	 *   [1] the lowest assoc digital outputs
	 *   [2] the 2nd assoc DACs
	 *      :
	 */
	this->ndacgroups = 0;
	for (assoc = 0; assoc < CORB_CD_ASSOCIATION_MAX; assoc++) {
		azalia_codec_add_dacgroup(this, assoc, 0);
		azalia_codec_add_dacgroup(this, assoc, COP_AWCAP_DIGITAL);
	}

	/* find DACs which do not connect with any pins by default */
	DPRINTF(("%s: find non-connected DACs\n", __func__));
	FOR_EACH_WIDGET(this, i) {
		boolean_t found;

		if (this->w[i].type != COP_AWTYPE_AUDIO_OUTPUT)
			continue;
		found = FALSE;
		for (group = 0; group < this->ndacgroups; group++) {
			for (j = 0; j < this->dacgroups[group].nconv; j++) {
				if (i == this->dacgroups[group].conv[j]) {
					found = TRUE;
					group = this->ndacgroups;
					break;
				}
			}
		}
		if (found)
			continue;
		if (this->ndacgroups >= 32)
			break;
		this->dacgroups[this->ndacgroups].nconv = 1;
		this->dacgroups[this->ndacgroups].conv[0] = i;
		this->ndacgroups++;
	}
	this->cur_dac = 0;

	/* enumerate ADCs */
	this->nadcs = 0;
	FOR_EACH_WIDGET(this, i) {
		if (this->w[i].type != COP_AWTYPE_AUDIO_INPUT)
			continue;
		this->adcs[this->nadcs++] = i;
		if (this->nadcs >= 32)
			break;
	}
	this->cur_adc = 0;
	return 0;
}

static int
azalia_codec_add_dacgroup(codec_t *this, int assoc, uint32_t digital)
{
	int i, j, n, dac, seq;

	n = 0;
	for (seq = 0 ; seq < CORB_CD_SEQUENCE_MAX; seq++) {
		i = azalia_codec_find_pin(this, assoc, seq, digital);
		if (i < 0)
			continue;
		dac = azalia_codec_find_dac(this, i, 0);
		if (dac < 0)
			continue;
		/* duplication check */
		for (j = 0; j < n; j++) {
			if (this->dacgroups[this->ndacgroups].conv[j] == dac)
				break;
		}
		if (j < n)	/* this group already has <dac> */
			continue;
		this->dacgroups[this->ndacgroups].conv[n++] = dac;
		DPRINTF(("%s: assoc=%d seq=%d ==> g=%d n=%d\n",
			 __func__, assoc, seq, this->ndacgroups, n-1));
	}
	if (n <= 0)		/* no such DACs */
		return 0;
	this->dacgroups[this->ndacgroups].nconv = n;

	/* check if the same combination is already registered */
	for (i = 0; i < this->ndacgroups; i++) {
		if (n != this->dacgroups[i].nconv)
			continue;
		for (j = 0; j < n; j++) {
			if (this->dacgroups[this->ndacgroups].conv[j] !=
			    this->dacgroups[i].conv[j])
				break;
		}
		if (j >= n) /* matched */
			return 0;
	}
	/* found no equivalent group */
	this->ndacgroups++;
	return 0;
}

static int
azalia_codec_find_pin(const codec_t *this, int assoc, int seq, uint32_t digital)
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

static int
azalia_codec_find_dac(const codec_t *this, int index, int depth)
{
	const widget_t *w;
	int i, j, ret;

	w = &this->w[index];
	if (w->type == COP_AWTYPE_AUDIO_OUTPUT) {
		DPRINTF(("%s: DAC: nid=0x%x index=%d\n",
		    __func__, w->nid, index));
		return index;
	}
	if (++depth > 50) {
		return -1;
	}
	if (w->selected >= 0) {
		j = w->connections[w->selected];
		ret = azalia_codec_find_dac(this, j, depth);
		if (ret >= 0) {
			DPRINTF(("%s: DAC path: nid=0x%x index=%d\n",
			    __func__, w->nid, index));
			return ret;
		}
	}
	for (i = 0; i < w->nconnections; i++) {
		j = w->connections[i];
		ret = azalia_codec_find_dac(this, j, depth);
		if (ret >= 0) {
			DPRINTF(("%s: DAC path: nid=0x%x index=%d\n",
			    __func__, w->nid, index));
			return ret;
		}
	}
	return -1;
}

/* ----------------------------------------------------------------
 * Realtek ALC260
 * ---------------------------------------------------------------- */

static int
alc260_init_dacgroup(codec_t *this)
{
	static const convgroup_t dacs[2] = {
		{1, {0x02}},	/* analog 2ch */
		{1, {0x03}}};	/* digital */

	this->ndacgroups = 2;
	this->dacgroups[0] = dacs[0];
	this->dacgroups[1] = dacs[1];

	this->nadcs = 3;
	this->adcs[0] = 0x04;
	this->adcs[1] = 0x05;
	this->adcs[2] = 0x06;	/* digital */
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC880
 * ---------------------------------------------------------------- */

static int
alc880_init_dacgroup(codec_t *this)
{
	static const convgroup_t dacs[2] = {
		{4, {0x02, 0x04, 0x03, 0x05}}, /* analog 8ch */
		{1, {0x06}}};	/* digital */

	this->ndacgroups = 2;
	this->dacgroups[0] = dacs[0];
	this->dacgroups[1] = dacs[1];

	this->nadcs = 4;
	this->adcs[0] = 0x07;
	this->adcs[1] = 0x08;
	this->adcs[2] = 0x09;
	this->adcs[3] = 0x0a;	/* digital */
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC882
 * ---------------------------------------------------------------- */

static int
alc882_init_dacgroup(codec_t *this)
{
	static const convgroup_t dacs[3] = {
		{4, {0x02, 0x04, 0x03, 0x05}}, /* analog 8ch */
		{1, {0x06}},	/* digital */
		{1, {0x25}}};	/* another analog */
		
	this->ndacgroups = 3;
	this->dacgroups[0] = dacs[0];
	this->dacgroups[1] = dacs[1];
	this->dacgroups[2] = dacs[2];

	this->nadcs = 4;
	this->adcs[0] = 0x07;
	this->adcs[1] = 0x08;
	this->adcs[2] = 0x09;
	this->adcs[3] = 0x0a;	/* digital */
	return 0;
}

static int
alc882_init_widget(const codec_t *this, widget_t *w, nid_t nid)
{
	switch (nid) {
	case 0x14:
		strlcpy(w->name, "green", sizeof(w->name));
		break;
	case 0x15:
		strlcpy(w->name, "gray", sizeof(w->name));
		break;
	case 0x16:
		strlcpy(w->name, "orange", sizeof(w->name));
		break;
	case 0x17:
		strlcpy(w->name, "black", sizeof(w->name));
		break;
	case 0x18:
		strlcpy(w->name, "mic1", sizeof(w->name));
		break;
	case 0x19:
		strlcpy(w->name, "mic2", sizeof(w->name));
		break;
	case 0x1a:
		strlcpy(w->name, AudioNline, sizeof(w->name));
		break;
	case 0x1b:
		/* AudioNheadphone is too long */
		strlcpy(w->name, "hp", sizeof(w->name));
		break;
	case 0x1c:
		strlcpy(w->name, AudioNcd, sizeof(w->name));
		break;
	case 0x1d:
		strlcpy(w->name, AudioNspeaker, sizeof(w->name));
		break;
	}
	return 0;
}

/* ----------------------------------------------------------------
 * Sigmatel STAC9221
 * ---------------------------------------------------------------- */

static int
stac9221_init_dacgroup(codec_t *this)
{
	static const convgroup_t dacs[3] = {
		{4, {0x02, 0x03, 0x05, 0x04}}, /* analog 8ch */
		{1, {0x08}},	/* digital */
		{1, {0x1a}}};	/* another digital? */

	this->ndacgroups = 3;
	this->dacgroups[0] = dacs[0];
	this->dacgroups[1] = dacs[1];
	this->dacgroups[2] = dacs[2];

	this->nadcs = 3;
	this->adcs[0] = 6;	/* XXX four channel recording */
	this->adcs[1] = 7;
	this->adcs[2] = 9;	/* digital */
	return 0;
}
