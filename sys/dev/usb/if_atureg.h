/*	$OpenBSD: if_atureg.h,v 1.9 2004/12/04 08:02:02 dlg Exp $ */
/*
 * Copyright (c) 2003
 *	Daan Vreeken <Danovitsch@Vitsch.net>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Daan Vreeken.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Daan Vreeken AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Daan Vreeken OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* $ATUWI: $Id: if_atureg.h,v 1.9 2004/12/04 08:02:02 dlg Exp $ */

/************ 		driver options 		************/

/*
 * If you want to disable Intersil or RFMD support in the driver, uncomment
 * one of the following line. This will make the driver about 30 KB smaller
 * since the firmware-images will be left out. (Yes, leaving both out saves
 * 60KB, but that leaves you with a driver that doesn't support any devices :)
 */
/* #define ATU_NO_INTERSIL */
/* #define ATU_NO_RFMD */
/* #define ATU_NO_RFMD2958 */
/* #define ATU_NO_RFMD2958_SMC */

#define ATU_CONFIG_NO		1
#define ATU_IFACE_IDX		0

/************ various performance optimizations ************/

/*
 * setting ATU_NO_OPTIMIZATIONS will set all further options to default
 * values...
 *
 * no optimizations	392.78 KB/sec
 * with optimizations	485.64 KB/sec
 * (+/- 23.6% increase)
 *
 * Disclaimer : Most speed tests were done by transferring a large file over
 * FTP with an MTU of 1500. The tests are done on slow (Pentium-133) machines
 * so small changes in driver-overhead would be easilly noticeable. These
 * numbers are different for every other PC, I have just put them here to show
 * in what order of magnitude the differences are.
 */
/* #define ATU_NO_OPTIMIZATIONS */


/* the number of simultaniuously requested RX transfers */
#define ATU_RX_LIST_CNT	1

/*
 * the number of simultaniously started TX transfers
 * my measurements :
 * 1		430.82 KB/sec
 * 2		534.66 KB/sec
 * 3		536.23 KB/sec
 * 4		537.80 KB/sec
 * 6		537.30 KB/sec
 * 8		535.31 KB/sec
 * 16		535.68 KB/sec
 * 128		535.67 KB/sec (before you ask : yes, 128 is silly :)
 * (+/- 24% increase)
 */
#define ATU_TX_LIST_CNT	8

/* number of simultaniously MGMT TX transfers (always 1, don't change) */
#define ATU_MGMT_LIST_CNT	1

/*
 * Normally, when a packet arrives at the driver (with a call to atu_start)
 * we start up a USB transfer and transfer the packet to the adapter. Then
 * atu_txeof gets called on interrupt and we hand over the mbuf to
 * usb_tx_done. usb_tx_done will queue the mbuf and schedules a net-isr.
 * After the interrupt the scheduler will call the net-isr. There the mbuf
 * is free'd and atu_start is called (if there are packets on the TX queue).
 * This means that on average 2 task-switches are performed for every single
 * packet we transmit. 
 * If we simply NOT call usb_tx_done when a single transfer is done, but wait
 * for the other transfers we started simultaneously to complete, we can save
 * the overhead of the extra task-switching.
 *
 * measurements :
 * off		465.55 KB/sec
 * on		487.57 KB/sec
 * (+/- 4.7% increase)
 */
#define ATU_LAZY_TX_NETISR

/*
 * Minimum number of simultaneous transfers before we start being lazy.
 * Setting this to 0 will make us so lazy we could keep mbuf's around forever
 * (which you do not want)
 */
#define ATU_LAZY_TX_MIN		1

/*
 * Maximum number of queued mbufs before we call usb_tx_done (should never
 * be higher than ATU_TX_LIST_CNT)
 */
#define ATU_LAZY_TX_MAX		8

/*
 * Whether or not we padd usb packets transfered to the adapter. looking at
 * drivers on other platforms there seems to be something magic about the
 * padding.
 * my measurements :
 * on	- 532.55 KB/sec
 * off	- 536.74 KB/sec
 * I don't see the reason why we should padd bytes here. The adapter seems
 * to be transmitting packets just fine without the padding. So the default is
 * to have no padding, but it's at least supplied as an option. This seems to
 * be necessary with newer firmware versions, but I haven't tested that yet.
 */
