/*
 * Copyright (c) 2016 Vincent Gross <vincent.gross@kilob.yt>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <net/bpf.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <arpa/inet.h>

#define PORTNUM "23000"
#define PAYLOAD "payload"

char cmd_tmpl[] = "route get %s | awk '/interface:/ { printf($2) }'";
#define CMD_TMPL_SZ sizeof(cmd_tmpl)

int fuzzit;

void check_packet_tx(int);

int
main(int argc, char *argv[])
{
	int			  i;
	char			 *argp, *addr, *flag;

	struct addrinfo		  hints;
	struct addrinfo		 *inai;

	struct sockaddr_in	 *dst_sin = NULL;
	struct sockaddr_in	 *reserved_sin = NULL;
	struct sockaddr_in	 *bind_sin = NULL;
	struct sockaddr_in	 *cmsg_sin = NULL;
	struct sockaddr_in	 *wire_sin = NULL;

	int			  ch, rc, wstat, expected = -1;
	int			  first_sock;
	pid_t			  pid;

	const char		 *numerr;
	char			  adrbuf[40];
	const char		 *adrp;

	char			 *dst_str = NULL;
	char			  cmd[CMD_TMPL_SZ + INET_ADDRSTRLEN];
	FILE			 *outif_pipe;
	char			  ifname_buf[IF_NAMESIZE];
	size_t			  ifname_len;

	int			  bpf_fd;


	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	expected = strtonum(argv[1], 0, 255, &numerr);
	if (numerr != NULL)
		errx(2, "strtonum(%s): %s", optarg, numerr);

	for (i = 2; i < argc; i++) {
		argp = argv[i];
		if (strcmp("fuzz",argp) == 0) {
			fuzzit = 1;
			continue;
		}
		addr = strsep(&argp, "=");
		rc = getaddrinfo(addr, PORTNUM, &hints, &inai);
		if (rc)
			errx(2, "getaddrinfo(%s) = %d: %s",
			    argv[0], rc, gai_strerror(rc));
		if (argp == NULL)
			errx(2, "arg must be of form <addr>=<flag>,<flag>");

		for (; (flag = strsep(&argp,",")) != NULL;) {
			if (strcmp("destination",flag) == 0 && dst_sin == NULL) {
				dst_sin = (struct sockaddr_in *)inai->ai_addr;
				/* get output interface */
				snprintf(cmd, sizeof(cmd), cmd_tmpl, addr);
				outif_pipe = popen(cmd, "re");
				if (outif_pipe == NULL)
					err(2, "popen(route get)");
				if (fgets(ifname_buf, IF_NAMESIZE, outif_pipe) == NULL)
					err(2, "fgets()");
				pclose(outif_pipe);
				if (strlen(ifname_buf) == 0)
					err(2, "strlen(ifname_buf) == 0");
			}
			if (strcmp("reserved_saddr",flag) == 0 && reserved_sin == NULL)
				reserved_sin = (struct sockaddr_in *)inai->ai_addr;
			if (strcmp("bind_saddr",flag) == 0 && bind_sin == NULL)
				bind_sin = (struct sockaddr_in *)inai->ai_addr;
			if (strcmp("cmsg_saddr",flag) == 0 && cmsg_sin == NULL)
				cmsg_sin = (struct sockaddr_in *)inai->ai_addr;
			if (strcmp("wire_saddr",flag) == 0 && wire_sin == NULL)
				wire_sin = (struct sockaddr_in *)inai->ai_addr;
		}
	}

	if (reserved_sin == NULL)
		errx(2, "reserved_sin == NULL");

	if (bind_sin == NULL)
		errx(2, "bind_sin == NULL");

	if (dst_sin == NULL)
		errx(2, "dst_sin == NULL");

	if (expected < 0)
		errx(2, "need expected");


	if (wire_sin != NULL)
		bpf_fd = setup_bpf(ifname_buf, wire_sin, dst_sin);

	first_sock = udp_first(reserved_sin);

	pid = fork();
	if (pid == 0) {
		return udp_override(dst_sin, bind_sin, cmsg_sin);
	}
	(void)wait(&wstat);
	close(first_sock);

	if (!WIFEXITED(wstat))
		errx(2, "error setting up override");

	if (WEXITSTATUS(wstat) != expected)
		errx(2, "expected %d, got %d", expected, WEXITSTATUS(wstat));

	if (wire_sin != NULL)
		check_packet_tx(bpf_fd);

	return EXIT_SUCCESS;
}


