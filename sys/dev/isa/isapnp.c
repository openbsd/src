/*	$OpenBSD: isapnp.c,v 1.4 1996/08/15 17:28:40 shawn Exp $	*/

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

#include "isapnpreg.h"
#include "isapnpvar.h"

#define SEND(d, r)	{ bus_io_write_1(sc->bc, sc->addrh, 0, d); \
                          bus_io_write_1(sc->bc, sc->wdh,   0, r); }

int isapnpmatch __P((struct device *, void *, void *));
void isapnpattach __P((struct device *, struct device *, void *));
int isapnpprint __P((void *aux, char *pnp));
int isapnpsubmatch __P((struct device *parent, void *match, void *aux));

struct isapnp_softc {
	struct device sc_dev;
	struct device *parent;

	bus_chipset_tag_t bc;
	bus_io_handle_t addrh;
	bus_io_handle_t wdh;
	bus_io_handle_t rdh;
	int rd_offset;

	int rd_port;

	TAILQ_HEAD(, cardinfo) q_card;
};

struct cfattach isapnp_ca = {
	sizeof(struct isapnp_softc), isapnpmatch, isapnpattach
};

struct cfdriver isapnp_cd = {
	NULL, "isapnp", DV_DULL, 1
};

static int isapnpquery __P((struct isapnp_softc *sc,
			    u_int32_t dev_id, struct isa_attach_args *ia));
static void send_Initiation_LFSR __P((struct isapnp_softc *sc));
static int get_serial __P((struct isapnp_softc *sc, unsigned char *data));
static int isolation_protocol __P((struct isapnp_softc *sc));
static void read_config __P((struct isapnp_softc *sc,
			     struct cardinfo *card, int csn));
static int get_resource_info __P((struct isapnp_softc *sc,
				  char *buffer, int len));
static void config_device __P((struct isapnp_softc *sc,
			       struct isa_attach_args *data));
static int find_free_irq __P((int irq_mask));
static int find_free_drq __P((int drq_mask));
static int find_free_io  __P((struct isapnp_softc *sc, int desc,
			      int min_addr, int max_addr, int size,
			      int alignment, int range_check));

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
	struct cardinfo *card;
	int iobase;
	int num_pnp_devs;

	/*
	 * a reference to corresponding isapnp_softc
	 */
	isc->pnpsc = sc;

	sc->bc = ia->ia_bc;
	sc->parent = parent;
	TAILQ_INIT(&sc->q_card);

	/*
	 * WRITE_DATA port is located at fixed offset (0x0800)
	 * from ADDRESS port,
	 * and valid READ_DATA ports are from 0x203 to 0x3ff.
	 */
	if (bus_io_map(sc->bc, ADDRESS, 1, &(sc->addrh)) ||
	    bus_io_map(sc->bc, ADDRESS+0x0800, 1, &(sc->wdh))   ||
	    bus_io_map(sc->bc, 0x0200, 0x200, &(sc->rdh)))
		panic("isapnpattach: io mapping failed");

	/* Try various READ_DATA ports from 0x203-0x3ff */
	for (sc->rd_port = 0x80; (sc->rd_port < 0xff); sc->rd_port += 0x10) {
		num_pnp_devs = isolation_protocol(sc);
		if (num_pnp_devs) {
			printf(": readport 0x%x, %d devices",
			    (sc->rd_port << 2) | 0x3, num_pnp_devs);
			break;
		}
	}
	printf("\n");
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

	for (card = sc->q_card.tqh_first; card;
	    card = card->card_link.tqe_next) {
		for (dev = card->q_dev.tqh_first; dev;
		    dev = dev->dev_link.tqe_next) {
			struct isa_attach_args ia;

			bzero(&ia, sizeof(ia));
			ia.ia_bc = iba->iba_bc;
			ia.ia_ic = iba->iba_ic;
			ia.id = dev->id;
			ia.comp_id = dev->comp_id;
			ia.csn = card->csn;
			ia.ldn = dev->ldn;
			ia.ia_delayioh = isc->sc_delayioh;

			isapnpquery(sc, ia.id, &ia);
			if (!config_found_sm(self, &ia, isapnpprint,
			    isapnpsubmatch)) {
				/*
				 * supplied configuration fails,
				 * disable the device.
				 */
				SEND(WAKE, ia.csn);
				SEND(SET_LDN, ia.ldn);
				SEND(ACTIVATE, 0);
			}
		}
	}
}

