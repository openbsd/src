#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <machine/cpu.h>

void mbattach __P((struct device *, struct device *, void *));
int mbprint __P((void *, const char *));
int mbmatch __P((struct device *, struct cfdata *, void *));
int submatch( struct device *parent, struct cfdata *self, void *aux);

/* 
 * mainbus driver 
 */
struct cfdriver mainbuscd = {
	NULL, "mainbus", mbmatch, mbattach, 
	DV_DULL, sizeof(struct device), NULL, 0
};

int
mbmatch(pdp, cfp, auxp)
	struct device *pdp;
	struct cfdata *cfp;
	void *auxp;
{
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
mbattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	struct cfdata *cf;
	extern int machineid;

	/* nothing to do for this bus */
	printf (" machine type %x\n", machineid);

	if ((cf = config_search(submatch, dp, auxp)) != NULL) {
		return;
	}

}

mbprint(auxp, pnp)
	void *auxp;
	const char *pnp;
{
	if (pnp)
		printf("%s at %s", (char *)auxp, pnp);
	return(UNCONF);
}

int
submatch(parent, self, aux)
	struct device *parent;
	struct cfdata *self;
	void *aux;
{
	if (!(*self->cf_driver->cd_match)(parent, self, NULL)) {
		/*
		 * STOLEN - BE CAREFUL
		 * If we don't do this, isa_configure() will repeatedly try to
		 * probe devices that weren't found.  But we need to be careful
		 * to do it only for the ISA bus, or we would cause things like
		 * `com0 at ast? slave ?' to not probe on the second ast.
		 */
		if (!parent)
			self->cf_fstate = FSTATE_FOUND;

		return 0;
	}

	config_attach(parent, self, NULL, mbprint);

	return 1;
}
