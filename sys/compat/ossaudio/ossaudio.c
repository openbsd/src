/*	$OpenBSD: ossaudio.c,v 1.2 1998/04/26 22:15:42 provos Exp $	*/
/*	$NetBSD: ossaudio.c,v 1.23 1997/10/19 07:41:52 augustss Exp $	*/

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

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/audioio.h>

#include <sys/syscallargs.h>

#include <compat/ossaudio/ossaudio.h>
#include <compat/ossaudio/ossaudiovar.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x) if (ossdebug) printf x
int ossdebug = 0;
#else
#define DPRINTF(x)
#endif

#define TO_OSSVOL(x) ((x) * 100 / 255)
#define FROM_OSSVOL(x) ((x) * 255 / 100)

static struct audiodevinfo *getdevinfo __P((struct file *, struct proc *));

static void setblocksize __P((struct file *, struct audio_info *, struct proc *));


int
oss_ioctl_audio(p, uap, retval)
	struct proc *p;
	struct oss_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap;
	register_t *retval;
{	       
	struct file *fp;
	struct filedesc *fdp;
	u_long com;
	struct audio_info tmpinfo;
	struct audio_offset tmpoffs;
	struct oss_audio_buf_info bufinfo;
	struct oss_count_info cntinfo;
	struct audio_encoding tmpenc;
	u_int u;
	int idat, idata;
	int error;
	int (*ioctlf) __P((struct file *, u_long, caddr_t, struct proc *));

	fdp = p->p_fd;
	if ((u_int)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return (EBADF);

	if ((fp->f_flag & (FREAD | FWRITE)) == 0)
		return (EBADF);

	ioctlf = fp->f_ops->fo_ioctl;

	com = SCARG(uap, com);
	retval[0] = 0;

	DPRINTF(("oss_sys_ioctl: com=%08lx\n", com));
	switch (com) {
	case OSS_SNDCTL_DSP_RESET:
		error = ioctlf(fp, AUDIO_FLUSH, (caddr_t)0, p);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_SYNC:
	case OSS_SNDCTL_DSP_POST:
		error = ioctlf(fp, AUDIO_DRAIN, (caddr_t)0, p);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_SPEED:
		AUDIO_INITINFO(&tmpinfo);
		error = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (error)
			return error;
		tmpinfo.play.sample_rate =
		tmpinfo.record.sample_rate = idat;
		error = ioctlf(fp, AUDIO_SETINFO, (caddr_t)&tmpinfo, p);
		DPRINTF(("oss_sys_ioctl: SNDCTL_DSP_SPEED %d = %d\n",
			 idat, error));
		if (error)
			return error;
		/* fall into ... */
	case OSS_SOUND_PCM_READ_RATE:
		error = ioctlf(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		idat = tmpinfo.play.sample_rate;
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_STEREO:
		AUDIO_INITINFO(&tmpinfo);
		error = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (error)
			return error;
		tmpinfo.play.channels =
		tmpinfo.record.channels = idat ? 2 : 1;
		(void) ioctlf(fp, AUDIO_SETINFO, (caddr_t)&tmpinfo, p);
		error = ioctlf(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		idat = tmpinfo.play.channels - 1;
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_GETBLKSIZE:
		error = ioctlf(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		setblocksize(fp, &tmpinfo, p);
		idat = tmpinfo.blocksize;
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_SETFMT:
		AUDIO_INITINFO(&tmpinfo);
		error = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (error)
			return error;
		switch (idat) {
		case OSS_AFMT_MU_LAW:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 8;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_ULAW;
			break;
		case OSS_AFMT_A_LAW:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 8;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_ALAW;
			break;
		case OSS_AFMT_U8:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 8;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_ULINEAR;
			break;
		case OSS_AFMT_S8:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 8;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_SLINEAR;
			break;
		case OSS_AFMT_S16_LE:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 16;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_SLINEAR_LE;
			break;
		case OSS_AFMT_S16_BE:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 16;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_SLINEAR_BE;
			break;
		case OSS_AFMT_U16_LE:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 16;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_ULINEAR_LE;
			break;
		case OSS_AFMT_U16_BE:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 16;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_ULINEAR_BE;
			break;
		default:
			return EINVAL;
		}
		(void) ioctlf(fp, AUDIO_SETINFO, (caddr_t)&tmpinfo, p);
		/* fall into ... */
	case OSS_SOUND_PCM_READ_BITS:
		error = ioctlf(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		switch (tmpinfo.play.encoding) {
		case AUDIO_ENCODING_ULAW:
			idat = OSS_AFMT_MU_LAW;
			break;
		case AUDIO_ENCODING_ALAW:
			idat = OSS_AFMT_A_LAW;
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			if (tmpinfo.play.precision == 16)
				idat = OSS_AFMT_S16_LE;
			else
				idat = OSS_AFMT_S8;
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			if (tmpinfo.play.precision == 16)
				idat = OSS_AFMT_S16_BE;
			else
				idat = OSS_AFMT_S8;
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (tmpinfo.play.precision == 16)
				idat = OSS_AFMT_U16_LE;
			else
				idat = OSS_AFMT_U8;
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (tmpinfo.play.precision == 16)
				idat = OSS_AFMT_U16_BE;
			else
				idat = OSS_AFMT_U8;
			break;
		case AUDIO_ENCODING_ADPCM:
			idat = OSS_AFMT_IMA_ADPCM;
			break;
		}
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_CHANNELS:
		AUDIO_INITINFO(&tmpinfo);
		error = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (error)
			return error;
		tmpinfo.play.channels =
		tmpinfo.record.channels = idat;
		(void) ioctlf(fp, AUDIO_SETINFO, (caddr_t)&tmpinfo, p);
		/* fall into ... */
	case OSS_SOUND_PCM_READ_CHANNELS:
		error = ioctlf(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		idat = tmpinfo.play.channels;
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case OSS_SOUND_PCM_WRITE_FILTER:
	case OSS_SOUND_PCM_READ_FILTER:
		return EINVAL; /* XXX unimplemented */
	case OSS_SNDCTL_DSP_SUBDIVIDE:
		error = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (error)
			return error;
		error = ioctlf(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		setblocksize(fp, &tmpinfo, p);
		if (error)
			return error;
		if (idat == 0)
			idat = tmpinfo.play.buffer_size / tmpinfo.blocksize;
		idat = (tmpinfo.play.buffer_size / idat) & -4;
		AUDIO_INITINFO(&tmpinfo);
		tmpinfo.blocksize = idat;
		error = ioctlf(fp, AUDIO_SETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		idat = tmpinfo.play.buffer_size / tmpinfo.blocksize;
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_SETFRAGMENT:
		AUDIO_INITINFO(&tmpinfo);
		error = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (error)
			return error;
		if ((idat & 0xffff) < 4 || (idat & 0xffff) > 17)
			return EINVAL;
		tmpinfo.blocksize = 1 << (idat & 0xffff);
		tmpinfo.hiwat = (idat >> 16) & 0xffff;
		DPRINTF(("oss_audio: SETFRAGMENT blksize=%d, hiwat=%d\n",
			 tmpinfo.blocksize, tmpinfo.hiwat));
		if (tmpinfo.hiwat == 0)	/* 0 means set to max */
			tmpinfo.hiwat = 65536;
		(void) ioctlf(fp, AUDIO_SETINFO, (caddr_t)&tmpinfo, p);
		error = ioctlf(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		u = tmpinfo.blocksize;
		for(idat = 0; u; idat++, u >>= 1)
			;
		idat |= (tmpinfo.hiwat & 0xffff) << 16;
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_GETFMTS:
		for(idat = 0, tmpenc.index = 0; 
		    ioctlf(fp, AUDIO_GETENC, (caddr_t)&tmpenc, p) == 0; 
		    tmpenc.index++) {
			if (tmpenc.flags & AUDIO_ENCODINGFLAG_EMULATED)
				continue; /* Don't report emulated modes */
			switch(tmpenc.encoding) {
			case AUDIO_ENCODING_ULAW:
				idat |= OSS_AFMT_MU_LAW;
				break;
			case AUDIO_ENCODING_ALAW:
				idat |= OSS_AFMT_A_LAW;
				break;
			case AUDIO_ENCODING_SLINEAR:
				idat |= OSS_AFMT_S8;
				break;
			case AUDIO_ENCODING_SLINEAR_LE:
				if (tmpenc.precision == 16)
					idat |= OSS_AFMT_S16_LE;
				else
					idat |= OSS_AFMT_S8;
				break;
			case AUDIO_ENCODING_SLINEAR_BE:
				if (tmpenc.precision == 16)
					idat |= OSS_AFMT_S16_BE;
				else
					idat |= OSS_AFMT_S8;
				break;
			case AUDIO_ENCODING_ULINEAR:
				idat |= OSS_AFMT_U8;
				break;
			case AUDIO_ENCODING_ULINEAR_LE:
				if (tmpenc.precision == 16)
					idat |= OSS_AFMT_U16_LE;
				else
					idat |= OSS_AFMT_U8;
				break;
			case AUDIO_ENCODING_ULINEAR_BE:
				if (tmpenc.precision == 16)
					idat |= OSS_AFMT_U16_BE;
				else
					idat |= OSS_AFMT_U8;
				break;
			case AUDIO_ENCODING_ADPCM:
				idat |= OSS_AFMT_IMA_ADPCM;
				break;
			default:
				break;
			}
		}
		DPRINTF(("oss_sys_ioctl: SNDCTL_DSP_GETFMTS = %x\n", idat));
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_GETOSPACE:
	case OSS_SNDCTL_DSP_GETISPACE:
		error = ioctlf(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		setblocksize(fp, &tmpinfo, p);
		bufinfo.fragsize = tmpinfo.blocksize;
		bufinfo.fragments = /* XXX */
		bufinfo.fragstotal = tmpinfo.play.buffer_size / bufinfo.fragsize;
		bufinfo.bytes = tmpinfo.play.buffer_size;
		DPRINTF(("oss_sys_ioctl: SNDCTL_DSP_GETxSPACE = %d %d %d %d\n",
			 bufinfo.fragsize, bufinfo.fragments, 
			 bufinfo.fragstotal, bufinfo.bytes));
		error = copyout(&bufinfo, SCARG(uap, data), sizeof bufinfo);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_NONBLOCK:
		idat = 1;
		error = ioctlf(fp, FIONBIO, (caddr_t)&idat, p);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_GETCAPS:
		error = ioctlf(fp, AUDIO_GETPROPS, (caddr_t)&idata, p);
		if (error)
			return error;
		idat = OSS_DSP_CAP_TRIGGER; /* pretend we have trigger */
		if (idata & AUDIO_PROP_FULLDUPLEX)
			idat |= OSS_DSP_CAP_DUPLEX;
		if (idata & AUDIO_PROP_MMAP)
			idat |= OSS_DSP_CAP_MMAP;
		DPRINTF(("oss_sys_ioctl: SNDCTL_DSP_GETCAPS = %x\n", idat));
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
#if 0
	case OSS_SNDCTL_DSP_GETTRIGGER:
		error = ioctlf(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		idat = (tmpinfo.play.pause ? 0 : OSS_PCM_ENABLE_OUTPUT) |
		       (tmpinfo.record.pause ? 0 : OSS_PCM_ENABLE_INPUT);
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_SETTRIGGER:
		AUDIO_INITINFO(&tmpinfo);
		error = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (error)
			return error;
		tmpinfo.play.pause = (idat & OSS_PCM_ENABLE_OUTPUT) == 0;
		tmpinfo.record.pause = (idat & OSS_PCM_ENABLE_INPUT) == 0;
		(void) ioctlf(fp, AUDIO_SETINFO, (caddr_t)&tmpinfo, p);
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
#else
	case OSS_SNDCTL_DSP_GETTRIGGER:
	case OSS_SNDCTL_DSP_SETTRIGGER:
		/* XXX Do nothing for now. */
		idat = OSS_PCM_ENABLE_OUTPUT;
		return copyout(&idat, SCARG(uap, data), sizeof idat);
#endif
	case OSS_SNDCTL_DSP_GETIPTR:
		error = ioctlf(fp, AUDIO_GETIOFFS, (caddr_t)&tmpoffs, p);
		if (error)
			return error;
		cntinfo.bytes = tmpoffs.samples;
		cntinfo.blocks = tmpoffs.deltablks;
		cntinfo.ptr = tmpoffs.offset;
		error = copyout(&cntinfo, SCARG(uap, data), sizeof cntinfo);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_GETOPTR:
		error = ioctlf(fp, AUDIO_GETOOFFS, (caddr_t)&tmpoffs, p);
		if (error)
			return error;
		cntinfo.bytes = tmpoffs.samples;
		cntinfo.blocks = tmpoffs.deltablks;
		cntinfo.ptr = tmpoffs.offset;
		error = copyout(&cntinfo, SCARG(uap, data), sizeof cntinfo);
		if (error)
			return error;
		break;
	case OSS_SNDCTL_DSP_MAPINBUF:
	case OSS_SNDCTL_DSP_MAPOUTBUF:
	case OSS_SNDCTL_DSP_SETSYNCRO:
	case OSS_SNDCTL_DSP_SETDUPLEX:
	case OSS_SNDCTL_DSP_PROFILE:
		return EINVAL; /* XXX unimplemented */
	default:
		return EINVAL;
	}

	return 0;
}

/* If the NetBSD mixer device should have more than 32 devices
 * some will not be available to Linux */
#define NETBSD_MAXDEVS 64
struct audiodevinfo {
	int done;
	dev_t dev;
	int16_t devmap[OSS_SOUND_MIXER_NRDEVICES], 
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
getdevinfo(fp, p)
	struct file *fp;
	struct proc *p;
{
	mixer_devinfo_t mi;
	int i;
	static struct {
		char *name;
		int code;
	} *dp, devs[] = {
		{ AudioNmicrophone,	OSS_SOUND_MIXER_MIC },
		{ AudioNline,		OSS_SOUND_MIXER_LINE },
		{ AudioNcd,		OSS_SOUND_MIXER_CD },
		{ AudioNdac,		OSS_SOUND_MIXER_PCM },
		{ AudioNrecord,		OSS_SOUND_MIXER_IMIX },
		{ AudioNmaster,		OSS_SOUND_MIXER_VOLUME },
		{ AudioNtreble,		OSS_SOUND_MIXER_TREBLE },
		{ AudioNbass,		OSS_SOUND_MIXER_BASS },
		{ AudioNspeaker,	OSS_SOUND_MIXER_SPEAKER },
/*		{ AudioNheadphone,	?? },*/
		{ AudioNoutput,		OSS_SOUND_MIXER_OGAIN },
		{ AudioNinput,		OSS_SOUND_MIXER_IGAIN },
/*		{ AudioNmaster,		OSS_SOUND_MIXER_SPEAKER },*/
/*		{ AudioNstereo,		?? },*/
/*		{ AudioNmono,		?? },*/
		{ AudioNfmsynth,	OSS_SOUND_MIXER_SYNTH },
/*		{ AudioNwave,		OSS_SOUND_MIXER_PCM },*/
		{ AudioNmidi,		OSS_SOUND_MIXER_SYNTH },
/*		{ AudioNmixerout,	?? },*/
		{ 0, -1 }
	};
	int (*ioctlf) __P((struct file *, u_long, caddr_t, struct proc *)) =
	    fp->f_ops->fo_ioctl;
	struct vnode *vp;
	struct vattr va;
	static struct audiodevinfo devcache = { 0 };
	struct audiodevinfo *di = &devcache;

	/* Figure out what device it is so we can check if the
	 * cached data is valid.
	 */
	vp = (struct vnode *)fp->f_data;
	if (vp->v_type != VCHR)
		return 0;
	if (VOP_GETATTR(vp, &va, p->p_ucred, p))
		return 0;
	if (di->done && di->dev == va.va_rdev)
		return di;

	di->done = 1;
	di->dev = va.va_rdev;
	di->devmask = 0;
	di->recmask = 0;
	di->stereomask = 0;
	di->source = -1;
	di->caps = 0;
	for(i = 0; i < OSS_SOUND_MIXER_NRDEVICES; i++)
		di->devmap[i] = -1;
	for(i = 0; i < NETBSD_MAXDEVS; i++)
		di->rdevmap[i] = -1;
	for(i = 0; i < NETBSD_MAXDEVS; i++) {
		mi.index = i;
		if (ioctlf(fp, AUDIO_MIXER_DEVINFO, (caddr_t)&mi, p) < 0)
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
				di->caps = OSS_SOUND_CAP_EXCL_INPUT;
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
oss_ioctl_mixer(p, uap, retval)
	struct proc *p;
	struct oss_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap;
	register_t *retval;
{	       
	struct file *fp;
	struct filedesc *fdp;
	u_long com;
	struct audiodevinfo *di;
	mixer_ctrl_t mc;
	int idat;
	int i;
	int error;
	int l, r, n;
	int (*ioctlf) __P((struct file *, u_long, caddr_t, struct proc *));

	fdp = p->p_fd;
	if ((u_int)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return (EBADF);

	if ((fp->f_flag & (FREAD | FWRITE)) == 0)
		return (EBADF);

	com = SCARG(uap, com);
	retval[0] = 0;

	di = getdevinfo(fp, p);
	if (di == 0)
		return EINVAL;

	ioctlf = fp->f_ops->fo_ioctl;
	switch (com) {
	case OSS_SOUND_MIXER_READ_RECSRC:
		if (di->source == -1)
			return EINVAL;
		mc.dev = di->source;
		if (di->caps & OSS_SOUND_CAP_EXCL_INPUT) {
			mc.type = AUDIO_MIXER_ENUM;
			error = ioctlf(fp, AUDIO_MIXER_READ, (caddr_t)&mc, p);
			if (error)
				return error;
			idat = 1 << di->rdevmap[mc.un.ord];
		} else {
			int k;
			unsigned int mask;
			mc.type = AUDIO_MIXER_SET;
			error = ioctlf(fp, AUDIO_MIXER_READ, (caddr_t)&mc, p);
			if (error)
				return error;
			idat = 0;
			for(mask = mc.un.mask, k = 0; mask; mask >>= 1, k++)
				if (mask & 1)
					idat |= 1 << di->rdevmap[k];
		}
		break;
	case OSS_SOUND_MIXER_READ_DEVMASK:
		idat = di->devmask;
		break;
	case OSS_SOUND_MIXER_READ_RECMASK:
		idat = di->recmask;
		break;
	case OSS_SOUND_MIXER_READ_STEREODEVS:
		idat = di->stereomask;
		break;
	case OSS_SOUND_MIXER_READ_CAPS:
		idat = di->caps;
		break;
	case OSS_SOUND_MIXER_WRITE_RECSRC:
	case OSS_SOUND_MIXER_WRITE_R_RECSRC:
		if (di->source == -1)
			return EINVAL;
		mc.dev = di->source;
		error = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (error)
			return error;
		if (di->caps & OSS_SOUND_CAP_EXCL_INPUT) {
			mc.type = AUDIO_MIXER_ENUM;
			for(i = 0; i < OSS_SOUND_MIXER_NRDEVICES; i++)
				if (idat & (1 << i))
					break;
			if (i >= OSS_SOUND_MIXER_NRDEVICES ||
			    di->devmap[i] == -1)
				return EINVAL;
			mc.un.ord = di->devmap[i];
		} else {
			mc.type = AUDIO_MIXER_SET;
			mc.un.mask = 0;
			for(i = 0; i < OSS_SOUND_MIXER_NRDEVICES; i++) {
				if (idat & (1 << i)) {
					if (di->devmap[i] == -1)
						return EINVAL;
					mc.un.mask |= 1 << di->devmap[i];
				}
			}
		}
		return ioctlf(fp, AUDIO_MIXER_WRITE, (caddr_t)&mc, p);
	default:
		if (OSS_MIXER_READ(OSS_SOUND_MIXER_FIRST) <= com &&
		    com < OSS_MIXER_READ(OSS_SOUND_MIXER_NRDEVICES)) {
			n = OSS_GET_DEV(com);
			if (di->devmap[n] == -1)
				return EINVAL;
		    doread:
			mc.dev = di->devmap[n];
			mc.type = AUDIO_MIXER_VALUE;
			mc.un.value.num_channels = di->stereomask & (1<<n) ? 2 : 1;
			error = ioctlf(fp, AUDIO_MIXER_READ, (caddr_t)&mc, p);
			if (error)
				return error;
			if (mc.un.value.num_channels != 2) {
				l = r = mc.un.value.level[AUDIO_MIXER_LEVEL_MONO];
			} else {
				l = mc.un.value.level[AUDIO_MIXER_LEVEL_LEFT];
				r = mc.un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
			}
			idat = TO_OSSVOL(l) | (TO_OSSVOL(r) << 8);
			DPRINTF(("OSS_MIXER_READ  n=%d (dev=%d) l=%d, r=%d, idat=%04x\n", 
				 n, di->devmap[n], l, r, idat));
			break;
		} else if ((OSS_MIXER_WRITE_R(OSS_SOUND_MIXER_FIRST) <= com &&
			   com < OSS_MIXER_WRITE_R(OSS_SOUND_MIXER_NRDEVICES)) ||
			   (OSS_MIXER_WRITE(OSS_SOUND_MIXER_FIRST) <= com &&
			   com < OSS_MIXER_WRITE(OSS_SOUND_MIXER_NRDEVICES))) {
			n = OSS_GET_DEV(com);
			if (di->devmap[n] == -1)
				return EINVAL;
			error = copyin(SCARG(uap, data), &idat, sizeof idat);
			if (error)
				return error;
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
			DPRINTF(("OSS_MIXER_WRITE n=%d (dev=%d) l=%d, r=%d, idat=%04x\n", 
				 n, di->devmap[n], l, r, idat));
			error = ioctlf(fp, AUDIO_MIXER_WRITE, (caddr_t)&mc, p);
			if (error)
				return error;
			if (OSS_MIXER_WRITE(OSS_SOUND_MIXER_FIRST) <= com &&
			   com < OSS_MIXER_WRITE(OSS_SOUND_MIXER_NRDEVICES))
				return 0;
			goto doread;
		} else {
#ifdef AUDIO_DEBUG
			printf("oss_audio: unknown mixer ioctl %04lx\n", com);
#endif
			return EINVAL;
		}
	}
	return copyout(&idat, SCARG(uap, data), sizeof idat);
}

/* XXX hook for sequencer emulation */
int
oss_ioctl_sequencer(p, uap, retval)
	struct proc *p;
	struct oss_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap;
	register_t *retval;
{	       
	struct file *fp;
	struct filedesc *fdp;
#if 0
	u_long com;
	int idat;
	int error;
#endif

	fdp = p->p_fd;
	if ((u_int)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return (EBADF);

	if ((fp->f_flag & (FREAD | FWRITE)) == 0)
		return (EBADF);

#if 0
	com = SCARG(uap, com);
#endif
	retval[0] = 0;

	return EINVAL;
}

/*
 * Check that the blocksize is a power of 2 as OSS wants.
 * If not, set it to be.
 */
static void setblocksize(fp, info, p)
	struct file *fp;
	struct audio_info *info;
	struct proc *p;
{
	struct audio_info set;
	int s;

	if (info->blocksize & (info->blocksize-1)) {
		for(s = 32; s < info->blocksize; s <<= 1)
			;
		AUDIO_INITINFO(&set);
		set.blocksize = s;
		fp->f_ops->fo_ioctl(fp, AUDIO_SETINFO, (caddr_t)&set, p);
		fp->f_ops->fo_ioctl(fp, AUDIO_GETINFO, (caddr_t)info, p);
	}
}
