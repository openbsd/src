/*	$OpenBSD: rl2cmd.h,v 1.1 1999/06/21 23:21:46 d Exp $	*/
/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * RangeLAN2 host-to-card message protocol.
 */

/*
 * Micro-message commands
 */

struct rl2_mm_cmd {
	u_int8_t cmd_letter;		/* command letter */
	u_int8_t cmd_seq;		/* must increment, <0x80 */
	u_int8_t cmd_fn;		/* function number */
	u_int8_t cmd_error;		/* reserved */
};

#define RL2_MM_CMD(l,n)		((((unsigned int)l)<<8) | ((unsigned int)n))
#define RL2_MM_CMD_LETTER(cmd)	((unsigned char)(((cmd) & 0xff00)>>8))
#define RL2_MM_CMD_FUNCTION(cmd) ((unsigned char)((cmd) & 0xff))

#if 0
  /* A: Initialisation commands */
	RL2_MM_INITIALIZE = 0,			/* A0   a0  */ /* [3]>0 */
	RL2_MM_SEARCH_AND_SYNC,			/* A1   a1- */
	RL2_MM_SEARCH_CONTINUE,			/* A2   a2. */
	RL2_MM_ABORT_SEARCH,			/* A3   a3. */
	RL2_MM_SYNCHRONISED,			/*      a4  */ /*HandleInSync*/
	RL2_MM_UNSYNCHRONISED,			/* A5       */
	RL2_MM_GOTO_STANDBY,			/* A6       */	/* stop() */
	RL2_MM_ITO,				/* A7   a7. */
	RL2_MM_KEEPALIVE,			/* A8   a8. */
	RL2_MM_MULTICAST,			/* A9   a9. */
	RL2_MM_RFNC_STATS,			/* A11  a11.*/
	RL2_MM_SECURITY,			/* A12  a12.*/
	RL2_MM_ROM_VERSION,			/* A13  a13 */
	RL2_MM_GLOBAL_ADDR,			/* A14  a14 */
	RL2_MM_CONFIGMAC,			/* A16  a16.*/
	RL2_MM_ROAMPARAM,			/* A17  a17.*/
	RL2_MM_ROAMING,				/*      a18 */ /* LLDRoam */
	RL2_MM_ROAM,				/* A19  a19 */
	RL2_MM_FAULT,				/*      a20 */ /* beep,reset */
	RL2_MM_SNIFF_MODE,			/* A22      */
	RL2_MM_DISABLE_EEPROM_WRITE,		/* A23  a23.*/
	RL2_MM_ENABLE_EEPROMP_WRITE,		/* A24  a24.*/
	RL2_MM_HOPSTATS,			/* A35  a35 */
  /* B: Data commands */
  /* C: Diagnostic commands */
	RL2_MM_DISABLE_HOPPING,			/* C9       */
#endif

#define RL2_CMDCODE(letter, num) ((((letter) & 0xff) << 8) | ((num) & 0xff))

#define u_int4_t u_int8_t

struct rl2_mm_setparam {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_SETPARAM 		{ 'A', 0, 0, 0 }
#define RL2_MM_SETPARAM_CODE		RL2_CMDCODE('A',0)
	u_int8_t enaddr[6];
	u_int8_t opmode;
#define RL2_MM_SETPARAM_OPMODE_NORMAL		0
#define RL2_MM_SETPARAM_OPMODE_PROMISC		1
#define RL2_MM_SETPARAM_OPMODE_PROTOCOL		2
	u_int8_t stationtype;
#define RL2_MM_SETPARAM_STATIONTYPE_SLAVE	RL2_STATIONTYPE_SLAVE
#define RL2_MM_SETPARAM_STATIONTYPE_ALTMASTER	RL2_STATIONTYPE_ALTMASTER
#define RL2_MM_SETPARAM_STATIONTYPE_MASTER	RL2_STATIONTYPE_MASTER
	u_int8_t hop_period;
	u_int8_t bfreq;
	u_int8_t sfreq;
	u_int4_t channel    : 4;	/* lower bits */
	u_int4_t subchannel : 4;	/* upper bits */
	char     mastername[11];
	u_int4_t sec1       : 4;	/* default 3 */
	u_int4_t domain     : 4;	/* default 0 */
	u_int8_t sec2;			/* default 2 */
	u_int8_t sec3;			/* default 1 */
	u_int8_t sync_to;		/* 1 if roaming */
	u_int8_t xxx_pad;		/* zero */
	char     syncname[11];
};

