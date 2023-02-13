/*	$OpenBSD: scmi.c,v 1.1 2023/02/13 19:26:15 kettenis Exp $	*/

/*
 * Copyright (c) 2023 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/fdt.h>

#include <dev/fdt/pscivar.h>

struct scmi_shmem {
	uint32_t reserved1;
	uint32_t channel_status;
#define SCMI_CHANNEL_ERROR		(1 << 1)
#define SCMI_CHANNEL_FREE		(1 << 0)
	uint32_t reserved2;
	uint32_t reserved3;
	uint32_t channel_flags;
	uint32_t length;
	uint32_t message_header;
	uint32_t message_payload[];
};

#define SCMI_SUCCESS		0
#define SCMI_NOT_SUPPORTED	-1
#define SCMI_BUSY		-6
#define SCMI_COMMS_ERROR	-7

/* Protocols */
#define SCMI_BASE		0x10
#define SCMI_CLOCK		0x14

/* Common messages */
#define SCMI_PROTOCOL_VERSION			0x0
#define SCMI_PROTOCOL_ATTRIBUTES		0x1
#define SCMI_PROTOCOL_MESSAGE_ATTRIBUTES	0x2

/* Clock management messages */
#define SCMI_CLOCK_ATTRIBUTES			0x3
#define SCMI_CLOCK_DESCRIBE_RATES		0x4
#define SCMI_CLOCK_RATE_SET			0x5
#define SCMI_CLOCK_RATE_GET			0x6
#define SCMI_CLOCK_CONFIG_SET			0x7
#define  SCMI_CLOCK_CONFIG_SET_ENABLE		(1 << 0)

static inline void
scmi_message_header(volatile struct scmi_shmem *shmem,
    uint32_t protocol_id, uint32_t message_id)
{
	shmem->message_header = (protocol_id << 10) | (message_id << 0);
}


struct scmi_softc {
	struct device			sc_dev;
	bus_space_tag_t			sc_iot;
	bus_space_handle_t		sc_ioh;
	volatile struct scmi_shmem	*sc_shmem;

	uint32_t			sc_smc_id;

	struct clock_device		sc_cd;
};

int	scmi_match(struct device *, void *, void *);
void	scmi_attach(struct device *, struct device *, void *);

const struct cfattach scmi_ca = {
	sizeof(struct scmi_softc), scmi_match, scmi_attach
};

struct cfdriver scmi_cd = {
	NULL, "scmi", DV_DULL
};

void	scmi_attach_proto(struct scmi_softc *, int);
void	scmi_attach_clock(struct scmi_softc *, int);
int32_t	scmi_command(struct scmi_softc *);

int
scmi_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "arm,scmi-smc");
}

void
scmi_attach(struct device *parent, struct device *self, void *aux)
{
	struct scmi_softc *sc = (struct scmi_softc *)self;
	volatile struct scmi_shmem *shmem;
	struct fdt_attach_args *faa = aux;
	struct fdt_reg reg;
	int32_t status;
	uint32_t version;
	uint32_t phandle;
	void *node;
	int proto;

	phandle = OF_getpropint(faa->fa_node, "shmem", 0);
	node = fdt_find_phandle(phandle);
	if (node == NULL || !fdt_is_compatible(node, "arm,scmi-shmem") ||
	    fdt_get_reg(node, 0, &reg)) {
		printf(": no shared memory\n");
		return;
	}

	sc->sc_smc_id = OF_getpropint(faa->fa_node, "arm,smc-id", 0);
	if (sc->sc_smc_id == 0) {
		printf(": no SMC id\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, reg.addr,
	    reg.size, 0, &sc->sc_ioh)) {
		printf(": can't map shared memory\n");
		return;
	}
	sc->sc_shmem = bus_space_vaddr(sc->sc_iot, sc->sc_ioh);
	shmem = sc->sc_shmem;

	if ((shmem->channel_status & SCMI_CHANNEL_FREE) == 0) {
		printf(": channel busy\n");
		return;
	}

	scmi_message_header(shmem, SCMI_BASE, SCMI_PROTOCOL_VERSION);
	shmem->length = sizeof(uint32_t);
	status = scmi_command(sc);
	if (status != SCMI_SUCCESS) {
		printf(": protocol version command failed\n");
		return;
	}

	version = shmem->message_payload[1];
	printf(": SCMI %d.%d\n", version >> 16, version & 0xffff);

	for (proto = OF_child(faa->fa_node); proto; proto = OF_peer(proto))
		scmi_attach_proto(sc, proto);
}

