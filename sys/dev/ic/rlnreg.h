/*	$OpenBSD: rlnreg.h,v 1.2 1999/08/19 06:15:38 d Exp $	*/
/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * RangeLAN2 registers
 */

/*
 * The RangeLAN2 cards provide four control registers for transferring
 * messaged between the host and the card using programmed i/o.
 *
 * A transfer protocol is followed when sending asynchronous messages to,
 * and receiving messages from, the card.
 *
 * DATA
 *      7       6       5       4       3       2       1       0    
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |                             data                              |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *
 * STATUS
 *      7       6       5       4       3       2       1       0    
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |WAKEUP |   tx message state    |CLRNAK |   rx message state    |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *
 * CONTROL
 *      7       6       5       4       3       2       1       0    
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |       |       | 16BIT | RESET |       |       | TXINT | RXINT |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *
 * INTSEL
 *      7       6       5       4       3       2       1       0    
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |       |       |       |ENABLE |        interrupt line         |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 */

/* Register offsets. */
#define RLN_REG_DATA			0
#define RLN_REG_STATUS			2
#define RLN_REG_CONTROL			4
#define RLN_REG_EOI			5
#define RLN_REG_INTSEL			6
#define RLN_NPORTS                      8

/*
 * A short delay is needed (16ms?) after register writes on some cards.
 * XXX This is done by performing an innocent and harmless bus read. (i386)
 * This is what Proxim's driver does, anyway.
 */
#define _rln_regacc_delay() \
	bus_space_read_1(I386_BUS_SPACE_IO, 0, 0x61)

static void	_rln_register_write_1 __P((struct rln_softc *, u_int8_t, 
			u_int8_t));
static void	_rln_register_write_2 __P((struct rln_softc *, u_int8_t, 
			u_int16_t));
static u_int8_t	_rln_register_read_1 __P((struct rln_softc *, u_int8_t));
static u_int16_t _rln_register_read_2 __P((struct rln_softc *, u_int8_t));
static int	rln_status_rx_ready __P((struct rln_softc *));

/* Write to a register. */
static inline void
_rln_register_write_1(sc, regoff, value)
	struct rln_softc *sc;
	u_int8_t regoff;
	u_int8_t value;
{

#ifdef RLNDEBUG_REG
	printf(" %c<%02x", "DDS3CEI7"[regoff], value);
#endif
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (regoff), (value));
	_rln_regacc_delay();
}

static inline void
_rln_register_write_2(sc, regoff, value)
	struct rln_softc *sc;
	u_int8_t regoff;
	u_int16_t value;
{

#ifdef RLNDEBUG_REG
	printf(" %c<%04x", "DDS3CEI7"[regoff], value);
#endif
	bus_space_write_2((sc)->sc_iot, (sc)->sc_ioh, (regoff), (value));
	_rln_regacc_delay();
}

/* Read from a register. */
static inline u_int8_t
_rln_register_read_1(sc, regoff)
	struct rln_softc *sc;
	u_int8_t regoff;
{
	u_int8_t ret;

	ret = bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (regoff));
#ifdef RLNDEBUG_REG
	if (ret != (sc)->dbg_oreg[regoff]) {
		/* avoid spewing out too much debug info */
		printf(" %c>%02x", "DDS3CEI7"[regoff], ret);
		(sc)->dbg_oreg[regoff] = ret;
	}
#endif
	return (ret);
}


static inline u_int16_t
_rln_register_read_2(sc, regoff)
	struct rln_softc *sc;
	u_int8_t regoff;
{
	u_int16_t ret;

	ret = bus_space_read_2((sc)->sc_iot, (sc)->sc_ioh, (regoff));
#ifdef RLNDEBUG_REG
	if (ret != (sc)->dbg_oreg[regoff]) {
		printf(" %c>%04x", "DDS3CEI7"[regoff], ret);
		(sc)->dbg_oreg[regoff] = ret;
	}
#endif
	return (ret);
}

/* 8-bit data register access. */
#define rln_data_write_1(sc, value) 					\
		_rln_register_write_1(sc, RLN_REG_DATA, (value))
#define rln_data_read_1(sc) 						\
		_rln_register_read_1(sc, RLN_REG_DATA)
#define rln_data_write_multi_1(sc, buf, len) 				\
		bus_space_write_multi_1((sc)->sc_iot, (sc)->sc_ioh,	\
			RLN_REG_DATA, (buf), (len))
#define rln_data_read_multi_1(sc, buf, len) 				\
		bus_space_read_multi_1((sc)->sc_iot, (sc)->sc_ioh,	\
			RLN_REG_DATA, (buf), (len))

/* 16-bit data register access. */
#define rln_data_write_2(sc, value) 					\
		_rln_register_write_2(sc, RLN_REG_DATA, (value))
#define rln_data_read_2(sc) 						\
		_rln_register_read_2(sc, RLN_REG_DATA)
#define rln_data_write_multi_2(sc, buf, len) 				\
		bus_space_write_multi_2((sc)->sc_iot, (sc)->sc_ioh,	\
			RLN_REG_DATA, (buf), (len))
#define rln_data_read_multi_2(sc, buf, len)				\
		bus_space_read_multi_2((sc)->sc_iot, (sc)->sc_ioh,	\
			RLN_REG_DATA, (buf), (len))

/* Status register. */
#define RLN_STATUS_CLRNAK		0x08
#define RLN_STATUS_WAKEUP		0x80

