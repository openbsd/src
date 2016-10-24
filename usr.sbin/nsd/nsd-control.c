/*
 * nsd-control.c - remote control utility for nsd.
 *
 * Copyright (c) 2011, NLnet Labs. All rights reserved.
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
 * The remote control utility contacts the nsd server over ssl and
 * sends the command, receives the answer, and displays the result
 * from the commandline.
 */

#include "config.h"
#ifdef HAVE_SSL

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#ifdef HAVE_OPENSSL_SSL_H
#include <openssl/ssl.h>
#endif
#ifdef HAVE_OPENSSL_ERR_H
#include <openssl/err.h>
#endif
#ifdef HAVE_OPENSSL_RAND_H
#include <openssl/rand.h>
#endif
#include "util.h"
#include "tsig.h"
#include "options.h"

/** Give nsd-control usage, and exit (1). */
static void
usage()
{
	printf("Usage:	nsd-control [options] command\n");
	printf("	Remote control utility for nsd server.\n");
	printf("Version %s. Report bugs to <%s>.\n",
		PACKAGE_VERSION, PACKAGE_BUGREPORT);
	printf("Options:\n");
	printf("  -c file	config file, default is %s\n", CONFIGFILE);
	printf("  -s ip[@port]	server address, if omitted config is used.\n");
	printf("  -h		show this usage help.\n");
	printf("Commands:\n");
	printf("  start				start server; runs nsd(8)\n");
	printf("  stop				stops the server\n");
	printf("  reload [<zone>]		reload modified zonefiles from disk\n");
	printf("  reconfig			reload the config file\n");
	printf("  repattern			the same as reconfig\n");
	printf("  log_reopen			reopen logfile (for log rotate)\n");
	printf("  status			display status of server\n");
	printf("  stats				print statistics\n");
	printf("  stats_noreset			peek at statistics\n");
	printf("  addzone <name> <pattern>	add a new zone\n");
	printf("  delzone <name>		remove a zone\n");
	printf("  addzones			add zone list on stdin {name space pattern newline}\n");
	printf("  delzones			remove zone list on stdin {name newline}\n");
	printf("  write [<zone>]		write changed zonefiles to disk\n");
	printf("  notify [<zone>]		send NOTIFY messages to slave servers\n");
	printf("  transfer [<zone>]		try to update slave zones to newer serial\n");
	printf("  force_transfer [<zone>]	update slave zones with AXFR, no serial check\n");
	printf("  zonestatus [<zone>]		print state, serial, activity\n");
	printf("  serverpid			get pid of server process\n");
	printf("  verbosity <number>		change logging detail\n");
	exit(1);
}

/** exit with ssl error */
static void ssl_err(const char* s)
{
	fprintf(stderr, "error: %s\n", s);
	ERR_print_errors_fp(stderr);
	exit(1);
}

/** setup SSL context */
static SSL_CTX*
setup_ctx(nsd_options_t* cfg)
{
	char* s_cert, *c_key, *c_cert;
	SSL_CTX* ctx;

	s_cert = cfg->server_cert_file;
	c_key = cfg->control_key_file;
	c_cert = cfg->control_cert_file;

	/* filenames may be relative to zonesdir */
	if (cfg->zonesdir && cfg->zonesdir[0] &&
		(s_cert[0] != '/' || c_key[0] != '/' || c_cert[0] != '/')) {
		if(chdir(cfg->zonesdir))
			ssl_err("could not chdir to zonesdir");
	}

        ctx = SSL_CTX_new(SSLv23_client_method());
	if(!ctx)
		ssl_err("could not allocate SSL_CTX pointer");
        if((SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2) & SSL_OP_NO_SSLv2)
		!= SSL_OP_NO_SSLv2)
		ssl_err("could not set SSL_OP_NO_SSLv2");
        if((SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3) & SSL_OP_NO_SSLv3)
		!= SSL_OP_NO_SSLv3)
		ssl_err("could not set SSL_OP_NO_SSLv3");
	if(!SSL_CTX_use_certificate_file(ctx,c_cert,SSL_FILETYPE_PEM) ||
		!SSL_CTX_use_PrivateKey_file(ctx,c_key,SSL_FILETYPE_PEM)
		|| !SSL_CTX_check_private_key(ctx))
		ssl_err("Error setting up SSL_CTX client key and cert");
	if (SSL_CTX_load_verify_locations(ctx, s_cert, NULL) != 1)
		ssl_err("Error setting up SSL_CTX verify, server cert");
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

	return ctx;
}

