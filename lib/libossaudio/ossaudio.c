/*	$OpenBSD: ossaudio.c,v 1.1.1.1 1998/05/01 09:23:00 provos Exp $	*/
/*	$NetBSD: ossaudio.c,v 1.5 1998/03/23 00:39:18 augustss Exp $	*/

/*
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

#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <sys/stat.h>
#include <errno.h>

#include "soundcard.h"
#undef ioctl

#define GET_DEV(com) ((com) & 0xff)

#define TO_OSSVOL(x) ((x) * 100 / 255)
#define FROM_OSSVOL(x) ((x) * 255 / 100)

static struct audiodevinfo *getdevinfo(int);

static void setblocksize(int, struct audio_info *);

static int audio_ioctl(int, unsigned long, void *);
static int mixer_ioctl(int, unsigned long, void *);

#define INTARG (*(int*)argp)

int
_oss_ioctl(int fd, unsigned long com, void *argp)
{
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
	case SNDCTL_DSP_POST:
		retval = ioctl(fd, AUDIO_DRAIN, 0);
		if (retval < 0)
			return retval;
		break;
	case SNDCTL_DSP_SPEED:
		AUDIO_INITINFO(&tmpinfo);
		tmpinfo.play.sample_rate =
		tmpinfo.record.sample_rate = INTARG;
		(void) ioctl(fd, AUDIO_SETINFO, &tmpinfo);
		/* fall into ... */
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
		/* fall into ... */
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
		/* fall into ... */
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
		tmpinfo.hiwat = (idat >> 16) & 0x7fff;
		if (tmpinfo.hiwat == 0)	/* 0 means set to max */
			tmpinfo.hiwat = 65536;
		(void) ioctl(fd, AUDIO_SETINFO, &tmpinfo);
		retval = ioctl(fd, AUDIO_GETINFO, &tmpinfo);
		if (retval < 0)
			return retval;
		u = tmpinfo.blocksize;
		for(idat = 0; u; idat++, u >>= 1)
			;
		idat |= (tmpinfo.hiwat & 0x7fff) << 16;
		INTARG = idat;
		break;
	case SNDCTL_DSP_GETFMTS:
		for(idat = 0, tmpenc.index = 0; 
		    ioctl(fd, AUDIO_GETENC, &tmpenc) == 0; 
		    tmpenc.index++) {
			if (tmpenc.flags & AUDIO_ENCODINGFLAG_EMULATED)
				continue; /* Don't report emulated modes */
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
	case SNDCTL_DSP_GETISPACE:
		retval = ioctl(fd, AUDIO_GETINFO, (caddr_t)&tmpinfo);
		if (retval < 0)
			return retval;
		setblocksize(fd, &tmpinfo);
		bufinfo.fragsize = tmpinfo.blocksize;
		bufinfo.fragments = /* XXX */
		bufinfo.fragstotal = tmpinfo.play.buffer_size / bufinfo.fragsize;
		bufinfo.bytes = tmpinfo.play.buffer_size;
		*(struct audio_buf_info *)argp = bufinfo;
		break;
	case SNDCTL_DSP_NONBLOCK:
		idat = 1;
		retval = ioctl(fd, FIONBIO, &idat);
		if (retval < 0)
			return retval;
		break;
	case SNDCTL_DSP_GETCAPS:
		retval = ioctl(fd, AUDIO_GETPROPS, (caddr_t)&idata);
		if (retval < 0)
			return retval;
		idat = DSP_CAP_TRIGGER; /* pretend we have trigger */
		if (idata & AUDIO_PROP_FULLDUPLEX)
			idat |= DSP_CAP_DUPLEX;
		if (idata & AUDIO_PROP_MMAP)
			idat |= DSP_CAP_MMAP;
		INTARG = idat;
		break;
#if 0
	case SNDCTL_DSP_GETTRIGGER:
		retval = ioctl(fd, AUDIO_GETINFO, (caddr_t)&tmpinfo);
		if (retval < 0)
			return retval;
		idat = (tmpinfo.play.pause ? 0 : PCM_ENABLE_OUTPUT) |
		       (tmpinfo.record.pause ? 0 : PCM_ENABLE_INPUT);
		retval = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (retval < 0)
			return retval;
		break;
	case SNDCTL_DSP_SETTRIGGER:
		AUDIO_INITINFO(&tmpinfo);
		retval = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (retval < 0)
			return retval;
		tmpinfo.play.pause = (idat & PCM_ENABLE_OUTPUT) == 0;
		tmpinfo.record.pause = (idat & PCM_ENABLE_INPUT) == 0;
		(void) ioctl(fd, AUDIO_SETINFO, (caddr_t)&tmpinfo);
		retval = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (retval < 0)
			return retval;
		break;
#else
	case SNDCTL_DSP_GETTRIGGER:
	case SNDCTL_DSP_SETTRIGGER:
		/* XXX Do nothing for now. */
		INTARG = PCM_ENABLE_OUTPUT;
		break;
#endif
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
	case SNDCTL_DSP_MAPINBUF:
	case SNDCTL_DSP_MAPOUTBUF:
	case SNDCTL_DSP_SETSYNCRO:
	case SNDCTL_DSP_SETDUPLEX:
	case SNDCTL_DSP_PROFILE:
		errno = EINVAL;
		return -1; /* XXX unimplemented */
	default:
		errno = EINVAL;
		return -1;
	}

	return 0;
}