/* #define ATU_TX_PADDING */

/*
 * ATU_PROFILING isn't a real optimization. it adds s bunch of sysctl
 * variables that count the number of function calls and data transfers we
 * make. with some shell scripts around it you can easilly measure the
 * performance of the driver and see where bottlenecks might be.
 */
/* #define ATU_PROFILING */




/**** NO user configurable options beyond this point ****/


#ifdef ATU_NO_OPTIMIZATIONS
#undef ATU_LAZY_TX_NETISR
#undef ATU_TX_PADDING
#undef ATU_TX_LIST_CNT
#define ATU_TX_LIST_CNT		1
#undef ATU_RX_LIST_CNT
#define ATU_RX_LIST_CNT		1
#endif



/*
 * According to the 802.11 spec (7.1.2) the frame body can be up to 2312 bytes
 */
#define ATU_RX_BUFSZ		(sizeof(struct at76c503_rx_buffer) +	\
				 sizeof(struct wi_80211_hdr) + 2312 + 4)
/* BE CAREFULL! should add ATU_TX_PADDING */
#define ATU_TX_BUFSZ		(sizeof(struct at76c503_tx_buffer) +	\
				 sizeof(struct wi_80211_hdr) + 2312)
#define ATU_MGMT_BUFSZ	(sizeof(struct at76c503_tx_buffer) + 300)

#define ATU_MIN_FRAMELEN	60

/*
 * Sending packets of more than 1500 bytes confuses some access points, so the
 * default MTU is set to 1500 but can be increased up to 2310 bytes using
 * ifconfig
 */
#define ATU_DEFAULT_MTU	1500
#define ATU_MAX_MTU		(2312 - 2)


#define ATU_ENDPT_RX		0x0
#define ATU_ENDPT_TX		0x1
#define ATU_ENDPT_MAX		0x2

#define ATU_TX_TIMEOUT	10000

#define ATU_JOIN_TIMEOUT	2000

#define ATU_MGMT_INTERVAL	1000 * hz / 1000
#define ATU_SCAN_RETRIES	2
#define ATU_JOIN_RETRIES	1
#define ATU_AUTH_RETRIES	3
#define ATU_ASSOC_RETRIES	3
#define ATU_IBSS_RETRIES	0

#define ATU_NO_QUIRK		0x0000
#define ATU_QUIRK_NO_REMAP	0x0001
#define ATU_QUIRK_FW_DELAY	0x0002

#define ATU_DEFAULT_SSID	""
#define ATU_DEFAULT_CHANNEL	10



enum atu_radio_type {
	RadioRFMD = 0,
	RadioRFMD2958,
	RadioRFMD2958_SMC,
	RadioIntersil
};

struct atu_type {
	u_int16_t		atu_vid;
	u_int16_t		atu_pid;
	enum atu_radio_type	atu_radio;
	u_int16_t		atu_quirk;
};


struct atu_softc;

struct atu_chain {
	struct atu_softc	*atu_sc;
	usbd_xfer_handle	atu_xfer;
	char			*atu_buf;
	struct mbuf		*atu_mbuf;
	u_int8_t		atu_idx;
	u_int16_t		atu_length;
	int			atu_in_xfer;
#ifdef ATU_LAZY_TX_NETISR
	u_int8_t		atu_not_freed;
#endif /* ATU_LAZY_TX_NETISR */
	SLIST_ENTRY(atu_chain)	atu_list;
};

struct atu_cdata {
	struct atu_chain	atu_tx_chain[ATU_TX_LIST_CNT];
	struct atu_chain	atu_rx_chain[ATU_RX_LIST_CNT];
	struct atu_chain	atu_mgmt_chain[ATU_MGMT_LIST_CNT];

