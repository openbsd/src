/*	$OpenBSD: acpi.c,v 1.6 2005/12/14 04:16:25 marco Exp $	*/
/*
 * Copyright (c) 2005 Thorsten Lockert <tholo@sigmasoft.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/event.h>

#include <machine/conf.h>
#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#ifdef ACPI_DEBUG
int acpi_debug = 20;
#endif

int	acpimatch(struct device *, void *, void *);
void	acpiattach(struct device *, struct device *, void *);
int	acpi_submatch(struct device *, void *, void *);
int	acpi_print(void *, const char *);
int	acpi_loadtables(struct acpi_softc *, struct acpi_rsdp *);
void	acpi_load_table(paddr_t, size_t, acpi_qhead_t *);
void	acpi_load_dsdt(paddr_t, struct acpi_q **);
void	acpi_softintr(void *);
void	acpi_filtdetach(struct knote *);
int	acpi_filtread(struct knote *, long);
void	acpi_foundhid(struct aml_node *, void *);

#define	ACPI_LOCK(sc)
#define	ACPI_UNLOCK(sc)

/* XXX move this into dsdt softc at some point */
extern struct	aml_node aml_root;

struct filterops acpiread_filtops = {
	1, NULL, acpi_filtdetach, acpi_filtread
};

struct cfattach acpi_ca = {
	sizeof(struct acpi_softc), acpimatch, acpiattach
};

struct cfdriver acpi_cd = {
	NULL, "acpi", DV_DULL
};

int acpi_evindex;
struct acpi_softc *acpi_softc;

#if 0
void
acpi_read_pm1_status(struct acpi_softc *sc, uint32_t *status_a, uint32_t *status_b)
{
  *status_a = bus_space_read_2(sc->sc_iot, sc->sc_ioh_pm1a_evt, ACPI_PM1_STATUS);
  *status_b = 0;
}
void
acpi_write_pm1_status(struct acpi_softc *sc, uint32_t status_a, uint32_t status_b)
{
  bus_space_write_2(sc->sc_iot, sc->sc_ioh_pm1a_evt, ACPI_PM1_STATUS, status_a);
}
void
acpi_read_pm1_enable(struct acpi_softc *sc, uint32_t *enable_a, uint32_t *enable_b)
{
  *status_a = bus_space_read_2(sc->sc_iot, sc->sc_ioh_pm1a_evt, ACPI_PM1_STATUS);
  *status_b = 0;
}
void
acpi_write_pm1_enable(struct acpi_softc *sc, uint32_t enable_a, uint32_t enable_b)
{
  bus_space_write_2(sc->sc_iot, sc->sc_ioh_pm1a_evt, ACPI_PM1_STATUS, status_a);
}

void
acpi_read_gpe_status(struct acpi_softc *sc, uint32_t *status_0, uint32_t *status_1)
{
}
void
acpi_write_gpe_status(struct acpi_softc *sc, uint32_t status_0, uint32_t status_1)
{
}
void
acpi_read_gpe_enable(struct acpi_softc *sc, uint32_t *enable_0, uint32_t *enable_1)
{
}
void
acpi_write_gpe_enable(struct acpi_softc *sc, uint32_t enable_0, uint32_t enable_1)
{
}
#endif

void
acpi_foundhid(struct aml_node *node, void *arg)
{
	struct acpi_softc	*sc = (struct acpi_softc *)arg;
	struct device		*self = (struct device *)arg;
	const char		*dev;

	dnprintf(10, "found hid device: %s ", node->parent->name);
	switch(node->child->value.type) {
	case AML_OBJTYPE_STRING:
		dev = node->child->value.v_string;
		break;
	case AML_OBJTYPE_INTEGER:
		dev = aml_eisaid(node->child->value.v_integer);
		break;
	default:
		dev = "unknown";
		break;
	}
	dnprintf(10, "  device: %s\n", dev);

	if (!strcmp(dev, ACPI_DEV_AC)) {
		struct acpi_attach_args aaa;

		memset(&aaa, 0, sizeof(aaa));
		aaa.aaa_name = "acpiac";
		aaa.aaa_iot = sc->sc_iot;
		aaa.aaa_memt = sc->sc_memt;
		config_found(self, &aaa, acpi_print);
	} else if (!strcmp(dev, ACPI_DEV_CMB)) {
		struct acpi_attach_args aaa;

		memset(&aaa, 0, sizeof(aaa));
		aaa.aaa_name = "acpibat";
		aaa.aaa_iot = sc->sc_iot;
		aaa.aaa_memt = sc->sc_memt;
		config_found(self, &aaa, acpi_print);
	}
}

