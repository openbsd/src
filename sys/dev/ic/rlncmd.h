/*	$OpenBSD: rlncmd.h,v 1.1 1999/07/30 13:43:36 d Exp $	*/
/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * RangeLAN2 host-to-card message protocol.
 */

/* Micro-message command header. */
struct rln_mm_cmd {
	u_int8_t	cmd_letter;	/* Command letter */
	u_int8_t	cmd_seq;	/* Incremented on each command */
#define RLN_MAXSEQ		0x7c
	u_int8_t	cmd_fn;		/* Function number */
	u_int8_t	cmd_error;	/* Reserved */
};
#define RLN_MM_CMD(l,n)		((((unsigned int)l)<<8) | ((unsigned int)n))
#define RLN_MM_CMD_LETTER(cmd)	((unsigned char)(((cmd) & 0xff00)>>8))
#define RLN_MM_CMD_FUNCTION(cmd) ((unsigned char)((cmd) & 0xff))
#define RLN_CMDCODE(letter, num) ((((letter) & 0xff) << 8) | ((num) & 0xff))

/* Initialise card, and set operational parameters. */
struct rln_mm_init {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_INIT 			{ 'A', 0, 0, 0 }
	u_int8_t	enaddr[6];
	u_int8_t	opmode;
#define RLN_MM_INIT_OPMODE_NORMAL		0
#define RLN_MM_INIT_OPMODE_PROMISC		1
#define RLN_MM_INIT_OPMODE_PROTOCOL		2
	u_int8_t	stationtype;		/* RLN_STATIONTYPE_... */
	u_int8_t	hop_period;
	u_int8_t	bfreq;
	u_int8_t	sfreq;
	u_char		channel    : 4;		/* lower bits */
	u_char		subchannel : 4;		/* upper bits */
	char		mastername[11];
	u_char		sec1       : 4;		/* default 3 */
	u_char		domain     : 4;		/* default 0 */
	u_int8_t	sec2;			/* default 2 */
	u_int8_t	sec3;			/* default 1 */
	u_int8_t	sync_to;		/* 1 if roaming */
	u_int8_t	xxx_pad;		/* zero */
	char		syncname[11];
};

/* Result of initialisation. */
struct rln_mm_initted {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_INITTED 			{ 'a', 0, 0, 0 }
	u_int8_t	xxx;
};

/* Start searching for other masters. */
struct rln_mm_search {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_SEARCH			{ 'A', 0, 1, 0 }
	u_int8_t	xxx1[23];
	u_char		xxx2       : 4;
	u_char		domain     : 4;
	u_int8_t	roaming;
	u_int8_t	xxx3;			/* default 0 */
	u_int8_t	xxx4;			/* default 1 */
	u_int8_t	xxx5;			/* default 0 */
	u_int8_t	xxx6[11];
};

/* Notification that searching has started. */
struct rln_mm_searching {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_SEARCHING		{ 'a', 0, 1, 0 }
	u_int8_t	xxx;
};

/* Terminate search. */
#define RLN_MM_ABORTSEARCH		{ 'A', 0, 3, 0 }

/* Station synchronised to a master. */
struct rln_mm_synchronised {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_SYNCHRONISED		{ 'a', 0, 4, 0 }
	u_char		channel    : 4;		/* lower bits */
	u_char		subchannel : 4;		/* upper bits */
	char		mastername[11];
	u_int8_t	enaddr[6];
};

/* Station lost synchronisation with a master. */
#define RLN_MM_UNSYNCHRONISED		{ 'a', 0, 5, 0 }

/* Send card to sleep. (See rln_wakeup().) */
struct rln_mm_standby {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_STANDBY			{ 'A', 0, 6, 0 }
	u_int8_t	xxx;			/* default 0 */
};

/* Set ITO (inactivity timeout timer). */
struct rln_mm_setito {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_SETITO			{ 'A', 0, 7, 0 }
	u_int8_t	xxx;			/* default 3 */
	u_int8_t	timeout;
	u_char		bd_wakeup : 1;
	u_char		pm_sync : 7;
	u_int8_t	sniff_time;
};

/* ITO acknowledgment */
#define RLN_MM_GOTITO			{ 'a', 0, 7, 0 }

/* Send keepalive protocol message (?). */
#define RLN_MM_SENDKEEPALIVE		{ 'A', 0, 8, 0 }

/* Set multicast mode. */
struct rln_mm_multicast {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_MULTICAST		{ 'A', 0, 9, 0 }
	u_int8_t	enable;
};

/* Ack multicast mode change. */
#define RLN_MM_MULTICASTING		{ 'a', 0, 9, 0 }

/* Request statistics. */
#define RLN_MM_GETSTATS			{ 'A', 0, 11, 0 }

/* Statistics results. */
#define RLN_MM_GOTSTATS			{ 'a', 0, 11, 0 }

