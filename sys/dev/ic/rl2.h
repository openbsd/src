/*	$OpenBSD: rl2.h,v 1.1 1999/06/21 23:21:46 d Exp $	*/
/*
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 *
 * Proxim RangeLAN2 parameters.
 *
 * Eventually, there should be a way of getting and setting these
 * from user space. Perhaps through ioctl().
 */

struct rl2_param {
	u_int8_t  rp_roamconfig;	/* roam speed */
#define RL2_ROAM_SLOW		0
#define RL2_ROAM_NORMAL		1
#define RL2_ROAM_FAST		2
	u_int32_t rp_security;		/* security id */
#define RL2_SECURITY_DEFAULT	0x0010203
	u_int8_t  rp_stationtype;
#define RL2_STATIONTYPE_SLAVE		0
#define RL2_STATIONTYPE_ALTMASTER	1
#define RL2_STATIONTYPE_MASTER		2
	u_int8_t  rp_domain;
	u_int8_t  rp_channel;
	u_int8_t  rp_subchannel;
	char	  rp_master[11];	/* valid only when st.type is master */
};

/* XXX possible ioctls to use */
#define RL2IOSPARAM	_IOW('2', 1, struct rl2_param)	/* set params */
#define RL2IOGPARAM	_IOR('2', 2, struct rl2_param)	/* get params */

