#include <sys/param.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>

#include <mvme88k/dev/pcctworeg.h>

struct pcctwosoftc {
	struct device		sc_dev;
	volatile struct pcc2reg *sc_pcc2reg;
};

int	pcctwomatch	__P((struct device *, void *, void *));
int	pcctwoscan	__P((struct device *, void *, void *));
void	pcctwoattach	__P((struct device *, struct device *, void *));

#ifdef MVME187
void	setupiackvectors __P((void));
#endif /* MVME187 */

struct cfattach pcctwo_ca = { 
        sizeof(struct pcctwosoftc), pcctwomatch, pcctwoattach
};

struct cfdriver pcctwo_cd = {
        NULL, "pcctwo", DV_DULL, 0
}; 

/*ARGSUSED*/
int
pcctwomatch(struct device *parent, void *self, void *aux)
{
	int 		ret;
	u_char		id, rev; 
	caddr_t		base;
	struct confargs *ca = aux;
	struct cfdata *cf = self;
	
#if 0
	if (cputyp != CPU_167 && cputyp != CPU_166
#ifdef MVME187
		&& cputyp != CPU_187
#endif
		)
	{
		return 0;
	}
#endif /* 0 */
	if (cputyp != CPU_187) {
		return 0;
	}
	
	/* 
	 * If bus or name do not match, fail.
	 */
	if (ca->ca_bustype != BUS_MAIN ||
		strcmp(cf->cf_driver->cd_name, "pcctwo")) {
		return 0;
	}

	if ((base = (caddr_t)cf->cf_loc[0]) == (caddr_t)-1) {
		return 0;
	}

	id  = badpaddr(base, 1);
	rev = badpaddr(base + 1, 1);

	if (id != PCC2_CHIP_ID || rev != PCC2_CHIP_REV) {
		return 0;
	}

	ca->ca_size = PCC2_SIZE;
	ca->ca_paddr = base;

	return 1;
}

int
pcctwoprint(void *aux, char *parent)
{
	struct confargs *ca = aux;

	/*
	 * We call pcctwoprint() via config_attach(). Parent
	 * will always be null and config_attach() would have already
	 * printed "nvram0 at pcctwo0".
	 */
	printf(" addr %x size %x", ca->ca_paddr, ca->ca_size);
	if (ca->ca_ipl != -1) {
		printf(" ipl %x", ca->ca_ipl);
	}

	return (UNCONF);
}

/*ARGSUSED*/
int
pcctwoscan(struct device *parent, void *self, void *aux)
{
	struct confargs ca;
	struct cfdata *cf = self;
	struct pcctwosoftc *sc = (struct pcctwosoftc *)parent;

	/*
	 * Pcctwoscan gets called by config_search() for each
	 * child of parent (pcctwo) specified in ioconf.c.
	 * Fill in the bus type to be PCCTWO and call the child's
	 * match routine. If the child's match returns 1, then
	 * we need to allocate device memory, set it in confargs
	 * and call config_attach(). This, in turn, will call the
	 * child's attach.
	 */

	ca.ca_bustype = BUS_PCCTWO;
	
	if ((*cf->cf_attach->ca_match)(parent, cf, &ca) == 0)
		return 0;
	
	/*
	 * The child would have fixed up ca to reflect what its
	 * requirements are.
	 */
	
	if (cf->cf_loc[2] != ca.ca_ipl) {
		printf("Changing ipl %x specified in ioconf.c to %x for %s\n",
			cf->cf_loc[2], ca.ca_ipl, cf->cf_driver->cd_name);
	}

	/*
	 * If the size specified by the child is 0, don't map
	 * any IO space, but pass in the address of pcc2reg as vaddr.
	 * This is for clock and parallel port which don't have a
	 * separate address space by themselves but use pcc2's register
	 * block.
	 */
	if (ca.ca_size == 0) {
		/*
		 * pcc2regs addr
		 */
#if 0
		ca.ca_vaddr = ((struct confargs *)aux)->ca_vaddr;
#endif /* 0 */
		ca.ca_vaddr = (caddr_t)sc->sc_pcc2reg;

	} else  {
		ca.ca_vaddr = ca.ca_paddr;
	}

#if 0
	ca.ca_parent = ((struct confargs *)aux)->ca_vaddr;
#endif /* 0 */
	ca.ca_parent = (caddr_t)sc->sc_pcc2reg;

	/*
	 * Call child's attach using config_attach().
	 */
	config_attach(parent, cf, &ca, pcctwoprint);
	return 1;
}

/*
 * This function calls the match routine of the configured children
 * in turn. For each configured child, map the device address into
 * iomap space and then call config_attach() to attach the child.
 */

/* ARGSUSED */
void
pcctwoattach(struct device *parent, struct device *self, void *aux)
{
	struct pcctwosoftc	*sc = (struct pcctwosoftc *)self;
	struct confargs		*ca = aux;
	caddr_t	base;

	if (self->dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}

	base = ca->ca_vaddr;

	printf(": PCCTWO id 0x%2x rev 0x%2x\n",
		*(u_char *)base, *((u_char *)base + 1));

	/*
	 * mainbus driver would have mapped Pcc2 at base. Save
	 * the address in pcctwosoftc.
	 */
	sc->sc_pcc2reg = (struct pcc2reg *)base;

	/*
	 * Set pcc2intr_mask and pcc2intr_ipl.
	 */
	pcc2intr_ipl = (u_char *)&(sc->sc_pcc2reg->pcc2_ipl);
	pcc2intr_mask = (u_char *)&(sc->sc_pcc2reg->pcc2_imask);

#ifdef MVME187
	/*
	 * Get mappings for iack vectors. This doesn't belong here
	 * but is more closely related to pcc than anything I can
	 * think of. (could probably do it in locore.s).
	 */
	
	setupiackvectors();
#endif /* MVME187 */

	(void)config_search(pcctwoscan, self, aux);
}
