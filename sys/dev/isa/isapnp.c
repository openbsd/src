/*	$OpenBSD: isapnp.c,v 1.10 1997/07/12 23:22:01 weingart Exp $	*/

/*
 * Copyright (c) 1996, Shawn Hsiao <shawn@alpha.secc.fju.edu.tw>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Note: Most of the basic code was originally written by Sujal M. Patel,
 *       plus some code takes from his pnpinfo(8).
 */
/*
 * Copyright (c) 1996, Sujal M. Patel
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/extent.h>
#include <machine/bus.h>

#include <dev/isa/isavar.h>

#include <dev/isa/isapnpreg.h>
#include <dev/isa/isapnpvar.h>
#include <dev/isa/pnpdevs.h>

#define SEND(d, r)	{ bus_space_write_1(sc->iot, sc->addrh, 0, d); \
                          bus_space_write_1(sc->iot, sc->wdh,   0, r); }

int isapnpmatch __P((struct device *, void *, void *));
void isapnpattach __P((struct device *, struct device *, void *));
int isapnpprint __P((void *aux, const char *pnp));
int isapnpsubmatch __P((struct device *parent, void *match, void *aux));

/* XXX */
struct emap {
	int whatever;
};

void add_extent __P((struct emap *, long, int));
struct emap *find_emap __P((char *));
int probe_extent __P((struct emap *, int, int));

struct isapnp_softc {
	struct device sc_dev;
	struct device *parent;

	bus_space_tag_t iot;
	bus_space_handle_t addrh;
	bus_space_handle_t wdh;
	bus_space_handle_t rdh;

	int rd_port;

	TAILQ_HEAD(, cardinfo) q_card;
};

struct cfattach isapnp_ca = {
	sizeof(struct isapnp_softc), isapnpmatch, isapnpattach
};

struct cfdriver isapnp_cd = {
	NULL, "isapnp", DV_DULL, 1
};

int isapnpquery __P((struct isapnp_softc *, u_int32_t,
	struct isa_attach_args *));
void send_Initiation_LFSR __P((struct isapnp_softc *));
int get_serial __P((struct isapnp_softc *, unsigned char *));
int isolation_protocol __P((struct isapnp_softc *));
void read_config __P((struct isapnp_softc *, struct cardinfo *, int));
int get_resource_info __P((struct isapnp_softc *, u_char *, int));
void config_device __P((struct isapnp_softc *, struct isa_attach_args *));
int find_free_irq __P((int));
int find_free_drq __P((int));
int find_free_io  __P((struct isapnp_softc *, int, int, int, int,
	int, int));
void postisapnpattach __P((struct device *, struct device *, void *));
char *searchpnpdevtab __P((char *));
char *makepnpname __P((u_int32_t));
void isapnpextent __P((struct isa_attach_args *));
int handle_small_res __P((unsigned char *, int, int, struct cardinfo *));
void handle_large_res __P((unsigned char *, int, int, struct cardinfo *));

int
isapnpmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;

	/* sure we exist */
	ia->ia_iosize = 0;
	return(1);
}

void
isapnpattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_softc *isc = (void *)parent;
	struct isapnp_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	int num_pnp_devs;

	/*
	 * a reference to corresponding isapnp_softc
	 */
	isc->pnpsc = sc;

	sc->iot = ia->ia_iot;
	sc->parent = parent;
	TAILQ_INIT(&sc->q_card);

	/* The bus_space_*() stuff needs to be done differently.
	 * With PNP you want to be able to allocate a region,
	 * but not necessarily map it.  Why?  The drivers themselves
	 * should really map the bus regions they need.  So, which
	 * way should this be done?
	 *
	 * Me thinks to seperate the resource allocation and mapping
	 * of said resources.
	 */

	/* ADDRESS and WRITE_DATA ports are static */
	if (bus_space_map(sc->iot, ADDRESS, 1, 0, &(sc->addrh)))
		panic("isapnpattach: io mapping for ADDRESS port failed");
	if (bus_space_map(sc->iot, WRITE_DATA, 1, 0, &(sc->wdh)))
		panic("isapnpattach: io mapping for WRITE_DATA port failed");

	/* Try various READ_DATA ports from 0x203-0x3ff
	 * We try in increments of 16.  Note that the rd_port
	 * figure is really port number ((rd_port << 2) | 0x3).
	 */
	for (sc->rd_port = 0x80; (sc->rd_port < 0xff); sc->rd_port += 0x10) {
		int real_port = (sc->rd_port << 2) | 0x3;

		/* Try to map a READ_DATA port */
		if (bus_space_map(sc->iot, real_port, 1, 0, &(sc->rdh))) {
#ifdef DEBUG
			printf("\nisapnpattach:  READ_PORT 0x%x failed", real_port);
#endif
			continue;
		}

		/* Try isolation protocol on this port */
		num_pnp_devs = isolation_protocol(sc);
		if (num_pnp_devs)
			break;
		
		/* isolation protocol failed for this port */
		bus_space_unmap(sc->iot, real_port, 1);
	}
	printf(": readport 0x%x, %d devices\n",
	    (sc->rd_port << 2) | 0x3, num_pnp_devs);
}

