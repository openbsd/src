# struct netinfo {
#	struct	sockaddr rip_dst;	/* destination net/host */
#	int	rip_metric;		/* cost of route */
# };
# struct rip {
#	u_char	rip_cmd;		/* request/response */
#	u_char	rip_vers;		/* protocol version # */
#	u_char	rip_res1[2];		/* pad to 32-bit boundary */
#	union {
#		struct	netinfo ru_nets[1];	/* variable length... */
#		char	ru_tracefile[1];	/* ditto ... */
#	} ripun;
#define	rip_nets	ripun.ru_nets
#define	rip_tracefile	ripun.ru_tracefile
#define	RIPCMD_REQUEST		1	/* want info */
#define	RIPCMD_RESPONSE		2	/* responding to request */
#define	RIPCMD_TRACEON		3	/* turn tracing on */
#define	RIPCMD_TRACEOFF		4	/* turn it off */
#define	HOPCNT_INFINITY		16	/* per Xerox NS */
#define	MAXPACKETSIZE		512	/* max broadcast size */

### RIP packet redux
### UDP send FROM clued-rtr/520 to target/520
2	# RIPCMD_RESPONSE
1	# version
0	# padding
0

# sockaddr-plus-metric  structs begin, as many as necessary...
0	# len
2	# AF_INET
0	# port
0
# addr bytes:
X
Y
Z
Q
0	# filler, out to 16 bytes [sizeof (sockaddr)] ...
0
0
0
0
0
0
0
0	# metric: net-order integer
0
0
1

## that's it
