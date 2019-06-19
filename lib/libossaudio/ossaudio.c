/*	$OpenBSD: ossaudio.c,v 1.19 2018/10/26 14:46:05 miko Exp $	*/
/*	$NetBSD: ossaudio.c,v 1.14 2001/05/10 01:53:48 augustss Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
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

/*
 * This is an OSS (Linux) sound API emulator.
 * It provides the essentials of the API.
 */

#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <sys/stat.h>
#include <errno.h>

#include "soundcard.h"
#undef ioctl

#define GET_DEV(com) ((com) & 0xff)

#define TO_OSSVOL(x)	(((x) * 100 + 127) / 255)
#define FROM_OSSVOL(x)	((((x) > 100 ? 100 : (x)) * 255 + 50) / 100)

static struct audiodevinfo *getdevinfo(int);

static int mixer_ioctl(int, unsigned long, void *);
static int opaque_to_enum(struct audiodevinfo *di, audio_mixer_name_t *label, int opq);
static int enum_to_ord(struct audiodevinfo *di, int enm);
static int enum_to_mask(struct audiodevinfo *di, int enm);

#define INTARG (*(int*)argp)

int
_oss_ioctl(int fd, unsigned long com, ...)
{
	va_list ap;
	void *argp;

	va_start(ap, com);
	argp = va_arg(ap, void *);
	va_end(ap);
	if (IOCGROUP(com) == 'P')
		return ENOTTY;
	else if (IOCGROUP(com) == 'M')
		return mixer_ioctl(fd, com, argp);
	else
		return ioctl(fd, com, argp);
}

/* If the mixer device should have more than MAX_MIXER_DEVS devices
 * some will not be available to Linux */
#define MAX_MIXER_DEVS 64
struct audiodevinfo {
	int done;
	dev_t dev;
	ino_t ino;
	int16_t devmap[SOUND_MIXER_NRDEVICES],
	        rdevmap[MAX_MIXER_DEVS];
	char names[MAX_MIXER_DEVS][MAX_AUDIO_DEV_LEN];
	int enum2opaque[MAX_MIXER_DEVS];
        u_long devmask, recmask, stereomask;
	u_long caps, recsource;
};

static int
opaque_to_enum(struct audiodevinfo *di, audio_mixer_name_t *label, int opq)
{
	int i, o;

	for (i = 0; i < MAX_MIXER_DEVS; i++) {
		o = di->enum2opaque[i];
		if (o == opq)
			break;
		if (o == -1 && label != NULL &&
		    !strncmp(di->names[i], label->name, sizeof di->names[i])) {
			di->enum2opaque[i] = opq;
			break;
		}
	}
	if (i >= MAX_MIXER_DEVS)
		i = -1;
	/*printf("opq_to_enum %s %d -> %d\n", label->name, opq, i);*/
	return (i);
}

static int
enum_to_ord(struct audiodevinfo *di, int enm)
{
	if (enm >= MAX_MIXER_DEVS)
		return (-1);

	/*printf("enum_to_ord %d -> %d\n", enm, di->enum2opaque[enm]);*/
	return (di->enum2opaque[enm]);
}

static int
enum_to_mask(struct audiodevinfo *di, int enm)
{
	int m;
	if (enm >= MAX_MIXER_DEVS)
		return (0);

	m = di->enum2opaque[enm];
	if (m == -1)
		m = 0;
	/*printf("enum_to_mask %d -> %d\n", enm, di->enum2opaque[enm]);*/
	return (m);
}

/*
 * Collect the audio device information to allow faster
 * emulation of the Linux mixer ioctls.  Cache the information
 * to eliminate the overhead of repeating all the ioctls needed
 * to collect the information.
 */
