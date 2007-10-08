/*	$OpenBSD: ossaudio.c,v 1.11 2007/10/08 01:00:13 jakemsr Exp $	*/
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

/*
 * This is an OSS (Linux) sound API emulator.
 * It provides the essentials of the API.
 */

/* XXX This file is essentially the same as sys/compat/ossaudio.c.
 * With some preprocessor magic it could be the same file.
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

static void setblocksize(int, struct audio_info *);

static int audio_ioctl(int, unsigned long, void *);
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
		return audio_ioctl(fd, com, argp);
	else if (IOCGROUP(com) == 'M')
		return mixer_ioctl(fd, com, argp);
	else
		return ioctl(fd, com, argp);
}

static int
audio_ioctl(int fd, unsigned long com, void *argp)
{

	struct audio_info tmpinfo;
	struct audio_offset tmpoffs;
	struct audio_buf_info bufinfo;
	struct count_info cntinfo;
	struct audio_encoding tmpenc;
	struct audio_bufinfo tmpab;
	u_int u;
	int idat, idata;
	int retval;

	switch (com) {
	case SNDCTL_DSP_RESET:
		retval = ioctl(fd, AUDIO_FLUSH, 0);
		if (retval < 0)
			return retval;
		break;
	case SNDCTL_DSP_SYNC:
		retval = ioctl(fd, AUDIO_DRAIN, 0);
		if (retval < 0)
			return retval;
		break;
	case SNDCTL_DSP_POST:
		/* This call is merely advisory, and may be a nop. */
		break;
	case SNDCTL_DSP_SPEED:
		AUDIO_INITINFO(&tmpinfo);
		tmpinfo.play.sample_rate =
		tmpinfo.record.sample_rate = INTARG;
		(void) ioctl(fd, AUDIO_SETINFO, &tmpinfo);
		/* FALLTHRU */
	case SOUND_PCM_READ_RATE:
		retval = ioctl(fd, AUDIO_GETINFO, &tmpinfo);
		if (retval < 0)
			return retval;
		INTARG = tmpinfo.play.sample_rate;
		break;
	case SNDCTL_DSP_STEREO:
		AUDIO_INITINFO(&tmpinfo);
		tmpinfo.play.channels =
		tmpinfo.record.channels = INTARG ? 2 : 1;
		(void) ioctl(fd, AUDIO_SETINFO, &tmpinfo);
		retval = ioctl(fd, AUDIO_GETINFO, &tmpinfo);
		if (retval < 0)
			return retval;
		INTARG = tmpinfo.play.channels - 1;
		break;
	case SNDCTL_DSP_GETBLKSIZE:
		retval = ioctl(fd, AUDIO_GETINFO, &tmpinfo);
		if (retval < 0)
			return retval;
		setblocksize(fd, &tmpinfo);
		INTARG = tmpinfo.blocksize;
		break;
	case SNDCTL_DSP_SETFMT:
		AUDIO_INITINFO(&tmpinfo);
		switch (INTARG) {
		case AFMT_MU_LAW:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 8;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_ULAW;
			break;
		case AFMT_A_LAW:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 8;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_ALAW;
			break;
		case AFMT_U8:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 8;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_ULINEAR;
			break;
		case AFMT_S8:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 8;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_SLINEAR;
			break;
		case AFMT_S16_LE:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 16;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_SLINEAR_LE;
			break;
		case AFMT_S16_BE:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 16;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_SLINEAR_BE;
			break;
		case AFMT_U16_LE:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 16;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_ULINEAR_LE;
			break;
		case AFMT_U16_BE:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 16;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_ULINEAR_BE;
			break;
		default:
			return EINVAL;
		}
		(void) ioctl(fd, AUDIO_SETINFO, &tmpinfo);
		/* FALLTHRU */
	case SOUND_PCM_READ_BITS:
		retval = ioctl(fd, AUDIO_GETINFO, &tmpinfo);
		if (retval < 0)
			return retval;
		switch (tmpinfo.play.encoding) {
		case AUDIO_ENCODING_ULAW:
			idat = AFMT_MU_LAW;
			break;
		case AUDIO_ENCODING_ALAW:
			idat = AFMT_A_LAW;
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			if (tmpinfo.play.precision == 16)
				idat = AFMT_S16_LE;
			else
				idat = AFMT_S8;
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			if (tmpinfo.play.precision == 16)
				idat = AFMT_S16_BE;
			else
				idat = AFMT_S8;
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (tmpinfo.play.precision == 16)
				idat = AFMT_U16_LE;
			else
				idat = AFMT_U8;
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (tmpinfo.play.precision == 16)
				idat = AFMT_U16_BE;
			else
				idat = AFMT_U8;
			break;
		case AUDIO_ENCODING_ADPCM:
			idat = AFMT_IMA_ADPCM;
			break;
		}
		INTARG = idat;
		break;
	case SNDCTL_DSP_CHANNELS:
		AUDIO_INITINFO(&tmpinfo);
		tmpinfo.play.channels =
		tmpinfo.record.channels = INTARG;
		(void) ioctl(fd, AUDIO_SETINFO, &tmpinfo);
		/* FALLTHRU */
	case SOUND_PCM_READ_CHANNELS:
		retval = ioctl(fd, AUDIO_GETINFO, &tmpinfo);
		if (retval < 0)
			return retval;
		INTARG = tmpinfo.play.channels;
		break;
	case SOUND_PCM_WRITE_FILTER:
	case SOUND_PCM_READ_FILTER:
		errno = EINVAL;
		return -1; /* XXX unimplemented */
	case SNDCTL_DSP_SUBDIVIDE:
		retval = ioctl(fd, AUDIO_GETINFO, &tmpinfo);
		if (retval < 0)
			return retval;
		setblocksize(fd, &tmpinfo);
		idat = INTARG;
		if (idat == 0)
			idat = tmpinfo.play.buffer_size / tmpinfo.blocksize;
		idat = (tmpinfo.play.buffer_size / idat) & -4;
		AUDIO_INITINFO(&tmpinfo);
		tmpinfo.blocksize = idat;
		retval = ioctl(fd, AUDIO_SETINFO, &tmpinfo);
		if (retval < 0)
			return retval;
		INTARG = tmpinfo.play.buffer_size / tmpinfo.blocksize;
		break;
	case SNDCTL_DSP_SETFRAGMENT:
		AUDIO_INITINFO(&tmpinfo);
		idat = INTARG;
		if ((idat & 0xffff) < 4 || (idat & 0xffff) > 17)
			return EINVAL;
		tmpinfo.blocksize = 1 << (idat & 0xffff);
		tmpinfo.hiwat = ((unsigned)idat >> 16) & 0x7fff;
		if (tmpinfo.hiwat == 0)	/* 0 means set to max */
			tmpinfo.hiwat = 65536;
		(void) ioctl(fd, AUDIO_SETINFO, &tmpinfo);
		retval = ioctl(fd, AUDIO_GETINFO, &tmpinfo);
		if (retval < 0)
			return retval;
		u = tmpinfo.blocksize;
		for(idat = 0; u > 1; idat++, u >>= 1)
			;
		idat |= (tmpinfo.hiwat & 0x7fff) << 16;
		INTARG = idat;
		break;
	case SNDCTL_DSP_GETFMTS:
		for(idat = 0, tmpenc.index = 0;
		    ioctl(fd, AUDIO_GETENC, &tmpenc) == 0;
		    tmpenc.index++) {
			switch(tmpenc.encoding) {
			case AUDIO_ENCODING_ULAW:
				idat |= AFMT_MU_LAW;
				break;
			case AUDIO_ENCODING_ALAW:
				idat |= AFMT_A_LAW;
				break;
			case AUDIO_ENCODING_SLINEAR:
				idat |= AFMT_S8;
				break;
			case AUDIO_ENCODING_SLINEAR_LE:
				if (tmpenc.precision == 16)
					idat |= AFMT_S16_LE;
				else
					idat |= AFMT_S8;
				break;
			case AUDIO_ENCODING_SLINEAR_BE:
				if (tmpenc.precision == 16)
					idat |= AFMT_S16_BE;
				else
					idat |= AFMT_S8;
				break;
			case AUDIO_ENCODING_ULINEAR:
				idat |= AFMT_U8;
				break;
			case AUDIO_ENCODING_ULINEAR_LE:
				if (tmpenc.precision == 16)
					idat |= AFMT_U16_LE;
				else
					idat |= AFMT_U8;
				break;
			case AUDIO_ENCODING_ULINEAR_BE:
				if (tmpenc.precision == 16)
					idat |= AFMT_U16_BE;
				else
					idat |= AFMT_U8;
				break;
			case AUDIO_ENCODING_ADPCM:
				idat |= AFMT_IMA_ADPCM;
				break;
			default:
				break;
			}
		}
		INTARG = idat;
		break;
	case SNDCTL_DSP_GETOSPACE:
		retval = ioctl(fd, AUDIO_GETPRINFO, &tmpab);
		if (retval < 0)
			return retval;
		bufinfo.fragsize = tmpab.blksize;
		bufinfo.fragstotal = tmpab.hiwat;
		bufinfo.bytes = tmpab.hiwat * tmpab.blksize - tmpab.seek;
		bufinfo.fragments = bufinfo.bytes / tmpab.blksize;
		*(struct audio_buf_info *)argp = bufinfo;
		break;
	case SNDCTL_DSP_GETISPACE:
		retval = ioctl(fd, AUDIO_GETRRINFO, &tmpab);
		if (retval < 0)
			return retval;
		bufinfo.fragsize = tmpab.blksize;
		bufinfo.fragstotal = tmpab.hiwat;
		bufinfo.bytes = tmpab.seek;
		bufinfo.fragments = bufinfo.bytes / tmpab.blksize;
		*(struct audio_buf_info *)argp = bufinfo;
		break;
	case SNDCTL_DSP_NONBLOCK:
		idat = 1;
		retval = ioctl(fd, FIONBIO, &idat);
		if (retval < 0)
			return retval;
		break;
	case SNDCTL_DSP_GETCAPS:
		retval = ioctl(fd, AUDIO_GETPROPS, &idata);
		if (retval < 0)
			return retval;
		idat = DSP_CAP_TRIGGER;
		if (idata & AUDIO_PROP_FULLDUPLEX)
			idat |= DSP_CAP_DUPLEX;
		if (idata & AUDIO_PROP_MMAP)
			idat |= DSP_CAP_MMAP;
		INTARG = idat;
		break;
	case SNDCTL_DSP_SETTRIGGER:
		idat = INTARG;
		AUDIO_INITINFO(&tmpinfo);
		tmpinfo.mode = 0;
		if (idat & PCM_ENABLE_OUTPUT) {
			tmpinfo.mode |= (AUMODE_PLAY | AUMODE_PLAY_ALL);
			tmpinfo.play.pause = 0;
		} else
			tmpinfo.play.pause = 1;
		if (idat & PCM_ENABLE_INPUT) {
			tmpinfo.mode |= AUMODE_RECORD;
			tmpinfo.record.pause = 0;
		} else
			tmpinfo.record.pause = 1;
		retval = ioctl(fd, AUDIO_SETINFO, &tmpinfo);
		if (retval < 0)
			return retval;
		/* FALLTHRU */
	case SNDCTL_DSP_GETTRIGGER:
		retval = ioctl(fd, AUDIO_GETINFO, &tmpinfo);
		if (retval < 0)
			return retval;
		idat = (tmpinfo.play.pause ? 0 : PCM_ENABLE_OUTPUT) |
		       (tmpinfo.record.pause ? 0 : PCM_ENABLE_INPUT);
		INTARG = idat;
		break;
	case SNDCTL_DSP_GETIPTR:
		retval = ioctl(fd, AUDIO_GETIOFFS, &tmpoffs);
		if (retval < 0)
			return retval;
		cntinfo.bytes = tmpoffs.samples;
		cntinfo.blocks = tmpoffs.deltablks;
		cntinfo.ptr = tmpoffs.offset;
		*(struct count_info *)argp = cntinfo;
		break;
	case SNDCTL_DSP_GETOPTR:
		retval = ioctl(fd, AUDIO_GETOOFFS, &tmpoffs);
		if (retval < 0)
			return retval;
		cntinfo.bytes = tmpoffs.samples;
		cntinfo.blocks = tmpoffs.deltablks;
		cntinfo.ptr = tmpoffs.offset;
		*(struct count_info *)argp = cntinfo;
		break;
	case SNDCTL_DSP_SETDUPLEX:
		idat = 1;
		retval = ioctl(fd, AUDIO_SETFD, &idat);
		if (retval < 0)
			return retval;
		break;
	case SNDCTL_DSP_MAPINBUF:
	case SNDCTL_DSP_MAPOUTBUF:
	case SNDCTL_DSP_SETSYNCRO:
	case SNDCTL_DSP_PROFILE:
		errno = EINVAL;
		return -1; /* XXX unimplemented */
	default:
		errno = EINVAL;
		return -1;
	}

	return 0;
}