#define RLN_STATUS_TX_IDLE		0x00
#define RLN_STATUS_TX_HILEN_AVAIL	0x01
#define RLN_STATUS_TX_HILEN_ACCEPT	0x02
#define RLN_STATUS_TX_XFR_COMPLETE	0x03
#define RLN_STATUS_TX_XFR		0x04
#define RLN_STATUS_TX_ERROR		0x05
#define RLN_STATUS_TX_LOLEN_AVAIL	0x06
#define RLN_STATUS_TX_LOLEN_ACCEPT	0x07
#define RLN_STATUS_TX_MASK		0x0f

#define RLN_STATUS_RX_IDLE		0x00
#define RLN_STATUS_RX_HILEN_AVAIL	0x10
#define RLN_STATUS_RX_HILEN_ACCEPT	0x20
#define RLN_STATUS_RX_XFR_COMPLETE	0x30
#define RLN_STATUS_RX_XFR		0x40
#define RLN_STATUS_RX_ERROR		0x50
#define RLN_STATUS_RX_LOLEN_AVAIL	0x60
#define RLN_STATUS_RX_LOLEN_ACCEPT	0x70
#define RLN_STATUS_RX_MASK		0x70

#define rln_status_write(sc, value) 					\
		_rln_register_write_1(sc, RLN_REG_STATUS, (value))
#define rln_status_set(sc, bits) 					\
		rln_status_write(sc, (sc)->sc_status |= (bits))
#define rln_status_clear(sc, bits) 					\
		rln_status_write(sc, (sc)->sc_status &= ~(bits))
#define _rln_status_setmask(sc, mask, bits)				\
do {									\
		int _s;							\
									\
		_s = splhigh();						\
	    	(sc)->sc_status = ((sc)->sc_status & (mask)) | (bits);	\
		rln_status_write(sc, (sc)->sc_status);			\
		splx(_s);						\
} while (0);
#define rln_status_rx_write(sc, state)  				\
		_rln_status_setmask((sc), ~RLN_STATUS_RX_MASK, state)
#define rln_status_tx_write(sc, state)  				\
		_rln_status_setmask((sc), ~RLN_STATUS_TX_MASK, state)
#define rln_status_read(sc) 						\
		_rln_register_read_1(sc, RLN_REG_STATUS)
#define rln_status_rx_read(sc) 						\
		(rln_status_read(sc) & ~RLN_STATUS_TX_MASK)
#define rln_status_tx_read(sc) 						\
		(rln_status_read(sc) & ~RLN_STATUS_RX_MASK)

static inline int
rln_status_rx_ready(sc)
	struct rln_softc *sc;
{
	u_int8_t status;

	status = rln_status_rx_read(sc);
	return (status == RLN_STATUS_RX_LOLEN_AVAIL ||
	    status == RLN_STATUS_RX_HILEN_AVAIL ||
	    status == RLN_STATUS_RX_ERROR);
}

#define rln_status_tx_int(sc) do { 					\
		int _s = splhigh();					\
									\
		rln_control_clear(sc, RLN_CONTROL_TXINT);		\
		rln_control_set(sc, RLN_CONTROL_TXINT);			\
		splx(_s);						\
} while (0)
#define rln_status_rx_int(sc) do { 					\
		int _s = splhigh();					\
									\
		rln_control_clear(sc, RLN_CONTROL_RXINT);		\
		rln_control_set(sc, RLN_CONTROL_RXINT);			\
		splx(_s);						\
} while (0)

/* Control register. */
#define RLN_CONTROL_RXINT		0x01
#define RLN_CONTROL_TXINT		0x02
#define RLN_CONTROL_BIT2		0x04
#define RLN_CONTROL_BIT3		0x08
#define RLN_CONTROL_RESET		0x10
#define RLN_CONTROL_16BIT		0x20
#define RLN_CONTROL_MASK		0x3f

#define rln_control_write(sc, value) 					\
		_rln_register_write_1(sc, RLN_REG_CONTROL,		\
			(sc)->sc_control = (value))
#define rln_control_read(sc) 						\
		_rln_register_read_1(sc, RLN_REG_CONTROL)
#define rln_control_set(sc, bits) 					\
		rln_control_write(sc, (sc)->sc_control | (bits))
#define rln_control_clear(sc, bits) 					\
		rln_control_write(sc, (sc)->sc_control & ~(bits))
#define rln_control_outofstandby(sc) do {				\
		rln_control_write(sc, (sc)->sc_control | RLN_CONTROL_RESET);\
		DELAY(30000);						\
		rln_control_write(sc, (sc)->sc_control);		\
} while (0)

/* Interrupt selection register. */
#define RLN_INTSEL_IRQMASK		0x07
#define RLN_INTSEL_ENABLE		0x10
#define RLN_INTSEL_BIT7			0x80

#define rln_intsel_disable(sc) do {					\
		int _s;							\
									\
		_s = splhigh();						\
		_rln_register_write_1(sc, RLN_REG_INTSEL, 		\
			(sc)->sc_intsel &= ~RLN_INTSEL_ENABLE);		\
		splx(_s);						\
} while (0)
#define rln_intsel_enable(sc) do {					\
		int _s;							\
									\
		_s = splhigh();						\
		_rln_register_write_1(sc, RLN_REG_INTSEL, 		\
			(sc)->sc_intsel |= RLN_INTSEL_ENABLE);		\
		splx(_s);						\
} while (0)
#define rln_intsel_write(sc, value) 					\
		_rln_register_write_1(sc, RLN_REG_INTSEL, 		\
		    (sc)->sc_intsel |= (value))

/* End of interrupt signal, used on some newer cards. */
#define rln_eoi(sc)							\
		(void) _rln_register_read_1(sc, RLN_REG_EOI)

