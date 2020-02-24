/*	$OpenBSD: radiusctl.c,v 1.8 2020/02/24 07:07:11 dlg Exp $	*/
/*
 * Copyright (c) 2015 YASUOKA Masahiko <yasuoka@yasuoka.net>
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <arpa/inet.h>
#include <errno.h>
#include <err.h>
#include <md5.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <radius.h>

#include <event.h>

#include "parser.h"
#include "chap_ms.h"


static int		 radius_test (struct parse_result *);
static void		 radius_dump (FILE *, RADIUS_PACKET *, bool,
			    const char *);
static const char	*radius_code_str (int code);
static const char	*hexstr(const u_char *, int, char *, int);

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s command [argument ...]\n", __progname);
}

int
main(int argc, char *argv[])
{
	int			 ch;
	struct parse_result	*result;
	int			 ecode = EXIT_SUCCESS;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch (ch) {
		default:
			usage();
			return (EXIT_FAILURE);
		}
	argc -= optind;
	argv += optind;

	if ((result = parse(argc, argv)) == NULL)
		return (EXIT_FAILURE);

	switch (result->action) {
	case NONE:
		break;
	case TEST:
		if (pledge("stdio dns inet", NULL) == -1)
			err(EXIT_FAILURE, "pledge");
		ecode = radius_test(result);
		break;
	}

	return (ecode);
}

struct radius_test {
	const struct parse_result	*res;
	int				 ecode;

	RADIUS_PACKET			*reqpkt;
	int				 sock;
	unsigned int			 tries;
	struct event			 ev_send;
	struct event			 ev_recv;
	struct event			 ev_timedout;
};

static void	radius_test_send(int, short, void *);
static void	radius_test_recv(int, short, void *);
static void	radius_test_timedout(int, short, void *);

static int
radius_test(struct parse_result *res)
{
	struct radius_test	 test = { .res = res };
	RADIUS_PACKET		*reqpkt;
	struct addrinfo		 hints, *ai;
	int			 sock, retval;
	struct sockaddr_storage	 sockaddr;
	socklen_t		 sockaddrlen;
	struct sockaddr_in	*sin4;
	struct sockaddr_in6	*sin6;
	uint32_t		 u32val;
	uint8_t			 id;

	reqpkt = radius_new_request_packet(RADIUS_CODE_ACCESS_REQUEST);
	if (reqpkt == NULL)
		err(1, "radius_new_request_packet");
	id = arc4random();
	radius_set_id(reqpkt, id);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	retval = getaddrinfo(res->hostname, "radius", &hints, &ai);
	if (retval)
		errx(1, "%s %s", res->hostname, gai_strerror(retval));

	if (res->port != 0)
		((struct sockaddr_in *)ai->ai_addr)->sin_port =
		    htons(res->port);

	sock = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK,
	    ai->ai_protocol);
	if (sock == -1)
		err(1, "socket");

	/* Prepare NAS-IP{,V6}-ADDRESS attribute */
	if (connect(sock, ai->ai_addr, ai->ai_addrlen) == -1)
		err(1, "connect");
	sockaddrlen = sizeof(sockaddr);
	if (getsockname(sock, (struct sockaddr *)&sockaddr, &sockaddrlen) == -1)
		err(1, "getsockname");
	sin4 = (struct sockaddr_in *)&sockaddr;
	sin6 = (struct sockaddr_in6 *)&sockaddr;
	switch (sockaddr.ss_family) {
	case AF_INET:
		radius_put_ipv4_attr(reqpkt, RADIUS_TYPE_NAS_IP_ADDRESS,
		    sin4->sin_addr);
		break;
	case AF_INET6:
		radius_put_raw_attr(reqpkt, RADIUS_TYPE_NAS_IPV6_ADDRESS,
		    sin6->sin6_addr.s6_addr, sizeof(sin6->sin6_addr.s6_addr));
		break;
	}

	/* User-Name and User-Password */
	radius_put_string_attr(reqpkt, RADIUS_TYPE_USER_NAME,
	    res->username);

	switch (res->auth_method) {
	case PAP:
		if (res->password != NULL)
			radius_put_user_password_attr(reqpkt, res->password,
			    res->secret);
		break;
	case CHAP:
	    {
		u_char	 chal[16];
		u_char	 resp[1 + MD5_DIGEST_LENGTH]; /* "1 + " for CHAP Id */
		MD5_CTX	 md5ctx;

		arc4random_buf(resp, 1);	/* CHAP Id is random */
		MD5Init(&md5ctx);
		MD5Update(&md5ctx, resp, 1);
		if (res->password != NULL)
			MD5Update(&md5ctx, res->password,
			    strlen(res->password));
		MD5Update(&md5ctx, chal, sizeof(chal));
		MD5Final(resp + 1, &md5ctx);
		radius_put_raw_attr(reqpkt, RADIUS_TYPE_CHAP_CHALLENGE,
		    chal, sizeof(chal));
		radius_put_raw_attr(reqpkt, RADIUS_TYPE_CHAP_PASSWORD,
		    resp, sizeof(resp));
	    }
		break;
	case MSCHAPV2:
	    {
		u_char	pass[256], chal[16];
		u_int	i, lpass;
		struct _resp {
			u_int8_t ident;
			u_int8_t flags;
			char peer_challenge[16];
			char reserved[8];
			char response[24];
		} __packed resp;

		if (res->password == NULL) {
			lpass = 0;
		} else {
			lpass = strlen(res->password);
			if (lpass * 2 >= sizeof(pass))
				err(1, "password too long");
			for (i = 0; i < lpass; i++) {
				pass[i * 2] = res->password[i];
				pass[i * 2 + 1] = 0;
			}
		}

		memset(&resp, 0, sizeof(resp));
		resp.ident = arc4random();
		arc4random_buf(chal, sizeof(chal));
		arc4random_buf(resp.peer_challenge,
		    sizeof(resp.peer_challenge));

		mschap_nt_response(chal, resp.peer_challenge,
		    (char *)res->username, strlen(res->username), pass,
		    lpass * 2, resp.response);

		radius_put_vs_raw_attr(reqpkt, RADIUS_VENDOR_MICROSOFT,
		    RADIUS_VTYPE_MS_CHAP_CHALLENGE, chal, sizeof(chal));
		radius_put_vs_raw_attr(reqpkt, RADIUS_VENDOR_MICROSOFT,
		    RADIUS_VTYPE_MS_CHAP2_RESPONSE, &resp, sizeof(resp));
		explicit_bzero(pass, sizeof(pass));
	    }
		break;

	}
	u32val = htonl(res->nas_port);
	radius_put_raw_attr(reqpkt, RADIUS_TYPE_NAS_PORT, &u32val, 4);

	radius_put_message_authenticator(reqpkt, res->secret);

	event_init();

	test.ecode = EXIT_FAILURE;
	test.res = res;
	test.sock = sock;
	test.reqpkt = reqpkt;
	
	event_set(&test.ev_recv, sock, EV_READ|EV_PERSIST,
	    radius_test_recv, &test);

	evtimer_set(&test.ev_send, radius_test_send, &test);
	evtimer_set(&test.ev_timedout, radius_test_timedout, &test);

	event_add(&test.ev_recv, NULL);
	evtimer_add(&test.ev_timedout, &res->maxwait);

	/* Send! */
	fprintf(stderr, "Sending:\n");
	radius_dump(stdout, reqpkt, false, res->secret);
	radius_test_send(0, EV_TIMEOUT, &test);

	event_dispatch();

	/* Release the resources */
	radius_delete_packet(reqpkt);
	close(sock);
	freeaddrinfo(ai);

	explicit_bzero((char *)res->secret, strlen(res->secret));
	if (res->password)
		explicit_bzero((char *)res->password, strlen(res->password));

	return (test.ecode);
}

