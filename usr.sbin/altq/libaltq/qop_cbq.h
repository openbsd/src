/*	$OpenBSD: qop_cbq.h,v 1.1.1.1 2001/06/27 18:23:29 kjc Exp $	*/
/*	$KAME: qop_cbq.h,v 1.2 2000/10/18 09:15:18 kjc Exp $	*/
/*
 * Copyright (c) Sun Microsystems, Inc. 1993-1998 All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the SMCC Technology
 *      Development Group at Sun Microsystems, Inc.
 *
 * 4. The name of the Sun Microsystems, Inc nor may not be used to endorse or
 *      promote products derived from this software without specific prior
 *      written permission.
 *
 * SUN MICROSYSTEMS DOES NOT CLAIM MERCHANTABILITY OF THIS SOFTWARE OR THE
 * SUITABILITY OF THIS SOFTWARE FOR ANY PARTICULAR PURPOSE.  The software is
 * provided "as is" without express or implied warranty of any kind.
 *  
 * These notices must be retained in any copies of any part of this software.
 */

#include <altq/altq_rmclass.h>
#include <altq/altq_cbq.h>

/* cbq admission types */
typedef enum {
	CBQ_QOS_NONE,
	CBQ_QOS_GUARANTEED,
	CBQ_QOS_PREDICTIVE,
	CBQ_QOS_CNTR_DELAY,
	CBQ_QOS_CNTR_LOAD
}  cbq_tos_t;

/*
 * cbq private ifinfo structure
 */
struct cbq_ifinfo {
	struct classinfo *root_class;		/* root class */
	struct classinfo *default_class;	/* default class */
	struct classinfo *ctl_class;		/* control class */

	double	nsPerByte;		/* bandwidth in ns per sec */
	int	is_wrr;			/* use weighted-round robin */
	int	is_efficient;		/* use work-conserving */
};

/*
 * cbq private classinfo structure
 */
struct cbq_classinfo {
	u_int	bandwidth;		/* bandwidth in bps */
	u_int	allocated;		/* bandwidth used by children */

	u_int	maxdelay;
	u_int	maxburst;
	u_int	minburst;
	u_int	av_pkt_size;
	u_int	max_pkt_size;

	cbq_class_spec_t	class_spec;	/* class parameters */
};

int cbq_interface_parser(const char *ifname, int argc, char **argv);
int cbq_class_parser(const char *ifname, const char *class_name,
		     const char *parent_name, int argc, char **argv);

int qcmd_cbq_add_if(const char *ifname, u_int bandwidth,
		    int is_wrr, int efficient);
int qcmd_cbq_add_class(const char *ifname, const char *class_name,
		       const char *parent_name, const char *borrow_name,
		       u_int pri, u_int bandwidth,
		       u_int maxdelay, u_int maxburst, u_int minburst,
		       u_int av_pkt_size, u_int max_pkt_size,
		       int admission_type, int flags);
int qcmd_cbq_modify_class(const char *ifname, const char *class_name,
			  u_int pri, u_int bandwidth,
			  u_int maxdelay, u_int maxburst, u_int minburst,
			  u_int av_pkt_size, u_int max_pkt_size, int flags);

int qop_cbq_add_if(struct ifinfo **rp, const char *ifname,
		   u_int bandwidth, int is_wrr, int efficient);
int qop_cbq_add_class(struct classinfo **rp, const char *class_name,
		      struct ifinfo *ifinfo, struct classinfo *parent, 
		      struct classinfo *borrow, u_int pri, u_int bandwidth,
		      u_int maxdelay, u_int maxburst, u_int minburst,
		      u_int av_pkt_size, u_int max_pkt_size,
		      int admission_type, int flags);
int qop_cbq_modify_class(struct classinfo *clinfo, u_int pri, u_int bandwidth,
			 u_int maxdelay, u_int maxburst, u_int minburst,
			 u_int av_pkt_size, u_int max_pkt_size, int flags);
	