static struct audiodevinfo *
getdevinfo(int fd)
{
	mixer_devinfo_t mi, cl;
	int i, j, e;
	static struct {
		char *name;
		int code;
	} *dp, devs[] = {
		{ AudioNmicrophone,	SOUND_MIXER_MIC },
		{ AudioNline,		SOUND_MIXER_LINE },
		{ AudioNcd,		SOUND_MIXER_CD },
		{ AudioNdac,		SOUND_MIXER_PCM },
		{ AudioNaux,		SOUND_MIXER_LINE1 },
		{ AudioNrecord,		SOUND_MIXER_IMIX },
		{ AudioNmaster,		SOUND_MIXER_VOLUME },
		{ AudioNtreble,		SOUND_MIXER_TREBLE },
		{ AudioNbass,		SOUND_MIXER_BASS },
		{ AudioNspeaker,	SOUND_MIXER_SPEAKER },
		{ AudioNoutput,		SOUND_MIXER_OGAIN },
		{ AudioNinput,		SOUND_MIXER_IGAIN },
		{ AudioNfmsynth,	SOUND_MIXER_SYNTH },
		{ AudioNmidi,		SOUND_MIXER_SYNTH },
		{ 0, -1 }
	};
	static struct audiodevinfo devcache = { 0 };
	struct audiodevinfo *di = &devcache;
	struct stat sb;

	/* Figure out what device it is so we can check if the
	 * cached data is valid.
	 */
	if (fstat(fd, &sb) < 0)
		return 0;
	if (di->done && (di->dev == sb.st_dev && di->ino == sb.st_ino))
		return di;

	di->done = 1;
	di->dev = sb.st_dev;
	di->ino = sb.st_ino;
	di->devmask = 0;
	di->recmask = 0;
	di->stereomask = 0;
	di->recsource = ~0;
	di->caps = 0;
	for(i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		di->devmap[i] = -1;
	for(i = 0; i < MAX_MIXER_DEVS; i++) {
		di->rdevmap[i] = -1;
		di->names[i][0] = '\0';
		di->enum2opaque[i] = -1;
	}
	for(i = 0; i < MAX_MIXER_DEVS; i++) {
		mi.index = i;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &mi) < 0)
			break;
		switch(mi.type) {
		case AUDIO_MIXER_VALUE:
			for(dp = devs; dp->name; dp++)
		    		if (strcmp(dp->name, mi.label.name) == 0)
					break;
			if (dp->code >= 0) {
				di->devmap[dp->code] = i;
				di->rdevmap[i] = dp->code;
				di->devmask |= 1 << dp->code;
				if (mi.un.v.num_channels == 2)
					di->stereomask |= 1 << dp->code;
				strncpy(di->names[i], mi.label.name,
					sizeof di->names[i]);
			}
			break;
		}
	}
	for(i = 0; i < MAX_MIXER_DEVS; i++) {
		mi.index = i;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &mi) < 0)
			break;
		if (strcmp(mi.label.name, AudioNsource) != 0)
			continue;
		cl.index = mi.mixer_class;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &cl) < 0)
			break;
		if ((cl.type != AUDIO_MIXER_CLASS) ||
		    (strcmp(cl.label.name, AudioCrecord) != 0))
			continue;
		di->recsource = i;
		switch(mi.type) {
		case AUDIO_MIXER_ENUM:
			for(j = 0; j < mi.un.e.num_mem; j++) {
				e = opaque_to_enum(di,
						   &mi.un.e.member[j].label,
						   mi.un.e.member[j].ord);
				if (e >= 0)
					di->recmask |= 1 << di->rdevmap[e];
			}
			di->caps = SOUND_CAP_EXCL_INPUT;
			break;
		case AUDIO_MIXER_SET:
			for(j = 0; j < mi.un.s.num_mem; j++) {
				e = opaque_to_enum(di,
						   &mi.un.s.member[j].label,
						   mi.un.s.member[j].mask);
				if (e >= 0)
					di->recmask |= 1 << di->rdevmap[e];
			}
			break;
		}
	}
	return di;
}