static void
radius_test_send(int thing, short revents, void *arg)
{
	struct radius_test	*test = arg;
	RADIUS_PACKET		*reqpkt = test->reqpkt;
	ssize_t			 rv;

retry:
	rv = send(test->sock,
	    radius_get_data(reqpkt), radius_get_length(reqpkt), 0);
	if (rv == -1) {
		switch (errno) {
		case EINTR:
		case EAGAIN:
			goto retry;
		default:
			break;
		}

		warn("send");
	}

	if (++test->tries >= test->res->tries)
		return;

	evtimer_add(&test->ev_send, &test->res->interval);
}

static void
radius_test_recv(int sock, short revents, void *arg)
{
	struct radius_test	*test = arg;
	RADIUS_PACKET		*respkt;
	RADIUS_PACKET		*reqpkt = test->reqpkt;

retry:
	respkt = radius_recv(sock, 0);
	if (respkt == NULL) {
		switch (errno) {
		case EINTR:
		case EAGAIN:
			goto retry;
		default:
			break;
		}

		warn("recv");
		return;
	}

	radius_set_request_packet(respkt, reqpkt);
	if (radius_get_id(respkt) == radius_get_id(reqpkt)) {
		fprintf(stderr, "\nReceived:\n");
		radius_dump(stdout, respkt, true, test->res->secret);

		event_del(&test->ev_recv);
		evtimer_del(&test->ev_send);
		evtimer_del(&test->ev_timedout);
		test->ecode = EXIT_SUCCESS;
	}

	radius_delete_packet(respkt);
}