	SLIST_HEAD(atu_list_head, atu_chain)	atu_rx_free;
	struct atu_list_head	atu_tx_free;
	struct atu_list_head	atu_mgmt_free;
		
	u_int8_t		atu_tx_inuse;
	u_int8_t		atu_tx_last_idx;	
};


#define MAX_SSID_LEN		32
#define ATU_AVG_TIME		20



enum atu_mgmt_state {
	STATE_NONE = 0,
	STATE_LISTENING,
	STATE_JOINING,
	
	STATE_AUTHENTICATING,           /* infra mode */
	STATE_ASSOCIATING,
	
	STATE_CREATING_IBSS,            /* adhoc mode */
	
	STATE_HAPPY_NETWORKING,
	STATE_GIVEN_UP
};

#ifdef ATU_DEBUG  
u_int8_t	*atu_mgmt_statename[] = {"NONE", "LISTENING", "JOINING",
    "AUTHENTICATING", "ASSOCIATING", "CREATING IBSS", "HAPPY NETWORKING :)",
    "GIVEN UP"};
#endif /* ATU_DEBUG */

struct atu_mgmt {
	enum atu_mgmt_state	state;
	int			retry;
};



struct atu_softc {
	USBBASEDEVICE           atu_dev;

	struct ieee80211com	sc_ic;

	int			(*sc_newstate)(struct ieee80211com *,
				    enum ieee80211_state, int);

	usbd_device_handle	atu_udev;
	usbd_interface_handle	atu_iface;
	struct ifmedia		atu_media;
	int			atu_ed[ATU_ENDPT_MAX];
	usbd_pipe_handle	atu_ep[ATU_ENDPT_MAX];
	int			atu_unit;
	int			atu_if_flags;

	struct atu_cdata	atu_cdata;

#ifdef ATU_LAZY_TX_NETISR
	int			atu_lazy_tx_cnt;
	/* struct mbuf		*atu_lazy_tx_list[ATU_LAZY_TX_MAX]; */
#endif /* ATU_LAZY_TX_NETISR */

	char			atu_dying;
	struct timeval		atu_rx_notice;
	
	u_int8_t		atu_bssid[ETHER_ADDR_LEN];
	enum atu_radio_type	atu_radio;
	u_int16_t		atu_quirk;
	
	/* used for debug : FLAG_SIGNAL */
	u_int8_t		atu_signalarr[ATU_AVG_TIME];
	u_int8_t		atu_signalptr;
	u_int16_t		atu_signaltotal;
	
	u_int8_t		atu_ssid[MAX_SSID_LEN];
	u_int8_t		atu_ssidlen;
	u_int8_t		atu_channel;
	u_int16_t		atu_desired_channel;
	u_int8_t		atu_mode;
#define NO_MODE_YET		0
#define AD_HOC_MODE		1
#define INFRASTRUCTURE_MODE	2

	u_int8_t		atu_radio_on;
	u_int8_t		atu_encrypt;
#define ATU_WEP_RX		0x01
#define ATU_WEP_TX		0x02
#define ATU_WEP_TXRX		(0x01 | 0x02)
	int			atu_wepkey;
	int			atu_wepkeylen;
	u_int8_t		atu_wepkeys[4][13];

	struct proc		*atu_mgmt_thread;
	struct atu_mgmt		atu_mgmt_vars;
	u_int16_t		atu_mgmt_flags;
#define ATU_TASK_RUNNING	0x01
#define ATU_CHANGED_SETTINGS	0x02
#define ATU_SEARCHING		0x04
#define ATU_FOUND_BSSID		0x08
#define ATU_AUTH_OK		0x10
#define ATU_RE_AUTH		0x20
#define ATU_ASSOC_OK		0x40
#define ATU_RE_ASSOC		0x80
#define ATU_NETWORK_OK		0x100
};






/* Commands for uploading the firmware (standard DFU interface) */
#define DFU_DNLOAD		UT_WRITE_CLASS_INTERFACE, 0x01
#define DFU_GETSTATUS		UT_READ_CLASS_INTERFACE, 0x03
#define DFU_GETSTATE		UT_READ_CLASS_INTERFACE, 0x05
#define DFU_REMAP		UT_WRITE_VENDOR_INTERFACE, 0x0a


