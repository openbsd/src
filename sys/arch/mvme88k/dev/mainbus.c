#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disklabel.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>

void mbattach __P((struct device *, struct device *, void *));
int mbprint __P((void *, const char *));
int mbmatch __P((struct device *, void *, void *));
int submatch __P((struct device *, void *, void *));

/* 
 * mainbus driver 
 */

struct cfattach mainbus_ca = {
	sizeof(struct device), mbmatch, mbattach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL, 0
};

int
mbmatch(struct device *pdp, void *self, void *auxp)
{
	struct cfdata *cfp = self;

	if (cfp->cf_unit > 0)
		return(0);
	/*
	 * We are always here
	 */
	return(1);
}

/*
 * "find" all the things that should be there.
 */
void
mbattach(struct device *pdp, struct device *dp, void *auxp)
{
	struct cfdata *cf;
	extern int cputyp;

	/* nothing to do for this bus */
	printf (" machine type %x\n", cputyp);

	if ((cf = config_search(submatch, dp, auxp)) != NULL) {
		return;
	}

}

int
mbprint(void *auxp, const char *pnp)
{
	if (pnp)
		printf("%s at %s", (char *)auxp, pnp);
	return(UNCONF);
}

int
submatch(struct device *parent, void *self, void *aux)
{
	struct confargs *ca = aux;
	struct cfdata   *cf = self;

	ca->ca_bustype = BUS_MAIN;
	ca->ca_paddr = (caddr_t)cf->cf_loc[0];
	ca->ca_size = cf->cf_loc[1];

	if (!(*cf->cf_attach->ca_match)(parent, cf, ca)) {
		if (!parent)
			cf->cf_fstate = FSTATE_FOUND;

		return 0;
	}

	/*
	 * mapin the device memory of the child and call attach.
	 */
#if 0
	ca->ca_vaddr = (caddr_t)iomap_mapin(ca->ca_paddr, ca->ca_size, 1);
#endif /* 0 */
	ca->ca_vaddr = ca->ca_paddr;

	config_attach(parent, cf, ca, mbprint);

	return 1;
}