int
acpimatch(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	/* sanity */
	if (strcmp(aaa->aaa_name, cf->cf_driver->cd_name))
		return (0);

	if (!acpi_probe(parent, cf, aaa))
		return (0);

	return (1);
}

void
acpiattach(struct device *parent, struct device *self, void *aux)
{
#ifdef ACPI_ENABLE
	bus_space_handle_t ioh;
#endif
	struct acpi_attach_args *aaa = aux;
	struct acpi_softc *sc = (struct acpi_softc *)self;
	struct acpi_mem_map handle;
	struct acpi_rsdp *rsdp;
	struct acpi_q *entry;
	paddr_t facspa;

	sc->sc_iot = aaa->aaa_iot;
	sc->sc_memt = aaa->aaa_memt;

	printf(": ");
	if (acpi_map(aaa->aaa_pbase, sizeof(struct acpi_rsdp), &handle))
		goto fail;

	rsdp = (struct acpi_rsdp *)handle.va;
	printf("revision %d ", (int)rsdp->rsdp_revision);

	SIMPLEQ_INIT(&sc->sc_tables);

	sc->sc_fadt = NULL;
	sc->sc_facs = NULL;
	sc->sc_powerbtn = 0;
	sc->sc_sleepbtn = 0;

	sc->sc_note = malloc(sizeof(struct klist), M_DEVBUF, M_NOWAIT);
	memset(sc->sc_note, 0, sizeof(struct klist));

	if (acpi_loadtables(sc, rsdp)) {
		acpi_unmap(&handle);
		return;
	}

	acpi_unmap(&handle);

	/*
	 * Find the FADT
	 */
	SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
		if (memcmp(entry->q_table, FADT_SIG, sizeof(FADT_SIG) - 1) == 0) {
			sc->sc_fadt = entry->q_table;
			break;
		}
	}
	if (sc->sc_fadt == NULL)
		goto fail;

	/*
	 * Check if we are able to enable ACPI control
	 */
	if (!sc->sc_fadt->smi_cmd ||
	    (!sc->sc_fadt->acpi_enable && !sc->sc_fadt->acpi_disable))
		goto fail;

	/*
	 * Load the DSDT from the FADT pointer -- use the
	 * extended (64-bit) pointer if it exists
	 */
	if (sc->sc_fadt->hdr_revision < 3 || sc->sc_fadt->x_dsdt == 0)
		acpi_load_dsdt(sc->sc_fadt->dsdt, &entry);
	else
		acpi_load_dsdt(sc->sc_fadt->x_dsdt, &entry);

	if (entry == NULL)
		printf("!DSDT ");
	SIMPLEQ_INSERT_HEAD(&sc->sc_tables, entry, q_next);

	/*
	 * Set up a pointer to the firmware control structure
	 */
	if (sc->sc_fadt->hdr_revision < 3 || sc->sc_fadt->x_firmware_ctl == 0)
		facspa = sc->sc_fadt->firmware_ctl;
	else
		facspa = sc->sc_fadt->x_firmware_ctl;

	if (acpi_map(facspa, sizeof(struct acpi_facs), &handle))
		printf("!FACS ");
	else
		sc->sc_facs = (struct acpi_facs *)handle.va;

	/*
	 * Take over ACPI control.  Note that once we do this, we
	 * effectively tell the system that we have ownership of
	 * the ACPI hardware registers, and that SMI should leave
	 * them alone
	 *
	 * This may prevent thermal control on some systems where
	 * that actually does work
	 */
#ifdef ACPI_ENABLE
	bus_space_map(sc->sc_iot, sc->sc_fadt->smi_cmd, 1, 0, &ioh);
	bus_space_write_1(sc->sc_iot, ioh, 0, sc->sc_fadt->acpi_enable);
	bus_space_unmap(sc->sc_iot, ioh, 1);
#endif

	bus_space_map(sc->sc_iot,
		      sc->sc_fadt->pm1a_evt_blk, sc->sc_fadt->pm1_evt_len,
		      0, &sc->sc_ioh_pm1a_evt);

#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	sc->sc_softih = softintr_establish(IPL_TTY, acpi_softintr, sc);
#else
	timeout_set(&sc->sc_timeout, acpi_softintr, sc);