/* DFU states */
#define DFUState_AppIdle	0
#define DFUState_AppDetach	1
#define DFUState_DFUIdle	2
#define DFUState_DnLoadSync	3
#define DFUState_DnLoadBusy	4
#define DFUState_DnLoadIdle	5
#define DFUState_ManifestSync	6
#define DFUState_Manifest	7
#define DFUState_ManifestWait	8
#define DFUState_UploadIdle	9
#define DFUState_DFUError	10

#define DFU_MaxBlockSize	1024


/* AT76c503 operating modes */
#define MODE_NONE		0x00
#define MODE_NETCARD		0x01
#define MODE_CONFIG		0x02
#define MODE_DFU		0x03
#define MODE_NOFLASHNETCARD	0x04




/* AT76c503 commands */
#define CMD_SET_MIB			0x01
#define CMD_START_SCAN			0x03
#define CMD_JOIN			0x04
#define CMD_START_IBSS			0x05
#define CMD_RADIO			0x06
#define CMD_RADIO_ON			0x06
#define CMD_RADIO_OFF			0x07
#define CMD_STARTUP			0x0b

/* AT76c503 status messages -  used in atu_wait_completion */
#define STATUS_IDLE			0x00
#define STATUS_COMPLETE			0x01
#define STATUS_UNKNOWN			0x02
#define STATUS_INVALID_PARAMETER	0x03
#define STATUS_FUNCTION_NOT_SUPPORTED	0x04
#define STATUS_TIME_OUT			0x07
#define STATUS_IN_PROGRESS		0x08
#define STATUS_HOST_FAILURE		0xff
#define STATUS_SCAN_FAILED		0xf0



/* AT76c503 command header */
struct atu_cmd {
	u_int8_t		Cmd;
	u_int8_t		Reserved;
	u_int16_t		Size;
};

/* CMD_SET_MIB command (0x01) */
struct atu_cmd_set_mib {
	/* AT76c503 command header */
	u_int8_t	AtCmd;
	u_int8_t	AtReserved;
	u_int16_t	AtSize;

	/* MIB header */
	u_int8_t	MIBType;
	u_int8_t	MIBSize;
	u_int8_t	MIBIndex;
	u_int8_t	MIBReserved;

	/* MIB data */
	u_int8_t	data[72];
};

/* CMD_STARTUP command (0x0b) */
struct atu_cmd_card_config {
	u_int8_t		Cmd;
	u_int8_t		Reserved;
	u_int16_t		Size;
		
	u_int8_t		ExcludeUnencrypted;
	u_int8_t		PromiscuousMode;
	u_int8_t		ShortRetryLimit;
	u_int8_t		EncryptionType;
	u_int16_t		RTS_Threshold;
	u_int16_t		FragThreshold;		/* 256 .. 2346 */
	u_int8_t		BasicRateSet[4];
	u_int8_t		AutoRateFallback;
	u_int8_t		Channel;
	u_int8_t		PrivacyInvoked;		/* wep */
	u_int8_t		WEP_DefaultKeyID;	/* 0 .. 3 */
	u_int8_t		SSID[MAX_SSID_LEN];
	u_int8_t		WEP_DefaultKey[4][13];
	u_int8_t		SSID_Len;
	u_int8_t		ShortPreamble;
	u_int16_t		BeaconPeriod;
};

/* CMD_SCAN command (0x03) */
struct atu_cmd_do_scan {
	u_int8_t		Cmd;
	u_int8_t		Reserved;
	u_int16_t		Size;
	
	u_int8_t		BSSID[ETHER_ADDR_LEN];
	u_int8_t		SSID[MAX_SSID_LEN];
	u_int8_t		ScanType;
	u_int8_t		Channel;
	u_int16_t		ProbeDelay;
	u_int16_t		MinChannelTime;
	u_int16_t		MaxChannelTime;
	u_int8_t		SSID_Len;
	u_int8_t		InternationalScan;  
};