char *
searchpnpdevtab(name)
	char *name;
{
	int i;

	for (i = 0; pnp_knowndevs[i].pnpid; i++)
		if (strcmp(pnp_knowndevs[i].pnpid, name) == 0)
			return (pnp_knowndevs[i].driver);
	return (NULL);
}

char *
makepnpname(id)
	u_int32_t id;
{
	static char name[8];
	u_char info[4];
	
	bcopy(&id, info, sizeof id);
	sprintf(name, "%c%c%c%02x%02x",
	    ((info[0] & 0x7c) >> 2) + 64,
	    (((info[0] & 0x03) << 3) | ((info[1] & 0xe0) >> 5)) + 64,
	    (info[1] & 0x1f) + 64,
	    info[2], info[3]);
	return (name);
}

void
postisapnpattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_softc *isc = (struct isa_softc *)self;
	struct isapnp_softc *sc = (struct isapnp_softc *)isc->pnpsc;
	struct isabus_attach_args *iba = aux;
	struct cardinfo *card;
	struct devinfo *dev;
	struct device *r;
#if 0
	extern char *msgs[];
#endif
	void *match;

	printf("postisapnpattach:\n");
	for (card = sc->q_card.tqh_first; card; card = card->card_link.tqe_next) {
		for (dev = card->q_dev.tqh_first; dev; dev = dev->dev_link.tqe_next) {
			struct isa_attach_args ia;

			bzero(&ia, sizeof(ia));
			ia.ia_iot = iba->iba_iot;
			ia.ia_ic = iba->iba_ic;
			ia.id = dev->id;
			ia.comp_id = dev->comp_id;
			ia.csn = card->csn;
			ia.ldn = dev->ldn;

			if (!isapnpquery(sc, ia.id, &ia)) {
				printf("isapnpquery failed\n");
				goto bail;
			}
			printf("id %x comp_id %x csn %x ldn %x\n", ia.id,
			    ia.comp_id, ia.csn, ia.ldn);
			printf("io %x/%x mem %x/%x irq %x drq %x\n",
				ia.ia_iobase, ia.ia_iosize, ia.ia_maddr,
				ia.ia_msize, ia.ia_irq, ia.ia_drq);
			match = config_search(isapnpsubmatch, self, &ia);
			printf("config search %x\n", match);

			if (match) {
				r = config_attach(self, match, &ia, NULL);
				printf("config attach %x\n", r);
			}

			if (match == NULL || r == NULL) {
bail:
#if 0
				printf(msgs[isapnpprint(&ia, self->dv_xname)]);
#endif
#if 1
				/*
				 * XXX does this shut down devices we
				 * are using??
				 * supplied configuration fails,
				 * disable the device.
				 */
				SEND(WAKE, ia.csn);
				SEND(SET_LDN, ia.ldn);
				SEND(ACTIVATE, 0);
#endif
			}
		}

		delay(1000*500);
	}
}

int
isapnpprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	register struct isa_attach_args *ia = aux;

	if (pnp) {
		printf("device ");
		if (ia->comp_id)
			printf("<%s> ", makepnpname(ia->comp_id));
		printf("<%s> at %s", makepnpname(ia->id), pnp);
	} else {
		if (ia->ia_iosize)
			printf(" port 0x%x", ia->ia_iobase);
		if (ia->ia_iosize > 1)
			printf("-0x%x", ia->ia_iobase + ia->ia_iosize - 1);
		if (ia->ia_msize)
			printf(" iomem 0x%x", ia->ia_maddr);
		if (ia->ia_msize > 1)
			printf("-0x%x", ia->ia_maddr + ia->ia_msize - 1);
		if (ia->ia_irq != IRQUNK)
			printf(" irq %d", ia->ia_irq);
		if (ia->ia_drq != DRQUNK)
			printf(" drq %d", ia->ia_drq);
	}
	return(UNCONF);
}

/* XXX */
/* Always return success */
int
probe_extent(ext, s, l)
	struct emap *ext;
	int s, l;
{
	return(0);
}

/* XXX */
/* Return bogus map */
struct emap *
find_emap(key)
	char *key;
{
	return(NULL);
}

/* XXX */
/* Do nothing at all */
void
add_extent(m, base, size)
	struct emap *m;
	long base;
	int size;
{
}

