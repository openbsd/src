/*	$OpenBSD: stub.c,v 1.2 1996/08/23 20:54:17 niklas Exp $	*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/lkm.h>

cdev_decl(ipl);
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