/* If the NetBSD mixer device should have more than 32 devices
 * some will not be available to Linux */
#define NETBSD_MAXDEVS 32
struct audiodevinfo {
	int done;
	dev_t dev;
	int16_t devmap[SOUND_MIXER_NRDEVICES], 
	        rdevmap[NETBSD_MAXDEVS];
        u_long devmask, recmask, stereomask;
	u_long caps, source;
};

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
	int i;
	static struct {
		char *name;
		int code;
	} *dp, devs[] = {
		{ AudioNmicrophone,	SOUND_MIXER_MIC },
		{ AudioNline,		SOUND_MIXER_LINE },
		{ AudioNcd,		SOUND_MIXER_CD },
		{ AudioNdac,		SOUND_MIXER_PCM },
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
	if (di->done && di->dev == sb.st_dev)
		return di;

	di->done = 1;
	di->dev = sb.st_dev;
	di->devmask = 0;
	di->recmask = 0;
	di->stereomask = 0;
	di->source = -1;
	di->caps = 0;
	for(i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		di->devmap[i] = -1;
	for(i = 0; i < NETBSD_MAXDEVS; i++)
		di->rdevmap[i] = -1;
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
			}
			break;
		case AUDIO_MIXER_ENUM:
			if (strcmp(mi.label.name, AudioNsource) == 0) {
				int j;
				di->source = i;
				for(j = 0; j < mi.un.e.num_mem; j++)
					di->recmask |= 1 << di->rdevmap[mi.un.e.member[j].ord];
				di->caps = SOUND_CAP_EXCL_INPUT;
			}
			break;
		case AUDIO_MIXER_SET:
			if (strcmp(mi.label.name, AudioNsource) == 0) {
				int j;
				di->source = i;
				for(j = 0; j < mi.un.s.num_mem; j++) {
					int k, mask = mi.un.s.member[j].mask;
					if (mask) {
						for(k = 0; !(mask & 1); mask >>= 1, k++)
							;
						di->recmask |= 1 << di->rdevmap[k];
					}
				}
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
	mixer_ctrl_t mc;
	int idat;
	int i;
	int retval;
	int l, r, n;

	di = getdevinfo(fd);
	if (di == 0)
		return -1;

	switch (com) {
	case SOUND_MIXER_READ_RECSRC:
		if (di->source == -1)
			return EINVAL;
		mc.dev = di->source;
		if (di->caps & SOUND_CAP_EXCL_INPUT) {
			mc.type = AUDIO_MIXER_ENUM;
			retval = ioctl(fd, AUDIO_MIXER_READ, &mc);
			if (retval < 0)
				return retval;
			idat = 1 << di->rdevmap[mc.un.ord];
		} else {
			int k;
			unsigned int mask;
			mc.type = AUDIO_MIXER_SET;
			retval = ioctl(fd, AUDIO_MIXER_READ, &mc);
			if (retval < 0)
				return retval;
			idat = 0;
			for(mask = mc.un.mask, k = 0; mask; mask >>= 1, k++)
				if (mask & 1)
					idat |= 1 << di->rdevmap[k];
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
			mc.un.ord = di->devmap[i];
		} else {
			mc.type = AUDIO_MIXER_SET;
			mc.un.mask = 0;
			for(i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
				if (idat & (1 << i)) {
					if (di->devmap[i] == -1)
						return EINVAL;
					mc.un.mask |= 1 << di->devmap[i];
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

