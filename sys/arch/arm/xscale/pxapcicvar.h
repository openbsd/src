/*	$OpenBSD: pxapcicvar.h,v 1.7 2005/12/14 15:08:51 uwe Exp $ */

/*
 * Copyright (c) 2005 Dale Rahn <drahn@openbsd.org>
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

struct pxapcic_socket {
        struct pxapcic_softc *sc;
        int socket;		/* socket number */
        struct device *pcmcia;
	struct proc *event_thread;

        int flags;
        int power_capability;	/* PXAPCIC_POWER_3V | PXAPCIC_POWER_5V */

	int irqpin;
	void *irq;

        void *pcictag_cookie;	/* opaque data for pcictag functions */
	struct pxapcic_tag *pcictag;
};

/* event */
#define PXAPCIC_EVENT_INSERTION	0
#define PXAPCIC_EVENT_REMOVAL	1

/* flags */
#define PXAPCIC_FLAG_CARDD	0
#define PXAPCIC_FLAG_CARDP	1

struct pxapcic_tag {
	u_int (*read)(struct pxapcic_socket *, int);
	void (*write)(struct pxapcic_socket *, int, u_int);
	void (*set_power)(struct pxapcic_socket *, int);
	void (*clear_intr)(struct pxapcic_socket *);
	void *(*intr_establish)(struct pxapcic_socket *, int,
	    int (*)(void *), void *);
	void (*intr_disestablish)(struct pxapcic_socket *, void *);
	const char *(*intr_string)(struct pxapcic_socket *);
};

/* pcictag registers and their values */
#define PXAPCIC_CARD_STATUS	0
#define  PXAPCIC_CARD_INVALID	0
#define  PXAPCIC_CARD_VALID	1
#define PXAPCIC_CARD_READY	1
#define PXAPCIC_CARD_POWER	2
#define  PXAPCIC_POWER_OFF	0
#define  PXAPCIC_POWER_3V	1
#define  PXAPCIC_POWER_5V	2
#define PXAPCIC_CARD_RESET	3

struct pxapcic_softc {
	struct device sc_dev;
	struct pxapcic_socket sc_socket[2];

        bus_space_tag_t sc_iot;
        bus_space_handle_t sc_memctl_ioh;

	void *sc_irq;
        int sc_shutdown;
	int sc_nslots;
	int sc_irqpin[2];
	int sc_irqcfpin[2];
};

void	pxapcic_attach(struct pxapcic_softc *,
    void (*socket_setup_hook)(struct pxapcic_socket *));
int	pxapcic_intr(void *);
