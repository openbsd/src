/*	$OpenBSD: rln.h,v 1.1 1999/07/30 13:43:36 d Exp $	*/
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
struct rln_param {
	u_int32_t	rp_security;		/* Security ID */
#define RLN_SECURITY_DEFAULT	0x0010203
	u_int8_t	rp_station_type;	/* Station type */
#define RLN_STATIONTYPE_SLAVE		0
#define RLN_STATIONTYPE_ALTMASTER	1
#define RLN_STATIONTYPE_MASTER		2
	u_int8_t	rp_domain;		/* Network domain */
	u_int8_t	rp_channel;		/* Phys channel when master */
	u_int8_t	rp_subchannel;		/* Logical master subchannel */
	char		rp_master[11];		/* Name when master */
	u_int8_t	rp_mac_optimize;
#define RLN_MAC_OPTIM_LIGHT	0
#define RLN_MAC_OPTIM_NORMAL	1
	u_int8_t	rp_roam_config;		/* Roaming speed */
#define RLN_ROAM_SLOW		0
#define RLN_ROAM_NORMAL		1
#define RLN_ROAM_FAST		2
	u_int8_t	rp_peer_to_peer;	/* Ability to talk to peers */
};

#ifdef notyet
#define RLNIOSPARAM    _IOW('2', 1, struct rln_param)  /* set params */
#define RLNIOGPARAM    _IOR('2', 2, struct rln_param)  /* get params */
#endif

