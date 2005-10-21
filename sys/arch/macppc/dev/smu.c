/*	$OpenBSD: smu.c,v 1.2 2005/10/21 22:07:45 kettenis Exp $	*/

/*
 * Copyright (c) 2005 Mark Kettenis
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
#include <sys/types.h>

#include <machine/autoconf.h>

#include <dev/clock_subr.h>
#include <dev/ofw/openfirm.h>

int     smu_match(struct device *, void *, void *);
void    smu_attach(struct device *, struct device *, void *);

struct smu_softc {
        struct device   sc_dev;

	/* SMU command buffer. */
        bus_dma_tag_t   sc_dmat;
        bus_dmamap_t    sc_cmdmap;
        bus_dma_segment_t sc_cmdseg[1];
        caddr_t         sc_cmd;

	/* Doorbell and mailbox. */
	struct ppc_bus_space sc_mem_bus_space;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_gpioh;
	bus_space_handle_t sc_buffh;
};

struct cfattach smu_ca = {
        sizeof(struct smu_softc), smu_match, smu_attach
};
struct cfdriver smu_cd = {
        NULL, "smu", DV_DULL,
};

/* SMU command. */
struct smu_cmd {
        u_int8_t        cmd;
        u_int8_t        len;
        u_int8_t        data[254];
};
#define SMU_CMDSZ       sizeof(struct smu_cmd)

/* Real Time Clock. */
#define SMU_RTC			0x8e
#define SMU_RTC_SET_DATETIME	0x80
#define SMU_RTC_GET_DATETIME	0x81

int	smu_do_cmd(struct smu_softc *, int);
int	smu_time_read(time_t *);
int	smu_time_write(time_t);

#define GPIO_DDR        0x04    /* Data direction */
#define GPIO_DDR_OUTPUT 0x04    /* Output */
#define GPIO_DDR_INPUT  0x00    /* Input */

#define GPIO_LEVEL	0x02	/* Pin level (RO) */

#define GPIO_DATA       0x01    /* Data */

int
smu_match(struct device *parent, void *cf, void *aux)
{
        struct confargs *ca = aux;

        if (strcmp(ca->ca_name, "smu") == 0)
                return (1);
        return (0);
}

/* XXX */
extern struct powerpc_bus_dma_tag pci_bus_dma_tag;

void
smu_attach(struct device *parent, struct device *self, void *aux)
{
        struct smu_softc *sc = (struct smu_softc *)self;
	int nseg;

	/* XXX */
	sc->sc_mem_bus_space.bus_base = 0x80000000;
	sc->sc_mem_bus_space.bus_size = 0;
	sc->sc_mem_bus_space.bus_io = 0;
	sc->sc_memt = &sc->sc_mem_bus_space;

	/* XXX Should get this from OF or macgpio. */
	if (bus_space_map(sc->sc_memt, 0x50 + 0x12, 1, 0, &sc->sc_gpioh)) {
		printf(": cannot map smu-doorbell gpio\n");
		return;
	}

	/* XXX Should get this from OF. */
	if (bus_space_map(sc->sc_memt, 0x860c, 4, 0, &sc->sc_buffh)) {
		printf(": cannot map smu-doorbell buffer\n");
		return;
	}

	/* XXX */
        sc->sc_dmat = &pci_bus_dma_tag;

	/* Allocate and map SMU command buffer.  */
	if (bus_dmamem_alloc(sc->sc_dmat, SMU_CMDSZ, 0, 0,
            sc->sc_cmdseg, 1, &nseg, BUS_DMA_NOWAIT)) {
                printf(": cannot allocate cmd buffer\n");
                return;
        }
        if (bus_dmamem_map(sc->sc_dmat, sc->sc_cmdseg, nseg,
            SMU_CMDSZ, &sc->sc_cmd, BUS_DMA_NOWAIT)) {
                printf(": cannot map cmd buffer\n");
                bus_dmamem_free(sc->sc_dmat, sc->sc_cmdseg, 1);
                return;
        }
        if (bus_dmamap_create(sc->sc_dmat, SMU_CMDSZ, 1, SMU_CMDSZ, 0,
            BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &sc->sc_cmdmap)) {
                printf(": cannot create cmd dmamap\n");
                bus_dmamem_unmap(sc->sc_dmat, sc->sc_cmd, SMU_CMDSZ);
                bus_dmamem_free(sc->sc_dmat, sc->sc_cmdseg, 1);
                return;
        }
        if (bus_dmamap_load(sc->sc_dmat, sc->sc_cmdmap, sc->sc_cmd,
            SMU_CMDSZ, NULL, BUS_DMA_NOWAIT)) {
                printf(": cannot load cmd dmamap\n");
                bus_dmamap_destroy(sc->sc_dmat, sc->sc_cmdmap);
                bus_dmamem_unmap(sc->sc_dmat, sc->sc_cmd, SMU_CMDSZ);
                bus_dmamem_free(sc->sc_dmat, sc->sc_cmdseg, nseg);
                return;
        }

	/* Initialize global variables that control RTC functionality. */
	time_read = smu_time_read;
	time_write = smu_time_write;

	printf("\n");
}

