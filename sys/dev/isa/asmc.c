/*	$OpenBSD: asmc.c,v 1.13 2015/10/29 13:29:04 jung Exp $	*/
/*
 * Copyright (c) 2015 Joerg Jung <jung@openbsd.org>
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

/*
 * Driver for Apple's System Management Controller (SMC)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/task.h>
#include <sys/sensors.h>

#include <machine/bus.h>

#include <dev/isa/isavar.h>

#define ASMC_BASE	0x300	/* SMC base address */
#define ASMC_IOSIZE	32	/* I/O region size 0x300-0x31f */

#define ASMC_DATA	0x00	/* SMC data port offset */
#define ASMC_COMMAND	0x04	/* SMC command port offset */
#define ASMC_STATUS	0x1e	/* SMC status port offset */
#define ASMC_INTERRUPT	0x1f	/* SMC interrupt port offset */

#define ASMC_READ	0x10	/* SMC read command */
#define ASMC_WRITE	0x11	/* SMC write command */
#define ASMC_INFO	0x13	/* SMC info/type command */

#define ASMC_RETRY	10
#define ASMC_MAXLEN	32	/* SMC maximum data size len */
#define ASMC_NOTFOUND	0x84	/* SMC status key not found */

#define ASMC_MAXTEMP	101	/* known asmc_prods temperature sensor keys */
#define ASMC_MAXFAN	10	/* fan keys with digits 0-9 */
#define ASMC_MAXLIGHT	2	/* left and right light sensor */
#define ASMC_MAXMOTION	3	/* x y z axis motion sensors */

struct asmc_prod {
	const char	*pr_name;
	const char	*pr_temp[ASMC_MAXTEMP];
};

struct asmc_softc {
	struct device		 sc_dev;

	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;

	struct asmc_prod	*sc_prod;
	uint8_t			 sc_init;	/* initialization done? */
	uint8_t			 sc_nfans;	/* number of fans */
	uint8_t			 sc_lightlen;	/* light data len */

	struct taskq		*sc_taskq;
	struct task		 sc_task_init;
	struct task		 sc_task_refresh;

	struct ksensor		 sc_sensor_temp[ASMC_MAXTEMP];
	struct ksensor		 sc_sensor_fan[ASMC_MAXFAN];
	struct ksensor		 sc_sensor_light[ASMC_MAXLIGHT];
	struct ksensor		 sc_sensor_motion[ASMC_MAXMOTION];
	struct ksensordev	 sc_sensor_dev;
	struct sensor_task	*sc_sensor_task;
};

uint8_t	asmc_status(struct asmc_softc *);
int	asmc_try(struct asmc_softc *, int, const char *, uint8_t *, uint8_t);
void	asmc_kbdled(struct asmc_softc *, uint8_t);

void	asmc_init(void *);
void	asmc_refresh(void *);
void	asmc_update(void *);

int	asmc_match(struct device *, void *, void *);
void	asmc_attach(struct device *, struct device *, void *);
int 	asmc_detach(struct device *, int);

const struct cfattach asmc_ca = {
	sizeof(struct asmc_softc), asmc_match, asmc_attach
};

struct cfdriver asmc_cd = {
	NULL, "asmc", DV_DULL
};