#endif
	acpi_attach_machdep(sc);

	/*
	 * If we have an interrupt handler, we can get notification
	 * when certain status bits changes in the ACPI registers,
	 * so let us enable some events we can forward to userland
	 */
	if (sc->sc_interrupt) {
		int16_t flags;

#if 0
		acpi_read_pm1_enable(sc, &ena, &enb);
		ena |= (ACPI_PM1_PWRBTN_EN|ACPI_PM1_SLPBTN_EN);
		enb |= (ACPI_PM1_PWRBTN_EN|ACPI_PM1_SLPBTN_EN);
		acpi_write_pm1_enable(sc, ena, enb);
#else
		flags = bus_space_read_2(sc->sc_iot, sc->sc_ioh_pm1a_evt,
					 sc->sc_fadt->pm1_evt_len / 2);
		flags |= ACPI_PM1_PWRBTN_EN | ACPI_PM1_SLPBTN_EN;
		bus_space_write_2(sc->sc_iot, sc->sc_ioh_pm1a_evt,
				  sc->sc_fadt->pm1_evt_len / 2, flags);
#endif
	}

	printf("attached\n");

	/*
	 * ACPI is enabled now -- attach timer
	 */
	{
		struct acpi_attach_args aaa;

		memset(&aaa, 0, sizeof(aaa));
		aaa.aaa_name = "acpitimer";
		aaa.aaa_iot = sc->sc_iot;
		aaa.aaa_memt = sc->sc_memt;
#if 0
		aaa.aaa_pcit = sc->sc_pcit;
		aaa.aaa_smbust = sc->sc_smbust;
#endif
		config_found(self, &aaa, acpi_print);
	}

	/*
	 * Attach table-defined devices
	 */
	SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
		struct acpi_attach_args aaa;

		memset(&aaa, 0, sizeof(aaa));
		aaa.aaa_iot = sc->sc_iot;
		aaa.aaa_memt = sc->sc_memt;
#if 0
		aaa.aaa_pcit = sc->sc_pcit;
		aaa.aaa_smbust = sc->sc_smbust;
#endif
		aaa.aaa_table = entry->q_table;

		config_found_sm(self, &aaa, acpi_print, acpi_submatch);
	}

	acpi_softc = sc;

	/* attach devices found in dsdt */
	aml_find_node(aml_root.child, "_HID", acpi_foundhid, sc);

	return;

fail:
	printf(" failed attach\n");
}

int
acpi_submatch(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = (struct acpi_attach_args *)aux;
	struct cfdata *cf = match;

	if (aaa->aaa_table == NULL)
		return (0);
	return ((*cf->cf_attach->ca_match)(parent, match, aux));
}

int
acpi_print(void *aux, const char *pnp)
{
	/* XXX ACPIVERBOSE should be replaced with dnprintf */
	struct acpi_attach_args *aa = aux;
#ifdef ACPIVERBOSE
	struct acpi_table_header *hdr =
	    (struct acpi_table_header *)aa->aaa_table;
#endif

	if (pnp) {
		if (aa->aaa_name)
			printf("%s at %s", aa->aaa_name, pnp);
#ifdef ACPIVERBOSE
		else
			printf("acpi device at %s from", pnp);
#endif
	}
#ifdef ACPIVERBOSE
	if (hdr)
		printf(" table %c%c%c%c",
		    hdr->signature[0], hdr->signature[1],
		    hdr->signature[2], hdr->signature[3]);
#endif

	return (UNCONF);
}