int
mixer_ioctl(int fd, unsigned long com, void *argp)
{
	struct audiodevinfo *di;
	struct mixer_info *omi;
	struct audio_device adev;
	mixer_ctrl_t mc;
	int idat = 0;
	int i;
	int retval;
	int l, r, n, error, e;

	di = getdevinfo(fd);
	if (di == 0)
		return -1;

	switch (com) {
	case OSS_GETVERSION:
		idat = SOUND_VERSION;
		break;
	case SOUND_MIXER_INFO:
	case SOUND_OLD_MIXER_INFO:
		error = ioctl(fd, AUDIO_GETDEV, &adev);
		if (error)
			return (error);
		omi = argp;
		if (com == SOUND_MIXER_INFO)
			omi->modify_counter = 1;
		strncpy(omi->id, adev.name, sizeof omi->id);
		strncpy(omi->name, adev.name, sizeof omi->name);
		return 0;
	case SOUND_MIXER_READ_RECSRC:
		if (di->recsource == -1)
			return EINVAL;
		mc.dev = di->recsource;
		if (di->caps & SOUND_CAP_EXCL_INPUT) {
			mc.type = AUDIO_MIXER_ENUM;
			retval = ioctl(fd, AUDIO_MIXER_READ, &mc);
			if (retval < 0)
				return retval;
			e = opaque_to_enum(di, NULL, mc.un.ord);
			if (e >= 0)
				idat = 1 << di->rdevmap[e];
		} else {
			mc.type = AUDIO_MIXER_SET;
			retval = ioctl(fd, AUDIO_MIXER_READ, &mc);
			if (retval < 0)
				return retval;
			e = opaque_to_enum(di, NULL, mc.un.mask);
			if (e >= 0)
				idat = 1 << di->rdevmap[e];
		}
		break;
	case SOUND_MIXER_READ_DEVMASK:
		idat = di->devmask;
		break;
	case SOUND_MIXER_READ_RECMASK:
		idat = di->recmask;
		break;
	case SOUND_MIXER_READ_STEREODEVS:
		idat = di->stereomask;
		break;
	case SOUND_MIXER_READ_CAPS:
		idat = di->caps;
		break;
	case SOUND_MIXER_WRITE_RECSRC:
	case SOUND_MIXER_WRITE_R_RECSRC:
		if (di->recsource == -1)
			return EINVAL;
		mc.dev = di->recsource;
		idat = INTARG;
		if (di->caps & SOUND_CAP_EXCL_INPUT) {
			mc.type = AUDIO_MIXER_ENUM;
			for(i = 0; i < SOUND_MIXER_NRDEVICES; i++)
				if (idat & (1 << i))
					break;
			if (i >= SOUND_MIXER_NRDEVICES ||
			    di->devmap[i] == -1)
				return EINVAL;
			mc.un.ord = enum_to_ord(di, di->devmap[i]);
		} else {
			mc.type = AUDIO_MIXER_SET;
			mc.un.mask = 0;
			for(i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
				if (idat & (1 << i)) {
					if (di->devmap[i] == -1)
						return EINVAL;
					mc.un.mask |= enum_to_mask(di, di->devmap[i]);
				}
			}
		}
		return ioctl(fd, AUDIO_MIXER_WRITE, &mc);
	default:
		if (MIXER_READ(SOUND_MIXER_FIRST) <= com &&
		    com < MIXER_READ(SOUND_MIXER_NRDEVICES)) {
			n = GET_DEV(com);
			if (di->devmap[n] == -1)
				return EINVAL;
			mc.dev = di->devmap[n];
			mc.type = AUDIO_MIXER_VALUE;
		    doread:
			mc.un.value.num_channels = di->stereomask & (1<<n) ? 2 : 1;
			retval = ioctl(fd, AUDIO_MIXER_READ, &mc);
			if (retval < 0)
				return retval;
			if (mc.type != AUDIO_MIXER_VALUE)
				return EINVAL;
			if (mc.un.value.num_channels != 2) {
				l = r = mc.un.value.level[AUDIO_MIXER_LEVEL_MONO];
			} else {
				l = mc.un.value.level[AUDIO_MIXER_LEVEL_LEFT];
				r = mc.un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
			}
			idat = TO_OSSVOL(l) | (TO_OSSVOL(r) << 8);
			break;
		} else if ((MIXER_WRITE_R(SOUND_MIXER_FIRST) <= com &&
			   com < MIXER_WRITE_R(SOUND_MIXER_NRDEVICES)) ||
			   (MIXER_WRITE(SOUND_MIXER_FIRST) <= com &&
			   com < MIXER_WRITE(SOUND_MIXER_NRDEVICES))) {
			n = GET_DEV(com);
			if (di->devmap[n] == -1)
				return EINVAL;
			idat = INTARG;
			l = FROM_OSSVOL( idat       & 0xff);
			r = FROM_OSSVOL((idat >> 8) & 0xff);
			mc.dev = di->devmap[n];
			mc.type = AUDIO_MIXER_VALUE;
			if (di->stereomask & (1<<n)) {
				mc.un.value.num_channels = 2;
				mc.un.value.level[AUDIO_MIXER_LEVEL_LEFT] = l;
				mc.un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = r;
			} else {
				mc.un.value.num_channels = 1;
				mc.un.value.level[AUDIO_MIXER_LEVEL_MONO] = (l+r)/2;
			}
			retval = ioctl(fd, AUDIO_MIXER_WRITE, &mc);
			if (retval < 0)
				return retval;
			if (MIXER_WRITE(SOUND_MIXER_FIRST) <= com &&
			   com < MIXER_WRITE(SOUND_MIXER_NRDEVICES))
				return 0;
			goto doread;
		} else {
			errno = EINVAL;
			return -1;
		}
	}
	INTARG = idat;
	return 0;
}
