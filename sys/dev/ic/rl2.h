/*	$OpenBSD: rl2.h,v 1.2 1999/07/14 03:51:50 d Exp $	*/
/*
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 *
 * Proxim RangeLAN2 parameters.
 */

/*
 * Eventually, there should be a way of getting and setting these
 * from user space. Ideally, via ioctl().
 */

/* User-configurable station parameters. */
struct rl2_param {
	u_int32_t	rp_security;		/* Security ID */
#define RL2_SECURITY_DEFAULT	0x0010203
	u_int8_t	rp_station_type;	/* Station type */
#define RL2_STATIONTYPE_SLAVE		0
#define RL2_STATIONTYPE_ALTMASTER	1
#define RL2_STATIONTYPE_MASTER		2
	u_int8_t	rp_domain;		/* Network domain */
	u_int8_t	rp_channel;		/* Phys channel when master */
	u_int8_t	rp_subchannel;		/* Logical master subchannel */
	char		rp_master[11];		/* Name when master */
	u_int8_t	rp_mac_optimize;
#define RL2_MAC_OPTIM_LIGHT	0
#define RL2_MAC_OPTIM_NORMAL	1
	u_int8_t	rp_roam_config;		/* Roaming speed */
#define RL2_ROAM_SLOW		0
#define RL2_ROAM_NORMAL		1
#define RL2_ROAM_FAST		2
	u_int8_t	rp_peer_to_peer;	/* Ability to talk to peers */
};

#ifdef notyet
#define RL2IOSPARAM    _IOW('2', 1, struct rl2_param)  /* set params */
#define RL2IOGPARAM    _IOR('2', 2, struct rl2_param)  /* get params */
#endif

