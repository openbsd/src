/*      $OpenBSD: if_snvar.h,v 1.1 1997/03/12 13:20:33 briggs Exp $       */

/*
 * Copyright (c) 1991   Algorithmics Ltd (http://www.algor.co.uk)
 * You may use, copy, and modify this program so long as you retain the
 * copyright line.
 */

/*
 * if_sonic.h -- National Semiconductor DP83932BVF (SONIC)
 */

/*
 * buffer sizes in 32 bit mode
 * 1 TXpkt is 4 hdr words + (3 * FRAGMAX) + 1 link word
 * FRAGMAX == 16 => 54 words == 216 bytes
 *
 * 1 RxPkt is 7 words == 28 bytes
 * 1 Rda   is 4 words == 16 bytes
 */

#define NRRA    32		/* # receive resource descriptors */
#define RRAMASK 0x1f		/* the reason why it must be power of two */

#define NRBA    16		/* # receive buffers < NRRA */
#define RBAMASK 0x0f
#define NRDA    NRBA		/* # receive descriptors */
#define NTDA    4		/* # transmit descriptors */

#define CDASIZE sizeof(struct CDA)
#define RRASIZE (NRRA*sizeof(struct RXrsrc))
#define RDASIZE (NRDA*sizeof(struct RXpkt))
#define TDASIZE (NTDA*sizeof(struct TXpkt))

#define FCSSIZE 4		/* size of FCS appended to packets */

/*
 * maximum receive packet size plus 2 byte pad to make each
 * one aligned. 4 byte slop (required for eobc)
 */
#define RBASIZE(sc)	\
	(sizeof(struct ether_header) + ETHERMTU + FCSSIZE + \
	 ((sc)->sc_is16 ? 2 : 6))

/*
 * transmit buffer area
 */
#define NTXB    10      /* Number of xmit buffers */
#define TXBSIZE 1536    /* 6*2^8 -- the same size as the 8390 TXBUF */

/*
 * Statistics collected over time
 */
struct sn_stats {
	int     ls_opacks;	/* packets transmitted */
	int     ls_ipacks;	/* packets received */
	int     ls_tdr;		/* contents of tdr after collision */
	int     ls_tdef;	/* packets where had to wait */
	int     ls_tone;	/* packets with one retry */
	int     ls_tmore;	/* packets with more than one retry */
	int     ls_tbuff;	/* transmit buff errors */
	int     ls_tuflo;       /* "      uflo  "     */
	int     ls_tlcol;
	int     ls_tlcar;
	int     ls_trtry;
	int     ls_rbuff;       /* receive buff errors */
	int     ls_rfram;       /* framing     */
	int     ls_roflo;       /* overflow    */
	int     ls_rcrc;
	int     ls_rrng;	/* rx ring sequence error */
	int     ls_babl;	/* chip babl error */
	int     ls_cerr;	/* collision error */
	int     ls_miss;	/* missed packet */
	int     ls_merr;	/* memory error */
	int     ls_copies;      /* copies due to out of range mbufs */
	int     ls_maxmbufs;    /* max mbufs on transmit */
	int     ls_maxslots;    /* max ring slots on transmit */
};

/*
 * The sn_softc for Mac68k if_sn.
 */
typedef struct sn_softc {
	struct  device sc_dev;
	struct  arpcom sc_arpcom;
#define sc_if	   sc_arpcom.ac_if	 /* network visible interface */
#define sc_enaddr       sc_arpcom.ac_enaddr     /* hardware ethernet address */

	bus_space_tag_t		sc_regt;
	bus_space_handle_t	sc_regh;

	int		sc_is16;

	unsigned int    s_dcr;	  /* DCR for this instance */
	int     	slotno;

	struct sonic_reg *sc_csr;       /* hardware pointer */

	int     sc_rxmark;	      /* pos. in rx ring for reading bufs */

	int     sc_rramark;	     /* index into rra of wp */

	int     sc_txhead;	      /* index of first TDA passed to chip */
	int     sc_missed;	      /* missed packet counter */

	int     txb_cnt;		/* total number of xmit buffers */
	int     txb_inuse;	      /* number of active xmit buffers */
	int     txb_new;		/* index of next open slot. */

	void	*sc_lrxp;	 /* last RDA available to chip */

	struct  sn_stats sc_sum;
	short   sc_iflags;

	void	*p_rra; /* struct RXrsrc: receiver resource descriptors */
	int	 v_rra; /* DMA address of rra */
	void	*p_rda; /* struct RXpkt: receiver desriptors */
	int	 v_rda;
	void	*p_tda; /* struct TXpkt: transmitter descriptors */
	int	 v_tda;
	void	*p_cda; /* struct CDA: CAM descriptors */
	int	 v_cda; /* DMA address of CDA */

	void	(*rxint) __P((struct sn_softc *));
	void	(*txint) __P((struct sn_softc *));

	caddr_t	rbuf[NRBA];
	caddr_t	tbuf[NTXB];
	int	vtbuf[NTXB];
	unsigned char   space[(1 + 1 + 8 + 5) * NBPG];
} sn_softc_t;