static struct asmc_prod asmc_prods[] = {
	{ "MacBookAir", {
		"TB0T", "TB1S", "TB1T", "TB2S", "TB2T", "TC0C", "TC0D", "TC0E",
		"TC0F", "TC0P", "TC1C", "TC1E", "TC2C", "TCFP", "TCGC", "TCSA",
		"TCZ3", "TCZ4", "TCZ5", "TG0E", "TG1E", "TG2E", "TGZ3", "TGZ4",
		"TGZ5", "THSP", "TM0P", "TN0D", "TPCD", "TTF0", "TV0P", "TVFP",
		"TW0P", "Ta0P", "Th0H", "Th0P", "Th1H", "Tm0P", "Tm1P", "Tp0P",
		"Tp1P", "TpFP", "Ts0P", "Ts0S", NULL }
	},
	{ "MacBookPro", {
		"TA0P", "TALP", "TB0T", "TB1T", "TB2T", "TB3T", "TBXT", "TC0C",
		"TC0D", "TC0E", "TC0F", "TC0P", "TC1C", "TC2C", "TC3C", "TC4C",
		"TCFC", "TCGC", "TCSA", "TCTD", "TCXC", "TG0D", "TG0F", "TG0H",
		"TG0P", "TG0T", "TG1D", "TG1F", "TG1H", "TG1d", "TH0A", "TH0B",
		"TH0F", "TH0R", "TH0V", "TH0a", "TH0b", "TH0c", "THSP", "TM0P",
		"TM0S", "TMBS", "TMCD", "TN0D", "TN0P", "TN0S", "TN1D", "TN1F",
		"TN1G", "TN1S", "TP0P", "TPCD", "TTF0", "TW0P", "Ta0P", "TaSP",
		"Th0H", "Th1H", "Th2H", "Tm0P", "Ts0P", "Ts0S", "Ts1P", "Ts1S",
		NULL }
	},
	{ "MacBook", {
		"TB0T", "TB1T", "TB2T", "TB3T", "TC0D", "TC0P", "TM0P", "TN0D",
		"TN0P", "TN1P", "TTF0", "TW0P", "Th0H", "Th0S", "Th1H", "ThFH",
		"Ts0P", "Ts0S", NULL }
	},
	{ "MacPro", {
		"TA0P", "TC0C", "TC0D", "TC0P", "TC1C", "TC1D", "TC2C", "TC2D",
		"TC3C", "TC3D", "TCAC", "TCAD", "TCAG", "TCAH", "TCAS", "TCBC",
		"TCBD", "TCBG", "TCBH", "TCBS", "TH0P", "TH1F", "TH1P", "TH1V",
		"TH2F", "TH2P", "TH2V", "TH3F", "TH3P", "TH3V", "TH4F", "TH4P",
		"TH4V", "THPS", "THTG", "TM0P", "TM0S", "TM1P", "TM1S", "TM2P",
		"TM2S", "TM2V", "TM3P", "TM3S", "TM3V", "TM4P", "TM5P", "TM6P",
		"TM6V", "TM7P", "TM7V", "TM8P", "TM8S", "TM8V", "TM9P", "TM9S",
		"TM9V", "TMA1", "TMA2", "TMA3", "TMA4", "TMAP", "TMAS", "TMB1",
		"TMB2", "TMB3", "TMB4", "TMBS", "TMHS", "TMLS", "TMPS", "TMPV",
		"TMTG", "TN0C", "TN0D", "TN0H", "TNTG", "TS0C", "Te1F", "Te1P",
		"Te1S", "Te2F", "Te2S", "Te3F", "Te3S", "Te4F", "Te4S", "Te5F",
		"Te5S", "TeGG", "TeGP", "TeRG", "TeRP", "TeRV", "Tp0C", "Tp1C",
		"TpPS", "TpTG", "Tv0S", "Tv1S", NULL }
	},
	{ "MacMini", {
		"TC0D", "TC0H", "TC0P", "TH0P", "TN0D", "TN0P", "TN1P", "TW0P",
		NULL }
	},
	{ "iMac", {
		"TA0P", "TC0D", "TC0H", "TC0P", "TG0D", "TG0H", "TG0P", "TH0P",
		"TL0P", "TN0D", "TN0H", "TN0P", "TO0P", "TW0P", "Tm0P", "Tp0C",
		"Tp0P", NULL }
	},
	{ NULL, { NULL } }
};

