/*	$OpenBSD: acd_stub.c,v 1.1 1999/07/18 23:21:54 csapuntz Exp $	*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mtio.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/cdio.h>
#include <sys/proc.h>  

int	acdmatch __P((struct device *, void *, void *));
void	acdattach __P((struct device *, struct device *, void *));


struct cfattach acd_ca = {
	sizeof(struct acd_softc), acdmatch, acdattach
};

struct cfdriver acd_cd = {
	NULL, "acd", DV_DISK
};

int
acdmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	return (0);
}

void
acdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	panic("acdattach called");
}