/* Set security ID used in channel. */
struct rln_mm_setsecurity {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_SETSECURITY		{ 'A', 0, 12, 0 }
	u_int8_t	sec1;
	u_int8_t	sec2;
	u_int8_t	sec3;
};

/* Ack set security ID. */
#define RLN_MM_GOTSECURITY		{ 'a', 0, 12, 0 }

/* Request firmware version. */
#define RLN_MM_GETPROMVERSION		{ 'A', 0, 13, 0 }

/* Reply with firmware version. */
struct rln_mm_gotpromversion {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_GOTPROMVERSION		{ 'a', 0, 13, 0 }
	u_int8_t	xxx;			/* sizeof version? */
	char		version[7];
};

/* Request station's MAC address (same as ethernet). */
#define RLN_MM_GETENADDR		{ 'A', 0, 14, 0 }

/* Reply with station's MAC address. */
struct rln_mm_gotenaddr {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_GOTENADDR		{ 'a', 0, 14, 0 }
	u_int8_t	xxx;
	u_int8_t	enaddr[6];
};

/* Tune various channel parameters. */
struct rln_mm_setmagic {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_SETMAGIC			{ 'A', 0, 16, 0 }
	u_char		fairness_slot : 3;
	u_char		deferral_slot : 5;
	u_int8_t	regular_mac_retry;	/* default 0x07 */
	u_int8_t	frag_mac_retry;		/* default 0x0a */
	u_int8_t	regular_mac_qfsk;	/* default 0x02 */
	u_int8_t	frag_mac_qfsk;		/* default 0x05 */
	u_int8_t	xxx1;			/* default 0xff */
	u_int8_t	xxx2;			/* default 0xff */
	u_int8_t	xxx3;			/* default 0xff */
	u_int8_t	xxx4;			/* zero */
};

/* Ack channel tuning. */
#define RLN_MM_GOTMAGIC			{ 'a', 0, 16, 0 }

/* Set roaming parameters - used when multiple masters available. */
struct rln_mm_setroaming {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_SETROAMING		{ 'A', 0, 17, 0 }
	u_int8_t	sync_alarm;
	u_int8_t	retry_thresh;
	u_int8_t	rssi_threshold;
	u_int8_t	xxx1;			/* default 0x5a */
	u_int8_t	sync_rssi_threshold;
	u_int8_t	xxx2;			/* default 0xa5 */
	u_int8_t	missed_sync;
};

/* Ack roaming parameter change. */
#define RLN_MM_GOTROAMING		{ 'a', 0, 17, 0 }

#define RLN_MM_ROAMING			{ 'a', 0, 18, 0 }
#define RLN_MM_ROAM			{ 'A', 0, 19, 0 }

/* Hardware fault notification. (Usually the antenna.) */
#define RLN_MM_FAULT			{ 'a', 0, 20, 0 }

#define RLN_MM_EEPROM_PROTECT		{ 'A', 0, 23, 0 }
#define RLN_MM_EEPROM_PROTECTED		{ 'a', 0, 23, 0 }
#define RLN_MM_EEPROM_UNPROTECT		{ 'A', 0, 24, 0 }
#define RLN_MM_EEPROM_UNPROTECTED	{ 'a', 0, 24, 0 }

/* Receive hop statistics. */
#define RLN_MM_HOP_STATISTICS		{ 'a', 0, 35, 0 }

/* Transmit a frame on the channel. */
struct rln_mm_sendpacket {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_SENDPACKET		{ 'B', 0, 0, 0 }
	u_int8_t	mode;
#define RLN_MM_SENDPACKET_MODE_BIT7	0x80
#define RLN_MM_SENDPACKET_MODE_ZFIRST	0x20
#define RLN_MM_SENDPACKET_MODE_QFSK	0x03
	u_int8_t	power;			/* default 0x70 */
	u_int8_t	length_lo;
	u_int8_t	length_hi;
	u_int8_t	xxx1;			/* default 0 */
	u_int8_t	xxx2;			/* default 0 */
	u_int8_t	sequence;		/* must increment */
	u_int8_t	xxx3;			/* default 0 */
};

/* Ack packet transmission. */
#define RLN_MM_SENTPACKET		{ 'b', 0, 0, 0 }

/* Notification of frame received from channel. */
struct rln_mm_recvpacket {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_RECVPACKET		{ 'b', 0, 1, 0 }
	u_int8_t	xxx[8];
};

/* Disable hopping. (?) */
struct rln_mm_disablehopping {
	struct		rln_mm_cmd mm_cmd;
#define RLN_MM_DISABLEHOPPING		{ 'C', 0, 9, 0 }
	u_int8_t	hopflag;
#define RLN_MM_DISABLEHOPPING_HOPFLAG_DISABLE	0x52
};

