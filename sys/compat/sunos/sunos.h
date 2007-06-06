/*	$OpenBSD: sunos.h,v 1.9 2007/06/06 17:15:13 deraadt Exp $	*/
/*	$NetBSD: sunos.h,v 1.8 1996/05/05 16:07:43 veego Exp $	*/

#define	SUNM_RDONLY	0x01	/* mount fs read-only */
#define	SUNM_NOSUID	0x02	/* mount fs with setuid disallowed */
#define	SUNM_NEWTYPE	0x04	/* type is string (char *), not int */
#define	SUNM_GRPID	0x08	/* (bsd semantics; ignored) */
#define	SUNM_REMOUNT	0x10	/* update existing mount */
#define	SUNM_NOSUB	0x20	/* prevent submounts (rejected) */
#define	SUNM_MULTI	0x40	/* (ignored) */
#define	SUNM_SYS5	0x80	/* Sys 5-specific semantics (rejected) */

struct sunos_nfs_args {
	struct	sockaddr_in *addr;	/* file server address */
	caddr_t	fh;			/* file handle to be mounted */
	int	flags;			/* flags */
	int	wsize;			/* write size in bytes */
	int	rsize;			/* read size in bytes */
	int	timeo;			/* initial timeout in .1 secs */
	int	retrans;		/* times to retry send */
	char	*hostname;		/* server's hostname */
	int	acregmin;		/* attr cache file min secs */
	int	acregmax;		/* attr cache file max secs */
	int	acdirmin;		/* attr cache dir min secs */
	int	acdirmax;		/* attr cache dir max secs */
	char	*netname;		/* server's netname */
	struct	pathcnf *pathconf;	/* static pathconf kludge */
};
/* SunOS nfs flag values: */
#define SUNNFS_SOFT	0x1
#define SUNNFS_WSIZE	0x2
#define SUNNFS_RSIZE	0x4
#define SUNNFS_TIMEO	0x8
#define SUNNFS_RETRANS	0x10
#define SUNNFS_HOSTNAME	0x20
#define SUNNFS_INT	0x40
#define SUNNFS_NOAC	0x80
#define SUNNFS_ACREGMIN	0x100
#define SUNNFS_ACREGMAX	0x200
#define SUNNFS_ACDIRMIN	0x400
#define SUNNFS_ACDIRMAX	0x800
#define SUNNFS_SECURE	0x1000
#define SUNNFS_NOCTO	0x2000
#define SUNNFS_POSIX	0x4000


struct sunos_ustat {
	int32_t	f_tfree;	/* total free */
	ino_t	f_tinode;	/* total inodes free */
	char	f_path[6];	/* filsys name */
	char	f_fpack[6];	/* filsys pack name */
};

struct sunos_statfs {
	long	f_type;		/* type of info, zero for now */
	long	f_bsize;	/* fundamental file system block size */
	long	f_blocks;	/* total blocks in file system */
	long	f_bfree;	/* free blocks */
	long	f_bavail;	/* free blocks available to non-super-user */
	long	f_files;	/* total file nodes in file system */
	long	f_ffree;	/* free file nodes in fs */
	fsid_t	f_fsid;		/* file system id */
	long	f_spare[7];	/* spare for later */
};


struct sunos_utsname {
	char    sysname[9];
	char    nodename[9];
	char    nodeext[65-9];
	char    release[9];
	char    version[9];
	char    machine[9];
};


struct sunos_ttysize {
	int	ts_row;
	int	ts_col;
};

struct sunos_termio {
	u_short	c_iflag;
	u_short	c_oflag;
	u_short	c_cflag;
	u_short	c_lflag;
	char	c_line;
	unsigned char c_cc[8];
};
#define SUNOS_TCGETA	_IOR('T', 1, struct sunos_termio)
#define SUNOS_TCSETA	_IOW('T', 2, struct sunos_termio)
#define SUNOS_TCSETAW	_IOW('T', 3, struct sunos_termio)
#define SUNOS_TCSETAF	_IOW('T', 4, struct sunos_termio)
#define SUNOS_TCSBRK	_IO('T', 5)

struct sunos_termios {
	u_long	c_iflag;
	u_long	c_oflag;
	u_long	c_cflag;
	u_long	c_lflag;
	char	c_line;
	u_char	c_cc[17];
};
#define SUNOS_TCXONC	_IO('T', 6)
#define SUNOS_TCFLSH	_IO('T', 7)
#define SUNOS_TCGETS	_IOR('T', 8, struct sunos_termios)
#define SUNOS_TCSETS	_IOW('T', 9, struct sunos_termios)
#define SUNOS_TCSETSW	_IOW('T', 10, struct sunos_termios)
#define SUNOS_TCSETSF	_IOW('T', 11, struct sunos_termios)
#define SUNOS_TCSNDBRK	_IO('T', 12)
#define SUNOS_TCDRAIN	_IO('T', 13)

/* Sun audio compatibility */
struct sunos_audio_prinfo {
	u_int	sample_rate;
	u_int	channels;
	u_int	precision;
	u_int	encoding;
	u_int	gain;
	u_int	port;
	u_int	avail_ports;
	u_int	reserved0[3];
	u_int	samples;
	u_int	eof;
	u_char	pause;
	u_char	error;
	u_char	waiting;
	u_char	balance;
	u_short	minordev;
	u_char	open;
	u_char	active;
};
struct sunos_audio_info {
	struct sunos_audio_prinfo play;
	struct sunos_audio_prinfo record;
	u_int monitor_gain;
	u_int reserved[4];
};

/* Values for AUDIO_GETDEV ioctl: */
#define SUNOS_AUDIO_DEV_UNKNOWN			0
#define SUNOS_AUDIO_DEV_AMD			1
#define SUNOS_AUDIO_DEV_SPEAKERBOX		2
#define SUNOS_AUDIO_DEV_CODEC			3

__BEGIN_DECLS
/* Defined in arch/m68k/m68k/sunos_machdep.c -- sparc uses regular sendsig() */
#ifndef __sparc__
void	sunos_sendsig(sig_t, int, int, u_long, int, union sigval);
#endif
__END_DECLS