int
acpi_loadtables(struct acpi_softc *sc, struct acpi_rsdp *rsdp)
{
	struct acpi_mem_map hrsdt, handle;
	struct acpi_table_header *hdr;
	int i, ntables;
	size_t len;

	if (rsdp->rsdp_revision == 2) {
		struct acpi_xsdt *xsdt;

		if (acpi_map(rsdp->rsdp_xsdt, sizeof(*hdr), &handle)) {
			printf("couldn't map rsdt\n");
			return (ENOMEM);
		}

		hdr = (struct acpi_table_header *)handle.va;
		len = hdr->length;
		acpi_unmap(&handle);
		hdr = NULL;

		acpi_map(rsdp->rsdp_xsdt, len, &hrsdt);
		xsdt = (struct acpi_xsdt *)hrsdt.va;

		ntables = (len - sizeof(struct acpi_table_header)) /
			  sizeof(xsdt->table_offsets[0]);

		for (i = 0; i < ntables; i++) {
			acpi_map(xsdt->table_offsets[i], sizeof(*hdr), &handle);
			hdr = (struct acpi_table_header *)handle.va;
			acpi_load_table(xsdt->table_offsets[i], hdr->length,
			    &sc->sc_tables);
			acpi_unmap(&handle);
		}
		acpi_unmap(&hrsdt);
	} else {
		struct acpi_rsdt *rsdt;

		if (acpi_map(rsdp->rsdp_rsdt, sizeof(*hdr), &handle)) {
			printf("couldn't map rsdt\n");
			return (ENOMEM);
		}

		hdr = (struct acpi_table_header *)handle.va;
		len = hdr->length;
		acpi_unmap(&handle);
		hdr = NULL;

		acpi_map(rsdp->rsdp_rsdt, len, &hrsdt);
		rsdt = (struct acpi_rsdt *)hrsdt.va;

		ntables = (len - sizeof(struct acpi_table_header)) /
		    sizeof(rsdt->table_offsets[0]);

		for (i = 0; i < ntables; i++) {
			acpi_map(rsdt->table_offsets[i], sizeof(*hdr), &handle);
			hdr = (struct acpi_table_header *)handle.va;
			acpi_load_table(rsdt->table_offsets[i], hdr->length,
			    &sc->sc_tables);
			acpi_unmap(&handle);
		}
		acpi_unmap(&hrsdt);
	}

	return (0);
}

void
acpi_load_table(paddr_t pa, size_t len, acpi_qhead_t *queue)
{
	struct acpi_mem_map handle;
	struct acpi_q *entry;

	entry = malloc(len + sizeof(struct acpi_q), M_DEVBUF, M_NOWAIT);

	if (entry != NULL) {
		if (acpi_map(pa, len, &handle)) {
			free(entry, M_DEVBUF);
			return;
		}
		memcpy(entry->q_data, handle.va, len);
		entry->q_table = entry->q_data;
		acpi_unmap(&handle);
		SIMPLEQ_INSERT_TAIL(queue, entry, q_next);
	}
}

void
acpi_load_dsdt(paddr_t pa, struct acpi_q **dsdt)
{
	struct acpi_mem_map handle;
	struct acpi_table_header *hdr;
	size_t len;

	if (acpi_map(pa, sizeof(*hdr), &handle))
		return;
	hdr = (struct acpi_table_header *)handle.va;
	len = hdr->length;
	acpi_unmap(&handle);

	*dsdt = malloc(len + sizeof(struct acpi_q), M_DEVBUF, M_NOWAIT);

	if (*dsdt != NULL) {
		if (acpi_map(pa, len, &handle)) {
			free(*dsdt, M_DEVBUF);
			*dsdt = NULL;
			return;
		}
		memcpy((*dsdt)->q_data, handle.va, len);
		(*dsdt)->q_table = (*dsdt)->q_data;
		acpi_unmap(&handle);
	}
}