/* If the NetBSD mixer device should have more than NETBSD_MAXDEVS devices
 * some will not be available to Linux */
#define NETBSD_MAXDEVS 64
struct audiodevinfo {
	int done;
	dev_t dev;
	ino_t ino;
	int16_t devmap[SOUND_MIXER_NRDEVICES],
	        rdevmap[NETBSD_MAXDEVS];
	char names[NETBSD_MAXDEVS][MAX_AUDIO_DEV_LEN];
	int enum2opaque[NETBSD_MAXDEVS];
        u_long devmask, recmask, stereomask;
	u_long caps, source;
};

static int
opaque_to_enum(struct audiodevinfo *di, audio_mixer_name_t *label, int opq)
{
	int i, o;

	for (i = 0; i < NETBSD_MAXDEVS; i++) {
		o = di->enum2opaque[i];
		if (o == opq)
			break;
		if (o == -1 && label != NULL &&
		    !strncmp(di->names[i], label->name, sizeof di->names[i])) {
			di->enum2opaque[i] = opq;
			break;
		}
	}
	if (i >= NETBSD_MAXDEVS)
		i = -1;
	/*printf("opq_to_enum %s %d -> %d\n", label->name, opq, i);*/
	return (i);
}

static int
enum_to_ord(struct audiodevinfo *di, int enm)
{
	if (enm >= NETBSD_MAXDEVS)
		return (-1);

	/*printf("enum_to_ord %d -> %d\n", enm, di->enum2opaque[enm]);*/
	return (di->enum2opaque[enm]);
}