int
isapnpprint(aux, pnp)
	void *aux;
	char *pnp;
{
	register struct isa_attach_args *ia = aux;
	unsigned char info[4];
	struct emap *io_map, *mem_map, *irq_map, *drq_map;

	io_map = find_emap("io");
	mem_map = find_emap("mem");
	irq_map = find_emap("irq");
	drq_map = find_emap("drq");

	bcopy(&ia->id, info, 4);
	if (pnp) {
		printf("device <%c%c%c%02x%02x> at %s",
		       ((info[0] & 0x7c) >> 2) + 64,
		       (((info[0] & 0x03) << 3) |
			((info[1] & 0xe0) >> 5)) + 64,
		       (info[1] & 0x1f) + 64,
		       info[2], info[3], pnp);
	}
	if (!pnp) {
		if (ia->ia_iosize)
			printf(" port 0x%x", ia->ia_iobase);
		if (ia->ia_iosize > 1) {
			printf("-0x%x",
			       ia->ia_iobase + ia->ia_iosize - 1);
			add_extent(io_map, ia->ia_iobase, ia->ia_iosize);
		}
		if (ia->ia_msize)
			printf(" iomem 0x%x", ia->ia_maddr);
		if (ia->ia_msize > 1) {
			printf("-0x%x",
			       ia->ia_maddr + ia->ia_msize - 1);
			add_extent(mem_map, ia->ia_maddr, ia->ia_msize);
		}
		if (ia->ia_irq != IRQUNK) {
			printf(" irq %d", ia->ia_irq);
			add_extent(irq_map, ia->ia_irq, 1);
		}
		if (ia->ia_drq != DRQUNK) {
			printf(" drq %d", ia->ia_drq);
			add_extent(drq_map, ia->ia_drq, 1);
		}
	}
	return(UNCONF);
}