#define ATU_SCAN_ACTIVE	0x00
#define ATU_SCAN_PASSIVE	0x01

/* CMD_JOIN command (0x04) */
struct atu_cmd_join {
	u_int8_t		Cmd;
	u_int8_t		Reserved;
	u_int16_t		Size;
	
	u_int8_t		bssid[ETHER_ADDR_LEN];
	u_int8_t		essid[32];
	u_int8_t		bss_type;
	u_int8_t		channel;
	u_int16_t		timeout;
	u_int8_t		essid_size;
	u_int8_t		reserved;
};

/* CMD_START_IBSS (0x05) */
struct atu_cmd_start_ibss {
	u_int8_t	Cmd;
	u_int8_t	Reserved;
	u_int16_t	Size;
	
	u_int8_t	BSSID[ETHER_ADDR_LEN];
	u_int8_t	SSID[32];
	u_int8_t	BSSType; 
	u_int8_t	Channel; 
	u_int8_t	SSIDSize;
	u_int8_t	Res[3];  
};


/*
 * The At76c503 adapters come with different types of radios on them.
 * At this moment the driver supports adapters with RFMD and Intersil radios.
 */

/* The config structure of an RFMD radio */
struct atu_rfmd_conf {
	u_int8_t		CR20[14];
	u_int8_t		CR21[14];
	u_int8_t		BB_CR[14];
	u_int8_t		PidVid[4];
	u_int8_t		MACAddr[ETHER_ADDR_LEN];
	u_int8_t		RegulatoryDomain;
	u_int8_t		LowPowerValues[14];
	u_int8_t		NormalPowerValues[14];
	u_int8_t		Reserved[3];
	/* then we have 84 bytes, somehow Windows reads 95?? */
	u_int8_t		Rest[11];
};

/* The config structure of an Intersil radio */
struct atu_intersil_conf {
	u_int8_t		MACAddr[ETHER_ADDR_LEN];
	/* From the HFA3861B manual : */
	/* Manual TX power control (7bit : -64 to 63) */
	u_int8_t		CR31[14];
	/* TX power measurement */
	u_int8_t		CR58[14];
	u_int8_t		PidVid[4];
	u_int8_t		RegulatoryDomain;
	u_int8_t		Reserved[1];
};


/* Firmware information request */
struct atu_fw {
	u_int8_t		major;
	u_int8_t		minor;
	u_int8_t		patch;
	u_int8_t		build;
};
        

/*
 * The header the AT76c503 puts in front of RX packets (for both managment &
 * data)
 */
struct at76c503_rx_buffer {
	u_int16_t		wlength;
	u_int8_t		rx_rate;
	u_int8_t		newbss;
	u_int8_t		fragmentation;
	u_int8_t		rssi;
	u_int8_t		link_quality;
	u_int8_t		noise_level;
	u_int32_t		rx_time;
};

/* The total packet the AT76c503 spits out looks like this */
struct atu_rxpkt {
	struct at76c503_rx_buffer	AtHeader;
	struct wi_80211_hdr		WiHeader;
	u_int8_t			Packet[2312 + 4];
};

/*
 * The header we have to put in front of a TX packet before sending it to the
 * AT76c503
 */
struct at76c503_tx_buffer {
	u_int16_t			wlength;
	u_int8_t			tx_rate;
	u_int8_t			padding;
	u_int8_t			reserved[4];
};

/* The total packet we send to the AT76c503 looks like this */
struct atu_txpkt {
	struct at76c503_tx_buffer	AtHeader;
	struct wi_80211_hdr		WiHeader;
	/* TODO - change this to a more correct value */
	u_int8_t			Packet[2312];
};

/* Managment packet */
struct atu_mgmt_packet {
	struct at76c503_tx_buffer	athdr;
	struct wi_mgmt_hdr		mgmt_hdr;
	
	u_int8_t			payload[0];
};

/* Authentication packet */
struct atu_auth_packet {
	struct at76c503_tx_buffer	athdr;
	struct wi_mgmt_hdr		mgmt_hdr;
	struct wi_mgmt_auth_hdr		auth_hdr;
	
