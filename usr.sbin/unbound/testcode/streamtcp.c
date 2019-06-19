/*
 * testcode/streamtcp.c - debug program perform multiple DNS queries on tcp.
 *
 * Copyright (c) 2008, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This program performs multiple DNS queries on a TCP stream.
 */

#include "config.h"
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "util/locks.h"
#include "util/log.h"
#include "util/net_help.h"
#include "util/data/msgencode.h"
#include "util/data/msgparse.h"
#include "util/data/msgreply.h"
#include "util/data/dname.h"
#include "sldns/sbuffer.h"
#include "sldns/str2wire.h"
#include "sldns/wire2str.h"
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#ifndef PF_INET6
/** define in case streamtcp is compiled on legacy systems */
#define PF_INET6 10
#endif

/** usage information for streamtcp */
static void usage(char* argv[])
{
	printf("usage: %s [options] name type class ...\n", argv[0]);
	printf("	sends the name-type-class queries over TCP.\n");
	printf("-f server	what ipaddr@portnr to send the queries to\n");
	printf("-u 		use UDP. No retries are attempted.\n");
	printf("-n 		do not wait for an answer.\n");
	printf("-a 		print answers as they arrive.\n");
	printf("-d secs		delay after connection before sending query\n");
	printf("-s		use ssl\n");
	printf("-h 		this help text\n");
	exit(1);
}

/** open TCP socket to svr */
static int
open_svr(const char* svr, int udp)
{
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int fd = -1;
	/* svr can be ip@port */
	memset(&addr, 0, sizeof(addr));
	if(!extstrtoaddr(svr, &addr, &addrlen)) {
		printf("fatal: bad server specs '%s'\n", svr);
		exit(1);
	}
	fd = socket(addr_is_ip6(&addr, addrlen)?PF_INET6:PF_INET,
		udp?SOCK_DGRAM:SOCK_STREAM, 0);
	if(fd == -1) {
#ifndef USE_WINSOCK
		perror("socket() error");
#else
		printf("socket: %s\n", wsa_strerror(WSAGetLastError()));
#endif
		exit(1);
	}
	if(connect(fd, (struct sockaddr*)&addr, addrlen) < 0) {
#ifndef USE_WINSOCK
		perror("connect() error");
#else
		printf("connect: %s\n", wsa_strerror(WSAGetLastError()));
#endif
		exit(1);
	}
	return fd;
}

/** write a query over the TCP fd */
static void
write_q(int fd, int udp, SSL* ssl, sldns_buffer* buf, uint16_t id, 
	const char* strname, const char* strtype, const char* strclass)
{
	struct query_info qinfo;
	uint16_t len;
	/* qname */
	qinfo.qname = sldns_str2wire_dname(strname, &qinfo.qname_len);
	if(!qinfo.qname) {
		printf("cannot parse query name: '%s'\n", strname);
		exit(1);
	}

	/* qtype and qclass */
	qinfo.qtype = sldns_get_rr_type_by_name(strtype);
	qinfo.qclass = sldns_get_rr_class_by_name(strclass);

	/* clear local alias */
	qinfo.local_alias = NULL;

	/* make query */
	qinfo_query_encode(buf, &qinfo);
	sldns_buffer_write_u16_at(buf, 0, id);
	sldns_buffer_write_u16_at(buf, 2, BIT_RD);

	if(1) {
		/* add EDNS DO */
		struct edns_data edns;
		memset(&edns, 0, sizeof(edns));
		edns.edns_present = 1;
		edns.bits = EDNS_DO;
		edns.udp_size = 4096;
		if(sldns_buffer_capacity(buf) >=
			sldns_buffer_limit(buf)+calc_edns_field_size(&edns))
			attach_edns_record(buf, &edns);
	}

	/* send it */
	if(!udp) {
		len = (uint16_t)sldns_buffer_limit(buf);
		len = htons(len);
		if(ssl) {
			if(SSL_write(ssl, (void*)&len, (int)sizeof(len)) <= 0) {
				log_crypto_err("cannot SSL_write");
				exit(1);
			}
		} else {
			if(send(fd, (void*)&len, sizeof(len), 0) <
				(ssize_t)sizeof(len)){
#ifndef USE_WINSOCK
				perror("send() len failed");
#else
				printf("send len: %s\n", 
					wsa_strerror(WSAGetLastError()));
#endif
				exit(1);
			}
		}
	}
	if(ssl) {
		if(SSL_write(ssl, (void*)sldns_buffer_begin(buf),
			(int)sldns_buffer_limit(buf)) <= 0) {
			log_crypto_err("cannot SSL_write");
			exit(1);
		}
	} else {
		if(send(fd, (void*)sldns_buffer_begin(buf),
			sldns_buffer_limit(buf), 0) < 
			(ssize_t)sldns_buffer_limit(buf)) {
#ifndef USE_WINSOCK
			perror("send() data failed");
#else
			printf("send data: %s\n", wsa_strerror(WSAGetLastError()));
#endif
			exit(1);
		}
	}

	free(qinfo.qname);
}