int
isapnpsubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct device *dev = match;
	struct cfdata *cf = dev->dv_cfdata;
	struct isa_attach_args *ia = aux;
	int ret;

	if (cf->cf_pnpid == ia->id ||
	    cf->cf_pnpid == ia->comp_id) {
		ret = (*cf->cf_attach->ca_match)(parent, match, aux);
		return (ret);
	}
	return (0);
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
	int irq, drq, iobase, mbase;
	int c, i, j, fail, success;

	for (card = sc->q_card.tqh_first; card;
	     card = card->card_link.tqe_next) {
	  for (dev = card->q_dev.tqh_first; dev;
	       dev = dev->dev_link.tqe_next) {
	    if (dev_id == dev->id ||
		dev_id == dev->comp_id) {
	      tmp = malloc(sizeof(struct isa_attach_args), M_DEVBUF, M_WAITOK);
	      bzero(tmp, sizeof(struct isa_attach_args));
	      SEND(WAKE, card->csn);
	      SEND(SET_LDN, dev->ldn);

	      for (conf = dev->q_conf.tqh_first; conf;
		   conf = conf->conf_link.tqe_next) {
		/*
		 *  BASIC CONFIGURATION
		 */
		if (conf->prio == BASIC_CONFIGURATION) {
		  if (conf->irq[0]) {
		    for (c = 0; conf->irq[c] && c < 2; c++) {
		      i = conf->irq[c]->num;
		      j = find_free_irq(i);
		      if (j) {
			ipa->irq[c].num = j;
			/*
			 * if the interrupt can not be configured as
			 * low true level-triggered,
			 * then set it to high true edge-triggered.
			 * XXX needs rework
			 */
			if (conf->irq[c]->info & 0x08) {
			  ipa->irq[c].type = 0x01;
			}
			else {
			  ipa->irq[c].type = 0x10;
			}
		      }
		    }
		  }
		  if (conf->dma[0]) {
		    for (c = 0; conf->dma[c] && c < 2; c++) {
		      i = conf->dma[c]->channel;
		      j = find_free_drq(i);
		      if (j) {
			ipa->drq[c] = j;
		      }
		    }
		  }
		  if (conf->io[0]){
		    for (c = 0; conf->io[c] && c < 8; c++) {
		      ipa->port[c] = find_free_io(sc, c,
						 conf->io[c]->min_base,
						 conf->io[c]->max_base,
						 conf->io[c]->size,
						 conf->io[c]->alignment,
						 dev->io_range_check);
		    }
		  }
		  /* XXX mem */
		  if (conf->mem[0]) {
		    for (c = 0; conf->mem[c] && c < 4; c++) {
		      ipa->mem[c].base = conf->mem[c]->min_base;
		      ipa->mem[c].range = conf->mem[c]->size;
		    }
		  }
		}

		/*
		 *  DEPENDENT FUNCTION
		 */
		fail = 0; success = 1;
		if (conf->irq[0]) {
		  for (c = 0; conf->irq[c] && c < 2; c++) {
		    i = conf->irq[c]->num;
		    j = find_free_irq(i);
		    if (j) {
		      tmp->irq[c].num = j;
		      /*
		       * if the interrupt can not be
		       * low true level-triggered,
		       * then set it to high true edge-triggered.
		       * XXX rework
		       */
		      if (conf->irq[c]->info & 0x08) {
			tmp->irq[c].type = 0x01;
		      }
		      else {
			tmp->irq[c].type = 0x10;
		      }
		    }
		    else {
		      fail = 1;
		      success = 0;
		      break;
		    }
		  }
		}
		if (conf->dma[0]) {
		  for (c = 0; conf->dma[c] && c < 2; c++) {
		    i = conf->dma[c]->channel;
		    j = find_free_drq(i);
		    if (j) {
		      tmp->drq[c] = j;
		    }
		    else {
		      fail = 1;
		      success = 0;
		      break;
		    }
		  }
		}
		if (conf->io[0]) {
		  for (c = 0; conf->io[c] && c < 8; c++) {
		    tmp->port[c] = find_free_io(sc, c,
						conf->io[c]->min_base,
						conf->io[c]->max_base,
						conf->io[c]->size,
						conf->io[c]->alignment,
						dev->io_range_check);
		    if (!tmp->port[c]) {
		      fail = 1;
		      success = 0;
		      break;
		    }
		  }
		}
		if (conf->mem[0]) {
		  for (c = 0; conf->mem[c] && c < 4; c++) {
		    tmp->mem[c].base = conf->mem[c]->min_base;
		    tmp->mem[c].range = conf->mem[c]->size;

		    if (!tmp->mem[c].base) {
		      fail = 1;
		      success = 0;
		      break;
		    }
		  }
		}

		if (fail) {
		  continue;
		}
	      }

	      if (!success) {
		return(0);
	      }

	      for (c = 0; c < 2; c++) {
		if (tmp->irq[c].num) {
		  ipa->irq[c].num = tmp->irq[c].num;
		  ipa->irq[c].type = tmp->irq[c].type;
		}
	      }
	      for (c = 0; c < 8; c++) {
	        if (tmp->port[c]) {
		  ipa->port[c] = tmp->port[c];
		}
	      }
	      for (c = 0; c < 4; c++) {
		if (tmp->mem[c].base) {
		  ipa->mem[c].base = tmp->mem[c].base;
		}
	      }
	      config_device(sc, ipa);
	      ipa->ia_iobase = ipa->port[0];
	      ipa->ia_irq = ipa->irq[0].num;
	      ipa->ia_drq = ipa->drq[0];
	      free(tmp, M_DEVBUF);
	      return(1);
	    }
	  }
	}
	return(0);
}