void
isapnpextent(ia)
	struct isa_attach_args *ia;
{
	struct emap *io_map, *mem_map, *irq_map, *drq_map;

	io_map = find_emap("io");
	mem_map = find_emap("mem");
	irq_map = find_emap("irq");
	drq_map = find_emap("drq");

	if (ia->ia_iosize > 0)
		add_extent(io_map, ia->ia_iobase, ia->ia_iosize);
	if (ia->ia_msize > 0)
		add_extent(mem_map, ia->ia_maddr, ia->ia_msize);
	if (ia->ia_irq != IRQUNK)
		add_extent(irq_map, ia->ia_irq, 1);
	if (ia->ia_drq != DRQUNK)
		add_extent(drq_map, ia->ia_drq, 1);
}

int
isapnpsubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct device *dev = match;
	struct cfdata *cf = dev->dv_cfdata;
	struct isa_attach_args *ia = aux;
	char *name, *dname;
	int ret = 0;

	/* XXX should check for id/comp_id being 0 */
	if (cf->cf_pnpid != 0 &&
	    (ia->id == cf->cf_pnpid || ia->comp_id == cf->cf_pnpid)) {
		printf("isapnpsubmatch going\n");
		ret = (*cf->cf_attach->ca_match)(parent, match, aux);
		goto done;
	}

	if (ia->comp_id) {
		name = makepnpname(ia->comp_id);
		dname = searchpnpdevtab(name);
		if (dname && strcmp(dname, cf->cf_driver->cd_name) == 0) {
			ret = (*cf->cf_attach->ca_match)(parent, match, aux);
			goto done;
		}
	}
	name = makepnpname(ia->id);
	dname = searchpnpdevtab(name);
	if (dname && strcmp(dname, cf->cf_driver->cd_name) == 0) {
		printf("match %s\n", dname);
		ret = (*cf->cf_attach->ca_match)(parent, match, aux);
	}
done:
	if (ret)
		isapnpextent(ia);
	return (ret);
}

/*
 * given the logical device ID, return 1 if found and configured.
 */
int
isapnpquery(sc, dev_id, ipa)
	struct isapnp_softc *sc;
	u_int32_t dev_id;
	struct isa_attach_args *ipa;
{
	struct cardinfo *card;
	struct devinfo *dev;
	struct confinfo *conf;
	struct isa_attach_args *tmp;
	int c, i, j, fail, success = 0;

