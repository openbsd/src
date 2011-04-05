/*	$OpenBSD: sysarch.h,v 1.8 2011/04/05 21:14:00 guenther Exp $	*/
/*	$NetBSD: sysarch.h,v 1.1 2003/04/26 18:39:48 fvdl Exp $	*/

#ifndef _MACHINE_SYSARCH_H_
#define _MACHINE_SYSARCH_H_

/*
 * Architecture specific syscalls (amd64)
 */
#define	AMD64_IOPL		2
#define	AMD64_GET_IOPERM	3
#define	AMD64_SET_IOPERM	4
#define	AMD64_VM86		5
#define	AMD64_PMC_INFO		8
#define	AMD64_PMC_STARTSTOP	9
#define	AMD64_PMC_READ		10
#define	AMD64_GET_FSBASE	11
#define	AMD64_SET_FSBASE	12

struct amd64_iopl_args {
	int iopl;
};

struct amd64_get_ioperm_args {
	u_long *iomap;
};

struct amd64_set_ioperm_args {
	u_long *iomap;
};

struct amd64_pmc_info_args {
	int	type;
	int	flags;
};

#define	PMC_TYPE_NONE		0
#define	PMC_TYPE_I586		1
#define	PMC_TYPE_I686		2

#define	PMC_INFO_HASTSC		0x01

#define	PMC_NCOUNTERS		2

struct amd64_pmc_startstop_args {
	int counter;
	u_int64_t val;
	u_int8_t event;
	u_int8_t unit;
	u_int8_t compare;
	u_int8_t flags;
};

#define	PMC_SETUP_KERNEL	0x01
#define	PMC_SETUP_USER		0x02
#define	PMC_SETUP_EDGE		0x04
#define	PMC_SETUP_INV		0x08

struct amd64_pmc_read_args {
	int counter;
	u_int64_t val;
	u_int64_t time;
};


#ifdef _KERNEL
int amd64_iopl(struct proc *, void *, register_t *);
int amd64_set_fsbase(struct proc *, void *);
int amd64_get_fsbase(struct proc *, void *);
#else
int amd64_iopl(int);
int amd64_get_ioperm(u_long *);
int amd64_set_ioperm(u_long *);
int amd64_pmc_info(struct amd64_pmc_info_args *);
int amd64_pmc_startstop(struct amd64_pmc_startstop_args *);
int amd64_pmc_read(struct amd64_pmc_read_args *);
int amd64_set_fsbase(void *);
int amd64_get_fsbase(void **);
int sysarch(int, void *);
#endif

#endif /* !_MACHINE_SYSARCH_H_ */