struct bpf_insn outgoing_bpf_filter[] = {
	/* ethertype = IP */
	BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 12),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, ETHERTYPE_IP, 0, 9),

	/* Make sure it's a UDP packet. */
	BPF_STMT(BPF_LD + BPF_B + BPF_ABS, 23),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, IPPROTO_UDP, 0, 7),

	/* Fragments are handled as errors */
	BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 20),
	BPF_JUMP(BPF_JMP + BPF_JSET + BPF_K, 0x1fff, 5, 0),

	/* Make sure it's from the right address */
	BPF_STMT(BPF_LD + BPF_W + BPF_ABS, 26),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x0, 0, 3), /* Need to patch this */

	/* Make sure it's to the right address */
	BPF_STMT(BPF_LD + BPF_W + BPF_ABS, 30),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x0, 0, 1), /* Need to patch this */
#if 0
	/* Get the IP header length. */
	BPF_STMT(BPF_LDX + BPF_B + BPF_MSH, 14),

	/* Make sure it's to the right port. */
	BPF_STMT(BPF_LD + BPF_H + BPF_IND, 16),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x0, 0, 1),
#endif
	/* If we passed all the tests, ask for the whole packet. */
	BPF_STMT(BPF_RET+BPF_K, (u_int)-1),

	/* Otherwise, drop it. */
	BPF_STMT(BPF_RET+BPF_K, 0),
};

int outgoing_bpf_filter_len = sizeof(outgoing_bpf_filter)/sizeof(struct bpf_insn);

int
setup_bpf(char *ifname, struct sockaddr_in *from, struct sockaddr_in *to)
{
	int fd;
	struct ifreq ifr;
	u_int flag;
	struct bpf_version vers;
	struct bpf_program prog;

	fd = open("/dev/bpf", O_RDWR | O_CLOEXEC);
	if (fd == -1)
		err(2, "open(/dev/bpf)");

	if (ioctl(fd, BIOCVERSION, &vers) < 0)
		err(2, "ioctl(BIOCVERSION)");

	if (vers.bv_major != BPF_MAJOR_VERSION ||
	    vers.bv_minor < BPF_MINOR_VERSION)
		errx(2, "bpf version mismatch, expected %d.%d, got %d.%d",
		    BPF_MAJOR_VERSION, BPF_MINOR_VERSION,
		    vers.bv_major, vers.bv_minor);

	strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(fd, BIOCSETIF, &ifr) < 0)
		err(2, "ioctl(BIOCSETIF)");

	flag = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &flag) < 0)
		err(2, "ioctl(BIOCIMMEDIATE)");

	flag = BPF_DIRECTION_IN;
	if (ioctl(fd, BIOCSDIRFILT, &flag) < 0)
		err(2, "ioctl(BIOCDIRFILT)");

	outgoing_bpf_filter[7].k = ntohl(from->sin_addr.s_addr) ;
	outgoing_bpf_filter[9].k = ntohl(to->sin_addr.s_addr) ;
#if 0
	outgoing_bpf_filter[12].k = (u_int32_t)ntohs(to->sin_port) ;
#endif

	prog.bf_len = outgoing_bpf_filter_len;
	prog.bf_insns = outgoing_bpf_filter;
	if (ioctl(fd, BIOCSETF, &prog) < 0)
		err(2, "ioctl(BIOCSETF)");

	return fd;
}

void
check_packet_tx(int fd)
{
	u_int		 buf_max;
	size_t		 len;
	struct pollfd	 pfd;
	int		 pollrc;
	char		*buf, *payload;
	struct bpf_hdr	*hdr;
	struct ip	*ip;

	if (ioctl(fd, BIOCGBLEN, &buf_max) < 0)
		err(2, "ioctl(BIOCGBLEN)");

	if (buf_max <= 0)
		errx(2, "buf_max = %d <= 0", buf_max);

	buf = malloc(buf_max);
	if (!buf)
		err(2, "malloc(buf_max)");

	pfd.fd = fd;
	pfd.events = POLLIN;
	pollrc = poll(&pfd, 1, 5000);
	if (pollrc == -1)
		err(2, "poll()");
	if (pollrc == 0)
		errx(2, "poll() timeout");

	len = read(fd, buf, buf_max);
	if (len <= 0)
		err(2, "read(/dev/bpf)");
	len = BPF_WORDALIGN(len);

	if (len < sizeof(hdr))
		errx(2, "short read, len < sizeof(bpf_hdr)");

	hdr = (struct bpf_hdr *)buf;
	if (hdr->bh_hdrlen + hdr->bh_caplen > len)
		errx(2, "buffer too small for the whole capture");

	/* XXX we could try again if enough space in the buffer */
	if (hdr->bh_caplen != hdr->bh_datalen)
		errx(2, "partial capture");

	ip = (struct ip *)(buf + hdr->bh_hdrlen + ETHER_HDR_LEN);
	payload = ((char *)ip + ip->ip_hl*4 + 8);

	if (strcmp(PAYLOAD,payload) != 0)
		errx(2, "payload corrupted");

	return;
}