struct rl2_mm_paramset {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_PARAMSET 		{ 'a', 0, 0, 0 }
#define RL2_MM_PARAMSET_CODE		RL2_CMDCODE('a',0)
	u_int8_t xxx;
};

struct rl2_mm_search {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_SEARCH			{ 'A', 0, 1, 0 }
#define RL2_MM_SEARCH_CODE		RL2_CMDCODE('A',1)
	u_int8_t xxx1[23];
	u_int4_t xxx2       : 4;
	u_int4_t domain     : 4;
	u_int8_t roaming;
	u_int8_t xxx3;			/* default 0 */
	u_int8_t xxx4;			/* default 1 */
	u_int8_t xxx5;			/* default 0 */
	u_int8_t xxx6[11];
};

struct rl2_mm_searching {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_SEARCHING		{ 'a', 0, 1, 0 }
#define RL2_MM_SEARCHING_CODE		RL2_CMDCODE('a',1)
	u_int8_t xxx;
};

#define RL2_MM_ABORTSEARCH		{ 'A', 0, 3, 0 }
#define RL2_MM_ABORTSEARCH_CODE		RL2_CMDCODE('A',3)

struct rl2_mm_synchronised {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_SYNCHRONISED		{ 'a', 0, 4, 0 }
#define RL2_MM_SYNCHRONISED_CODE	RL2_CMDCODE('a',4)
	u_int4_t channel    : 4;	/* lower bits */
	u_int4_t subchannel : 4;	/* upper bits */
	char	 mastername[11];
	u_int8_t enaddr[6];
};

#define RL2_MM_UNSYNCHRONISED		{ 'a', 0, 5, 0 }
#define RL2_MM_UNSYNCHRONISED_CODE	RL2_CMDCODE('a',5)

struct rl2_mm_standby {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_STANDBY			{ 'A', 0, 6, 0 }
	u_int8_t xxx;			/* default 0 */
};

struct rl2_mm_setito {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_SETITO			{ 'A', 0, 7, 0 }
	u_int8_t xxx;			/* default 3 */
	u_int8_t timeout;
	unsigned int bd_wakeup : 1;
	unsigned int pm_sync : 7;
	u_int8_t sniff_time;
};

#define RL2_MM_GOTITO			{ 'a', 0, 7, 0 }

#define RL2_MM_SENDKEEPALIVE		{ 'A', 0, 8, 0 }

struct rl2_mm_multicast {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_MULTICAST		{ 'A', 0, 9, 0 }
	u_int8_t enable;
};

#define RL2_MM_MULTICASTING		{ 'a', 0, 9, 0 }

#define RL2_MM_GETSTATS			{ 'A', 0, 11, 0 }
#define RL2_MM_GOTSTATS			{ 'a', 0, 11, 0 }

struct rl2_mm_setsecurity {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_SETSECURITY		{ 'A', 0, 12, 0 }
	u_int8_t sec1;
	u_int8_t sec2;
	u_int8_t sec3;
};

#define RL2_MM_GOTSECURITY		{ 'a', 0, 12, 0 }

#define RL2_MM_GETPROMVERSION		{ 'A', 0, 13, 0 }

struct rl2_mm_gotpromversion {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_GOTPROMVERSION		{ 'a', 0, 13, 0 }
	u_int8_t xxx;
	char	 version[7];
};