static const char *asmc_temp_desc[][2] = {
	{ "TA0P", "Ambient" }, { "TA0P", "HDD Bay 1" },
	{ "TA0S", "PCI Slot 1 Pos 1" }, { "TA1P", "Ambient 2" },
	{ "TA1S", "PCI Slot 1 Pos 2" }, { "TA2S", "PCI Slot 2 Pos 1" },
	{ "TA3S", "PCI Slot 2 Pos 2" },
	{ "TB0T", "Enclosure Bottom" }, { "TB1T", "Enclosure Bottom 2" },
	{ "TB2T", "Enclosure Bottom 3" }, { "TB3T", "Enclosure Bottom 4" },
	{ "TC0D", "CPU0 Die Core" }, { "TC0H", "CPU0 Heatsink" },
	{ "TC0P", "CPU0 Proximity" },
	{ "TC1D", "CPU1" }, { "TC2D", "CPU2" }, { "TC3D", "CPU3" },
	{ "TCAH", "CPU0" }, { "TCBH", "CPU1" }, { "TCCH", "CPU2" },
	{ "TCDH", "CPU3" },
	{ "TG0D", "GPU0 Diode" }, { "TG0H", "GPU0 Heatsink" },
	{ "TG0P", "GPU0 Proximity" },
	{ "TG1H", "GPU Heatsink 2" },
	{ "TH0P", "HDD Bay 1" }, { "TH1P", "HDD Bay 2" },
	{ "TH2P", "HDD Bay 3" }, { "TH3P", "HDD Bay 4" },
	{ "TL0P", "LCD Proximity"},
	{ "TM0P", "Mem Bank A1" }, { "TM0S", "Mem module A1" },
	{ "TM1P", "Mem Bank A2" }, { "TM1S", "Mem module A2" },
	{ "TM2P", "Mem Bank A3" }, { "TM2S", "Mem module A3" },
	{ "TM3P", "Mem Bank A4" }, { "TM3S", "Mem module A4" },
	{ "TM4P", "Mem Bank A5" }, { "TM4S", "Mem module A5" },
	{ "TM5P", "Mem Bank A6" }, { "TM5S", "Mem module A6" },
	{ "TM6P", "Mem Bank A7" }, { "TM6S", "Mem module A7" },
	{ "TM7P", "Mem Bank A8" }, { "TM7S", "Mem module A8" },
	{ "TM8P", "Mem Bank B1" }, { "TM8S", "Mem module B1" },
	{ "TM9P", "Mem Bank B2" }, { "TM9S", "Mem module B2" },
	{ "TMA1", "RAM A1" }, { "TMA2", "RAM A2" },
	{ "TMA3", "RAM A3" }, { "TMA4", "RAM A4" },
	{ "TMB1", "RAM B1" }, { "TMB2", "RAM B2" },
	{ "TMB3", "RAM B3" }, { "TMB4", "RAM B4" },
	{ "TMAP", "Mem Bank B3" }, { "TMAS", "Mem module B3" },
	{ "TMBP", "Mem Bank B4" }, { "TMBS", "Mem module B4" },
	{ "TMCP", "Mem Bank B5" }, { "TMCS", "Mem module B5" },
	{ "TMDP", "Mem Bank B6" }, { "TMDS", "Mem module B6" },
	{ "TMEP", "Mem Bank B7" }, { "TMES", "Mem module B7" },
	{ "TMFP", "Mem Bank B8" }, { "TMFS", "Mem module B8" },
	{ "TN0D", "Northbridge Die Core" }, { "TN0H", "Northbridge" },
	{ "TN0P", "Northbridge Proximity" }, { "TN1P", "Northbridge 2" },
	{ "TO0P", "Optical Drive" }, { "TS0C", "Expansion Slots" },
	{ "TW0P", "Wireless Airport Card" },
	{ "Th0H", "Main Heatsink A" }, { "Th1H", "Main Heatsink B" },
	{ "Th2H", "Main Heatsink C" },
	{ "Tm0P", "Memory Controller" },
	{ "Tp0C", "Power supply 1" }, { "Tp0P", "Power supply 1" },
	{ "Tp1C", "Power supply 2" }, { "Tp1P", "Power supply 2" },
	{ "Tp2P", "Power supply 3" }, { "Tp3P", "Power supply 4" },
	{ "Tp4P", "Power supply 5" }, { "Tp5P", "Power supply 6" },
	{ NULL, NULL }
};