/** contact the server with TCP connect */
static int
contact_server(const char* svr, nsd_options_t* cfg, int statuscmd)
{
#ifdef INET6
	struct sockaddr_storage addr;
#else
	struct sockaddr_in addr;
#endif
	socklen_t addrlen;
	int fd;
	int port = cfg->control_port;
	/* use svr or a config entry */
	if(!svr) {
		if(cfg->control_interface)
			svr = cfg->control_interface->address;
		else	svr = "127.0.0.1";
		/* config 0 addr (everything), means ask localhost */
		if(strcmp(svr, "0.0.0.0") == 0)
			svr = "127.0.0.1";
		else if(strcmp(svr, "::0") == 0 ||
			strcmp(svr, "0::0") == 0 ||
			strcmp(svr, "0::") == 0 ||
			strcmp(svr, "::") == 0)
			svr = "::1";
	}
	if(strchr(svr, '@')) {
		char* ps = strchr(svr, '@');
		*ps++ = 0;
		port = atoi(ps);
		if(!port) {
			fprintf(stderr, "could not parse port %s\n", ps);
			exit(1);
		}
	} 
        if(strchr(svr, ':')) {
		struct sockaddr_in6 sa;
		addrlen = (socklen_t)sizeof(struct sockaddr_in6);
		memset(&sa, 0, addrlen);
		sa.sin6_family = AF_INET6;
		sa.sin6_port = (in_port_t)htons((uint16_t)port);
		if(inet_pton((int)sa.sin6_family, svr, &sa.sin6_addr) <= 0) {
			fprintf(stderr, "could not parse IP: %s\n", svr);
			exit(1);
		}
		memcpy(&addr, &sa, addrlen);
	} else { /* ip4 */
		struct sockaddr_in sa;
		addrlen = (socklen_t)sizeof(struct sockaddr_in);
		memset(&sa, 0, addrlen);
		sa.sin_family = AF_INET;
		sa.sin_port = (in_port_t)htons((uint16_t)port);
		if(inet_pton((int)sa.sin_family, svr, &sa.sin_addr) <= 0) {
			fprintf(stderr, "could not parse IP: %s\n", svr);
			exit(1);
		}
		memcpy(&addr, &sa, addrlen);
	}

	fd = socket(strchr(svr, ':')?AF_INET6:AF_INET, SOCK_STREAM, 0);
	if(fd == -1) {
		fprintf(stderr, "socket: %s\n", strerror(errno));
		exit(1);
	}
	if(connect(fd, (struct sockaddr*)&addr, addrlen) < 0) {
		fprintf(stderr, "error: connect (%s@%d): %s\n", svr, port,
			strerror(errno));
		if(errno == ECONNREFUSED && statuscmd) {
			printf("nsd is stopped\n");
			exit(3);
		}
		exit(1);
	}
	return fd;
}

/** setup SSL on the connection */
static SSL*
setup_ssl(SSL_CTX* ctx, int fd)
{
	SSL* ssl;
	X509* x;
	int r;

	ssl = SSL_new(ctx);
	if(!ssl)
		ssl_err("could not SSL_new");
	SSL_set_connect_state(ssl);
	(void)SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
	if(!SSL_set_fd(ssl, fd))
		ssl_err("could not SSL_set_fd");
	while(1) {
		ERR_clear_error();
		if( (r=SSL_do_handshake(ssl)) == 1)
			break;
		r = SSL_get_error(ssl, r);
		if(r != SSL_ERROR_WANT_READ && r != SSL_ERROR_WANT_WRITE)
			ssl_err("SSL handshake failed");
		/* wants to be called again */
	}

	/* check authenticity of server */
	if(SSL_get_verify_result(ssl) != X509_V_OK)
		ssl_err("SSL verification failed");
	x = SSL_get_peer_certificate(ssl);
	if(!x)
		ssl_err("Server presented no peer certificate");
	X509_free(x);
	return ssl;
}

/** send stdin to server */
static void
send_file(SSL* ssl, FILE* in, char* buf, size_t sz)
{
	char e[] = {0x04, 0x0a};
	while(fgets(buf, (int)sz, in)) {
		if(SSL_write(ssl, buf, (int)strlen(buf)) <= 0)
			ssl_err("could not SSL_write contents");
	}
	/* send end-of-file marker */
	if(SSL_write(ssl, e, (int)sizeof(e)) <= 0)
		ssl_err("could not SSL_write end-of-file marker");
}

