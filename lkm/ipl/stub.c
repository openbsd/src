/*	$OpenBSD: stub.c,v 1.3 1997/11/23 05:19:06 mickey Exp $	*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/lkm.h>

static struct cdevsw	ipl_cdevsw = cdev_gen_ipf(1,ipl);
MOD_DEV("ipl", LM_DT_CHAR, -1, &ipl_cdevsw );

static int
lkm_load( lkmtp, cmd )
	struct lkm_table *lkmtp;
	int	cmd;
{
	return 0;
}

int
ipl(lkmtp, cmd, ver)
	struct lkm_table *lkmtp;	
	int cmd;
	int ver;
{
	DISPATCH(lkmtp, cmd, ver, lkm_load, lkm_nofunc, lkm_nofunc)
}