/*
 * Accessing SONIC data structures and registers as 32 bit values
 * makes code endianess independent.  The SONIC is however always in
 * bigendian mode so it is necessary to ensure that data structures shared
 * between the CPU and the SONIC are always in bigendian order.
 */

/*
 * Receive Resource Descriptor
 * This structure describes the buffers into which packets
 * will be received.  Note that more than one packet may be
 * packed into a single buffer if constraints permit.
 */
struct RXrsrc {
	u_long  buff_ptrlo;	/* buffer address LO */
	u_long  buff_ptrhi;	/* buffer address HI */
	u_long  buff_wclo;	/* buffer size (16bit words) LO */ 
	u_long  buff_wchi;	/* buffer size (16bit words) HI */
};
struct _short_RXrsrc {
	u_short  buff_ptrlo;	/* buffer address LO */
	u_short  buff_ptrhi;	/* buffer address HI */
	u_short  buff_wclo;	/* buffer size (16bit words) LO */ 
	u_short  buff_wchi;	/* buffer size (16bit words) HI */
};

/*
 * Receive Descriptor
 * This structure holds information about packets received.
 */
struct RXpkt {
	u_long  status;		/* + receive status */
	u_long  byte_count;	/* + packet byte count (including FCS) */
	u_long  pkt_ptrlo;	/* + packet data LO (in RBA) */
	u_long  pkt_ptrhi;	/* + packet data HI (in RBA) */
	u_long  seq_no;		/* + RBA sequence numbers */
	u_long  rlink;		/* link to next receive descriptor */
	u_long  in_use;		/* + packet available to SONIC */
};
struct _short_RXpkt {
	u_short  status;	/* + receive status */
	u_short  byte_count;	/* + packet byte count (including FCS) */
	u_short  pkt_ptrlo;	/* + packet data LO (in RBA) */
	u_short  pkt_ptrhi;	/* + packet data HI (in RBA) */
	u_short  seq_no;	/* + RBA sequence numbers */
	u_short  rlink;		/* link to next receive descriptor */
	u_short  in_use;	/* + packet available to SONIC */
};
#define RBASEQ(x) (((x)>>8)&0xff)
#define PSNSEQ(x) ((x) & 0xff)

/*
 * Transmit Descriptor
 * This structure holds information about packets to be transmitted.
 */
#define FRAGMAX 16	      /* maximum number of fragments in a packet */
struct TXpkt {
	u_long  status;		/* + transmitted packet status */
	u_long  config;		/* transmission configuration */
	u_long  pkt_size;	/* entire packet size in bytes */
	u_long  frag_count;	/* # fragments in packet */
	struct {
		u_long  frag_ptrlo;	/* pointer to packet fragment LO */
		u_long  frag_ptrhi;	/* pointer to packet fragment HI */
		u_long  frag_size;	/* fragment size */
	} u[FRAGMAX];
	u_long  :32;	/* This makes tcp->u[FRAGMAX].u_link.link valid! */
};
struct _short_TXpkt {
	u_short  status;	/* + transmitted packet status */
	u_short  config;	/* transmission configuration */
	u_short  pkt_size;	/* entire packet size in bytes */
	u_short  frag_count;	/* # fragments in packet */
	struct {
		u_short  frag_ptrlo;     /* pointer to packet fragment LO */
		u_short  frag_ptrhi;     /* pointer to packet fragment HI */
		u_short  frag_size;      /* fragment size */
	} u[FRAGMAX];
	u_short  :16;	/* This makes tcp->u[FRAGMAX].u_link.link valid! */
};

#define tlink frag_ptrlo

#define EOL     0x0001	  /* end of list marker for link fields */

#define MAXCAM  16      /* number of user entries in CAM */
struct CDA {
	struct {
		u_long  cam_ep;     /* CAM Entry Pointer */
		u_long  cam_ap0;    /* CAM Address Port 0 xx-xx-xx-xx-YY-YY */
		u_long  cam_ap1;    /* CAM Address Port 1 xx-xx-YY-YY-xxxx */
		u_long  cam_ap2;    /* CAM Address Port 2 YY-YY-xx-xx-xx-xx */
	} desc[MAXCAM];
	u_long  enable;		    /* mask enabling CAM entries */
};
struct _short_CDA {
	struct {
		u_short  cam_ep;    /* CAM Entry Pointer */
		u_short  cam_ap0;   /* CAM Address Port 0 xx-xx-xx-xx-YY-YY */
		u_short  cam_ap1;   /* CAM Address Port 1 xx-xx-YY-YY-xxxx */
		u_short  cam_ap2;   /* CAM Address Port 2 YY-YY-xx-xx-xx-xx */
	} desc[MAXCAM];
	u_short  enable;	    /* mask enabling CAM entries */
};

void	snsetup __P((struct sn_softc *sc));
