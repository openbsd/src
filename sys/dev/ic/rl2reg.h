/*	$OpenBSD: rl2reg.h,v 1.1 1999/06/21 23:21:47 d Exp $	*/
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

/* Register offsets */

#define RL2_REG_DATA			0
#define RL2_REG_STATUS			2
#define RL2_REG_CONTROL			4
#define RL2_REG_EOI			5
#define RL2_REG_INTSEL			6
#define RL2_NPORTS                      8

/*
 * A short delay is needed (16ms?) after register writes.
 * XXX This is done by performing an innocent and harmless bus read. (i386)
 * This is what Proxim's driver does, anyway.
 */
#define _rl2_regacc_delay() \
	bus_space_read_1(I386_BUS_SPACE_IO, 0, 0x61)

static void	_rl2_register_write_1 __P((struct rl2_softc *, u_int8_t, 
			u_int8_t));
static u_int8_t	_rl2_register_read_1 __P((struct rl2_softc *, u_int8_t));
static int	rl2_status_rx_ready __P((struct rl2_softc *));

/* Register access */

/* Write to a register */
static inline void
_rl2_register_write_1(sc, regoff, value)
	struct rl2_softc *sc;
	u_int8_t regoff;
	u_int8_t value;
{

#ifdef RL2DEBUG_REG
	printf(" %c<%02x", "DDS3CEI7"[regoff], value);
#endif
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (regoff), (value));
	_rl2_regacc_delay();
}

/* Read from a register */
static inline u_int8_t
_rl2_register_read_1(sc, regoff)
	struct rl2_softc *sc;
	u_int8_t regoff;
{
	u_int8_t ret;

	ret = bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (regoff));
#ifdef RL2DEBUG_REG
	if (ret != (sc)->dbg_oreg[regoff]) {
		/* avoid spewing out too much debug info */
		printf(" %c>%02x", "DDS3CEI7"[regoff], ret);
		(sc)->dbg_oreg[regoff] = ret;
	}
#endif
	return (ret);
}

/* Data register */

/* 8-bit data access */
#define rl2_data_write_1(sc, value) 					\
		_rl2_register_write_1(sc, RL2_REG_DATA, (value))
#define rl2_data_read_1(sc) 						\
		_rl2_register_read_1(sc, RL2_REG_DATA)
#define rl2_data_write_multi_1(sc, buf, len) 				\
		bus_space_write_multi_1((sc)->sc_iot, (sc)->sc_ioh,	\
			RL2_REG_DATA, (buf), (len))
#define rl2_data_read_multi_1(sc, buf, len) 				\
		bus_space_read_multi_1((sc)->sc_iot, (sc)->sc_ioh,	\
			RL2_REG_DATA, (buf), (len))

/* 16-bit data access */
#define rl2_data_write_2(sc, value) 					\
		bus_space_write_2((sc)->sc_iot, (sc)->sc_ioh,		\
			RL2_REG_DATA, (value))
#define rl2_data_read_2(sc) 						\
		bus_space_read_2((sc)->sc_iot, (sc)->sc_ioh, 		\
			RL2_REG_DATA)
#define rl2_data_write_multi_2(sc, buf, len) 				\
		bus_space_write_multi_2((sc)->sc_iot, (sc)->sc_ioh,	\
			RL2_REG_DATA, (buf), (len))
#define rl2_data_read_multi_2(sc, buf, len)				\
		bus_space_read_multi_2((sc)->sc_iot, (sc)->sc_ioh,	\
			RL2_REG_DATA, (buf), (len))

/* Status register */

#define RL2_STATUS_CLRNAK		0x08
#define RL2_STATUS_WAKEUP		0x80

/* Status codes */
#define RL2_STATUS_TX_IDLE		0x00
#define RL2_STATUS_TX_HILEN_AVAIL	0x01
#define RL2_STATUS_TX_HILEN_ACCEPT	0x02
#define RL2_STATUS_TX_XFR_COMPLETE	0x03
#define RL2_STATUS_TX_XFR		0x04
#define RL2_STATUS_TX_ERROR		0x05
#define RL2_STATUS_TX_LOLEN_AVAIL	0x06
#define RL2_STATUS_TX_LOLEN_ACCEPT	0x07
#define RL2_STATUS_TX_MASK		0x0f

#define RL2_STATUS_RX_IDLE		0x00
#define RL2_STATUS_RX_HILEN_AVAIL	0x10
#define RL2_STATUS_RX_HILEN_ACCEPT	0x20
#define RL2_STATUS_RX_XFR_COMPLETE	0x30
#define RL2_STATUS_RX_XFR		0x40
#define RL2_STATUS_RX_ERROR		0x50
#define RL2_STATUS_RX_LOLEN_AVAIL	0x60
#define RL2_STATUS_RX_LOLEN_ACCEPT	0x70
#define RL2_STATUS_RX_MASK		0x70