	{
		char resp[10];
		printf("isapnpquery? ");
		getsn(resp, sizeof resp);
	}
	for (card = sc->q_card.tqh_first; card; card = card->card_link.tqe_next) {
		for (dev = card->q_dev.tqh_first; dev; dev = dev->dev_link.tqe_next) {
			if (dev_id != dev->id && dev_id != dev->comp_id)
				continue;
			tmp = malloc(sizeof(*tmp), M_DEVBUF, M_WAITOK);
			bzero(tmp, sizeof(*tmp));
			SEND(WAKE, card->csn);
			SEND(SET_LDN, dev->ldn);

			/* Find a usable and acceptable configuration */
			for (conf = dev->q_conf.tqh_first; conf;
			    conf = conf->conf_link.tqe_next) {
				/*
				 *  BASIC CONFIGURATION
				 */
				if (conf->prio == BASIC_CONFIGURATION) {
					for (c=0; c < 2; c++) {
						ipa->irq[c].num = -1;
						if (conf->irq[c] == 0)
							continue;
#if 0
						printf("irq %d %d %d\n", c,
						    conf->irq[c]->num,
						    conf->irq[c]->info);
#endif
						i = conf->irq[c]->num;
						if (i == 0)
							continue;
						j = find_free_irq(i);
						if (j != 0)
							continue;
						ipa->irq[c].num = j;

						/*
						 * XXX if the interrupt cannot
						 * be configured as low true
						 * level-triggered then set it
						 * to high true edge-triggered.
						 */
						if (conf->irq[c]->info & 0x08)
							ipa->irq[c].type = 0x01;
						else
							ipa->irq[c].type = 0x10;
					}
					for (c=0; c < 2; c++) {
						ipa->drq[c] = -1;
						if (conf->dma[c] == 0)
							continue;
#if 0
						printf("dma %d %d\n", c,
						    conf->dma[c]->channel);
#endif
						i = conf->dma[c]->channel;
						if (i == 0)
							continue;
						j = find_free_drq(i);
						if (j)
							ipa->drq[c] = j;
					}
					for (c=0; c < 8; c++) {
						if (conf->io[c] == 0)
							continue;
#if 0
						printf("io %d %d %d %d %d %d\n",
						    c, conf->io[c]->min_base,
						    conf->io[c]->max_base,
						    conf->io[c]->size,
						    conf->io[c]->alignment,
						    dev->io_range_check);
#endif
						ipa->port[c] = find_free_io(sc,
						    c, conf->io[c]->min_base,
						    conf->io[c]->max_base,
						    conf->io[c]->size,
						    conf->io[c]->alignment,
						    dev->io_range_check);
					}
					for (c=0; c < 4; c++) {
						if (conf->mem[c] == 0)
							continue;
#if 0
						printf("mem %d %d %d\n", c,
						    conf->mem[c]->min_base,
						    conf->mem[c]->size);
#endif
						ipa->mem[c].base =
						    conf->mem[c]->min_base;
						ipa->mem[c].range =
						    conf->mem[c]->size;
					}
				}

				/*
				 *  DEPENDENT FUNCTION
				 */
				fail = 0;
				success = 1;
				for (c=0; conf->irq[c] && c < 2; c++) {
					i = conf->irq[c]->num;
					tmp->irq[c].num = -1;
					if (i == 0)
						continue;
					j = find_free_irq(i);
					if (j) {
						tmp->irq[c].num = j;
						if (conf->irq[c]->info & 0x08)
							tmp->irq[c].type = 0x01;
						else
							tmp->irq[c].type = 0x10;
					} else {
						fail = 1;
#if 0
						printf("irq bail\n");
#endif
						success = 0;
						break;
					}
				}
				for (c=0; conf->dma[c] && c < 2; c++) {
					i = conf->dma[c]->channel;
					tmp->drq[c] = -1;
					if (i == 0)
						continue;
					j = find_free_drq(i);
					if (j)
						tmp->drq[c] = j;
					else {
						fail = 1;
#if 0
						printf("dma bail\n");
#endif
						success = 0;
						break;
					}
				}

#if 0
				printf("ports:");
#endif
				for (c=0; conf->io[c] && c < 8; c++) {
#if 0
					printf("%x/%x ", conf->io[c]->min_base,
					    conf->io[c]->size);
#endif
					if (conf->io[c]->size == 0)
						continue;
					tmp->port[c] = find_free_io(sc, c,
					    conf->io[c]->min_base,
					    conf->io[c]->max_base,
					    conf->io[c]->size,
					    conf->io[c]->alignment,
					    dev->io_range_check);
					if (tmp->port[c] == NULL) {
						fail = 1;
#if 0
						printf("io bail\n");
#endif
						success = 0;
						break;
					}
				}
#if 0
				printf("\n");

				printf("mem:");
#endif
				for (c=0; conf->mem[c] && c < 4; c++) {
#if 0
					printf("%x/%x ", conf->mem[c]->min_base,
					    conf->mem[c]->size);
#endif
					if (conf->mem[c]->size == 0)
						continue;
					tmp->mem[c].base =
					    conf->mem[c]->min_base;
					tmp->mem[c].range =
					    conf->mem[c]->size;
					if (tmp->mem[c].base == NULL) {
						fail = 1;
#if 0
						printf("mem bail\n");
#endif
						success = 0;
						break;
					}
				}
#if 0
				printf("\n");
#endif
		
				if (fail) {
					continue;
				}

				if (!success) {
					free(tmp, M_DEVBUF);
#if 0
					printf("bailing\n");
#endif
					return(0);
				}
			}


			/* Copy usable configuration */
			for (c = 0; c < 2; c++) {
				if (tmp->irq[c].num) {
					  ipa->irq[c].num = tmp->irq[c].num;
					  ipa->irq[c].type = tmp->irq[c].type;
				}
			}
			for (c = 0; c < 8; c++) {
				if (tmp->port[c])
					ipa->port[c] = tmp->port[c];
			}
			for (c = 0; c < 4; c++) {
				if (tmp->mem[c].base)
					ipa->mem[c].base = tmp->mem[c].base;
			}

			/* Configure device */
			config_device(sc, ipa);
			ipa->ia_iobase = ipa->port[0];
			ipa->ia_iosize = 16;		/* XXX */
			ipa->ia_irq = ipa->irq[0].num;
			ipa->ia_drq = ipa->drq[0];
			free(tmp, M_DEVBUF);
			return(1);
		}
	}
	return(0);
}

/*
 * Send Initiation LFSR as described in "Plug and Play ISA Specification,
 * Intel May 94."
 */
void
send_Initiation_LFSR(sc)
	struct isapnp_softc *sc;
{
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t addrh = sc->addrh;
	int cur, i;

	/* Reset the LSFR */
	bus_space_write_1(iot, addrh, 0, 0);
	bus_space_write_1(iot, addrh, 0, 0);

	cur = 0x6a;
	bus_space_write_1(iot, addrh, 0, cur);

	for (i = 1; i < 32; i++) {
		cur = (cur >> 1) | (((cur ^ (cur >> 1)) << 7) & 0xff);
		bus_space_write_1(iot, addrh, 0, cur);
	}
}

