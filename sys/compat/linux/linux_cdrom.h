/*	$OpenBSD: linux_cdrom.h,v 1.1 1997/12/07 22:59:14 provos Exp $	*/

struct linux_cdrom_blk 
{
	unsigned from;
	unsigned short len;
};


struct linux_cdrom_msf 
{
	u_char	cdmsf_min0;	/* start */
	u_char	cdmsf_sec0;
	u_char	cdmsf_frame0;
	u_char	cdmsf_min1;	/* end */
	u_char	cdmsf_sec1;
	u_char	cdmsf_frame1;
};

struct linux_cdrom_ti 
{
	u_char	cdti_trk0;	/* start */
	u_char	cdti_ind0;
	u_char	cdti_trk1;	/* end */
	u_char	cdti_ind1;
};

struct linux_cdrom_tochdr 	
{
	u_char	cdth_trk0;	/* start */
	u_char	cdth_trk1;	/* end */
};

struct linux_cdrom_msf0
{
	u_char	minute;
	u_char	second;
	u_char	frame;
};

union linux_cdrom_addr
{
	struct linux_cdrom_msf0	msf;
	int			lba;
};

struct linux_cdrom_tocentry 
{
	u_char	cdte_track;
	u_char	cdte_adr	:4;
	u_char	cdte_ctrl	:4;
	u_char	cdte_format;
	union linux_cdrom_addr cdte_addr;
	u_char	cdte_datamode;
};

#define	LINUX_CDROM_LBA 0x01
#define	LINUX_CDROM_MSF 0x02

#define	LINUX_CDROM_DATA_TRACK	0x04

#define	LINUX_CDROM_LEADOUT	0xAA

struct linux_cdrom_subchnl 
{
	u_char	cdsc_format;
	u_char	cdsc_audiostatus;
	u_char	cdsc_adr:	4;
	u_char	cdsc_ctrl:	4;
	u_char	cdsc_trk;
	u_char	cdsc_ind;
	union linux_cdrom_addr cdsc_absaddr;
	union linux_cdrom_addr cdsc_reladdr;
};

struct linux_cdrom_mcn {
  u_char medium_catalog_number[14];
};


struct linux_cdrom_volctrl
{
	u_char	channel0;
	u_char	channel1;
	u_char	channel2;
	u_char	channel3;
};

struct linux_cdrom_read      
{
	int	cdread_lba;
	caddr_t	cdread_bufaddr;
	int	cdread_buflen;
};

#define LINUX_CDROMPAUSE		0x5301
#define LINUX_CDROMRESUME		0x5302
#define LINUX_CDROMPLAYMSF		0x5303
#define LINUX_CDROMPLAYTRKIND		0x5304

#define LINUX_CDROMREADTOCHDR		0x5305
#define LINUX_CDROMREADTOCENTRY	        0x5306

#define LINUX_CDROMSTOP		        0x5307
#define LINUX_CDROMSTART		0x5308

#define LINUX_CDROMEJECT		0x5309

#define LINUX_CDROMVOLCTRL		0x530a

#define LINUX_CDROMSUBCHNL		0x530b

#define LINUX_CDROMREADMODE2		0x530c
#define LINUX_CDROMREADMODE1		0x530d
#define LINUX_CDROMREADAUDIO		0x530e

#define LINUX_CDROMEJECT_SW		0x530f
 
#define LINUX_CDROMMULTISESSION	        0x5310

#define LINUX_CDROM_GET_UPC		0x5311

#define LINUX_CDROMRESET		0x5312
#define LINUX_CDROMVOLREAD		0x5313

#define LINUX_CDROMPLAYBLK		0x5317
