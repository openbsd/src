#ifndef _DEV_IC_MPT_IOCTL_H_
#define _DEV_IC_MPT_IOCTL_H_

#include <dev/ic/mpt_mpilib.h>

/* ioctl tunnel defines */
#define MPT_IOCTL_DUMMY _IOWR('B', 32, struct mpt_dummy)
struct mpt_dummy {
	void *cookie;
	int x;
};

/* structures are inside mpt_mpilib.h */
#define MPT_IOCTL_MFG0 _IOWR('B', 33, struct mpt_mfg0)
struct mpt_mfg0 {
	void *cookie;
	fCONFIG_PAGE_MANUFACTURING_0 cpm0;
};

#define MPT_IOCTL_MFG1 _IOWR('B', 34, struct _CONFIG_PAGE_MANUFACTURING_1)
#define MPT_IOCTL_MFG2 _IOWR('B', 35, struct _CONFIG_PAGE_MANUFACTURING_2)
#define MPT_IOCTL_MFG3 _IOWR('B', 36, struct _CONFIG_PAGE_MANUFACTURING_3)
#define MPT_IOCTL_MFG4 _IOWR('B', 37, struct _CONFIG_PAGE_MANUFACTURING_4)

#endif _DEV_IC_MPT_IOCTL_H_
