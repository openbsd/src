/*	$OpenBSD: samsabi.c,v 1.1 2026/05/26 03:51:11 mglocker Exp $ */

/*
 * Copyright (c) 2026 Marcus Glocker <mglocker@openbsd.org>
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
 * A driver to control the Samsung Advanced BIOS Interface (SABI), a Samsung
 * proprietary firmware/EC command interface on address 0x62.
 *
 * For now the driver supports:
 * 
 *  - Keyboard backlight control
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <dev/i2c/i2cvar.h>
#include <dev/wscons/wsconsio.h>

/* Debugging */
/* #define SAMSABI_DEBUG */
#ifdef SAMSABI_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

/* Keyboard control */
#define SAMSABI_KBD_CMD_SET		0x10
#define SAMSABI_KBD_CMD_GET		0x11
#define SAMSABI_KBD_MIN_BACKLIGHT	0
#define SAMSABI_KBD_MAX_BACKLIGHT	3

struct samsabi_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	int		sc_addr;

	unsigned int	sc_backlight;
	struct timeout	sc_to_keepalive;
};

int		samsabi_match(struct device *, void *, void *);
void		samsabi_attach(struct device *, struct device *, void *);
int		samsabi_activate(struct device *, int);

int		samsabi_cmd(struct samsabi_softc *, const uint8_t *, size_t,
		    uint8_t *, size_t);
int		samsabi_kbd_get(struct samsabi_softc *, uint8_t *);
int		samsabi_kbd_set(struct samsabi_softc *, uint8_t);
int		samsabi_kbd_probe(struct samsabi_softc *);
void		samsabi_kbd_keepalive(void *);
int		samsabi_get_backlight(struct wskbd_backlight *);
int		samsabi_set_backlight(struct wskbd_backlight *);
extern int	(*wskbd_get_backlight)(struct wskbd_backlight *);
extern int	(*wskbd_set_backlight)(struct wskbd_backlight *);

const struct cfattach samsabi_ca = {
	sizeof(struct samsabi_softc), samsabi_match, samsabi_attach, NULL,
	samsabi_activate
};

struct cfdriver samsabi_cd = {
	NULL, "samsabi", DV_DULL
};

int
samsabi_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "samsung,galaxybook-sabi") == 0)
		return 1;

	return 0;
}

void
samsabi_attach(struct device *parent, struct device *self, void *aux)
{
	struct samsabi_softc *sc = (struct samsabi_softc *)self;
	struct i2c_attach_args *ia = aux;
	int error;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	error = samsabi_kbd_probe(sc);
	if (error) {
		printf(": keyboard backlight probe error=%d\n", error);
		return;
	}

	wskbd_get_backlight = samsabi_get_backlight;
	wskbd_set_backlight = samsabi_set_backlight;

	timeout_set_proc(&sc->sc_to_keepalive, samsabi_kbd_keepalive, sc);

	printf(": %s\n", ia->ia_name);
}

int
samsabi_activate(struct device *self, int act)
{
	struct samsabi_softc *sc = (struct samsabi_softc *)self;

	switch (act) {
	case DVACT_QUIESCE:
		timeout_del(&sc->sc_to_keepalive);
		samsabi_kbd_set(sc, 0);
		break;
	case DVACT_WAKEUP:
		samsabi_kbd_set(sc, sc->sc_backlight);
		if (sc->sc_backlight != 0)
			timeout_add_sec(&sc->sc_to_keepalive, 1);
		break;
	}

	return 0;
}

int
samsabi_cmd(struct samsabi_softc *sc, const uint8_t *cmd, size_t cmdlen,
    uint8_t *rsp, size_t rsplen)
{
	int error;

	iic_acquire_bus(sc->sc_tag, 0);

	if (rsplen > 0) {
		/* write command and read response */
		error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, cmd, cmdlen, rsp, rsplen, 0);
	} else {
		/* write command */
		error = iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
		    sc->sc_addr, cmd, cmdlen, NULL, 0, 0);
	}

	iic_release_bus(sc->sc_tag, 0);

	return error;
}

int
samsabi_kbd_get(struct samsabi_softc *sc, uint8_t *level)
{
	uint8_t cmd = SAMSABI_KBD_CMD_GET;
	/*
	 * GET command response:
	 *     rsp[3] = Backlight brightness level (0-3)
	 */
	uint8_t rsp[8];
	int error;

	memset(rsp, 0, sizeof(rsp));

	error = samsabi_cmd(sc, &cmd, 1, rsp, sizeof(rsp));
	if (error)
		return error;

	DPRINTF(("%s: rsp=%02x %02x %02x %02x %02x %02x %02x %02x\n",
	    __func__, rsp[0], rsp[1], rsp[2], rsp[3], rsp[4], rsp[5], rsp[6],
	    rsp[7]));

	*level = rsp[3];

	return 0;
}

int
samsabi_kbd_set(struct samsabi_softc *sc, uint8_t level)
{
	/*
	 * SET command:
	 *     cmd[0] = 0x10
	 *     cmd[2] = Backlight brightness level:
	 *         0 = off, 1 = low, 2 = mid, 3 = high
	 */
	uint8_t cmd[3] = { SAMSABI_KBD_CMD_SET, 0, level };
	int error;

	error = samsabi_cmd(sc, cmd, sizeof(cmd), NULL, 0);
	if (error)
		return error;

	return 0;
}

int
samsabi_kbd_probe(struct samsabi_softc *sc)
{
	int error;
	uint8_t level;

	/* Check if we can get the level value */
	error = samsabi_kbd_get(sc, &level);
	if (error)
		return error;

	/* Make the keyboard light up once */
	error = samsabi_kbd_set(sc, SAMSABI_KBD_MAX_BACKLIGHT);
	if (error)
		return error;

	return 0;
}

void
samsabi_kbd_keepalive(void *arg)
{
	struct samsabi_softc *sc = arg;

	DPRINTF(("%s: level=%d\n", __func__, sc->sc_backlight));

	(void)samsabi_kbd_set(sc, sc->sc_backlight);

	timeout_add_sec(&sc->sc_to_keepalive, 1);
}

int
samsabi_get_backlight(struct wskbd_backlight *kbl)
{
	struct samsabi_softc *sc = samsabi_cd.cd_devs[0];

	KASSERT(sc != NULL);

	kbl->min = SAMSABI_KBD_MIN_BACKLIGHT;
	kbl->max = SAMSABI_KBD_MAX_BACKLIGHT;
	kbl->curval = sc->sc_backlight;

	return 0;
}

int
samsabi_set_backlight(struct wskbd_backlight *kbl)
{
	struct samsabi_softc *sc = samsabi_cd.cd_devs[0];
	int error;
	uint8_t level;

	KASSERT(sc != NULL);

	DPRINTF(("%s: curval=%d\n", __func__, kbl->curval));

	/*
	 * XXX Only full brightness can be held lit: the EC fades lower
	 * levels faster than the keepalive timeout can refresh them, so
	 * treat any non-zero request as full brightness for now.
	 */
	level = kbl->curval ? SAMSABI_KBD_MAX_BACKLIGHT : 0;

	error = samsabi_kbd_set(sc, level);
	if (error)
		return error;

	sc->sc_backlight = level;

	if (sc->sc_backlight == 0)
		timeout_del(&sc->sc_to_keepalive);
	else
		timeout_add_sec(&sc->sc_to_keepalive, 1);

	return 0;
}