int
smu_do_cmd(struct smu_softc *sc, int timo)
{
	struct smu_cmd *cmd = (struct smu_cmd *)sc->sc_cmd;
	u_int8_t gpio, ack = ~cmd->cmd;

	/* Write to mailbox.  */
	bus_space_write_4(sc->sc_memt, sc->sc_buffh, 0,
	    sc->sc_cmdmap->dm_segs->ds_addr);

	/* Flush to RAM. */
	asm __volatile__ ("dcbst 0,%0; sync" :: "r"(sc->sc_cmd): "memory");

	/* Ring doorbell.  */
	bus_space_write_1(sc->sc_memt, sc->sc_gpioh, 0, GPIO_DDR_OUTPUT);
	do {
		gpio = bus_space_read_1(sc->sc_memt, sc->sc_gpioh, 0);
		timo -= 50;
		if (timo < 0)
			return (ETIMEDOUT);
		delay(50 * 1000);
	} while (!(gpio & (GPIO_DATA)));

	/* CPU might have brought back the cache line. */
	asm __volatile__ ("dcbf 0,%0; sync" :: "r"(sc->sc_cmd) : "memory");

	if (cmd->cmd != ack)
		return (EIO);
	return (0);
}

int
smu_time_read(time_t *secs)
{
	struct smu_softc *sc = smu_cd.cd_devs[0];
	struct smu_cmd *cmd = (struct smu_cmd *)sc->sc_cmd;
	struct clock_ymdhms dt;
	int error;

	cmd->cmd = SMU_RTC;
	cmd->len = 1;
	cmd->data[0] = SMU_RTC_GET_DATETIME;
	error = smu_do_cmd(sc, 800);
	if (error) {
		*secs = 0;
		return (error);
	}

	dt.dt_year = 2000 + FROMBCD(cmd->data[6]);
	dt.dt_mon = FROMBCD(cmd->data[5]);
	dt.dt_day = FROMBCD(cmd->data[4]);
	dt.dt_hour = FROMBCD(cmd->data[2]);
	dt.dt_min = FROMBCD(cmd->data[1]);
	dt.dt_sec = FROMBCD(cmd->data[0]);
	*secs = clock_ymdhms_to_secs(&dt);
	return (0);
}

int
smu_time_write(time_t secs)
{
	struct smu_softc *sc = smu_cd.cd_devs[0];
	struct smu_cmd *cmd = (struct smu_cmd *)sc->sc_cmd;
	struct clock_ymdhms dt;

	clock_secs_to_ymdhms(secs, &dt);

	cmd->cmd = SMU_RTC;
	cmd->len = 8;
	cmd->data[0] = SMU_RTC_SET_DATETIME;
	cmd->data[1] = TOBCD(dt.dt_sec);
	cmd->data[2] = TOBCD(dt.dt_min);
	cmd->data[3] = TOBCD(dt.dt_hour);
	cmd->data[4] = TOBCD(dt.dt_wday);
	cmd->data[5] = TOBCD(dt.dt_day);
	cmd->data[6] = TOBCD(dt.dt_mon);
	cmd->data[7] = TOBCD(dt.dt_year - 2000);

	return (smu_do_cmd(sc, 800));
}