/** send command and display result */
static int
go_cmd(SSL* ssl, int argc, char* argv[])
{
	char pre[10];
	const char* space=" ";
	const char* newline="\n";
	int was_error = 0, first_line = 1;
	int r, i;
	char buf[1024];
	snprintf(pre, sizeof(pre), "NSDCT%d ", NSD_CONTROL_VERSION);
	if(SSL_write(ssl, pre, (int)strlen(pre)) <= 0)
		ssl_err("could not SSL_write");
	for(i=0; i<argc; i++) {
		if(SSL_write(ssl, space, (int)strlen(space)) <= 0)
			ssl_err("could not SSL_write");
		if(SSL_write(ssl, argv[i], (int)strlen(argv[i])) <= 0)
			ssl_err("could not SSL_write");
	}
	if(SSL_write(ssl, newline, (int)strlen(newline)) <= 0)
		ssl_err("could not SSL_write");

	/* send contents to server */
	if(argc == 1 && (strcmp(argv[0], "addzones") == 0 ||
		strcmp(argv[0], "delzones") == 0)) {
		send_file(ssl, stdin, buf, sizeof(buf));
	}

	while(1) {
		ERR_clear_error();
		if((r = SSL_read(ssl, buf, (int)sizeof(buf)-1)) <= 0) {
			if(SSL_get_error(ssl, r) == SSL_ERROR_ZERO_RETURN) {
				/* EOF */
				break;
			}
			ssl_err("could not SSL_read");
		}
		buf[r] = 0;
		printf("%s", buf);
		if(first_line && strncmp(buf, "error", 5) == 0)
			was_error = 1;
		first_line = 0;
	}
	return was_error;
}

/** go ahead and read config, contact server and perform command and display */
static int
go(const char* cfgfile, char* svr, int argc, char* argv[])
{
	nsd_options_t* opt;
	int fd, ret;
	SSL_CTX* ctx;
	SSL* ssl;

	/* read config */
	if(!(opt = nsd_options_create(region_create(xalloc, free)))) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	tsig_init(opt->region);
	if(!parse_options_file(opt, cfgfile, NULL, NULL)) {
		fprintf(stderr, "could not read config file\n");
		exit(1);
	}
	if(!opt->control_enable)
		fprintf(stderr, "warning: control-enable is 'no' in the config file.\n");
	ctx = setup_ctx(opt);

	/* contact server */
	fd = contact_server(svr, opt, argc>0&&strcmp(argv[0],"status")==0);
	ssl = setup_ssl(ctx, fd);

	/* send command */
	ret = go_cmd(ssl, argc, argv);

	SSL_free(ssl);
	close(fd);
	SSL_CTX_free(ctx);
	region_destroy(opt->region);
	return ret;
}

/** getopt global, in case header files fail to declare it. */
extern int optind;
/** getopt global, in case header files fail to declare it. */
extern char* optarg;

/** Main routine for nsd-control */
int main(int argc, char* argv[])
{
	int c;
	const char* cfgfile = CONFIGFILE;
	char* svr = NULL;
#ifdef USE_WINSOCK
	int r;
	WSADATA wsa_data;
#endif
	log_init("nsd-control");

#ifdef HAVE_ERR_LOAD_CRYPTO_STRINGS
	ERR_load_crypto_strings();
#endif
	ERR_load_SSL_strings();
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
	OPENSSL_init_ssl(0, NULL);
#endif

	if(!RAND_status()) {
                /* try to seed it */
                unsigned char buf[256];
                unsigned int v, seed=(unsigned)time(NULL) ^ (unsigned)getpid();
                size_t i;
		v = seed;
                for(i=0; i<256/sizeof(v); i++) {
                        memmove(buf+i*sizeof(v), &v, sizeof(v));
                        v = v*seed + (unsigned int)i;
                }
                RAND_seed(buf, 256);
		fprintf(stderr, "warning: no entropy, seeding openssl PRNG with time\n");
	}

	/* parse the options */
	while( (c=getopt(argc, argv, "c:s:h")) != -1) {
		switch(c) {
		case 'c':
			cfgfile = optarg;
			break;
		case 's':
			svr = optarg;
			break;
		case '?':
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if(argc == 0)
		usage();
	if(argc >= 1 && strcmp(argv[0], "start")==0) {
		if(execl(NSD_START_PATH, "nsd", "-c", cfgfile, 
			(char*)NULL) < 0) {
			fprintf(stderr, "could not exec %s: %s\n",
				NSD_START_PATH, strerror(errno));
			exit(1);
		}
	}

	return go(cfgfile, svr, argc, argv);
}

#else /* HAVE_SSL */
int main(void)
{
	printf("error: NSD was compiled without SSL.\n");
	return 1;
}
#endif /* HAVE_SSL */