/*
 * Get the device's serial number.  Returns 1 if the serial is valid.
 */
int
get_serial(sc, data)
	struct isapnp_softc *sc;
	unsigned char *data;
{
	bus_space_tag_t iot = sc->iot;
	bus_space_handle_t rdh = sc->rdh;

	int i, bit, valid = 0, sum = 0x6a;

	bzero(data, sizeof(char) * 9);


	for (i = 0; i < 72; i++) {
		bit = bus_space_read_1(iot, rdh, 0) == 0x55;
		delay(250);	/* Delay 250 usec */

		/* Can't Short Circuit the next evaluation, so 'and' is last */
		bit = (bus_space_read_1(iot, rdh, 0) == 0xaa) && bit;
		delay(250);	/* Delay 250 usec */

		valid = valid || bit;

		if (i < 64)
			sum = (sum >> 1) |
				(((sum ^ (sum >> 1) ^ bit) << 7) & 0xff);

		data[i / 8] = (data[i / 8] >> 1) | (bit ? 0x80 : 0);
	}

	valid = valid && (data[8] == sum);

	return valid;
}

int
get_resource_info(sc, buffer, len)
	struct isapnp_softc *sc;
	u_char *buffer;
	int len;
{
	int i, j;

#if 0
	printf("gri: ");
#endif
	for (i = 0; i < len; i++) {
		bus_space_write_1(sc->iot, sc->addrh, 0, STATUS);
		for (j = 0; j < 100; j++) {
			if ((bus_space_read_1(sc->iot, sc->rdh, 0)) & 0x1)
				break;
			delay(1);
		}
		if (j == 100) {
			printf("isapnp: %s failed to report resource data\n",
			       sc->sc_dev.dv_xname);
			return 0;
		}
		bus_space_write_1(sc->iot, sc->addrh, 0, RESOURCE_DATA);
		buffer[i] = bus_space_read_1(sc->iot, sc->rdh, 0);

#if 0
		printf("%2x ", buffer[i]);
#endif
	}
#if 0
	printf("\n");
#endif
	return 1;
}

/*
 *  Small Resource Tag Handler
 *
 *  Returns 1 if checksum was valid (and an END_TAG was received).
 *  Returns -1 if checksum was invalid (and an END_TAG was received).
 *  Returns 0 for other tags.
 *
 *  XXX checksum is ignored now ...
 */
int
handle_small_res(resinfo, item, len, card)
	unsigned char *resinfo;
	int item, len;
	struct cardinfo *card;
{
	int i;

