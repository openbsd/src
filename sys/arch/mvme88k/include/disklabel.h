#ifndef _MACHINE_DISKLABEL_H_
#define _MACHINE_DISKLABEL_H_

#define MAXPARTITIONS	16

/* number of boot pieces , ie xxboot bootxx */
#define NUMBOOT		2

#define RAW_PART	2		/* Xd0c is raw part. */

/* 
 * used to encode disk minor numbers
 * this should probably be moved to sys/disklabel.h
 */
#define DISKUNIT(dev)	(minor(dev) / MAXPARTITIONS)
#define DISKPART(dev)	(minor(dev) % MAXPARTITIONS)
#define MAKEDISKDEV(maj, unit, part) \
    (makedev((maj), ((unit) * MAXPARTITIONS) + (part)))

struct cpu_disklabel {
	/* VID */
	unsigned char	vid_id[4];
	unsigned char	vid_0[16];
	unsigned int	vid_oss;
	unsigned short	vid_osl;
	unsigned char	vid_1[4];
	unsigned short	vid_osa_u;
	unsigned short	vid_osa_l;
	unsigned char	vid_2[2];
	unsigned short	partitions;
	unsigned char	vid_vd[16];
	unsigned long	bbsize;
	unsigned long	magic1;		/* 4 */
	unsigned short	type;		/* 2 */
	unsigned short	subtype;		/* 2 */
	unsigned char	packname[16];	/* 16 */
	unsigned long	flags;		/* 4 */
	unsigned long	drivedata[5];	/* 4 */
	unsigned long	spare[5];		/* 4 */
	unsigned short	checksum;		/* 2 */

	unsigned long	secpercyl;	/* 4 */
	unsigned long	secperunit;	/* 4 */
	unsigned long	headswitch;	/* 4 */

	unsigned char	vid_3[4];
	unsigned int	vid_cas;
	unsigned char	vid_cal;
	unsigned char	vid_4_0[3];
	unsigned char	vid_4[64];
	unsigned char	vid_4_1[28];
	unsigned long	sbsize;
	unsigned char	vid_mot[8];
	/* CFG */
	unsigned char	cfg_0[4];
	unsigned short	cfg_atm;
	unsigned short	cfg_prm;
	unsigned short	cfg_atw;
	unsigned short	cfg_rec;

	unsigned short	sparespertrack;
	unsigned short	sparespercyl;
	unsigned long	acylinders;
	unsigned short	rpm;
	unsigned short	cylskew;

	unsigned char	cfg_spt;
	unsigned char	cfg_hds;
	unsigned short	cfg_trk;
	unsigned char	cfg_ilv;
	unsigned char	cfg_sof;
	unsigned short	cfg_psm;
	unsigned short	cfg_shd;
	unsigned char	cfg_2[2];
	unsigned short	cfg_pcom;
	unsigned char	cfg_3;
	unsigned char	cfg_ssr;
	unsigned short	cfg_rwcc;
	unsigned short	cfg_ecc;
	unsigned short	cfg_eatm;
	unsigned short	cfg_eprm;
	unsigned short	cfg_eatw;
	unsigned char	cfg_gpb1;
	unsigned char	cfg_gpb2;
	unsigned char	cfg_gpb3;
	unsigned char	cfg_gpb4;
	unsigned char	cfg_ssc;
	unsigned char	cfg_runit;
	unsigned short	cfg_rsvc1;
	unsigned short	cfg_rsvc2;
	unsigned long	magic2;
	unsigned char	cfg_4[192];
};
#endif _MACHINE_DISKLABEL_H_