static const char *asmc_fan_loc[] = {
	"left lower front", "center lower front", "right lower front",
	"left mid front",   "center mid front",   "right mid front",
	"left upper front", "center upper front", "right upper front",
	"left lower rear",  "center lower rear",  "right lower rear",
	"left mid rear",    "center mid rear",    "right mid rear",
	"left upper rear",  "center upper rear",  "right upper rear"
};

static const char *asmc_light_desc[ASMC_MAXLIGHT] = {
	"left", "right"
};

extern char *hw_vendor, *hw_prod;

int
asmc_match(struct device *parent, void *match, void *aux)
{
	struct asmc_softc *sc = match;
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;
	int i;

	if (!hw_vendor || !hw_prod || strncmp(hw_vendor, "Apple", 5))
		return 0;

	for (i = 0; asmc_prods[i].pr_name && !sc->sc_prod; i++)
		if (!strncasecmp(asmc_prods[i].pr_name, hw_prod,
		    strlen(asmc_prods[i].pr_name)))
			sc->sc_prod = &asmc_prods[i];
	if (!sc->sc_prod)
		return 0;

	if (ia->ia_iobase != ASMC_BASE ||
	    bus_space_map(ia->ia_iot, ia->ia_iobase, ASMC_IOSIZE, 0, &ioh))
		return 0;

	bus_space_unmap(ia->ia_iot, ioh, ASMC_IOSIZE);
	ia->ia_iosize = ASMC_IOSIZE;
	ia->ipa_nio = 1;
	ia->ipa_nmem = 0;
	ia->ipa_nirq = 0;
	ia->ipa_ndrq = 0;

	return 1;
}

void
asmc_attach(struct device *parent, struct device *self, void *aux)
{
	struct asmc_softc *sc = (struct asmc_softc *)self;
	struct isa_attach_args *ia = aux;
	uint8_t buf[6];
	int i;

	if (bus_space_map(ia->ia_iot, ia->ia_iobase, ia->ia_iosize, 0,
	    &sc->sc_ioh)) {
		printf(": can't map i/o space\n");
		return;
	}
	sc->sc_iot = ia->ia_iot;

	if (asmc_try(sc, ASMC_READ, "REV ", buf, 6)) {
		printf(": revision failed (0x%x)\n", asmc_status(sc));
		bus_space_unmap(ia->ia_iot, ia->ia_iobase, ASMC_IOSIZE);
		return;
	}
	printf(": rev %x.%x%x%x", buf[0], buf[1], buf[2],
	    ntohs(*(uint16_t *)buf + 4));

	if (asmc_try(sc, ASMC_READ, "#KEY", buf, 4)) {
		printf(", no of keys failed (0x%x)\n", asmc_status(sc));
		bus_space_unmap(ia->ia_iot, ia->ia_iobase, ASMC_IOSIZE);
		return;
	}
	printf(", %u key%s\n", ntohl(*(uint32_t *)buf),
	    (ntohl(*(uint32_t *)buf) == 1) ? "" : "s");

	asmc_kbdled(sc, 127);

	if (!(sc->sc_taskq = taskq_create("asmc", 1, IPL_NONE, 0))) {
		printf("%s: can't create task queue\n", sc->sc_dev.dv_xname);
		bus_space_unmap(ia->ia_iot, ia->ia_iobase, ASMC_IOSIZE);
		return;
	}
	task_set(&sc->sc_task_init, asmc_init, sc);
	task_set(&sc->sc_task_refresh, asmc_refresh, sc);

	strlcpy(sc->sc_sensor_dev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensor_dev.xname));
	for (i = 0; i < ASMC_MAXTEMP; i++) {
		sc->sc_sensor_temp[i].type = SENSOR_TEMP;
		sc->sc_sensor_temp[i].flags |= SENSOR_FINVALID;
		sc->sc_sensor_temp[i].flags |= SENSOR_FUNKNOWN;
	}
	for (i = 0; i < ASMC_MAXFAN; i++) {
		sc->sc_sensor_fan[i].type = SENSOR_FANRPM;
		sc->sc_sensor_fan[i].flags |= SENSOR_FINVALID;
		sc->sc_sensor_fan[i].flags |= SENSOR_FUNKNOWN;
	}
	for (i = 0; i < ASMC_MAXLIGHT; i++) {
		sc->sc_sensor_light[i].type = SENSOR_LUX;
		sc->sc_sensor_light[i].flags |= SENSOR_FINVALID;
		sc->sc_sensor_light[i].flags |= SENSOR_FUNKNOWN;
	}
	for (i = 0; i < ASMC_MAXMOTION; i++) {
		sc->sc_sensor_motion[i].type = SENSOR_ACCEL;
		sc->sc_sensor_motion[i].flags |= SENSOR_FINVALID;
		sc->sc_sensor_motion[i].flags |= SENSOR_FUNKNOWN;
	}
	if (!(sc->sc_sensor_task = sensor_task_register(sc, asmc_update, 5))) {
		printf("%s: unable to register task\n", sc->sc_dev.dv_xname);
		taskq_destroy(sc->sc_taskq);
		bus_space_unmap(ia->ia_iot, ia->ia_iobase, ASMC_IOSIZE);
		return;
	}
	sensordev_install(&sc->sc_sensor_dev);
	task_add(sc->sc_taskq, &sc->sc_task_init);
}