/*
 * Send Initiation LFSR as described in "Plug and Play ISA Specification,
 * Intel May 94."
 */
static void
send_Initiation_LFSR(sc)
	struct isapnp_softc *sc;
{
	bus_chipset_tag_t bc = sc->bc;
	bus_io_handle_t addrh = sc->addrh;
	int cur, i;

	/* Reset the LSFR */
	bus_io_write_1(bc, addrh, 0, 0);
	bus_io_write_1(bc, addrh, 0, 0);

	cur = 0x6a;
	bus_io_write_1(bc, addrh, 0, cur);

	for (i = 1; i < 32; i++) {
		cur = (cur >> 1) | (((cur ^ (cur >> 1)) << 7) & 0xff);
		bus_io_write_1(bc, addrh, 0, cur);
	}
}

/*
 * Get the device's serial number.  Returns 1 if the serial is valid.
 */
static int
get_serial(sc, data)
	struct isapnp_softc *sc;
	unsigned char *data;
{
	bus_chipset_tag_t bc = sc->bc;
	bus_io_handle_t rdh = sc->rdh;

	int i, bit, valid = 0, sum = 0x6a;

	bzero(data, sizeof(char) * 9);

	sc->rd_offset = ((sc->rd_port - 0x80) << 2) | 0x3;

	for (i = 0; i < 72; i++) {
		bit = bus_io_read_1(bc, rdh, sc->rd_offset) == 0x55;
		delay(250);	/* Delay 250 usec */

		/* Can't Short Circuit the next evaluation, so 'and' is last */
		bit = (bus_io_read_1(bc, rdh, sc->rd_offset) == 0xaa) && bit;
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

static int
get_resource_info(sc, buffer, len)
	struct isapnp_softc *sc;
	char *buffer;
	int len;
{
	int i, j;

	for (i = 0; i < len; i++) {
		bus_io_write_1(sc->bc, sc->addrh, 0, STATUS);
		for (j = 0; j < 100; j++) {
			if ((bus_io_read_1(sc->bc, sc->rdh, sc->rd_offset))
			    & 0x1)
				break;
			delay(1);
		}
		if (j == 100) {
			printf("%s: failed to report resource data\n",
			       sc->sc_dev.dv_xname);
			return 0;
		}
		bus_io_write_1(sc->bc, sc->addrh, 0, RESOURCE_DATA);
		buffer[i] = bus_io_read_1(sc->bc, sc->rdh, sc->rd_offset);
	}
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
static int
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
		 * we simple treat is as a special case.
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
		for (i = 0; card->dev->conf->irq[i]; i++) ;
		card->dev->conf->irq[i] = malloc(sizeof(struct irq_format),
						 M_DEVBUF, M_WAITOK);
		card->dev->conf->irq[i]->num = resinfo[0] | resinfo[1] << 8;
		if (len == 3) {
			card->dev->conf->irq[i]->info = resinfo[2];
		}
		else {
			card->dev->conf->irq[i]->info = 0;
		}
		break;
	case DMA_FORMAT:
		for (i = 0; card->dev->conf->dma[i]; i++) ;
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
		for (i = 0; card->dev->conf->io[i]; i++) ;
		card->dev->conf->io[i] = malloc(sizeof(struct io_descriptor),
						M_DEVBUF, M_WAITOK);
		card->dev->conf->io[i]->type = 0; /* 0 for normal I/O desc. */
		card->dev->conf->io[i]->info = resinfo[0];
		card->dev->conf->io[i]->min_base =
			resinfo[1] | resinfo[2] << 8;
		card->dev->conf->io[i]->max_base =
			resinfo[3] | resinfo[4] << 8;
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
		return(1);
	}

	return(0);
}

static void
handle_large_res(resinfo, item, len, card)
	unsigned char *resinfo;
	int item, len;
	struct cardinfo *card;
{
	int i;

	switch (item) {
	case MEMORY_RANGE_DESC:
		for (i = 0; card->dev->conf->mem[i]; i++) ;
		card->dev->conf->mem[i] = malloc(sizeof(struct mem_descriptor),
						 M_DEVBUF, M_WAITOK);
		card->dev->conf->mem[i]->type = 0; /* 0 for 24bit mem desc. */
		card->dev->conf->mem[i]->info = resinfo[0];
		card->dev->conf->mem[i]->min_base =
			(resinfo[1] | resinfo[2] << 8) << 8;
		card->dev->conf->mem[i]->max_base =
			(resinfo[3] | resinfo[4] << 8) << 8;
		card->dev->conf->mem[i]->alignment =
			(resinfo[5] | resinfo[6] << 8);
		if (!card->dev->conf->mem[i]->alignment)
			card->dev->conf->mem[i]->alignment = 1 << 16;
		card->dev->conf->mem[i]->size =
			(resinfo[7] | resinfo[8] << 8) << 8;
		break;
	case ID_STRING_ANSI:
		if (card->dev) {
			card->dev->id_string =
				(char *)malloc(len+1, M_DEVBUF, M_WAITOK);
			strncpy(card->dev->id_string, resinfo, len+1);
			card->dev->id_string[len] = '\0';
		} else {
			card->id_string = 
				(char *)malloc(len+1, M_DEVBUF, M_WAITOK);
			strncpy(card->id_string, resinfo, len+1);
			card->id_string[len] = '\0';
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
	case LG_RES_RESERVED:
		break;
	}
}

static void
read_config(sc, card, csn)
	struct isapnp_softc *sc;
	struct cardinfo *card;
	int csn;
{
	u_char serial[9], tag, *resinfo;
	int i, large_len;

	/*
	 * set card with csn to Config state
	 */
	SEND(SET_CSN, csn);

	/*
	 * since we are in the card isolation process, so ther's no reason
	 * to rewind and skip the first 9 bytes
	 */

	/*
	 * allow up to 1KB of resource info, should be plenty */
	for (i = 0; i < 1024; i++) {
		if (!get_resource_info(sc, &tag, 1))
			return;

#define TYPE   (tag >> 7)
#define S_ITEM (tag >> 3)
#define S_LEN  (tag & 0x7)
#define L_ITEM (tag & 0x7f)

		if (TYPE == 0) {
			resinfo = malloc(S_LEN, M_TEMP, M_WAITOK);
			if (!get_resource_info(sc, resinfo, S_LEN))
				return;

			if (handle_small_res(resinfo, S_ITEM, S_LEN, card) == 1)
				return;
			free(resinfo, M_TEMP);
		} else {
			large_len = 0;
			if (!get_resource_info(sc, (char *)&large_len, 2))
				return;

			resinfo = malloc(large_len, M_TEMP, M_WAITOK);
			if (!get_resource_info(sc, resinfo, large_len))
				return;

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
static int
isolation_protocol(sc)
	struct isapnp_softc *sc;
{
	int csn;
	unsigned char data[9];

	/* Reset CSN for All Cards */
	SEND(CONFIG_CONTROL, 0x05);

	send_Initiation_LFSR(sc);

	for (csn = 1; (csn < MAX_CARDS); csn++) {
		/* Wake up cards without a CSN */
		SEND(WAKE, 0);
		SEND(SET_RD_DATA, sc->rd_port);
		bus_io_write_1(sc->bc, sc->addrh, 0, SERIAL_ISOLATION);
		delay(1000);	/* Delay 1 msec */

		if (get_serial(sc, data)) {
			struct cardinfo *card;

			SEND(SET_CSN, csn);
			card = malloc(sizeof(struct cardinfo),
				      M_DEVBUF, M_WAITOK);
			bzero(card, sizeof(struct cardinfo));
			TAILQ_INSERT_TAIL(&sc->q_card, card, card_link);
			bcopy(data, card->serial, 9);
			card->csn = csn;
			TAILQ_INIT(&card->q_dev);
			/*
			 * read card's resource data
			 */
			read_config(sc, card, csn);
		}
		else
			break;
	}
	return csn - 1;
}

/*
 * Configure PnP devices, given a set of configuration data
 */
static void
config_device(sc, data)
	struct isapnp_softc *sc;
	struct isa_attach_args *data;
{
	int i;

	if (data->csn <= 0) {
		return;
	}

#if 0
	printf ("%s: configuring CSN %x (logical device %x)\n",
		sc->sc_dev.dv_xname,
		data->csn,
		data->ldn != -1 ? data->ldn : 0);
#endif

	SEND(WAKE, data->csn);
	if (data->ldn > 0)
		SEND (SET_LDN, data->ldn);

	for (i = 0; i < 8; i++)
		if (data->port[i] > 0) {
			SEND (IO_CONFIG_BASE + i * 2,
			      data->port[i] >> 8);
			SEND (IO_CONFIG_BASE + i * 2 + 1,
			      data->port[i] & 0xff);
		}

	for (i = 0; i < 2; i++)
		if (data->irq[i].num > 0) {
			SEND (IRQ_CONFIG + i * 2, data->irq[i].num);
			if (data->irq[i].type >= 0)
				SEND (IRQ_CONFIG + i * 2 + 1, data->irq[i].type);
		}

	for (i = 0; i < 2; i++)
		if (data->drq[i] > 0) {
			SEND (DRQ_CONFIG + i, data->drq[i]);
		}

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
	SEND (IO_RANGE_CHECK, 0);
	SEND (ACTIVATE, 1);
}

static int
find_free_irq(irq_mask)
	int irq_mask;
{
	int i, j;
	struct emap *irq_map;

	irq_map = find_emap("irq");
	i = irq_mask;

	while (1) {
		j = ffs(i);
		if (!j) {
			return(0);
		}
		/*
		 * irq_mask is starting from 0
		 */
		j --;
		if (!probe_extent(irq_map, j, 1)) {
			return(j);
		}
		i &= ~(0x1 << j);
	}
}

int
find_free_drq(drq_mask)
	int drq_mask;
{
	int i, j;
	struct emap *drq_map;

	drq_map = find_emap("drq");
	i = drq_mask;

	while (1) {
		j = ffs(i);
		if (!j) {
			return(0);
		}
		/*
		 * drq_mask is starting from 0
		 */
		j --;
		if (!probe_extent(drq_map, j, 1)) {
			return(j);
		}
		i &= ~(0x1 << j);
	}
}

/*
 * find free I/O space.
 * if device is capable of doing I/O range check, then use it.
 * else, try to find free region from extent map.
 *
 * assume caller has set csn and ldn properly.
 */
static int
find_free_io(sc, desc, min_addr, max_addr, size, alignment, range_check)
	struct isapnp_softc *sc;
	int desc, min_addr, max_addr, size, alignment, range_check;
{
	int addr, i, success = 0;
	bus_io_handle_t data;
	struct emap *io_map;

	if (range_check) {
		for (addr = min_addr; addr <= max_addr; addr += alignment) {
			SEND(ACTIVATE, 0);
			SEND(IO_CONFIG_BASE + desc * 2, addr >> 8);
			SEND(IO_CONFIG_BASE + desc * 2 + 1, addr & 0xff);
			SEND(IO_RANGE_CHECK, 0x2);
			bus_io_map(sc->bc, addr, size, &data);
			i = 0;
			for (i = 0; i < size; i++) {
				if (bus_io_read_1(sc->bc, data, i) != 0xAA) {
					bus_io_unmap(sc->bc, data, size);
					break;
				}
			}
			if (i == size) {
				success = 1;
				bus_io_unmap(sc->bc, data, size);
				break;
			}
		}

		if (success) {
			return(addr);
		}
		else {
			return(0);
		}
	}
	else {
		io_map = find_emap("io");
		addr = min_addr;
		if (!probe_extent(io_map, addr, size)) {
			return(addr);
		}
		return(0);
	}
}