#define RL2_MM_GETENADDR		{ 'A', 0, 14, 0 }

struct rl2_mm_gotenaddr {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_GOTENADDR		{ 'a', 0, 14, 0 }
	u_int8_t xxx;
	u_int8_t enaddr[6];
};

struct rl2_mm_setmagic {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_SETMAGIC			{ 'A', 0, 16, 0 }
	u_char   fairness_slot : 3;
	u_char   deferral_slot : 5;
	u_int8_t regular_mac_retry;	/* default 0x07 */
	u_int8_t frag_mac_retry;	/* default 0x0a */
	u_int8_t regular_mac_qfsk;	/* default 0x02 */
	u_int8_t frag_mac_qfsk;		/* default 0x05 */
	u_int8_t xxx1;			/* default 0xff */
	u_int8_t xxx2;			/* default 0xff */
	u_int8_t xxx3;			/* default 0xff */
	u_int8_t xxx4;			/* zero */
};

#define RL2_MM_GOTMAGIC			{ 'a', 0, 16, 0 }

struct rl2_mm_setroaming {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_SETROAMING		{ 'A', 0, 17, 0 }
	u_int8_t sync_alarm;
	u_int8_t retry_thresh;
	u_int8_t rssi_threshold;
	u_int8_t xxx1;			/* default 0x5a */
	u_int8_t sync_rssi_threshold;
	u_int8_t xxx2;			/* default 0xa5 */
	u_int8_t missed_sync;
};

#define RL2_MM_GOTROAMING		{ 'a', 0, 17, 0 }

#define RL2_MM_ROAMING			{ 'a', 0, 18, 0 }
#define RL2_MM_ROAM			{ 'A', 0, 19, 0 }
#define RL2_MM_FAULT			{ 'a', 0, 20, 0 }
#define RL2_MM_FAULT_CODE		RL2_CMDCODE('a',20)
#define RL2_MM_EEPROM_PROTECT		{ 'A', 0, 23, 0 }
#define RL2_MM_EEPROM_PROTECTED		{ 'a', 0, 23, 0 }
#define RL2_MM_EEPROM_UNPROTECT		{ 'A', 0, 24, 0 }
#define RL2_MM_EEPROM_UNPROTECTED	{ 'a', 0, 24, 0 }
#define RL2_MM_HOP_STATISTICS		{ 'a', 0, 35, 0 }

struct rl2_mm_sendpacket {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_SENDPACKET		{ 'B', 0, 0, 0 }
	u_int8_t mode;
#define RL2_MM_SENDPACKET_MODE_BIT7	0x80
#define RL2_MM_SENDPACKET_MODE_ZFIRST	0x20
#define RL2_MM_SENDPACKET_MODE_QFSK	0x03
	u_int8_t power;			/* default 0x70 */
	u_int8_t length_lo;
	u_int8_t length_hi;
	u_int8_t xxx1;			/* default 0 */
	u_int8_t xxx2;			/* default 0 */
	u_int8_t sequence;		/* must increment */
	u_int8_t xxx3;			/* default 0 */
};

#define RL2_MM_SENTPACKET		{ 'b', 0, 0, 0 }

struct rl2_mm_recvpacket {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_RECVPACKET		{ 'b', 0, 1, 0 }
	u_int8_t xxx[8];
};

struct rl2_mm_disablehopping {
	struct rl2_mm_cmd mm_cmd;
#define RL2_MM_DISABLEHOPPING		{ 'C', 0, 9, 0 }
	u_int8_t hopflag;
#define RL2_MM_DISABLEHOPPING_HOPFLAG_DISABLE	0x52
};

/* queue */
struct rl2_rx {
        SIMPLEQ_ENTRY(rl2_rx)   rx_entry;
        size_t                  rx_size;
        struct rl2_mm_cmd       rx_hdr;
        u_int8_t                rx_data[1];
};

#define RL2_MAX_RX_QUEUE_LEN		16

