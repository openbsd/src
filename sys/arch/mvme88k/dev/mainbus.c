/*	$OpenBSD: mainbus.c,v 1.9 2003/09/16 20:46:08 miod Exp $ */
/*  Copyright (c) 1998 Steve Murphree, Jr. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disklabel.h>

#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>

void mainbus_attach(struct device *, struct device *, void *);
int  mainbus_match(struct device *, void *, void *);
int mainbus_print(void *, const char *);
int mainbus_scan(struct device *, void *, void *);

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL, 0
};

int
mainbus_match(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	return (1);
}

int
mainbus_print(args, bus)
	void *args;
	const char *bus;
{
	struct confargs *ca = args;

	if (ca->ca_paddr != (void *)-1)
		printf(" addr 0x%x", (u_int32_t)ca->ca_paddr);
	return (UNCONF);
}

int
mainbus_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct confargs oca;

	bzero(&oca, sizeof oca);
	oca.ca_paddr = (void *)cf->cf_loc[0];
	oca.ca_vaddr = (void *)-1;
	oca.ca_ipl = -1;
	oca.ca_bustype = BUS_MAIN;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);
	config_attach(parent, cf, &oca, mainbus_print);
	return (1);
}

void
mainbus_attach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	extern char cpu_model[];

	printf(": %s\n", cpu_model);

	/*
	 * Display cpu/mmu details. Only for the master CPU so far.
	 */
	cpu_configuration_print(1);

	/* XXX
	 * should have a please-attach-first list for mainbus,
	 * to ensure that the pcc/vme2/mcc chips are attached
	 * first.
	 */

	(void)config_search(mainbus_scan, self, args);
}