int
asmc_detach(struct device *self, int flags)
{
	struct asmc_softc *sc = (struct asmc_softc *)self;
	int i;

	if (sc->sc_sensor_task) {
		sensor_task_unregister(sc->sc_sensor_task);
		sc->sc_sensor_task = NULL;
	}
	sensordev_deinstall(&sc->sc_sensor_dev);
	for (i = 0; i < ASMC_MAXMOTION; i++)
		sensor_detach(&sc->sc_sensor_dev, &sc->sc_sensor_motion[i]);
	for (i = 0; i < ASMC_MAXLIGHT; i++)
		sensor_detach(&sc->sc_sensor_dev, &sc->sc_sensor_light[i]);
	for (i = 0; i < ASMC_MAXFAN; i++)
		sensor_detach(&sc->sc_sensor_dev, &sc->sc_sensor_fan[i]);
	for (i = 0; i < ASMC_MAXTEMP; i++)
		sensor_detach(&sc->sc_sensor_dev, &sc->sc_sensor_temp[i]);

	if (sc->sc_taskq) {
		task_del(sc->sc_taskq, &sc->sc_task_refresh);
		task_del(sc->sc_taskq, &sc->sc_task_init);
		taskq_destroy(sc->sc_taskq);
	}

	asmc_kbdled(sc, 0);

	return 0;
}

uint8_t
asmc_status(struct asmc_softc *sc)
{
	return bus_space_read_1(sc->sc_iot, sc->sc_ioh, ASMC_STATUS);
}

static int
asmc_wait(struct asmc_softc *sc, uint8_t m)
{
	int us;

	for (us = (2 << 4); us < (2 << 16); us *= 2) { /* wait up to 128 ms */
		(!sc->sc_sensor_task) ? delay(us) :
		    tsleep(&sc->sc_taskq, 0, "asmc",
		        (us * hz + 999999) / 1000000 + 1);
		if (bus_space_read_1(sc->sc_iot, sc->sc_ioh, ASMC_COMMAND) & m)
			return 1;
	}
	return 0;
}

static int
asmc_write(struct asmc_softc *sc, uint8_t off, uint8_t val)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, off, val);
	if (asmc_wait(sc, 0x04)) /* write accepted? */
		return 0;
	return 1;
}

static int
asmc_read(struct asmc_softc *sc, uint8_t off, uint8_t *buf)
{
	if (asmc_wait(sc, 0x01)) { /* ready for read? */
		*buf = bus_space_read_1(sc->sc_iot, sc->sc_ioh, off);
		return 0;
	}
	return 1;
}