static void
radius_test_timedout(int thing, short revents, void *arg)
{
	struct radius_test	*test = arg;

	event_del(&test->ev_recv);
}

static void
radius_dump(FILE *out, RADIUS_PACKET *pkt, bool resp, const char *secret)
{
	size_t		 len;
	char		 buf[256], buf1[256];
	uint32_t	 u32val;
	struct in_addr	 ipv4;

	fprintf(out,
	    "    Id                        = %d\n"
	    "    Code                      = %s(%d)\n",
	    (int)radius_get_id(pkt), radius_code_str((int)radius_get_code(pkt)),
	    (int)radius_get_code(pkt));
	if (resp && secret) {
		fprintf(out, "    Authenticator             = %s\n",
		    (radius_check_response_authenticator(pkt, secret) == 0)
		    ? "Verified" : "NG");
		fprintf(out, "    Message-Authenticator     = %s\n",
		    (!radius_has_attr(pkt, RADIUS_TYPE_MESSAGE_AUTHENTICATOR))
		    ? "(Not present)"
		    : (radius_check_message_authenticator(pkt, secret) == 0)
		    ? "Verified" : "NG");
	}

	if (radius_get_string_attr(pkt, RADIUS_TYPE_USER_NAME, buf,
	    sizeof(buf)) == 0)
		fprintf(out, "    User-Name                 = \"%s\"\n", buf);

	if (secret &&
	    radius_get_user_password_attr(pkt, buf, sizeof(buf), secret) == 0)
		fprintf(out, "    User-Password             = \"%s\"\n", buf);

	memset(buf, 0, sizeof(buf));
	len = sizeof(buf);
	if (radius_get_raw_attr(pkt, RADIUS_TYPE_CHAP_PASSWORD, buf, &len)
	    == 0)
		fprintf(out, "    CHAP-Password             = %s\n",
		    (hexstr(buf, len, buf1, sizeof(buf1)))
			    ? buf1 : "(too long)");

	memset(buf, 0, sizeof(buf));
	len = sizeof(buf);
	if (radius_get_raw_attr(pkt, RADIUS_TYPE_CHAP_CHALLENGE, buf, &len)
	    == 0)
		fprintf(out, "    CHAP-Challenge            = %s\n",
		    (hexstr(buf, len, buf1, sizeof(buf1)))
			? buf1 : "(too long)");

	memset(buf, 0, sizeof(buf));
	len = sizeof(buf);
	if (radius_get_vs_raw_attr(pkt, RADIUS_VENDOR_MICROSOFT,
	    RADIUS_VTYPE_MS_CHAP_CHALLENGE, buf, &len) == 0)
		fprintf(out, "    MS-CHAP-Challenge         = %s\n",
		    (hexstr(buf, len, buf1, sizeof(buf1)))
			? buf1 : "(too long)");

	memset(buf, 0, sizeof(buf));
	len = sizeof(buf);
	if (radius_get_vs_raw_attr(pkt, RADIUS_VENDOR_MICROSOFT,
	    RADIUS_VTYPE_MS_CHAP2_RESPONSE, buf, &len) == 0)
		fprintf(out, "    MS-CHAP2-Response         = %s\n",
		    (hexstr(buf, len, buf1, sizeof(buf1)))
		    ? buf1 : "(too long)");

	memset(buf, 0, sizeof(buf));
	len = sizeof(buf) - 1;
	if (radius_get_vs_raw_attr(pkt, RADIUS_VENDOR_MICROSOFT,
	    RADIUS_VTYPE_MS_CHAP2_SUCCESS, buf, &len) == 0) {
		fprintf(out, "    MS-CHAP-Success           = Id=%u \"%s\"\n",
		    (u_int)(u_char)buf[0], buf + 1);
	}

	memset(buf, 0, sizeof(buf));
	len = sizeof(buf) - 1;
	if (radius_get_vs_raw_attr(pkt, RADIUS_VENDOR_MICROSOFT,
	    RADIUS_VTYPE_MS_CHAP_ERROR, buf, &len) == 0) {
		fprintf(out, "    MS-CHAP-Error             = Id=%u \"%s\"\n",
		    (u_int)(u_char)buf[0], buf + 1);
	}

	memset(buf, 0, sizeof(buf));
	len = sizeof(buf);
	if (radius_get_vs_raw_attr(pkt, RADIUS_VENDOR_MICROSOFT,
	    RADIUS_VTYPE_MPPE_SEND_KEY, buf, &len) == 0)
		fprintf(out, "    MS-MPPE-Send-Key          = %s\n",
		    (hexstr(buf, len, buf1, sizeof(buf1)))
		    ? buf1 : "(too long)");

	memset(buf, 0, sizeof(buf));
	len = sizeof(buf);
	if (radius_get_vs_raw_attr(pkt, RADIUS_VENDOR_MICROSOFT,
	    RADIUS_VTYPE_MPPE_RECV_KEY, buf, &len) == 0)
		fprintf(out, "    MS-MPPE-Recv-Key          = %s\n",
		    (hexstr(buf, len, buf1, sizeof(buf1)))
		    ? buf1 : "(too long)");

	memset(buf, 0, sizeof(buf));
	len = sizeof(buf);
	if (radius_get_vs_raw_attr(pkt, RADIUS_VENDOR_MICROSOFT,
	    RADIUS_VTYPE_MPPE_ENCRYPTION_POLICY, buf, &len) == 0)
		fprintf(out, "    MS-MPPE-Encryption-Policy = 0x%08x\n",
		    ntohl(*(u_long *)buf));


	memset(buf, 0, sizeof(buf));
	len = sizeof(buf);
	if (radius_get_vs_raw_attr(pkt, RADIUS_VENDOR_MICROSOFT,
	    RADIUS_VTYPE_MPPE_ENCRYPTION_TYPES, buf, &len) == 0)
		fprintf(out, "    MS-MPPE-Encryption-Types  = 0x%08x\n",
		    ntohl(*(u_long *)buf));

	if (radius_get_string_attr(pkt, RADIUS_TYPE_REPLY_MESSAGE, buf,
	    sizeof(buf)) == 0)
		fprintf(out, "    Reply-Message             = \"%s\"\n", buf);

	memset(buf, 0, sizeof(buf));
	len = sizeof(buf);
	if (radius_get_uint32_attr(pkt, RADIUS_TYPE_NAS_PORT, &u32val) == 0)
		fprintf(out, "    NAS-Port                  = %lu\n",
		    (u_long)u32val);

	memset(buf, 0, sizeof(buf));
	len = sizeof(buf);
	if (radius_get_ipv4_attr(pkt, RADIUS_TYPE_NAS_IP_ADDRESS, &ipv4) == 0)
		fprintf(out, "    NAS-IP-Address            = %s\n",
		    inet_ntoa(ipv4));

	memset(buf, 0, sizeof(buf));
	len = sizeof(buf);
	if (radius_get_raw_attr(pkt, RADIUS_TYPE_NAS_IPV6_ADDRESS, buf, &len)
	    == 0)
		fprintf(out, "    NAS-IPv6-Address          = %s\n",
		    inet_ntop(AF_INET6, buf, buf1, len));

}

