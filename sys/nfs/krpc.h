/*	$OpenBSD: krpc.h,v 1.5 2002/03/14 01:27:13 millert Exp $	*/
/*	$NetBSD: krpc.h,v 1.4 1995/12/19 23:07:11 cgd Exp $	*/

#include <sys/cdefs.h>

int krpc_call(struct sockaddr_in *sin,
	u_int prog, u_int vers, u_int func,
	struct mbuf **data, struct mbuf **from, int retries);

int krpc_portmap(struct sockaddr_in *sin,
	u_int prog, u_int vers, u_int16_t *portp);

struct mbuf *xdr_string_encode(char *str, int len);
struct mbuf *xdr_string_decode(struct mbuf *m, char *str, int *len_p);
struct mbuf *xdr_inaddr_encode(struct in_addr *ia);
struct mbuf *xdr_inaddr_decode(struct mbuf *m, struct in_addr *ia);


/*
 * RPC definitions for the portmapper
 */
#define	PMAPPORT		111
#define	PMAPPROG		100000
#define	PMAPVERS		2
#define	PMAPPROC_NULL		0
#define	PMAPPROC_SET		1
#define	PMAPPROC_UNSET		2
#define	PMAPPROC_GETPORT	3
#define	PMAPPROC_DUMP		4
#define	PMAPPROC_CALLIT		5


/*
 * RPC definitions for bootparamd
 */
#define	BOOTPARAM_PROG		100026
#define	BOOTPARAM_VERS		1
#define BOOTPARAM_WHOAMI	1
#define BOOTPARAM_GETFILE	2