	switch (item) {
	case PNP_VERSION:
		bcopy(resinfo, card->pnp_version, 2);
		break;
	case LOG_DEVICE_ID:
		card->dev = malloc(sizeof(struct devinfo), M_DEVBUF, M_WAITOK);
		bzero(card->dev, sizeof(struct devinfo));
		TAILQ_INSERT_TAIL(&card->q_dev, card->dev, dev_link);
		card->dev->id = *(u_int32_t *)resinfo;
		card->dev->ldn = card->num_ld;
		card->dev->io_range_check = resinfo[4] & 0x2 ? 1 : 0;
		TAILQ_INIT(&card->dev->q_conf);

		/*
		 * if the resource data is not enclosed in a START_DEPEND_FUNC and
		 * a END_DEPEND_FUNC, it's the basic configuration.
		 *
		 * we simply treat it as a special case.
		 */
		card->dev->basic = malloc(sizeof(struct confinfo), M_DEVBUF, M_WAITOK);
		TAILQ_INSERT_TAIL(&card->dev->q_conf, card->dev->basic, conf_link);
		card->dev->basic->prio = BASIC_CONFIGURATION;
		bzero(card->dev->basic->irq, 2*sizeof(void *));
		bzero(card->dev->basic->dma, 2*sizeof(void *));
		bzero(card->dev->basic->io, 8*sizeof(void *));
		bzero(card->dev->basic->mem, 4*sizeof(void *));
		card->dev->conf = card->dev->basic;

		card->num_ld++;
		break;
	case COMP_DEVICE_ID:
		card->dev->comp_id = *(u_int32_t *)resinfo;
		break;
	case IRQ_FORMAT:
		for (i = 0; card->dev->conf->irq[i]; i++)
			;
		card->dev->conf->irq[i] = malloc(sizeof(struct irq_format),
		    M_DEVBUF, M_WAITOK);
		card->dev->conf->irq[i]->num = resinfo[0] | (resinfo[1] << 8);
		if (len == 3)
			card->dev->conf->irq[i]->info = resinfo[2];
		else
			card->dev->conf->irq[i]->info = 0;
		break;
	case DMA_FORMAT:
		for (i = 0; card->dev->conf->dma[i]; i++)
			;
		card->dev->conf->dma[i] = malloc(sizeof(struct dma_format),
		    M_DEVBUF, M_WAITOK);
		card->dev->conf->dma[i]->channel = resinfo[0];
		card->dev->conf->dma[i]->info = resinfo[1];
		break;
	case START_DEPEND_FUNC:
		card->dev->conf = malloc(sizeof(struct confinfo),
		    M_DEVBUF, M_WAITOK);
		TAILQ_INSERT_TAIL(&card->dev->q_conf, card->dev->conf,
		    conf_link);
		card->dev->conf->prio = ACCEPTABLE_CONFIGURATION;
		bzero(card->dev->conf->irq, 2*sizeof(void *));
		bzero(card->dev->conf->dma, 2*sizeof(void *));
		bzero(card->dev->conf->io, 8*sizeof(void *));
		bzero(card->dev->conf->mem, 4*sizeof(void *));

		if (len == 1) {
			switch (resinfo[0]) {
			case GOOD_CONFIGURATION:
				card->dev->conf->prio =
				    GOOD_CONFIGURATION;
				break;
			case ACCEPTABLE_CONFIGURATION:
				card->dev->conf->prio =
				    ACCEPTABLE_CONFIGURATION;
				break;
			case SUBOPTIMAL_CONFIGURATION:
				card->dev->conf->prio =
				    SUBOPTIMAL_CONFIGURATION;
				break;
			default:
				card->dev->conf->prio =
				    RESERVED_CONFIGURATION;
				break;
			}
		}
		break;
	case END_DEPEND_FUNC:
		break;
	case IO_PORT_DESC:
		for (i = 0; card->dev->conf->io[i]; i++)
			;
		card->dev->conf->io[i] = malloc(sizeof(struct io_descriptor),
		    M_DEVBUF, M_WAITOK);
		card->dev->conf->io[i]->type = 0; /* 0 for normal I/O desc. */
		card->dev->conf->io[i]->info = resinfo[0];
		card->dev->conf->io[i]->min_base =
		    resinfo[1] | (resinfo[2] << 8);
		card->dev->conf->io[i]->max_base =
		    resinfo[3] | (resinfo[4] << 8);
		card->dev->conf->io[i]->alignment = resinfo[5];
		card->dev->conf->io[i]->size = resinfo[6];
		break;
	case FIXED_IO_PORT_DESC:
		for (i = 0; card->dev->conf->io[i]; i++) ;
		card->dev->conf->io[i] = malloc(sizeof(struct io_descriptor),
		    M_DEVBUF, M_WAITOK);
		card->dev->conf->io[i]->type = 1; /* 1 for fixed I/O desc. */
		card->dev->conf->io[i]->info = 0;
		card->dev->conf->io[i]->min_base =
		    resinfo[0] | (resinfo[1] & 0x3) << 8;
		card->dev->conf->io[i]->max_base =
		    card->dev->conf->io[i]->min_base;
		card->dev->conf->io[i]->alignment = 0;
		card->dev->conf->io[i]->size = resinfo[2];
		break;
	case END_TAG:
		/*
		 * XXX checksum is ignored
		 */
#if 0
		printf("end of res\n");
#endif
		return(1);
	}

	return(0);
}

void
handle_large_res(resinfo, item, len, card)
	unsigned char *resinfo;
	int item, len;
	struct cardinfo *card;
{
	int i;

	switch (item) {
	case MEMORY_RANGE_DESC:
		for (i = 0; card->dev->conf->mem[i]; i++)
			;
		card->dev->conf->mem[i] = malloc(sizeof(struct mem_descriptor),
		    M_DEVBUF, M_WAITOK);
		card->dev->conf->mem[i]->type = 0; /* 0 for 24bit mem desc. */
		card->dev->conf->mem[i]->info = resinfo[0];
		card->dev->conf->mem[i]->min_base =
		    (resinfo[1] | (resinfo[2] << 8)) << 8;
		card->dev->conf->mem[i]->max_base =
		    (resinfo[3] | (resinfo[4] << 8)) << 8;
		card->dev->conf->mem[i]->alignment =
		    resinfo[5] | (resinfo[6] << 8);
		if (!card->dev->conf->mem[i]->alignment)
		    card->dev->conf->mem[i]->alignment = 1 << 16;
		card->dev->conf->mem[i]->size =
		    (resinfo[7] | (resinfo[8] << 8)) << 8;
		break;
	case ID_STRING_ANSI:
		if (card->dev) {
			card->dev->id_string = (char *)malloc(len+1, M_DEVBUF,
			    M_WAITOK);
			strncpy(card->dev->id_string, resinfo, len+1);
			card->dev->id_string[len] = '\0';
#if 0
			printf("ID_STRING_ANSI: %s\n", card->dev->id_string);
#endif
		} else {
			card->id_string = (char *)malloc(len+1, M_DEVBUF,
			    M_WAITOK);
			strncpy(card->id_string, resinfo, len+1);
			card->id_string[len] = '\0';
#if 0
			printf("ID_STRING_ANSI: %s\n", card->id_string);
#endif
		}
		break;
	case ID_STRING_UNICODE:
		break;
	case LG_VENDOR_DEFINED:
		break;
	case _32BIT_MEM_RANGE_DESC:
		break;
	case _32BIT_FIXED_LOC_DESC:
		break;

	/* XXX - Check how this is defined!  Bogus!!! */
	case LG_RES_RESERVED:
		break;
	default:
		break;
	}
}