/** receive DNS datagram over TCP and print it */
static void
recv_one(int fd, int udp, SSL* ssl, sldns_buffer* buf)
{
	char* pktstr;
	uint16_t len;
	if(!udp) {
		if(ssl) {
			int sr = SSL_read(ssl, (void*)&len, (int)sizeof(len));
			if(sr == 0) {
				printf("ssl: stream closed\n");
				exit(1);
			}
			if(sr < 0) {
				log_crypto_err("could not SSL_read");
				exit(1);
			}
		} else {
			ssize_t r = recv(fd, (void*)&len, sizeof(len), 0);
			if(r == 0) {
				printf("recv: stream closed\n");
				exit(1);
			}	
			if(r < (ssize_t)sizeof(len)) {
#ifndef USE_WINSOCK
				perror("read() len failed");
#else
				printf("read len: %s\n", 
					wsa_strerror(WSAGetLastError()));
#endif
				exit(1);
			}
		}
		len = ntohs(len);
		sldns_buffer_clear(buf);
		sldns_buffer_set_limit(buf, len);
		if(ssl) {
			int r = SSL_read(ssl, (void*)sldns_buffer_begin(buf),
				(int)len);
			if(r <= 0) {
				log_crypto_err("could not SSL_read");
				exit(1);
			}
			if(r != (int)len)
				fatal_exit("ssl_read %d of %d", r, len);
		} else {
			if(recv(fd, (void*)sldns_buffer_begin(buf), len, 0) < 
				(ssize_t)len) {
#ifndef USE_WINSOCK
				perror("read() data failed");
#else
				printf("read data: %s\n", 
					wsa_strerror(WSAGetLastError()));
#endif
				exit(1);
			}
		}
	} else {
		ssize_t l;
		sldns_buffer_clear(buf);
		if((l=recv(fd, (void*)sldns_buffer_begin(buf), 
			sldns_buffer_capacity(buf), 0)) < 0) {
#ifndef USE_WINSOCK
			perror("read() data failed");
#else
			printf("read data: %s\n", 
				wsa_strerror(WSAGetLastError()));
#endif
			exit(1);
		}
		sldns_buffer_set_limit(buf, (size_t)l);
		len = (size_t)l;
	}
	printf("\nnext received packet\n");
	log_buf(0, "data", buf);

	pktstr = sldns_wire2str_pkt(sldns_buffer_begin(buf), len);
	printf("%s", pktstr);
	free(pktstr);
}

/** see if we can receive any results */
static void
print_any_answers(int fd, int udp, SSL* ssl, sldns_buffer* buf,
	int* num_answers, int wait_all)
{
	/* see if the fd can read, if so, print one answer, repeat */
	int ret;
	struct timeval tv, *waittv;
	fd_set rfd;
	while(*num_answers > 0) {
		memset(&rfd, 0, sizeof(rfd));
		memset(&tv, 0, sizeof(tv));
		FD_ZERO(&rfd);
		FD_SET(fd, &rfd);
		if(wait_all) waittv = NULL;
		else waittv = &tv;
		ret = select(fd+1, &rfd, NULL, NULL, waittv);
		if(ret < 0) {
			if(errno == EINTR || errno == EAGAIN) continue;
			perror("select() failed");
			exit(1);
		}
		if(ret == 0) {
			if(wait_all) continue;
			return;
		}
		(*num_answers) -= 1;
		recv_one(fd, udp, ssl, buf);
	}
}

static int get_random(void)
{
	int r;
	if (RAND_bytes((unsigned char*)&r, (int)sizeof(r)) == 1) {
		return r;
	}
	return arc4random();
}

