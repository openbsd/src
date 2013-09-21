/*	$OpenBSD: mc68681var.h,v 1.2 2013/09/21 20:05:01 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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

#define	N68681PORTS		2

#define	A_PORT			0
#define	B_PORT			1

/* speed lookup table entry */
struct mc68681_s {
	int			 speed;
	uint8_t			 brg_sets; /* bitmask of compatible sets */
	uint8_t			 csr;
};

/* per-line state */
struct mc68681_line {
	struct tty		*tty;
	const struct mc68681_s	*speed;
	uint			 swflags;
};

/* per-line hardware configuration */
struct mc68681_hw {
	uint8_t			 dtr_op;
	uint8_t			 rts_op;
	uint8_t			 dcd_ip;
	uint8_t			 dcd_active_low;
};

/* write-only chip registers values */
struct mc68681_sw_reg {
	uint8_t			 mr1[N68681PORTS];
	uint8_t			 mr2[N68681PORTS];
	uint8_t			 cr[N68681PORTS];
	uint8_t			 acr;
	uint8_t			 imr;
	uint8_t			 oprs;
	uint8_t			 opcr;
	int			*ct;	/* timer limit in timer mode */
};

struct mc68681_softc {
	struct device		 sc_dev;

	struct mc68681_sw_reg	*sc_sw_reg;
	struct mc68681_hw	 sc_hw[N68681PORTS];
	struct mc68681_line	 sc_line[N68681PORTS];

	int			 sc_consport;

	uint8_t			(*sc_read)(void *, uint);
	void			(*sc_write)(void *, uint, uint8_t);

	struct mc68681_sw_reg	 sc_sw_reg_store;
};

void	mc68681_common_attach(struct mc68681_softc *);
void	mc68681_intr(struct mc68681_softc *, uint8_t);
