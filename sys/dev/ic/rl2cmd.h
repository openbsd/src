/*	$OpenBSD: rl2cmd.h,v 1.2 1999/07/14 03:52:27 d Exp $	*/
/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * RangeLAN2 host-to-card message protocol.
 */

/* Micro-message command header. */
struct rl2_mm_cmd {
	u_int8_t	cmd_letter;	/* Command letter */
	u_int8_t	cmd_seq;	/* Incremented on each command */
#define RL2_MAXSEQ		0x7c
	u_int8_t	cmd_fn;		/* Function number */
	u_int8_t	cmd_error;	/* Reserved */
};
#define RL2_MM_CMD(l,n)		((((unsigned int)l)<<8) | ((unsigned int)n))
#define RL2_MM_CMD_LETTER(cmd)	((unsigned char)(((cmd) & 0xff00)>>8))
#define RL2_MM_CMD_FUNCTION(cmd) ((unsigned char)((cmd) & 0xff))
#define RL2_CMDCODE(letter, num) ((((letter) & 0xff) << 8) | ((num) & 0xff))

/* Initialise card, and set operational parameters. */
struct rl2_mm_init {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_INIT 			{ 'A', 0, 0, 0 }
	u_int8_t	enaddr[6];
	u_int8_t	opmode;
#define RL2_MM_INIT_OPMODE_NORMAL		0
#define RL2_MM_INIT_OPMODE_PROMISC		1
#define RL2_MM_INIT_OPMODE_PROTOCOL		2
	u_int8_t	stationtype;		/* RL2_STATIONTYPE_... */
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
struct rl2_mm_initted {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_INITTED 			{ 'a', 0, 0, 0 }
	u_int8_t	xxx;
};

/* Start searching for other masters. */
struct rl2_mm_search {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_SEARCH			{ 'A', 0, 1, 0 }
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
struct rl2_mm_searching {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_SEARCHING		{ 'a', 0, 1, 0 }
	u_int8_t	xxx;
};

/* Terminate search. */
#define RL2_MM_ABORTSEARCH		{ 'A', 0, 3, 0 }

/* Station synchronised to a master. */
struct rl2_mm_synchronised {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_SYNCHRONISED		{ 'a', 0, 4, 0 }
	u_char		channel    : 4;		/* lower bits */
	u_char		subchannel : 4;		/* upper bits */
	char		mastername[11];
	u_int8_t	enaddr[6];
};

/* Station lost synchronisation with a master. */
#define RL2_MM_UNSYNCHRONISED		{ 'a', 0, 5, 0 }

/* Send card to sleep. (See rl2_wakeup().) */
struct rl2_mm_standby {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_STANDBY			{ 'A', 0, 6, 0 }
	u_int8_t	xxx;			/* default 0 */
};

/* Set ITO (inactivity timeout timer). */
struct rl2_mm_setito {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_SETITO			{ 'A', 0, 7, 0 }
	u_int8_t	xxx;			/* default 3 */
	u_int8_t	timeout;
	u_char		bd_wakeup : 1;
	u_char		pm_sync : 7;
	u_int8_t	sniff_time;
};

/* ITO acknowledgment */
#define RL2_MM_GOTITO			{ 'a', 0, 7, 0 }

/* Send keepalive protocol message (?). */
#define RL2_MM_SENDKEEPALIVE		{ 'A', 0, 8, 0 }

/* Set multicast mode. */
struct rl2_mm_multicast {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_MULTICAST		{ 'A', 0, 9, 0 }
	u_int8_t	enable;
};

/* Ack multicast mode change. */
#define RL2_MM_MULTICASTING		{ 'a', 0, 9, 0 }

/* Request statistics. */
#define RL2_MM_GETSTATS			{ 'A', 0, 11, 0 }

/* Statistics results. */
#define RL2_MM_GOTSTATS			{ 'a', 0, 11, 0 }

/* Set security ID used in channel. */
struct rl2_mm_setsecurity {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_SETSECURITY		{ 'A', 0, 12, 0 }
	u_int8_t	sec1;
	u_int8_t	sec2;
	u_int8_t	sec3;
};

/* Ack set security ID. */
#define RL2_MM_GOTSECURITY		{ 'a', 0, 12, 0 }

/* Request firmware version. */
#define RL2_MM_GETPROMVERSION		{ 'A', 0, 13, 0 }

/* Reply with firmware version. */
struct rl2_mm_gotpromversion {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_GOTPROMVERSION		{ 'a', 0, 13, 0 }
	u_int8_t	xxx;			/* sizeof version? */
	char		version[7];
};

/* Request station's MAC address (same as ethernet). */
#define RL2_MM_GETENADDR		{ 'A', 0, 14, 0 }

/* Reply with station's MAC address. */
struct rl2_mm_gotenaddr {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_GOTENADDR		{ 'a', 0, 14, 0 }
	u_int8_t	xxx;
	u_int8_t	enaddr[6];
};

/* Tune various channel parameters. */
struct rl2_mm_setmagic {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_SETMAGIC			{ 'A', 0, 16, 0 }
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
#define RL2_MM_GOTMAGIC			{ 'a', 0, 16, 0 }

/* Set roaming parameters - used when multiple masters available. */
struct rl2_mm_setroaming {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_SETROAMING		{ 'A', 0, 17, 0 }
	u_int8_t	sync_alarm;
	u_int8_t	retry_thresh;
	u_int8_t	rssi_threshold;
	u_int8_t	xxx1;			/* default 0x5a */
	u_int8_t	sync_rssi_threshold;
	u_int8_t	xxx2;			/* default 0xa5 */
	u_int8_t	missed_sync;
};

/* Ack roaming parameter change. */
#define RL2_MM_GOTROAMING		{ 'a', 0, 17, 0 }

#define RL2_MM_ROAMING			{ 'a', 0, 18, 0 }
#define RL2_MM_ROAM			{ 'A', 0, 19, 0 }

/* Hardware fault notification. (Usually the antenna.) */
#define RL2_MM_FAULT			{ 'a', 0, 20, 0 }

#define RL2_MM_EEPROM_PROTECT		{ 'A', 0, 23, 0 }
#define RL2_MM_EEPROM_PROTECTED		{ 'a', 0, 23, 0 }
#define RL2_MM_EEPROM_UNPROTECT		{ 'A', 0, 24, 0 }
#define RL2_MM_EEPROM_UNPROTECTED	{ 'a', 0, 24, 0 }

/* Receive hop statistics. */
#define RL2_MM_HOP_STATISTICS		{ 'a', 0, 35, 0 }

/* Transmit a frame on the channel. */
struct rl2_mm_sendpacket {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_SENDPACKET		{ 'B', 0, 0, 0 }
	u_int8_t	mode;
#define RL2_MM_SENDPACKET_MODE_BIT7	0x80
#define RL2_MM_SENDPACKET_MODE_ZFIRST	0x20
#define RL2_MM_SENDPACKET_MODE_QFSK	0x03
	u_int8_t	power;			/* default 0x70 */
	u_int8_t	length_lo;
	u_int8_t	length_hi;
	u_int8_t	xxx1;			/* default 0 */
	u_int8_t	xxx2;			/* default 0 */
	u_int8_t	sequence;		/* must increment */
	u_int8_t	xxx3;			/* default 0 */
};

/* Ack packet transmission. */
#define RL2_MM_SENTPACKET		{ 'b', 0, 0, 0 }

/* Notification of frame received from channel. */
struct rl2_mm_recvpacket {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_RECVPACKET		{ 'b', 0, 1, 0 }
	u_int8_t	xxx[8];
};

/* Disable hopping. (?) */
struct rl2_mm_disablehopping {
	struct		rl2_mm_cmd mm_cmd;
#define RL2_MM_DISABLEHOPPING		{ 'C', 0, 9, 0 }
	u_int8_t	hopflag;
#define RL2_MM_DISABLEHOPPING_HOPFLAG_DISABLE	0x52
};

