/*	$OpenBSD: rl2var.h,v 1.2 1999/06/23 04:48:49 d Exp $	*/
/*
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 *
 * Proxim RangeLAN2 soft state copy.
 */

/*
 * Mailboxes are used to communicate card-initiated messages
 * from the interrupt handler to other kernel threads.
 */
struct rl2_mbox {
	void *		mb_buf;		/* caller's buffer */
	size_t		mb_len;		/* buffer size */
	size_t		mb_actlen;	/* actual message size */
	volatile u_int8_t mb_state;	/* mailbox state */
#define RL2MBOX_VOID		0
#define RL2MBOX_EMPTY		1
#define RL2MBOX_FILLING		2
#define RL2MBOX_FILLED		3
};

#define RL2_NMBOX	0x7c		/* = maximum sequence number */

/* Soft state */
struct rl2_softc {
	struct device sc_dev;
	void *sc_ih;			/* interrupt handler */
	struct arpcom sc_arpcom;	/* Ethernet common part */
	bus_space_tag_t sc_iot;		/* bus cookie */
	bus_space_handle_t sc_ioh;	/* bus i/o handle */

	u_int8_t sc_width;		/* bus transfer width */
	u_int8_t sc_irq;		/* irq for card */

	u_int16_t sc_cardtype;		/* set from the 'flags' directive */
#define RL2_CTYPE_OEM		0x01
#define RL2_CTYPE_UISA		0x02
#define RL2_CTYPE_ONE_PIECE	0x04

	u_int8_t sc_intsel;		/* copy of INTSEL */
	u_int8_t sc_status;		/* copy of STATUS */
	u_int8_t sc_control;		/* copy of CONTROL */
#ifdef RL2DEBUG_REG
	u_int8_t dbg_oreg[8];		/* last value written to registers */
#endif

	u_int8_t sc_pktseq;		/* card message seq no */
	u_int8_t sc_txseq;		/* tx packet seq no */

	u_int16_t sc_state;
#define RL2_STATE_SYNC		0x0001	/* card is synchronised */

	struct rl2_mbox sc_mbox[0x80];	/* per-message mailboxes */
	struct rl2_param sc_param;	/* user-configurable parameters */
	u_int8_t  sc_promisc;		/* receive all packets */
};

/* Structure used to hold partial read state for rl2_rx_pdata() */
struct rl2_pdata {
	u_int8_t p_data;		/* extra data read but not consumed */
        int      p_nremain;		/* size of unconsumed data */
};
#define RL2_PDATA_INIT {0,0}

/* Structure used to hold partial transmit state for rl2_msg_tx_*() */
struct rl2_msg_tx_state {
	int      ien;			/* saved interrupt state */
	u_int8_t w;			/* saved wakup state */
	struct rl2_pdata pd;		/* saved partial write state */
};

struct rl2_mm_cmd;			/* fwd decl */

#define RL2_WAKEUP_SET		0xff
#define RL2_WAKEUP_NOCHANGE	(0x80|0x10)

void		rl2config __P((struct rl2_softc *));
int		rl2intr __P((void *));
void		rl2read __P((struct rl2_softc *, struct rl2_mm_cmd *, int));
int		rl2_enable __P((struct rl2_softc *, int));
int		rl2_reset __P((struct rl2_softc *));
u_int8_t	rl2_wakeup __P((struct rl2_softc *, u_int8_t));
int		rl2_rx_request __P((struct rl2_softc *, int));
int		rl2_rx_data __P((struct rl2_softc *, void *, int));
void		rl2_rx_pdata __P((struct rl2_softc *, void *, int,
			struct rl2_pdata *));
void		rl2_rx_end __P((struct rl2_softc *));
void		rl2_clear_nak __P((struct rl2_softc *));
u_int8_t	rl2_newseq __P((struct rl2_softc *));

void		rl2_msg_tx_data __P((struct rl2_softc *, void *, u_int16_t,
			struct rl2_msg_tx_state *));
int		rl2_msg_tx_start __P((struct rl2_softc *, void *, int,
			struct rl2_msg_tx_state *));
int		rl2_msg_tx_end __P((struct rl2_softc *, 
			struct rl2_msg_tx_state *));
int		rl2_msg_txrx __P((struct rl2_softc *, void *, int, 
			void *, int));

int		rl2_mbox_create __P((struct rl2_softc *, u_int8_t, void *,
			size_t));
int		rl2_mbox_wait __P((struct rl2_softc *, u_int8_t, int));
int		rl2_mbox_lock __P((struct rl2_softc *, u_int8_t, void **,
			size_t*));
void		rl2_mbox_unlock __P((struct rl2_softc *, u_int8_t, size_t));

/* debug all card operations */
#ifdef RL2DEBUG
#define dprintf(fmt, args...) printf(fmt , ## args)
	/* log(LOG_DEBUG, fmt , ## args) */
#define dprinthex(buf, len)	do {				\
	unsigned char *_b = (unsigned char*)(buf);		\
	int _i, _l=(len); 					\
	printf("{");						\
	for(_i = 0; _i < _l; _i++) {				\
		printf("%02x", _b[_i]);				\
		if (_i % 4 == 3 && _i != _l - 1)		\
			printf(",");				\
	}							\
	printf("}");						\
} while (0)
#else
#define dprintf(fmt, args...) /* nothing */
#define dprinthex(buf, len)	/* nothing */
#endif

/* debug messages to/from card. prints 4-octet groups separated by commas */
#define RL2DUMP
#define RL2DUMPHEX(buf, buflen) do {				\
	int _i;							\
	for (_i = 0; _i < (buflen); _i++) {			\
		printf("%02x", ((unsigned char *)(buf))[_i]);	\
		if (_i != (buflen) - 1 && _i % 4 == 3)		\
			printf(",");				\
	}							\
} while (0)
