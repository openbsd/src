/*	$OpenBSD: rlnvar.h,v 1.3 2000/02/05 13:55:46 d Exp $	*/
/*
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 *
 * Proxim RangeLAN2 soft state copy.
 */

/*
 * Mailboxes are used to communicate card-initiated messages
 * from the interrupt handler to other kernel threads.
 */
struct rln_mbox {
	void *		mb_buf;		/* Message buffer */
	size_t		mb_len;		/* Message buffer size */
	size_t		mb_actlen;	/* Actual message size */
	u_int8_t	mb_state;	/* Mailbox state */
#define RLNMBOX_VOID		0
#define RLNMBOX_EMPTY		1
#define RLNMBOX_FILLING		2
#define RLNMBOX_FILLED		3
};

#define RLN_NMBOX	0x7c			/* Same as max msg seq number */

/* Soft state */
struct rln_softc {
	struct		device sc_dev;
	void		*sc_ih;			/* Interrupt handler */
	struct		arpcom sc_arpcom;	/* Ethernet common part */
	bus_space_tag_t	sc_iot;			/* Bus cookie */
	bus_space_handle_t sc_ioh;		/* Bus i/o handle */

	u_int8_t	sc_width;		/* Bus transfer width */
	u_int8_t	sc_irq;			/* IRQ for card */

	u_int16_t	sc_cardtype;		/* Set from config flags */
#define RLN_CTYPE_OEM		0x01
#define RLN_CTYPE_UISA		0x02
#define RLN_CTYPE_ONE_PIECE	0x04

	u_int8_t	sc_intsel;		/* Copy of INTSEL */
	u_int8_t	sc_status;		/* Copy of STATUS */
	u_int8_t	sc_control;		/* Copy of CONTROL */
#ifdef RLNDEBUG_REG
	u_int16_t	dbg_oreg[8];		/* Last reg value written */
#endif

	u_int8_t	sc_pktseq;		/* Card message seq no */
	u_int8_t	sc_txseq;		/* Tx packet seq no */

	u_int16_t	sc_state;		/* Soft state. */
#define RLN_STATE_SYNC		0x0001	/* Card is synchronised */
#define RLN_STATE_NEEDINIT	0x0002	/* Card needs reset+init */
#define RLN_STATE_PROMISC	0x0004	/* Receive all packets */

	struct		rln_mbox sc_mbox[0x80];	/* Per-message mailboxes */
	struct		rln_param sc_param;	/* User controlled parameters */
};

#define rln_need_reset(sc) \
	(sc)->sc_state |= RLN_STATE_NEEDINIT

/* Structure used to hold partial read state for rln_rx_pdata() */
struct rln_pdata {
	u_int8_t p_data;		/* extra data read but not consumed */
        int      p_nremain;		/* size of unconsumed data */
};
#define RLN_PDATA_INIT {0,0}

/* Structure used to hold partial transmit state for rln_msg_tx_*() */
struct rln_msg_tx_state {
	int      ien;			/* saved interrupt state */
	u_int8_t w;			/* saved wakup state */
	struct rln_pdata pd;		/* saved partial write state */
};

struct rln_mm_cmd;			/* fwd decl */

#define RLN_WAKEUP_SET		0xff
#define RLN_WAKEUP_NOCHANGE	(0x80|0x10)

void		rlnconfig __P((struct rln_softc *));
int		rlnintr __P((void *));
void		rlninit __P((struct rln_softc *));
void		rlnstop __P((struct rln_softc *));
void		rlnread __P((struct rln_softc *, struct rln_mm_cmd *, int));
int		rln_enable __P((struct rln_softc *, int));
int		rln_reset __P((struct rln_softc *));
u_int8_t	rln_wakeup __P((struct rln_softc *, u_int8_t));
int		rln_rx_request __P((struct rln_softc *, int));
int		rln_rx_data __P((struct rln_softc *, void *, int));
void		rln_rx_pdata __P((struct rln_softc *, void *, int,
			struct rln_pdata *));
void		rln_rx_end __P((struct rln_softc *));
void		rln_clear_nak __P((struct rln_softc *));
u_int8_t	rln_newseq __P((struct rln_softc *));

void		rln_msg_tx_data __P((struct rln_softc *, void *, u_int16_t,
			struct rln_msg_tx_state *));
int		rln_msg_tx_start __P((struct rln_softc *, void *, int,
			struct rln_msg_tx_state *));
int		rln_msg_tx_end __P((struct rln_softc *, 
			struct rln_msg_tx_state *));
int		rln_msg_txrx __P((struct rln_softc *, void *, int, 
			void *, int));

int		rln_mbox_create __P((struct rln_softc *, u_int8_t, void *,
			size_t));
int		rln_mbox_wait __P((struct rln_softc *, u_int8_t, int));
int		rln_mbox_lock __P((struct rln_softc *, u_int8_t, void **,
			size_t*));
void		rln_mbox_unlock __P((struct rln_softc *, u_int8_t, size_t));

/* debug all card operations */
#ifdef RLNDEBUG
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
#define RLNDUMP
#define RLNDUMPHEX(buf, buflen) do {				\
	int _i;							\
	for (_i = 0; _i < (buflen); _i++) {			\
		printf("%02x", ((unsigned char *)(buf))[_i]);	\
		if (_i != (buflen) - 1 && _i % 4 == 3)		\
			printf(",");				\
	}							\
} while (0)