int
udp_first(struct sockaddr_in *src)
{
	int s_con;

	s_con = socket(src->sin_family, SOCK_DGRAM, 0);
	if (s_con == -1)
		err(2, "udp_bind: socket()");

	if (bind(s_con, (struct sockaddr *)src, src->sin_len))
		err(2, "udp_bind: bind()");

	return s_con;
}


int
udp_override(struct sockaddr_in *dst, struct sockaddr_in *src_bind,
    struct sockaddr_in *src_sendmsg)
{
	int			 s, optval, error, saved_errno;
	ssize_t			 send_rc;
	struct msghdr		 msg;
	struct iovec		 iov;
	struct cmsghdr		*cmsg;
	struct in_addr		*sendopt;
	int			*hopopt;
#define CMSGSP_SADDR	CMSG_SPACE(sizeof(u_int32_t))
#define CMSGSP_HOPLIM	CMSG_SPACE(sizeof(int))
#define CMSGSP_BOGUS	CMSG_SPACE(12)
#define CMSGBUF_SP	CMSGSP_SADDR + CMSGSP_HOPLIM + CMSGSP_BOGUS + 3
	unsigned char		 cmsgbuf[CMSGBUF_SP];

	bzero(&msg, sizeof(msg));
	bzero(&cmsgbuf, sizeof(cmsgbuf));

	s = socket(src_bind->sin_family, SOCK_DGRAM, 0);
	if (s == -1) {
		warn("udp_override: socket()");
		kill(getpid(), SIGTERM);
	}

	optval = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int))) {
		warn("udp_override: setsockopt(SO_REUSEADDR)");
		kill(getpid(), SIGTERM);
	}

	if (bind(s, (struct sockaddr *)src_bind, src_bind->sin_len)) {
		warn("udp_override: bind()");
		kill(getpid(), SIGTERM);
	}

	iov.iov_base = PAYLOAD;
	iov.iov_len = strlen(PAYLOAD) + 1;
	msg.msg_name = dst;
	msg.msg_namelen = dst->sin_len;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	if (src_sendmsg) {
		msg.msg_control = &cmsgbuf;
		msg.msg_controllen = CMSGSP_SADDR;
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len = CMSG_LEN(sizeof(struct in_addr));
		cmsg->cmsg_level = IPPROTO_IP;
		cmsg->cmsg_type = IP_SENDSRCADDR;
		sendopt = (struct in_addr *)CMSG_DATA(cmsg);
		memcpy(sendopt, &src_sendmsg->sin_addr, sizeof(*sendopt));
		if (fuzzit) {
			msg.msg_controllen = CMSGBUF_SP;
			cmsg = CMSG_NXTHDR(&msg, cmsg);
			cmsg->cmsg_len = CMSG_LEN(sizeof(int));
			cmsg->cmsg_level = IPPROTO_IPV6;
			cmsg->cmsg_type = IPV6_UNICAST_HOPS;
			hopopt = (int *)CMSG_DATA(cmsg);
			*hopopt = 8;

			cmsg = CMSG_NXTHDR(&msg, cmsg);
			cmsg->cmsg_len = CMSG_LEN(sizeof(int)) + 15;
			cmsg->cmsg_level = IPPROTO_IPV6;
			cmsg->cmsg_type = IPV6_UNICAST_HOPS;
		}
	}

	send_rc = sendmsg(s, &msg, 0);
	saved_errno = errno;

	close(s);

	if (send_rc == iov.iov_len)
		return 0;
	return saved_errno;
}