	u_int8_t			challenge[0];
};

/* Association packet */
struct atu_assoc_packet {
	struct at76c503_tx_buffer	athdr;
	struct wi_mgmt_hdr		mgmt_hdr;
	u_int16_t			capability;
	u_int16_t			listen_interval;
	
	u_int8_t			data[0];
};


struct wi_80211_beacon {
	u_int8_t		timestamp[8];
	u_int16_t		interval;
	u_int16_t		flags;
	u_int8_t		data[1500];
};

struct tlv {
	u_int8_t		type;
	u_int8_t		length;
	u_int8_t		value[255];
};

#define TYPE_MASK		0x000c




#define NR(x)		(void *)((long)x)

/*
 * The linux driver uses seperate routines for every mib request they do
 * (eg. set_radio / set_preamble / set_frag / etc etc )
 * We just define a list of types, sizes and offsets and use those
 */

/*	Name				Type		Size	Index	*/
#define MIB_LOCAL			0x01
#define  MIB_LOCAL__BEACON_ENABLE	MIB_LOCAL,	1,	2
#define  MIB_LOCAL__AUTO_RATE_FALLBACK	MIB_LOCAL,	1,	3
#define  MIB_LOCAL__SSID_SIZE		MIB_LOCAL,	1,	5
#define  MIB_LOCAL__PREAMBLE		MIB_LOCAL,	1,	9
#define MIB_MAC_ADDR			0x02
#define  MIB_MAC_ADDR__ADDR		MIB_MAC_ADDR,	6,	0
#define MIB_MAC				0x03
#define  MIB_MAC__FRAG			MIB_MAC,	2,	8
#define  MIB_MAC__RTS			MIB_MAC,	2,	10
#define  MIB_MAC__DESIRED_SSID		MIB_MAC,	32,	28
#define MIB_MAC_MGMT			0x05
#define  MIB_MAC_MGMT__BEACON_PERIOD	MIB_MAC_MGMT,	2,	0
#define  MIB_MAC_MGMT__CURRENT_BSSID	MIB_MAC_MGMT,	6,	14
#define  MIB_MAC_MGMT__CURRENT_ESSID	MIB_MAC_MGMT,	32,	20
#define  MIB_MAC_MGMT__POWER_MODE	MIB_MAC_MGMT,	1,	53
#define  MIB_MAC_MGMT__IBSS_CHANGE	MIB_MAC_MGMT,	1,	54
#define MIB_MAC_WEP			0x06
#define  MIB_MAC_WEP__PRIVACY_INVOKED	MIB_MAC_WEP,	1,	0
#define  MIB_MAC_WEP__KEY_ID		MIB_MAC_WEP,	1,	1
#define  MIB_MAC_WEP__ICV_ERROR_COUNT	MIB_MAC_WEP,	4,	4
#define  MIB_MAC_WEP__EXCLUDED_COUNT	MIB_MAC_WEP,	4,	8
#define  MIB_MAC_WEP__KEYS(nr)		MIB_MAC_WEP,	13,	12+(nr)*13
#define  MIB_MAC_WEP__ENCR_LEVEL	MIB_MAC_WEP,	1,	64
#define MIB_PHY				0x07
#define  MIB_PHY__CHANNEL		MIB_PHY,	1,	20
#define  MIB_PHY__REG_DOMAIN		MIB_PHY,	1,	23
#define MIB_FW_VERSION			0x08
#define MIB_DOMAIN			0x09
#define  MIB_DOMAIN__POWER_LEVELS	MIB_DOMAIN,	14,	0
#define  MIB_DOMAIN__CHANNELS		MIB_DOMAIN,	14,	14

#define ATU_WEP_OFF			0
#define ATU_WEP_40BITS		1
#define ATU_WEP_104BITS		2

#define POWER_MODE_ACTIVE		1
#define POWER_MODE_SAVE			2
#define POWER_MODE_SMART		3

#define PREAMBLE_SHORT			1
#define PREAMBLE_LONG			0