void
read_config(sc, card, csn)
	struct isapnp_softc *sc;
	struct cardinfo *card;
	int csn;
{
	u_char tag, *resinfo;
	u_short large_len;
	int i;

#if 0
	/*
	 * set card with csn to Config state
	 */
	SEND(SET_CSN, csn);
#endif

	/*
	 * since we are in the card isolation process, so theres no reason
	 * to rewind and skip the first 9 bytes
	 */

	/* allow up to 1KB of resource info, should be plenty */
	for (i = 0; i < 4096; i++) {
		if (!get_resource_info(sc, &tag, 1))
			return;

#define TYPE   (tag >> 7)
#define S_ITEM (tag >> 3)
#define S_LEN  (tag & 0x7)
#define L_ITEM (tag & 0x7f)

		if (TYPE == 0) {
#if 0
			printf("small %d %d\n", S_ITEM, S_LEN);
#endif
			resinfo = malloc(S_LEN, M_TEMP, M_WAITOK);
			if (!get_resource_info(sc, resinfo, S_LEN)) {
				printf("bail getting small info\n");
				return;
			}

			if (handle_small_res(resinfo, S_ITEM, S_LEN, card) == 1)
				return;
			free(resinfo, M_TEMP);
		} else {
			large_len = 0;
			if (!get_resource_info(sc, (char *)&large_len, 2)) {
				printf("bail getting large info\n");
				return;
			}
			resinfo = malloc(large_len, M_TEMP, M_WAITOK);
			if (!get_resource_info(sc, resinfo, large_len)) {
				printf("bail sadf info\n");
				free(resinfo, M_TEMP);
				return;
			}

#if 0
			printf("large %d %d\n", L_ITEM, large_len);
#endif
			handle_large_res(resinfo, L_ITEM, large_len, card);
			free(resinfo, M_TEMP);
		}
	}
}

/*
 * Run the isolaion protocol. Use rd_port as the READ_DATA port value (caller
 * should try multiple READ_DATA locations before giving up). Upon exiting,
 * all cards are aware that they should use rd_port as the READ_DATA port;
 */
int
isolation_protocol(sc)
	struct isapnp_softc *sc;
{
	int csn;
	unsigned char data[9];

	/* Reset CSN for All Cards */
	/* Well, all cards who are *NOT* in Wait for Key state:
	 *
	 * 0x01 - Reset command.  (READ_PORT, CSN, PNP state preserved)
	 * 0x02 - Wait for Key.  (Everything preserved)
	 * 0x04 - Reset CSN.  (Nuke CSN goto wait for key state)
	 *
	 * Use 0x08 for the equivelant to RESET_DRV.
	 */
	SEND(CONFIG_CONTROL, 0x05);

	/* Move all PNP cards from WFK state to sleep state */
	send_Initiation_LFSR(sc);

	/* We should do the following until we do not
	 * find anymore PNP cards, with the max being
	 * 255 cards.  This is faster
	 */
	for (csn = 1; (csn < MAX_CARDS); csn++) {
		/* Wake up cards without a CSN */
		SEND(WAKE, 0);
		SEND(SET_RD_DATA, sc->rd_port);
		bus_space_write_1(sc->iot, sc->addrh, 0, SERIAL_ISOLATION);
		delay(1000);	/* Delay 1 msec */

		if (get_serial(sc, data)) {
			struct cardinfo *card;

			/* Move card into config state */
			SEND(SET_CSN, csn);
			card = malloc(sizeof(struct cardinfo), M_DEVBUF, M_WAITOK);
			bzero(card, sizeof(struct cardinfo));
			TAILQ_INSERT_TAIL(&sc->q_card, card, card_link);
			bcopy(data, card->serial, 9);
			card->csn = csn;
			TAILQ_INIT(&card->q_dev);
			/*
			 * read card's resource data
			 */
			read_config(sc, card, csn);
		} else
			break;
	}
	return csn - 1;
}