int
acpi_interrupt(void *arg)
{
	struct acpi_softc *sc = (struct acpi_softc *)arg;
	u_int16_t flags;

#if 0
	acpi_read_pm1_status(sc, &sts_a, &sts_b);
	if ((sts_a & ACPI_PM1_PWRBTN_STS) && (sts_b & ACPI_PM1_PWRBTN_STS) {
		acpi_write_pm1_status(sc, 
#else
	flags = bus_space_read_2(sc->sc_iot, sc->sc_ioh_pm1a_evt,
	    ACPI_PM1_STATUS);
	if (flags & (ACPI_PM1_PWRBTN_STS | ACPI_PM1_SLPBTN_STS)) {
		if (flags & ACPI_PM1_PWRBTN_STS) {
			bus_space_write_2(sc->sc_iot, sc->sc_ioh_pm1a_evt,
			    ACPI_PM1_STATUS, ACPI_PM1_PWRBTN_STS);
			/*
			 * Power-button has been pressed, do something!
			 */
			sc->sc_powerbtn = 1;
		}
		if (flags & ACPI_PM1_SLPBTN_STS) {
			bus_space_write_2(sc->sc_iot, sc->sc_ioh_pm1a_evt,
			    ACPI_PM1_STATUS, ACPI_PM1_SLPBTN_STS);
			/*
			 * Sleep-button has been pressed, do something!
			 */
			sc->sc_sleepbtn = 1;
		}
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
		softintr_schedule(sc->sc_softih);
#else
		if (!timeout_pending(&sc->sc_timeout))
			timeout_add(&sc->sc_timeout, 0);
#endif
		return (1);
	}
#endif
	return (0);
}

void
acpi_softintr(void *arg)
{
	struct acpi_softc *sc = arg;

	if (sc->sc_powerbtn) {
		sc->sc_powerbtn = 0;
		acpi_evindex++;
		KNOTE(sc->sc_note, ACPI_EVENT_COMPOSE(ACPI_EV_PWRBTN,
		    acpi_evindex));
	}
	if (sc->sc_sleepbtn) {
		sc->sc_sleepbtn = 0;
		acpi_evindex++;
		KNOTE(sc->sc_note, ACPI_EVENT_COMPOSE(ACPI_EV_SLPBTN,
		    acpi_evindex));
	}
}

int
acpiopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct acpi_softc *sc;
	int error = 0;

	if (!acpi_cd.cd_ndevs || minor(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[minor(dev)]))
		return (ENXIO);

	if (!(flag & FREAD) || (flag & FWRITE))
		error = EINVAL;

	return (error);
}

void
acpi_enter_sleep_state(struct acpi_softc *sc, int state)
{
#ifdef ACPI_ENABLE
	bus_space_handle_t ioh;
	u_int16_t bits;

	bus_space_map(sc->sc_iot,
		      sc->sc_fadt->pm1a_cnt_blk, sc->sc_fadt->pm1_cnt_len,
		      0, &ioh);
	bits = bus_space_read_2(sc->sc_iot, ioh, 0);
	bits |= state << 10;	/* XXX This is sick and wrong and illegal! */
	bus_space_write_2(sc->sc_iot, ioh, 0, bits);
	bits |= ACPI_PM1_SLP_EN;
	bus_space_write_2(sc->sc_iot, ioh, 0, bits);
	bus_space_unmap(sc->sc_iot, ioh, sc->sc_fadt->pm1_cnt_len);
#endif
}

int
acpiclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct acpi_softc *sc;

	if (!acpi_cd.cd_ndevs || minor(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[minor(dev)]))
		return (ENXIO);

	return (0);
}

int
acpiioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct acpi_softc *sc;
	int error = 0;

	if (!acpi_cd.cd_ndevs || minor(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[minor(dev)]))
		return (ENXIO);

	ACPI_LOCK(sc);
	switch (cmd) {
	case ACPI_IOC_GETFACS:
		if (suser(p, 0) != 0)
			error = EPERM;
		else {
			struct acpi_facs *facs = (struct acpi_facs *)data;

			bcopy(sc->sc_facs, facs, sc->sc_facs->length);
		}
		break;

	case ACPI_IOC_GETTABLE:
		if (suser(p, 0) != 0)
			error = EPERM;
		else {
			struct acpi_table *table = (struct acpi_table *)data;
			struct acpi_table_header *hdr;
			struct acpi_q *entry;

			error = ENOENT;
			SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
				if (table->offset-- == 0) {
					hdr = (struct acpi_table_header *)entry->q_table;
					if (table->table == NULL) {
						table->size = hdr->length;
						error = 0;
					} else if (hdr->length > table->size)
						error = ENOSPC;
					else
						error = copyout(hdr,
						    table->table, hdr->length);
					break;
				}
			}
		}
		break;

	default:
		error = ENOTTY;
	}

	ACPI_UNLOCK(sc);
	return (error);
}

void
acpi_filtdetach(struct knote *kn)
{
	struct acpi_softc *sc = kn->kn_hook;

	ACPI_LOCK(sc);
	SLIST_REMOVE(sc->sc_note, kn, knote, kn_selnext);
	ACPI_UNLOCK(sc);
}

int
acpi_filtread(struct knote *kn, long hint)
{
	/* XXX weird kqueue_scan() semantics */
	if (hint & !kn->kn_data)
		kn->kn_data = hint;

	return(1);
}

int
acpikqfilter(dev_t dev, struct knote *kn)
{
	struct acpi_softc *sc;

	if (!acpi_cd.cd_ndevs || minor(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[minor(dev)]))
		return (ENXIO);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &acpiread_filtops;
		break;
	default:
		return (1);
	}

	kn->kn_hook = sc;

	ACPI_LOCK(sc);
	SLIST_INSERT_HEAD(sc->sc_note, kn, kn_selnext);
	ACPI_UNLOCK(sc);

	return (0);
}
