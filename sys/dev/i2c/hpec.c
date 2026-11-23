/*	$OpenBSD: hpec.c,v 1.1 2026/11/23 20:02:25 mglocker Exp $ */

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
 * A driver for the embedded controller found on the HP OmniBook X at
 * I2C address 0x28.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <dev/i2c/i2cvar.h>
#include <dev/wscons/wsconsio.h>

#define HPEC_IDX_KBDBL		0x97
#define HPEC_CMD_GET		0x02
#define HPEC_CMD_SET		0x03
#define HPEC_KBDBL_STATE_OFF	0x00
#define HPEC_KBDBL_STATE_HIGH	0x01
#define HPEC_KBDBL_STATE_LOW	0x02
#define HPEC_KBDBL_STATE_MAX	0x02

static const uint8_t hpec_kbdbl_map[] = {
	HPEC_KBDBL_STATE_OFF,
	HPEC_KBDBL_STATE_LOW,
	HPEC_KBDBL_STATE_HIGH
};

struct hpec_softc {
	struct device	sc_dev;
	i2c_tag_t	sc_tag;
	int		sc_addr;

	uint8_t		sc_kbdbl_state;
	struct timeout	sc_kbdbl_to_probe;
};

int		hpec_match(struct device *, void *, void *);
void		hpec_attach(struct device *, struct device *, void *);
int		hpec_activate(struct device *, int);

int		hpec_cmd(struct hpec_softc *, const uint8_t *, size_t,
		    uint8_t *, size_t);
int		hpec_get(struct hpec_softc *, uint8_t *);
int		hpec_set(struct hpec_softc *, uint8_t);
int		hpec_kbdbl_probe(struct hpec_softc *);
void		hpec_kbdbl_probe_done(void *);

int		hpec_kbdbl_get_backlight(struct wskbd_backlight *);
int		hpec_kbdbl_set_backlight(struct wskbd_backlight *);
extern int	(*wskbd_get_backlight)(struct wskbd_backlight *);
extern int	(*wskbd_set_backlight)(struct wskbd_backlight *);

const struct cfattach hpec_ca = {
	sizeof(struct hpec_softc), hpec_match, hpec_attach, NULL,
	hpec_activate
};

struct cfdriver hpec_cd = {
	NULL, "hpec", DV_DULL
};

int
hpec_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "hp,omnibook-x14-ec") == 0)
		return 1;

	return 0;
}

void
hpec_attach(struct device *parent, struct device *self, void *aux)
{
	struct hpec_softc *sc = (struct hpec_softc *)self;
	struct i2c_attach_args *ia = aux;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	if (hpec_kbdbl_probe(sc))
		return;

	wskbd_get_backlight = hpec_kbdbl_get_backlight;
	wskbd_set_backlight = hpec_kbdbl_set_backlight;

	printf("\n");
}

int
hpec_activate(struct device *self, int act)
{
	struct hpec_softc *sc = (struct hpec_softc *)self;

	switch (act) {
	case DVACT_QUIESCE:
		hpec_set(sc, HPEC_KBDBL_STATE_OFF);
		break;
	case DVACT_WAKEUP:
		hpec_set(sc, hpec_kbdbl_map[sc->sc_kbdbl_state]);
		break;
	}

	return 0;
}

int
hpec_cmd(struct hpec_softc *sc, const uint8_t *cmd, size_t cmdlen,
    uint8_t *rsp, size_t rsplen)
{
	int error;

	iic_acquire_bus(sc->sc_tag, 0);

	if (rsplen > 0) {
		/* read */
		error = iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, cmd, cmdlen, rsp, rsplen, 0);
	} else {
		/* write */
		error = iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
		    sc->sc_addr, cmd, cmdlen, NULL, 0, 0);
	}

	iic_release_bus(sc->sc_tag, 0);

	return error;
}

int
hpec_get(struct hpec_softc *sc, uint8_t *state)
{
	uint8_t cmd[4] =
	    { HPEC_CMD_GET, HPEC_IDX_KBDBL, 0x00, 0x01 };
	uint8_t rsp;
	int error;

	error = hpec_cmd(sc, cmd, sizeof(cmd), &rsp, sizeof(rsp));
	if (error)
		return error;

	*state = rsp;

	return 0;
}

int
hpec_set(struct hpec_softc *sc, uint8_t state)
{
	uint8_t cmd[5] =
	    { HPEC_CMD_SET, HPEC_IDX_KBDBL, 0x00, 0x01, state };
	int error;

	error = hpec_cmd(sc, cmd, sizeof(cmd), NULL, 0);
	if (error)
		return error;

	return 0;
}

int
hpec_kbdbl_probe(struct hpec_softc *sc)
{
	uint8_t state;
	int error;

	error = hpec_get(sc, &state);
	if (error) {
		printf(": kbd get failed\n");
		return error;
	}
	if (state > HPEC_KBDBL_STATE_MAX) {
		printf(": unexpected state 0x%02x\n", state);
		return ENXIO;
	}

	error = hpec_set(sc, HPEC_KBDBL_STATE_HIGH);
	if (error) {
		printf(": kbd set failed\n");
		return error;
	}

	/* Make keyboard blink during attach. */
	timeout_set_proc(&sc->sc_kbdbl_to_probe, hpec_kbdbl_probe_done, sc);
	timeout_add_sec(&sc->sc_kbdbl_to_probe, 1);

	return 0;
}

void
hpec_kbdbl_probe_done(void *arg)
{
	struct hpec_softc *sc = arg;

	hpec_set(sc, HPEC_KBDBL_STATE_OFF);
	sc->sc_kbdbl_state = 0;
}

int
hpec_kbdbl_get_backlight(struct wskbd_backlight *kbl)
{
	struct hpec_softc *sc = hpec_cd.cd_devs[0];

	KASSERT(sc != NULL);

	kbl->min = 0;
	kbl->max = nitems(hpec_kbdbl_map) - 1;
	kbl->curval = sc->sc_kbdbl_state;

	return 0;
}

int
hpec_kbdbl_set_backlight(struct wskbd_backlight *kbl)
{
	struct hpec_softc *sc = hpec_cd.cd_devs[0];
	int error;

	KASSERT(sc != NULL);

	if (kbl->curval >= nitems(hpec_kbdbl_map))
		return EINVAL;

	error = hpec_set(sc, hpec_kbdbl_map[kbl->curval]);
	if (error)
		return error;

	sc->sc_kbdbl_state = kbl->curval;

	return 0;
}