static const char *
radius_code_str(int code)
{
	int i;
	static struct _codestr {
		int		 code;
		const char	*str;
	} codestr[] = {
	    { RADIUS_CODE_ACCESS_REQUEST,	"Access-Request" },
	    { RADIUS_CODE_ACCESS_ACCEPT,	"Access-Accept" },
	    { RADIUS_CODE_ACCESS_REJECT,	"Access-Reject" },
	    { RADIUS_CODE_ACCOUNTING_REQUEST,	"Accounting-Request" },
	    { RADIUS_CODE_ACCOUNTING_RESPONSE,	"Accounting-Response" },
	    { RADIUS_CODE_ACCESS_CHALLENGE,	"Access-Challenge" },
	    { RADIUS_CODE_STATUS_SERVER,	"Status-Server" },
	    { RADIUS_CODE_STATUS_CLIENT,	"Status-Client" },
	    { -1, NULL }
	};

	for (i = 0; codestr[i].code != -1; i++) {
		if (codestr[i].code == code)
			return (codestr[i].str);
	}

	return ("Unknown");
}

static const char *
hexstr(const u_char *data, int len, char *str, int strsiz)
{
	int			 i, off = 0;
	static const char	 hex[] = "0123456789abcdef";

	for (i = 0; i < len; i++) {
		if (strsiz - off < 3)
			return (NULL);
		str[off++] = hex[(data[i] & 0xf0) >> 4];
		str[off++] = hex[(data[i] & 0x0f)];
		str[off++] = ' ';
	}
	if (strsiz - off < 1)
		return (NULL);

	str[off++] = '\0';

	return (str);
}
