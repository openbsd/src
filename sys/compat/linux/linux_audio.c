#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/audioio.h>

#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_audio.h>

int
linux_ioctl_audio(p, uap, retval)
	register struct proc *p;
	register struct linux_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap;
	register_t *retval;
{	       
	register struct file *fp;
	register struct filedesc *fdp;
	u_long com;
	struct audio_info tmpinfo;
	int idat;
	int error;

	fdp = p->p_fd;
	if ((u_int)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return (EBADF);

	if ((fp->f_flag & (FREAD | FWRITE)) == 0)
		return (EBADF);

	com = SCARG(uap, com);
	retval[0] = 0;

	switch (com) {
	case LINUX_SNDCTL_DSP_RESET:
		error = (*fp->f_ops->fo_ioctl)(fp, AUDIO_FLUSH, (caddr_t)0, p);
		if (error)
			return error;
		break;
	case LINUX_SNDCTL_DSP_SYNC:
	case LINUX_SNDCTL_DSP_POST:
		error = (*fp->f_ops->fo_ioctl)(fp, AUDIO_DRAIN, (caddr_t)0, p);
		if (error)
			return error;
		break;
	case LINUX_SNDCTL_DSP_SPEED:
		AUDIO_INITINFO(&tmpinfo);
		error = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (error)
			return error;
		tmpinfo.play.sample_rate =
		tmpinfo.record.sample_rate = idat;
		(void) (*fp->f_ops->fo_ioctl)(fp, AUDIO_SETINFO, (caddr_t)&tmpinfo, p);
		error = (*fp->f_ops->fo_ioctl)(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		idat = tmpinfo.play.sample_rate;
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case LINUX_SNDCTL_DSP_STEREO:
		AUDIO_INITINFO(&tmpinfo);
		error = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (error)
			return error;
		tmpinfo.play.channels =
		tmpinfo.record.channels = idat ? 2 : 1;
		(void) (*fp->f_ops->fo_ioctl)(fp, AUDIO_SETINFO, (caddr_t)&tmpinfo, p);
		error = (*fp->f_ops->fo_ioctl)(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		idat = tmpinfo.play.channels - 1;
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case LINUX_SNDCTL_DSP_GETBLKSIZE:
		error = (*fp->f_ops->fo_ioctl)(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		idat = tmpinfo.blocksize;
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case LINUX_SNDCTL_DSP_SETFMT:
		AUDIO_INITINFO(&tmpinfo);
		error = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (error)
			return error;
		switch (idat) {
		case LINUX_AFMT_MU_LAW:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 8;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_ULAW;
			break;
		case LINUX_AFMT_A_LAW:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 8;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_ALAW;
			break;
		case LINUX_AFMT_U8:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 8;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_LINEAR;
			break;
		case LINUX_AFMT_S16_LE:
			tmpinfo.play.precision =
			tmpinfo.record.precision = 16;
			tmpinfo.play.encoding =
			tmpinfo.record.encoding = AUDIO_ENCODING_LINEAR;
			break;
		default:
			return EINVAL;
		}
		(void) (*fp->f_ops->fo_ioctl)(fp, AUDIO_SETINFO, (caddr_t)&tmpinfo, p);
		error = (*fp->f_ops->fo_ioctl)(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		/*XXXX*/
		break;
	case LINUX_SNDCTL_DSP_SETFRAGMENT:
		AUDIO_INITINFO(&tmpinfo);
		error = copyin(SCARG(uap, data), &idat, sizeof idat);
		if (error)
			return error;
		if ((idat & 0xffff) < 4 || (idat & 0xffff) > 17)
			return EINVAL;
		tmpinfo.blocksize = 1 << (idat & 0xffff);
		tmpinfo.hiwat = (idat >> 16) & 0xffff;
		(void) (*fp->f_ops->fo_ioctl)(fp, AUDIO_SETINFO, (caddr_t)&tmpinfo, p);
		error = (*fp->f_ops->fo_ioctl)(fp, AUDIO_GETINFO, (caddr_t)&tmpinfo, p);
		if (error)
			return error;
		idat = tmpinfo.blocksize;
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	case LINUX_SNDCTL_DSP_GETFMTS:
		idat = LINUX_AFMT_MU_LAW | LINUX_AFMT_U8 | LINUX_AFMT_S16_LE;
		error = copyout(&idat, SCARG(uap, data), sizeof idat);
		if (error)
			return error;
		break;
	default:
		return EINVAL;
	}

	return 0;
}