/*
 * Configure PNP devices, given a set of configuration data
 */
void
config_device(sc, data)
	struct isapnp_softc *sc;
	struct isa_attach_args *data;
{
	int i;

	if (data->csn <= 0) {
		return;
	}

#if 1
	printf ("%s: configuring CSN %x (LDN %x)\n",
		sc->sc_dev.dv_xname, data->csn,
		data->ldn != -1 ? data->ldn : 0);
#endif

	/* Wake up card, set LDN */
	SEND(WAKE, data->csn);
	if (data->ldn > 0)
		SEND (SET_LDN, data->ldn);

	/* Config IO */
	for (i = 0; i < 8; i++)
		if (data->port[i] > 0) {
			SEND (IO_CONFIG_BASE + i * 2,
			      data->port[i] >> 8);
			SEND (IO_CONFIG_BASE + i * 2 + 1,
			      data->port[i] & 0xff);
		}

	/* Config IRQ */
	for (i = 0; i < 2; i++)
		if (data->irq[i].num > 0) {
			SEND (IRQ_CONFIG + i * 2, data->irq[i].num);
			if (data->irq[i].type >= 0)
				SEND (IRQ_CONFIG + i * 2 + 1, data->irq[i].type);
		}

	/* Config DRQ */
	for (i = 0; i < 2; i++)
		if (data->drq[i] > 0) {
			SEND (DRQ_CONFIG + i, data->drq[i]);
		}

	/* Config MEM */
	for (i = 0; i < 4; i++)
		if (data->mem[i].base > 0) {
			SEND (MEM_CONFIG + i * 8,
			      data->mem[i].base >> 16);
			SEND (MEM_CONFIG + i * 8 + 1,
			      (data->mem[i].base >> 8) &
			      0xff);
			/*
			 * This needs to be handled better for
			 * the user's sake. XXX
			 */
			if (data->mem[i].control >= 0) {
				SEND (MEM_CONFIG + i * 8 + 2,
				      data->mem[i].control);
			}
			SEND (MEM_CONFIG + i * 8 + 3,
			      data->mem[i].range >> 16);
			SEND (MEM_CONFIG + i * 8 + 4,
			      (data->mem[i].range >> 8) &
			      0xff);
		}

	/* Disable RANGE_CHECK & ACTIVATE logical device */
	SEND (IO_RANGE_CHECK, 0);
	SEND (ACTIVATE, 1);
}

int
find_free_irq(irq_mask)
	int irq_mask;
{
	struct emap *irq_map;
	int i, j;

	irq_map = find_emap("irq");
	i = irq_mask;

	while (1) {
		j = ffs(i);
		if (j == 0)
			return(0);
		j--;
		if (!probe_extent(irq_map, j, 1))
			return(j);
		i &= ~(0x1 << j);
	}

	return(0);
}

int
find_free_drq(drq_mask)
	int drq_mask;
{
	struct emap *drq_map;
	int i, j;

	drq_map = find_emap("drq");
	i = drq_mask;

	while (1) {
		j = ffs(i);
		if (j == 0)
			return(0);
		j--;
		if (!probe_extent(drq_map, j, 1))
			return(j);
		i &= ~(0x1 << j);
	}

	return(0);
}

/*
 * find free I/O space.
 * if device is capable of doing I/O range check, then use it.
 * else, try to find free region from extent map.
 *
 * assume caller has set csn and ldn properly.
 */
int
find_free_io(sc, desc, min_addr, max_addr, size, alignment, range_check)
	struct isapnp_softc *sc;
	int desc, min_addr, max_addr, size, alignment, range_check;
{
	int addr, i, success = 0;
	bus_space_handle_t data;
	struct emap *io_map;

	if (range_check) {
		for (addr = min_addr; addr <= max_addr; addr += alignment) {
			SEND(ACTIVATE, 0);
			SEND(IO_CONFIG_BASE + desc * 2, addr >> 8);
			SEND(IO_CONFIG_BASE + desc * 2 + 1, addr & 0xff);
			SEND(IO_RANGE_CHECK, 0x2);
			bus_space_map(sc->iot, addr, size, 0, &data);
			i = 0;
			for (i = 0; i < size; i++) {
				if (bus_space_read_1(sc->iot, data, i) !=
				    0xAA) {
					bus_space_unmap(sc->iot, data, size);
					break;
				}
			}
			if (i == size) {
				success = 1;
				bus_space_unmap(sc->iot, data, size);
				break;
			}
		}

		if (success) {
			return(addr);
		}
		else {
			return(0);
		}
	} else {
#if 0
		printf("%x len %x\n", addr, size);
#endif
		io_map = find_emap("io");
		addr = min_addr;
		if (!probe_extent(io_map, addr, size))
			return(addr);
		return (0);
	}
}
