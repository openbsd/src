#define LINUX_IOCPARM_MASK    0x7f            /* parameters must be < 128 bytes */
#define LINUX_IOC_VOID        0x00000000      /* no parameters */
#define LINUX_IOC_IN          0x40000000      /* copy in parameters */
#define LINUX_IOC_OUT         0x80000000      /* copy out parameters */
#define LINUX_IOC_INOUT       (LINUX_IOC_IN | LINUX_IOC_OUT)
#define	_LINUX_IOCTL(w,x,y,z) ((int)((w)|(((z)&LINUX_IOCPARM_MASK)<<16)|((x)<<8)|(y)))
#if 0
#define _LINUX_IO(x,y)        _LINUX_IOCTL(LINUX_IOC_VOID, x, y, 0)
#endif
#define _LINUX_IOR(x,y,t)     _LINUX_IOCTL(LINUX_IOC_OUT, x, y, sizeof(t))
#define _LINUX_IOW(x,y,t)     _LINUX_IOCTL(LINUX_IOC_IN, x, y, sizeof(t))
#define _LINUX_IOWR(x,y,t)    _LINUX_IOCTL(LINUX_IOC_INOUT, x, y, sizeof(t))

#define	LINUX_SNDCTL_DSP_RESET		_LINUX_IO('P', 0)
#define	LINUX_SNDCTL_DSP_SYNC		_LINUX_IO('P', 1)
#define	LINUX_SNDCTL_DSP_SPEED		_LINUX_IOWR('P', 2, int)
#define	LINUX_SNDCTL_DSP_STEREO		_LINUX_IOWR('P', 3, int)
#define	LINUX_SNDCTL_DSP_GETBLKSIZE	_LINUX_IOWR('P', 4, int)
#define	LINUX_SNDCTL_DSP_SETFMT		_LINUX_IOWR('P', 5, int)
#define	LINUX_SNDCTL_DSP_POST		_LINUX_IO('P', 8)
#define	LINUX_SNDCTL_DSP_SETFRAGMENT	_LINUX_IOWR('P', 10, int)
#define	LINUX_SNDCTL_DSP_GETFMTS	_LINUX_IOR('P', 11, int)

#define	LINUX_AFMT_QUERY		0x00000000	/* Return current fmt */
#define	LINUX_AFMT_MU_LAW		0x00000001
#define	LINUX_AFMT_A_LAW		0x00000002
#define	LINUX_AFMT_IMA_ADPCM		0x00000004
#define	LINUX_AFMT_U8			0x00000008
#define	LINUX_AFMT_S16_LE		0x00000010	/* Little endian signed 16 */
#define	LINUX_AFMT_S16_BE		0x00000020	/* Big endian signed 16 */
#define	LINUX_AFMT_S8			0x00000040
#define	LINUX_AFMT_U16_LE		0x00000080	/* Little endian U16 */
#define	LINUX_AFMT_U16_BE		0x00000100	/* Big endian U16 */
#define	LINUX_AFMT_MPEG			0x00000200	/* MPEG (2) audio */