static int
asmc_command(struct asmc_softc *sc, int cmd, const char *key, uint8_t *buf,
    uint8_t len)
{
	uint8_t n;
	int i;

	if (len > ASMC_MAXLEN)
		return 1;
	if (asmc_write(sc, ASMC_COMMAND, cmd))
		return 1;
	for (i = 0; i < 4; i++)
		if (asmc_write(sc, ASMC_DATA, key[i]))
			return 1;
	if (asmc_write(sc, ASMC_DATA, len))
		return 1;
	if (cmd == ASMC_READ || cmd == ASMC_INFO) {
		for (i = 0; i < len; i++)
			if (asmc_read(sc, ASMC_DATA, &buf[i]))
				return 1;
		for (i = 0; i < ASMC_MAXLEN; i++) /* sanity flush */
			if (asmc_read(sc, ASMC_DATA, &n))
				break;
	} else if (cmd == ASMC_WRITE) {
		for (i = 0; i < len; i++)
			if (asmc_write(sc, ASMC_DATA, buf[i]))
				return 1;
	} else
		return 1;
	return 0;
}

int
asmc_try(struct asmc_softc *sc, int cmd, const char *key, uint8_t *buf,
    uint8_t len)
{
	int i;

	for (i = 0; i < ASMC_RETRY; i++)
		if (!asmc_command(sc, cmd, key, buf, len))
			return 0;
	return 1;
}

static uint32_t
asmc_uk(uint8_t *buf)
{
	/* spe78: floating point, signed, 7 bits exponent, 8 bits fraction */
	return (((int16_t)ntohs(*(uint16_t *)buf)) >> 8) * 1000000 + 273150000;
}

static uint16_t
asmc_rpm(uint8_t *buf)
{
	/* fpe2: floating point, unsigned, 14 bits exponent, 2 bits fraction */
	return ntohs(*(uint16_t *)buf) >> 2;
}

static uint32_t
asmc_lux(uint8_t *buf, uint8_t lightlen)
{
	/* newer macbooks report a 10 bit big endian value */
	return (lightlen == 10) ?
	    /* fp18.14: floating point, 18 bits exponent, 14 bits fraction */
	    (ntohl(*(uint32_t *)(buf + 6)) >> 14) * 1000000 :
	    /*
	     * todo: calculate lux from ADC raw data
	     * buf[1] true/false for high/low gain chan reads
	     * chan 0: ntohs(*(uint16_t *)(buf + 2));
	     * chan 1: ntohs(*(uint16_t *)(buf + 4));
	     */
	    ntohs(*(uint16_t *)(buf + 2)) * 1000000;
}

static int
asmc_temp(struct asmc_softc *sc, uint8_t idx)
{
	uint8_t buf[2];
	int i;

	if (asmc_try(sc, ASMC_READ, sc->sc_prod->pr_temp[idx], buf, 2))
		return 1;
	sc->sc_sensor_temp[idx].value = asmc_uk(buf);
	sc->sc_sensor_temp[idx].flags &= ~SENSOR_FUNKNOWN;

	if (sc->sc_init)
		return 0;

	strlcpy(sc->sc_sensor_temp[idx].desc, sc->sc_prod->pr_temp[idx],
	    sizeof(sc->sc_sensor_temp[idx].desc));
	for (i = 0; asmc_temp_desc[i][0]; i++)
		if (!strcmp(asmc_temp_desc[i][0], sc->sc_prod->pr_temp[idx]))
			break;
	if (asmc_temp_desc[i][0]) {
		strlcat(sc->sc_sensor_temp[idx].desc, " ",
		    sizeof(sc->sc_sensor_temp[idx].desc));
		strlcat(sc->sc_sensor_temp[idx].desc, asmc_temp_desc[i][1],
		    sizeof(sc->sc_sensor_temp[idx].desc));
	}
	sc->sc_sensor_temp[idx].flags &= ~SENSOR_FINVALID;
	sensor_attach(&sc->sc_sensor_dev, &sc->sc_sensor_temp[idx]);
	return 0;
}

