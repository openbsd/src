/*	$OpenBSD: bugio.h,v 1.6 1999/04/11 03:26:28 smurph Exp $ */
#include "sys/cdefs.h"

struct bugdisk_io {
	char	clun;
	char	dlun;
	short	status;
	void	*addr;
	int	blkno;
#define	fileno	blkno
	short	nblks;
	char	flag;
#define	FILEMARKFLAG	0x80
#define	IGNOREFILENO	0x02
#define	ENDOFFILE	0x01
	char	am;
};	

/* values are in BCD {upper nibble+lower nibble} */

struct bugrtc {
	unsigned char	Y;
	unsigned char	M;
	unsigned char	D;
	unsigned char	d;
	unsigned char	H;
	unsigned char	m;
	unsigned char	s;
	unsigned char	c;
};

/* Board ID - lots of info */

struct bugbrdid {
	unsigned char	eye[4];
	char	rev;
	char	month;
	char	day;
	char	year;
	short	packetsize;
	short	dummy;
	short	brdno;
	unsigned char	brdsuf[2];
	char	options[3];
	char	family:4;
	char	cpu:4;
	short	clun;
	short	dlun;
	short	type;
	short	dev;
	int	option;
	char	version[4];
	char	serial[12];			/* SBC serial number */
	char	id[16];				/* SBC id */
	char	pwa[16];				/* printed wiring assembly number */
	char	speed[4];			/* cpu speed */
	char	etheraddr[6];		/* mac address, all zero if no ether */
	char	fill[2];		
	char	scsiid[2];			/* local SCSI id */
	char	sysid[8];			/* system id - nothing on mvme187 */
	char	brd1_pwb[8];		/* memory board 1 pwb */
	char	brd1_serial[8];	/* memory board 1 serial */
	char	brd2_pwb[8];		/* memory board 2 pwb */
	char	brd2_serial[8];	/* memory board 2 serial */
	char	reserved[153];
	char	cksum[1];
};

struct bugniocall {
	unsigned char clun;
	unsigned char dlun;
	unsigned char ci;
	unsigned char cd;
#define	NETCTRL_INITDEVICE	0
#define	NETCTRL_GETHDW			1
#define	NETCTRL_TX				2
#define	NETCTRL_RX				3
#define	NETCTRL_FLUSH			4
#define	NETCTRL_RESET			5
	unsigned long cid;
	unsigned long memaddr;
	unsigned long nbytes;
	unsigned long csword;
};

char buginchr	__P((void));
int buginstat	__P((void));
int bugoutchr	__P((unsigned char));
int bugoutstr	__P((char *, char *));
int bugpcrlf	__P((void));
int bugdskrd	__P((struct bugdisk_io *));
int bugdskwr	__P((struct bugdisk_io *));
int bugrtcrd	__P((struct bugrtc *));
int bugreturn	__P((void));
int bugbrdid	__P((struct bugbrdid *));
int bugnetctrl	__P((struct bugniocall *));