static int
enum_to_mask(struct audiodevinfo *di, int enm)
{
	int m;
	if (enm >= NETBSD_MAXDEVS)
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
	mixer_devinfo_t mi;
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
/*		{ AudioNheadphone,	?? },*/
		{ AudioNoutput,		SOUND_MIXER_OGAIN },
		{ AudioNinput,		SOUND_MIXER_IGAIN },
/*		{ AudioNmaster,		SOUND_MIXER_SPEAKER },*/
/*		{ AudioNstereo,		?? },*/
/*		{ AudioNmono,		?? },*/
		{ AudioNfmsynth,	SOUND_MIXER_SYNTH },
/*		{ AudioNwave,		SOUND_MIXER_PCM },*/
		{ AudioNmidi,		SOUND_MIXER_SYNTH },
/*		{ AudioNmixerout,	?? },*/
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
	di->source = ~0;
	di->caps = 0;
	for(i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		di->devmap[i] = -1;
	for(i = 0; i < NETBSD_MAXDEVS; i++) {
		di->rdevmap[i] = -1;
		di->names[i][0] = '\0';
		di->enum2opaque[i] = -1;
	}
	for(i = 0; i < NETBSD_MAXDEVS; i++) {
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
	for(i = 0; i < NETBSD_MAXDEVS; i++) {
		mi.index = i;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &mi) < 0)
			break;
		if (strcmp(mi.label.name, AudioNsource) != 0)
			continue;
		di->source = i;
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
		if (di->source == -1)
			return EINVAL;
		mc.dev = di->source;
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
		if (di->source == -1)
			return EINVAL;
		mc.dev = di->source;
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

/*
 * Check that the blocksize is a power of 2 as OSS wants.
 * If not, set it to be.
 */
static void
setblocksize(int fd, struct audio_info *info)
{
	struct audio_info set;
	int s;

	if (info->blocksize & (info->blocksize-1)) {
		for(s = 32; s < info->blocksize; s <<= 1)
			;
		AUDIO_INITINFO(&set);
		set.blocksize = s;
		ioctl(fd, AUDIO_SETINFO, &set);
		ioctl(fd, AUDIO_GETINFO, info);
	}
}