static int
asmc_fan(struct asmc_softc *sc, uint8_t idx)
{
	char key[5];
	uint8_t buf[16], *end;

	snprintf(key, sizeof(key), "F%dAc", idx);
	if (asmc_try(sc, ASMC_READ, key, buf, 2))
		return 1;
	sc->sc_sensor_fan[idx].value = asmc_rpm(buf);
	sc->sc_sensor_fan[idx].flags &= ~SENSOR_FUNKNOWN;

	if (sc->sc_init)
		return 0;

	snprintf(key, sizeof(key), "F%dID", idx);
	if (asmc_try(sc, ASMC_READ, key, buf, 16))
		return 1;
	end = buf + 4 + strlen((char *)buf + 4) - 1;
	while (buf + 4 < end && *end == ' ') /* trim trailing spaces */
		*end-- = '\0';
	strlcpy(sc->sc_sensor_fan[idx].desc, buf + 4,
	    sizeof(sc->sc_sensor_fan[idx].desc));
	if (buf[2] < nitems(asmc_fan_loc)) {
		strlcat(sc->sc_sensor_fan[idx].desc, " ",
		    sizeof(sc->sc_sensor_fan[idx].desc));
		strlcat(sc->sc_sensor_fan[idx].desc, asmc_fan_loc[buf[2]],
		    sizeof(sc->sc_sensor_fan[idx].desc));
	}
	sc->sc_sensor_fan[idx].flags &= ~SENSOR_FINVALID;
	sensor_attach(&sc->sc_sensor_dev, &sc->sc_sensor_fan[idx]);
	return 0;
}

static int
asmc_light(struct asmc_softc *sc, uint8_t idx)
{
	char key[5];
	uint8_t buf[10];

	snprintf(key, sizeof(key), "ALV%d", idx);
	if (!sc->sc_lightlen) {
		if (asmc_try(sc, ASMC_INFO, key, buf, 6))
			return 1;
		if ((sc->sc_lightlen = buf[0]) > 10)
			return 1;
	}
	if (asmc_try(sc, ASMC_READ, key, buf, sc->sc_lightlen))
		return 1;
	if (!buf[0]) /* valid data? */
		return 0;
	sc->sc_sensor_light[idx].value = asmc_lux(buf, sc->sc_lightlen);
	sc->sc_sensor_light[idx].flags &= ~SENSOR_FUNKNOWN;

	if (sc->sc_init)
		return 0;

	strlcpy(sc->sc_sensor_light[idx].desc, asmc_light_desc[idx],
	    sizeof(sc->sc_sensor_light[idx].desc));
	sc->sc_sensor_light[idx].flags &= ~SENSOR_FINVALID;
	sensor_attach(&sc->sc_sensor_dev, &sc->sc_sensor_light[idx]);
	return 0;
}

#if 0 /* todo: implement motion sensors update and initialization */
static int
asmc_motion(struct asmc_softc *sc, uint8_t idx)
{
	char key[5];
	uint8_t buf[2];

	snprintf(key, sizeof(key), "MO_%c", 88 + idx); /* X, Y, Z */
	if (asmc_try(sc, ASMC_READ, key, buf, 2))
		return 1;
	sc->sc_sensor_motion[idx].value = 0;
	sc->sc_sensor_motion[idx].flags &= ~SENSOR_FUNKNOWN;

	if (sc->sc_init)
		return 0;

	/* todo: setup and attach sensors and description */
	strlcpy(sc->sc_sensor_motion[idx].desc, 120 + idx, /* x, y, z */
	    sizeof(sc->sc_sensor_motion[idx].desc));
	strlcat(sc->sc_sensor_motion[idx].desc, "-axis",
	    sizeof(sc->sc_sensor_motion[idx].desc));
	sc->sc_sensor_motion[idx].flags &= ~SENSOR_FINVALID;
	sensor_attach(&sc->sc_sensor_dev, &sc->sc_sensor_motion[idx]);
	return 0;
}
#endif

