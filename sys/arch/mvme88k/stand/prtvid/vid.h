/*	$OpenBSD: vid.h,v 1.1 2006/05/18 06:10:07 miod Exp $	*/

#ifndef __MACHINE_VID_H__
#define __MACHINE_VID_H__

#define START_BLOCK 1
#define LOADER_SIZE 2
#define LOADER_ADDRESS 0x1F0000

#ifndef _LOCORE
struct vid {
	unsigned char	vid_id[4];
	unsigned char	vid_0[16];
	unsigned int	vid_oss;
	unsigned short	vid_osl;
	unsigned char	vid_1[4];
	unsigned short	vid_osa_u;
	unsigned short	vid_osa_l;
	unsigned char	vid_2[4];
	unsigned char	vid_vd[20];
	unsigned char	vid_3[86];
	unsigned int	vid_cas;
	unsigned char	vid_cal;
	unsigned char	vid_4[99];
	unsigned char	vid_mot[8];
};
struct cfg {

	unsigned char	cfg_0[4];
	unsigned short	cfg_atm;
	unsigned short	cfg_prm;
	unsigned short	cfg_atw;
	unsigned short	cfg_rec;
	unsigned char	cfg_1[12];
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
	unsigned char	cfg_4[196];
};
#endif
#endif /* __MACHINE_VID_H__ */