/** send the TCP queries and print answers */
static void
send_em(const char* svr, int udp, int usessl, int noanswer, int onarrival,
	int delay, int num, char** qs)
{
	sldns_buffer* buf = sldns_buffer_new(65553);
	int fd = open_svr(svr, udp);
	int i, wait_results = 0;
	SSL_CTX* ctx = NULL;
	SSL* ssl = NULL;
	if(!buf) fatal_exit("out of memory");
	if(usessl) {
		ctx = connect_sslctx_create(NULL, NULL, NULL, 0);
		if(!ctx) fatal_exit("cannot create ssl ctx");
		ssl = outgoing_ssl_fd(ctx, fd);
		if(!ssl) fatal_exit("cannot create ssl");
		while(1) {
			int r;
			ERR_clear_error();
			if( (r=SSL_do_handshake(ssl)) == 1)
				break;
			r = SSL_get_error(ssl, r);
			if(r != SSL_ERROR_WANT_READ &&
				r != SSL_ERROR_WANT_WRITE) {
				log_crypto_err("could not ssl_handshake");
				exit(1);
			}
		}
		if(1) {
			X509* x = SSL_get_peer_certificate(ssl);
			if(!x) printf("SSL: no peer certificate\n");
			else {
				X509_print_fp(stdout, x);
				X509_free(x);
			}
		}
	}
	for(i=0; i<num; i+=3) {
		if (delay != 0) {
#ifdef HAVE_SLEEP
			sleep((unsigned)delay);
#else
			Sleep(delay*1000);
#endif
		}
		printf("\nNext query is %s %s %s\n", qs[i], qs[i+1], qs[i+2]);
		write_q(fd, udp, ssl, buf, (uint16_t)get_random(), qs[i],
			qs[i+1], qs[i+2]);
		/* print at least one result */
		if(onarrival) {
			wait_results += 1; /* one more answer to fetch */
			print_any_answers(fd, udp, ssl, buf, &wait_results, 0);
		} else if(!noanswer) {
			recv_one(fd, udp, ssl, buf);
		}
	}
	if(onarrival)
		print_any_answers(fd, udp, ssl, buf, &wait_results, 1);

	if(usessl) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
		SSL_CTX_free(ctx);
	}
#ifndef USE_WINSOCK
	close(fd);
#else
	closesocket(fd);
#endif
	sldns_buffer_free(buf);
	printf("orderly exit\n");
}

#ifdef SIGPIPE
/** SIGPIPE handler */
static RETSIGTYPE sigh(int sig)
{
	if(sig == SIGPIPE) {
		printf("got SIGPIPE, remote connection gone\n");
		exit(1);
	}
	printf("Got unhandled signal %d\n", sig);
	exit(1);
}
#endif /* SIGPIPE */

/** getopt global, in case header files fail to declare it. */
extern int optind;
/** getopt global, in case header files fail to declare it. */
extern char* optarg;

/** main program for streamtcp */
int main(int argc, char** argv) 
{
	int c;
	const char* svr = "127.0.0.1";
	int udp = 0;
	int noanswer = 0;
	int onarrival = 0;
	int usessl = 0;
	int delay = 0;

#ifdef USE_WINSOCK
	WSADATA wsa_data;
	if(WSAStartup(MAKEWORD(2,2), &wsa_data) != 0) {
		printf("WSAStartup failed\n");
		return 1;
	}
#endif

	/* lock debug start (if any) */
	log_init(0, 0, 0);
	checklock_start();

#ifdef SIGPIPE
	if(signal(SIGPIPE, &sigh) == SIG_ERR) {
		perror("could not install signal handler");
		return 1;
	}
#endif

	/* command line options */
	if(argc == 1) {
		usage(argv);
	}
	while( (c=getopt(argc, argv, "af:hnsud:")) != -1) {
		switch(c) {
			case 'f':
				svr = optarg;
				break;
			case 'a':
				onarrival = 1;
				break;
			case 'n':
				noanswer = 1;
				break;
			case 'u':
				udp = 1;
				break;
			case 's':
				usessl = 1;
				break;
			case 'd':
				if(atoi(optarg)==0 && strcmp(optarg,"0")!=0) {
					printf("error parsing delay, "
					    "number expected: %s\n", optarg);
					return 1;
				}
				delay = atoi(optarg);
				break;
			case 'h':
			case '?':
			default:
				usage(argv);
		}
	}
	argc -= optind;
	argv += optind;

	if(argc % 3 != 0) {
		printf("queries must be multiples of name,type,class\n");
		return 1;
	}
	if(usessl) {
#if OPENSSL_VERSION_NUMBER < 0x10100000 || !defined(HAVE_OPENSSL_INIT_SSL)
		ERR_load_SSL_strings();
#endif
#if OPENSSL_VERSION_NUMBER < 0x10100000 || !defined(HAVE_OPENSSL_INIT_CRYPTO)
		OpenSSL_add_all_algorithms();
#else
		OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS
			| OPENSSL_INIT_ADD_ALL_DIGESTS
			| OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
#endif
#if OPENSSL_VERSION_NUMBER < 0x10100000 || !defined(HAVE_OPENSSL_INIT_SSL)
		(void)SSL_library_init();
#else
		(void)OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS, NULL);
#endif
	}
	send_em(svr, udp, usessl, noanswer, onarrival, delay, argc, argv);
	checklock_stop();
#ifdef USE_WINSOCK
	WSACleanup();
#endif
	return 0;
}