int32_t
scmi_command(struct scmi_softc *sc)
{
	volatile struct scmi_shmem *shmem = sc->sc_shmem;
	int32_t status;

	shmem->channel_status = 0;
	status = smccc(sc->sc_smc_id, 0, 0, 0);
	if (status != PSCI_SUCCESS)
		return SCMI_NOT_SUPPORTED;
	if ((shmem->channel_status & SCMI_CHANNEL_ERROR))
		return SCMI_COMMS_ERROR;
	if ((shmem->channel_status & SCMI_CHANNEL_FREE) == 0)
		return SCMI_BUSY;
	return shmem->message_payload[0];
}

void
scmi_attach_proto(struct scmi_softc *sc, int node)
{
	switch (OF_getpropint(node, "reg", -1)) {
	case SCMI_CLOCK:
		scmi_attach_clock(sc, node);
		break;
	default:
		break;
	}
}

/* Clock management. */

void	scmi_clock_enable(void *, uint32_t *, int);
uint32_t scmi_clock_get_frequency(void *, uint32_t *);
int	scmi_clock_set_frequency(void *, uint32_t *, uint32_t);

void
scmi_attach_clock(struct scmi_softc *sc, int node)
{
	volatile struct scmi_shmem *shmem = sc->sc_shmem;
	int32_t status;
	int nclocks;

	scmi_message_header(shmem, SCMI_CLOCK, SCMI_PROTOCOL_ATTRIBUTES);
	shmem->length = sizeof(uint32_t);
	status = scmi_command(sc);
	if (status != SCMI_SUCCESS)
		return;

	nclocks = shmem->message_payload[1] & 0xffff;
	if (nclocks == 0)
		return;

	sc->sc_cd.cd_node = node;
	sc->sc_cd.cd_cookie = sc;
	sc->sc_cd.cd_enable = scmi_clock_enable;
	sc->sc_cd.cd_get_frequency = scmi_clock_get_frequency;
	sc->sc_cd.cd_set_frequency = scmi_clock_set_frequency;
	clock_register(&sc->sc_cd);
}

void
scmi_clock_enable(void *cookie, uint32_t *cells, int on)
{
	struct scmi_softc *sc = cookie;
	volatile struct scmi_shmem *shmem = sc->sc_shmem;
	uint32_t idx = cells[0];

	scmi_message_header(shmem, SCMI_CLOCK, SCMI_CLOCK_CONFIG_SET);
	shmem->length = 3 * sizeof(uint32_t);
	shmem->message_payload[0] = idx;
	shmem->message_payload[1] = on ? SCMI_CLOCK_CONFIG_SET_ENABLE : 0;
	scmi_command(sc);
}

uint32_t
scmi_clock_get_frequency(void *cookie, uint32_t *cells)
{
	struct scmi_softc *sc = cookie;
	volatile struct scmi_shmem *shmem = sc->sc_shmem;
	uint32_t idx = cells[0];
	int32_t status;

	scmi_message_header(shmem, SCMI_CLOCK, SCMI_CLOCK_RATE_GET);
	shmem->length = 2 * sizeof(uint32_t);
	shmem->message_payload[0] = idx;
	status = scmi_command(sc);
	if (status != SCMI_SUCCESS)
		return 0;
	if (shmem->message_payload[2] != 0)
		return 0;

	return shmem->message_payload[1];
}

int
scmi_clock_set_frequency(void *cookie, uint32_t *cells, uint32_t freq)
{
	struct scmi_softc *sc = cookie;
	volatile struct scmi_shmem *shmem = sc->sc_shmem;
	uint32_t idx = cells[0];
	int32_t status;

	scmi_message_header(shmem, SCMI_CLOCK, SCMI_CLOCK_RATE_SET);
	shmem->length = 5 * sizeof(uint32_t);
	shmem->message_payload[0] = 0;
	shmem->message_payload[1] = idx;
	shmem->message_payload[2] = freq;
	shmem->message_payload[3] = 0;
	status = scmi_command(sc);
	if (status != SCMI_SUCCESS)
		return -1;

	return 0;
}
