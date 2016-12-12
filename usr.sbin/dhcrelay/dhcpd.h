/*	$OpenBSD: dhcpd.h,v 1.18 2016/12/12 15:41:05 rzalamena Exp $	*/

/*
 * Copyright (c) 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.    All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#define	SERVER_PORT	67
#define	CLIENT_PORT	68

/* Maximum size of client hardware address. */
#define CHADDR_SIZE	16

struct packet_ctx {
	uint8_t				 pc_htype;
	uint8_t				 pc_hlen;
	uint8_t				 pc_smac[CHADDR_SIZE];
	uint8_t				 pc_dmac[CHADDR_SIZE];

	struct sockaddr_storage		 pc_src;
	struct sockaddr_storage		 pc_dst;

	u_int8_t			*pc_circuit;
	int				 pc_circuitlen;
	u_int8_t			*pc_remote;
	int				 pc_remotelen;
};

struct iaddr {
	int len;
	unsigned char iabuf[CHADDR_SIZE];
};

struct hardware {
	u_int8_t htype;
	u_int8_t hlen;
	u_int8_t haddr[CHADDR_SIZE];
};

/* Possible states in which the client can be. */
enum dhcp_state {
	S_REBOOTING,
	S_INIT,
	S_SELECTING,
	S_REQUESTING,
	S_BOUND,
	S_RENEWING,
	S_REBINDING
};

/* DHCP relaying modes. */
enum dhcp_relay_mode {
	DRM_UNKNOWN,
	DRM_LAYER2,
	DRM_LAYER3,
};

struct interface_info {
	struct interface_info	*next;
	struct hardware		 hw_address;
	struct in_addr		 primary_address;
	char			 name[IFNAMSIZ];
	int			 rfdesc;
	int			 wfdesc;
	unsigned char		*rbuf;
	size_t			 rbuf_max;
	size_t			 rbuf_offset;
	size_t			 rbuf_len;
	struct ifreq		 ifr;
	int			 noifmedia;
	int			 errors;
	int			 dead;
	u_int16_t		 index;
};

struct timeout {
	struct timeout	*next;
	time_t		 when;
	void		 (*func)(void *);
	void		*what;
};

struct protocol {
	struct protocol	*next;
	int fd;
	void (*handler)(struct protocol *);
	void *local;
};

#define	DHCPD_LOG_FACILITY	LOG_DAEMON

/* External definitions... */

/* errwarn.c */
void error(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int warning(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int note(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int debug(char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));

/* bpf.c */
int if_register_bpf(struct interface_info *);
void if_register_send(struct interface_info *);
void if_register_receive(struct interface_info *, int);
ssize_t send_packet(struct interface_info *,
    struct dhcp_packet *, size_t, struct packet_ctx *);
ssize_t receive_packet(struct interface_info *, unsigned char *, size_t,
    struct packet_ctx *);

/* dispatch.c */
extern void (*bootp_packet_handler)(struct interface_info *,
    struct dhcp_packet *, int, struct packet_ctx *);
struct interface_info *get_interface(const char *,
    void (*)(struct protocol *), int isserver);
void dispatch(void);
void got_one(struct protocol *);
void add_protocol(char *, int, void (*)(struct protocol *), void *);
void remove_protocol(struct protocol *);

/* packet.c */
void assemble_hw_header(struct interface_info *, unsigned char *,
    int *, struct packet_ctx *);
void assemble_udp_ip_header(struct interface_info *, unsigned char *,
    int *, struct packet_ctx *pc, unsigned char *, int);
ssize_t decode_hw_header(struct interface_info *, unsigned char *,
    int, struct packet_ctx *);
ssize_t decode_udp_ip_header(struct interface_info *, unsigned char *,
    int, struct packet_ctx *, int);

/* dhcrelay.c */
extern u_int16_t server_port;
extern u_int16_t client_port;
extern int server_fd;

/* crap */
extern time_t cur_time;
extern int log_priority;
extern int log_perror;

static inline struct sockaddr_in *
ss2sin(struct sockaddr_storage *ss)
{
	return ((struct sockaddr_in *)ss);
}