#define rl2_status_write(sc, value) 					\
		_rl2_register_write_1(sc, RL2_REG_STATUS, (value))
#define rl2_status_set(sc, bits) 					\
		rl2_status_write(sc, (sc)->sc_status |= (bits))
#define rl2_status_clear(sc, bits) 					\
		rl2_status_write(sc, (sc)->sc_status &= ~(bits))
#define _rl2_status_setmask(sc, mask, bits)				\
do {									\
		int _s;							\
									\
		_s = splhigh();						\
	    	(sc)->sc_status = ((sc)->sc_status & (mask)) | (bits);	\
		rl2_status_write(sc, (sc)->sc_status);			\
		splx(_s);						\
} while (0);
#define rl2_status_rx_write(sc, state)  				\
		_rl2_status_setmask((sc), ~RL2_STATUS_RX_MASK, state)
#define rl2_status_tx_write(sc, state)  				\
		_rl2_status_setmask((sc), ~RL2_STATUS_TX_MASK, state)
#define rl2_status_read(sc) 						\
		_rl2_register_read_1(sc, RL2_REG_STATUS)
#define rl2_status_rx_read(sc) 						\
		(rl2_status_read(sc) & ~RL2_STATUS_TX_MASK)
#define rl2_status_tx_read(sc) 						\
		(rl2_status_read(sc) & ~RL2_STATUS_RX_MASK)

static inline int
rl2_status_rx_ready(sc)
	struct rl2_softc *sc;
{
	u_int8_t status;

	status = rl2_status_rx_read(sc);
	return (status == 0x60 || status == 0x10 || status == 0x50);
}

#define rl2_status_tx_int(sc) do { 					\
		int _s = splhigh();					\
									\
		rl2_control_clear(sc, RL2_CONTROL_TXINT);		\
		rl2_control_set(sc, RL2_CONTROL_TXINT);			\
		splx(_s);						\
} while (0)
#define rl2_status_rx_int(sc) do { 					\
		int _s = splhigh();					\
									\
		rl2_control_clear(sc, RL2_CONTROL_RXINT);		\
		rl2_control_set(sc, RL2_CONTROL_RXINT);			\
		splx(_s);						\
} while (0)

/* Control register */

#define RL2_CONTROL_RXINT		0x01
#define RL2_CONTROL_TXINT		0x02
#define RL2_CONTROL_BIT2		0x04
#define RL2_CONTROL_BIT3		0x08
#define RL2_CONTROL_RESET		0x10
#define RL2_CONTROL_16BIT		0x20
#define RL2_CONTROL_MASK		0x3f

#define rl2_control_write(sc, value) 					\
		_rl2_register_write_1(sc, RL2_REG_CONTROL,		\
			(sc)->sc_control = (value))
#define rl2_control_read(sc) 						\
		_rl2_register_read_1(sc, RL2_REG_CONTROL)
#define rl2_control_set(sc, bits) 					\
		rl2_control_write(sc, (sc)->sc_control | (bits))
#define rl2_control_clear(sc, bits) 					\
		rl2_control_write(sc, (sc)->sc_control & ~(bits))
#define rl2_control_outofstandby(sc) do {				\
		rl2_control_write(sc, (sc)->sc_control | RL2_CONTROL_RESET);\
		DELAY(30000);						\
		rl2_control_write(sc, (sc)->sc_control);		\
} while (0)

/* IntSel register */

#define RL2_INTSEL_IRQMASK		0x07
#define RL2_INTSEL_ENABLE		0x10
#define RL2_INTSEL_BIT7			0x80

#define rl2_intsel_disable(sc) do {					\
		int _s;							\
									\
		_s = splhigh();						\
		_rl2_register_write_1(sc, RL2_REG_INTSEL, 		\
			(sc)->sc_intsel &= ~RL2_INTSEL_ENABLE);		\
		splx(_s);						\
} while (0)
#define rl2_intsel_enable(sc) do {					\
		int _s;							\
									\
		_s = splhigh();						\
		_rl2_register_write_1(sc, RL2_REG_INTSEL, 		\
			(sc)->sc_intsel |= RL2_INTSEL_ENABLE);		\
		splx(_s);						\
} while (0)

#define rl2_intsel_write(sc, value) 					\
		_rl2_register_write_1(sc, RL2_REG_INTSEL, 		\
		    (sc)->sc_intsel |= (value))

/* End of interrupt signal (used on some newer cards?) */

#define rl2_eoi(sc)							\
		(void) _rl2_register_read_1(sc, RL2_REG_EOI)

/* Strings useful for debugging with printf("%b") */

#ifdef RL2DEBUG
#define RL2_BSTR_STATUS		"\20\4BIT3\10BIT7"
#define RL2_BSTR_CONTROL	"\20\1RXINT\2TXINT\3BIT2\4BIT3\5RESET\6BUS16"
#define RL2_BSTR_INTSEL		"\20\5ENABLE"
#endif