void
asmc_kbdled(struct asmc_softc *sc, uint8_t b)
{
	uint8_t buf[2] = { b, 0 }, s;

	/* keyboard backlight led is optional */
	if (asmc_try(sc, ASMC_WRITE, "LKSB", buf, 2)) {
		if ((s = asmc_status(sc)) != ASMC_NOTFOUND)
			printf("%s: keyboard backlight failed (0x%x)\n",
			    sc->sc_dev.dv_xname, s);
	}
}

void
asmc_init(void *arg)
{
	struct asmc_softc *sc = arg;
	uint8_t buf[2], s;
	int i;

	/* number of temperature sensors depends on product */
	for (i = 0; sc->sc_prod->pr_temp[i] && i < ASMC_MAXTEMP; i++)
		if (asmc_temp(sc, i) && (s = asmc_status(sc)) != ASMC_NOTFOUND)
			printf("%s: read temp %d failed (0x%x)\n",
			    sc->sc_dev.dv_xname, i, s);
	/* number of fan sensors depends on product */
	if (asmc_try(sc, ASMC_READ, "FNum", buf, 1))
		printf("%s: read FNum failed (0x%x)\n", sc->sc_dev.dv_xname,
		    asmc_status(sc));
	else
		sc->sc_nfans = buf[0];
	for (i = 0; i < sc->sc_nfans && i < ASMC_MAXFAN; i++)
		if (asmc_fan(sc, i) && (s = asmc_status(sc)) != ASMC_NOTFOUND)
			printf("%s: read fan %d failed (0x%x)\n",
			    sc->sc_dev.dv_xname, i, s);
	/* left and right light sensors are optional */
	for (i = 0; i < ASMC_MAXLIGHT; i++)
		if (asmc_light(sc, i) &&
		    (s = asmc_status(sc)) != ASMC_NOTFOUND)
			printf("%s: read light %d failed (0x%x)\n",
			    sc->sc_dev.dv_xname, i, s);
	/* motion sensors are optional */
	if (asmc_try(sc, ASMC_READ, "MOCN", buf, 2) &&
	    (s = asmc_status(sc)) != ASMC_NOTFOUND)
		printf("%s: read MOCN failed (0x%x)\n",
		    sc->sc_dev.dv_xname, s);
#if 0 /* todo: initialize sudden motion sensors and setup interrupt handling */
	buf[0] = 0xe0, buf[1] = 0xf8;
	if (asmc_try(sc, ASMC_WRITE, "MOCN", buf, 2))
		printf("%s write MOCN failed (0x%x)\n", sc->sc_dev.dv_xname,
		    asmc_status(sc));
	for (i = 0; i < ASMC_MAXMOTION; i++)
		if (asmc_motion(sc, i) &&
		    (s = asmc_status(sc)) != ASMC_NOTFOUND)
			printf("%s: read motion %d failed (0x%x)\n",
			    sc->sc_dev.dv_xname, i, s);
#endif
	sc->sc_init = 1;
}

void
asmc_refresh(void *arg)
{
	struct asmc_softc *sc = arg;
	int i;

	for (i = 0; sc->sc_prod->pr_temp[i] && i < ASMC_MAXTEMP; i++)
		if (!(sc->sc_sensor_temp[i].flags & SENSOR_FINVALID))
			asmc_temp(sc, i);
	for (i = 0; i < sc->sc_nfans && i < ASMC_MAXFAN; i++)
		if (!(sc->sc_sensor_fan[i].flags & SENSOR_FINVALID))
			asmc_fan(sc, i);
	for (i = 0; i < ASMC_MAXLIGHT; i++)
		if (!(sc->sc_sensor_light[i].flags & SENSOR_FINVALID))
			asmc_light(sc, i);
#if 0
	for (i = 0; i < ASMC_MAXMOTION; i++)
		if (!(sc->sc_sensor_motion[i].flags & SENSOR_FINVALID))
			asmc_motion(sc, i);
#endif
}

void
asmc_update(void *arg)
{
	struct asmc_softc *sc = arg;

	if (sc->sc_init)
		task_add(sc->sc_taskq, &sc->sc_task_refresh);
}
