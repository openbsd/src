/*
 * Copyright (C) 2000-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: nsupdate.c,v 1.103.2.11 2002/08/06 04:23:20 marka Exp $ */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>

#include <isc/app.h>
#include <isc/base64.h>
#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/event.h>
#include <isc/lex.h>
#include <isc/mem.h>
#include <isc/region.h>
#include <isc/sockaddr.h>
#include <isc/socket.h>
#include <isc/stdio.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/types.h>
#include <isc/util.h>

#include <dns/callbacks.h>
#include <dns/dispatch.h>
#include <dns/events.h>
#include <dns/fixedname.h>
#include <dns/masterdump.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/request.h>
#include <dns/result.h>
#include <dns/tsig.h>

#include <dst/dst.h>

#include <lwres/lwres.h>
#include <lwres/net.h>

#ifdef HAVE_ADDRINFO
#ifdef HAVE_GETADDRINFO
#ifdef HAVE_GAISTRERROR
#define USE_GETADDRINFO
#endif
#endif
#endif

#ifndef USE_GETADDRINFO
#ifndef ISC_PLATFORM_NONSTDHERRNO
extern int h_errno;
#endif
#endif

#define MAXCMD (4 * 1024)
#define MAXWIRE (64 * 1024)
#define NAMEBUF 512
#define WORDLEN 512
#define PACKETSIZE ((64 * 1024) - 1)
#define INITTEXT (2 * 1024)
#define MAXTEXT (128 * 1024)
#define FIND_TIMEOUT 5
#define TTL_MAX 2147483647	/* Maximum signed 32 bit integer. */

#define DNSDEFAULTPORT 53

#ifndef RESOLV_CONF
#define RESOLV_CONF "/etc/resolv.conf"
#endif

static isc_boolean_t debugging = ISC_FALSE, ddebugging = ISC_FALSE;
static isc_boolean_t memdebugging = ISC_FALSE;
static isc_boolean_t have_ipv4 = ISC_FALSE;
static isc_boolean_t have_ipv6 = ISC_FALSE;
static isc_boolean_t is_dst_up = ISC_FALSE;
static isc_boolean_t usevc = ISC_FALSE;
static isc_taskmgr_t *taskmgr = NULL;
static isc_task_t *global_task = NULL;
static isc_event_t *global_event = NULL;
static isc_mem_t *mctx = NULL;
static dns_dispatchmgr_t *dispatchmgr = NULL;
static dns_requestmgr_t *requestmgr = NULL;
static isc_socketmgr_t *socketmgr = NULL;
static isc_timermgr_t *timermgr = NULL;
static dns_dispatch_t *dispatchv4 = NULL;
static dns_dispatch_t *dispatchv6 = NULL;
static dns_message_t *updatemsg = NULL;
static dns_fixedname_t resolvdomain; /* from resolv.conf's domain line */
static dns_name_t *origin; /* Points to one of above, or dns_rootname */
static dns_fixedname_t fuserzone;
static dns_name_t *userzone = NULL;
static dns_tsigkey_t *key = NULL;
static lwres_context_t *lwctx = NULL;
static lwres_conf_t *lwconf;
static isc_sockaddr_t *servers;
static int ns_inuse = 0;
static int ns_total = 0;
static isc_sockaddr_t *userserver = NULL;
static isc_sockaddr_t *localaddr = NULL;
static char *keystr = NULL, *keyfile = NULL;
static isc_entropy_t *entp = NULL;
static isc_boolean_t shuttingdown = ISC_FALSE;
static FILE *input;
static isc_boolean_t interactive = ISC_TRUE;
static isc_boolean_t seenerror = ISC_FALSE;
static const dns_master_style_t *style;
static int requests = 0;

typedef struct nsu_requestinfo {
	dns_message_t *msg;
	isc_sockaddr_t *addr;
} nsu_requestinfo_t;

static void
sendrequest(isc_sockaddr_t *srcaddr, isc_sockaddr_t *destaddr,
	    dns_message_t *msg, dns_request_t **request);
static void
fatal(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

static void
debug(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

static void
ddebug(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

#define STATUS_MORE	(isc_uint16_t)0
#define STATUS_SEND	(isc_uint16_t)1
#define STATUS_QUIT	(isc_uint16_t)2
#define STATUS_SYNTAX	(isc_uint16_t)3

static void
fatal(const char *format, ...) {
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(1);
}

static void
debug(const char *format, ...) {
	va_list args;

	if (debugging) {
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);
		fprintf(stderr, "\n");
	}
}

static void
ddebug(const char *format, ...) {
	va_list args;

	if (ddebugging) {
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);
		fprintf(stderr, "\n");
	}
}

static inline void
check_result(isc_result_t result, const char *msg) {
	if (result != ISC_R_SUCCESS)
		fatal("%s: %s", msg, isc_result_totext(result));
}

static void *
mem_alloc(void *arg, size_t size) {
	return (isc_mem_get(arg, size));
}

static void
mem_free(void *arg, void *mem, size_t size) {
	isc_mem_put(arg, mem, size);
}

static char *
nsu_strsep(char **stringp, const char *delim) {
	char *string = *stringp;
	char *s;
	const char *d;
	char sc, dc;

	if (string == NULL)
		return (NULL);

	for (; *string != '\0'; string++) {
		sc = *string;
		for (d = delim; (dc = *d) != '\0'; d++) {
			if (sc == dc)
				break;
		}
		if (dc == 0)
			break;
	}

	for (s = string; *s != '\0'; s++) {
		sc = *s;
		for (d = delim; (dc = *d) != '\0'; d++) {
			if (sc == dc) {
				*s++ = '\0';
				*stringp = s;
				return (string);
			}
		}
	}
	*stringp = NULL;
	return (string);
}

static unsigned int
count_dots(char *s, isc_boolean_t *last_was_dot) {
	int i = 0;
	*last_was_dot = ISC_FALSE;
	while (*s != 0) {
		if (*s++ == '.') {
			i++;
			*last_was_dot = ISC_TRUE;
		} else
			*last_was_dot = ISC_FALSE;
	}
	return (i);
}

static void
reset_system(void) {
	isc_result_t result;

	ddebug("reset_system()");
	/* If the update message is still around, destroy it */
	if (updatemsg != NULL)
		dns_message_reset(updatemsg, DNS_MESSAGE_INTENTRENDER);
	else {
		result = dns_message_create(mctx, DNS_MESSAGE_INTENTRENDER,
					    &updatemsg);
		check_result(result, "dns_message_create");
	}
	updatemsg->opcode = dns_opcode_update;
}

static void
setup_keystr(void) {
	unsigned char *secret = NULL;
	int secretlen;
	isc_buffer_t secretbuf;
	isc_result_t result;
	isc_buffer_t keynamesrc;
	char *secretstr;
	char *s;
	dns_fixedname_t fkeyname;
	dns_name_t *keyname;

	dns_fixedname_init(&fkeyname);
	keyname = dns_fixedname_name(&fkeyname);

	debug("Creating key...");

	s = strchr(keystr, ':');
	if (s == NULL || s == keystr || *s == 0)
		fatal("key option must specify keyname:secret");
	secretstr = s + 1;

	isc_buffer_init(&keynamesrc, keystr, s - keystr);
	isc_buffer_add(&keynamesrc, s - keystr);

	debug("namefromtext");
	result = dns_name_fromtext(keyname, &keynamesrc, dns_rootname,
				   ISC_FALSE, NULL);
	check_result(result, "dns_name_fromtext");

	secretlen = strlen(secretstr) * 3 / 4;
	secret = isc_mem_allocate(mctx, secretlen);
	if (secret == NULL)
		fatal("out of memory");

	isc_buffer_init(&secretbuf, secret, secretlen);
	result = isc_base64_decodestring(secretstr, &secretbuf);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not create key from %s: %s\n",
			keystr, isc_result_totext(result));
		goto failure;
	}

	secretlen = isc_buffer_usedlength(&secretbuf);

	debug("keycreate");
	result = dns_tsigkey_create(keyname, dns_tsig_hmacmd5_name,
				    secret, secretlen, ISC_TRUE, NULL,
				    0, 0, mctx, NULL, &key);
	if (result != ISC_R_SUCCESS)
		fprintf(stderr, "could not create key from %s: %s\n",
			keystr, dns_result_totext(result));
 failure:
	if (secret != NULL)
		isc_mem_free(mctx, secret);
}

static void
setup_keyfile(void) {
	dst_key_t *dstkey = NULL;
	isc_result_t result;

	debug("Creating key...");

	result = dst_key_fromnamedfile(keyfile, DST_TYPE_PRIVATE, mctx,
				       &dstkey);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not read key from %s: %s\n",
			keyfile, isc_result_totext(result));
		return;
	}
	result = dns_tsigkey_createfromkey(dst_key_name(dstkey),
					   dns_tsig_hmacmd5_name,
					   dstkey, ISC_FALSE, NULL,
					   0, 0, mctx, NULL, &key);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not create key from %s: %s\n",
			keyfile, isc_result_totext(result));
		dst_key_free(&dstkey);
		return;
	}
}

static void
doshutdown(void) {
	isc_task_detach(&global_task);

	if (userserver != NULL)
		isc_mem_put(mctx, userserver, sizeof(isc_sockaddr_t));

	if (localaddr != NULL)
		isc_mem_put(mctx, localaddr, sizeof(isc_sockaddr_t));

	if (key != NULL) {
		ddebug("Freeing key");
		dns_tsigkey_detach(&key);
	}

	if (updatemsg != NULL)
		dns_message_destroy(&updatemsg);

	if (is_dst_up) {
		ddebug("Destroy DST lib");
		dst_lib_destroy();
		is_dst_up = ISC_FALSE;
	}

	if (entp != NULL) {
		ddebug("Detach from entropy");
		isc_entropy_detach(&entp);
	}

	lwres_conf_clear(lwctx);
	lwres_context_destroy(&lwctx);

	isc_mem_put(mctx, servers, ns_total * sizeof(isc_sockaddr_t));

	ddebug("Destroying request manager");
	dns_requestmgr_detach(&requestmgr);

	ddebug("Freeing the dispatchers");
	if (have_ipv4)
		dns_dispatch_detach(&dispatchv4);
	if (have_ipv6)
		dns_dispatch_detach(&dispatchv6);

	ddebug("Shutting down dispatch manager");
	dns_dispatchmgr_destroy(&dispatchmgr);

}

static void
maybeshutdown(void) {
	ddebug("Shutting down request manager");
	dns_requestmgr_shutdown(requestmgr);

	if (requests != 0)
		return;

	doshutdown();
}

static void
shutdown_program(isc_task_t *task, isc_event_t *event) {
	REQUIRE(task == global_task);
	UNUSED(task);

	ddebug("shutdown_program()");
	isc_event_free(&event);

	shuttingdown = ISC_TRUE;
	maybeshutdown();
}

static void
setup_system(void) {
	isc_result_t result;
	isc_sockaddr_t bind_any, bind_any6;
	isc_buffer_t buf;
	lwres_result_t lwresult;
	unsigned int attrs, attrmask;
	int i;

	ddebug("setup_system()");

	dns_result_register();

	result = isc_net_probeipv4();
	if (result == ISC_R_SUCCESS)
		have_ipv4 = ISC_TRUE;

	result = isc_net_probeipv6();
	if (result == ISC_R_SUCCESS)
		have_ipv6 = ISC_TRUE;

	if (!have_ipv4 && !have_ipv6)
		fatal("could not find either IPv4 or IPv6");

	result = isc_mem_create(0, 0, &mctx);
	check_result(result, "isc_mem_create");

	lwresult = lwres_context_create(&lwctx, mctx, mem_alloc, mem_free, 1);
	if (lwresult != LWRES_R_SUCCESS)
		fatal("lwres_context_create failed");

	(void)lwres_conf_parse(lwctx, RESOLV_CONF);
	lwconf = lwres_conf_get(lwctx);

	ns_total = lwconf->nsnext;
	if (ns_total <= 0) {
		/* No name servers in resolv.conf; default to loopback. */
		struct in_addr localhost;
		ns_total = 1;
		servers = isc_mem_get(mctx, ns_total * sizeof(isc_sockaddr_t));
		if (servers == NULL)
			fatal("out of memory");
		localhost.s_addr = htonl(INADDR_LOOPBACK);
		isc_sockaddr_fromin(&servers[0], &localhost, DNSDEFAULTPORT);
	} else {
		servers = isc_mem_get(mctx, ns_total * sizeof(isc_sockaddr_t));
		if (servers == NULL)
			fatal("out of memory");
		for (i = 0; i < ns_total; i++) {
			if (lwconf->nameservers[i].family == LWRES_ADDRTYPE_V4) {
				struct in_addr in4;
				memcpy(&in4, lwconf->nameservers[i].address, 4);
				isc_sockaddr_fromin(&servers[i], &in4, DNSDEFAULTPORT);
			} else {
				struct in6_addr in6;
				memcpy(&in6, lwconf->nameservers[i].address, 16);
				isc_sockaddr_fromin6(&servers[i], &in6,
						     DNSDEFAULTPORT);
			}
		}
	}

	result = isc_entropy_create(mctx, &entp);
	check_result(result, "isc_entropy_create");

	result = dns_dispatchmgr_create(mctx, entp, &dispatchmgr);
	check_result(result, "dns_dispatchmgr_create");

	result = isc_socketmgr_create(mctx, &socketmgr);
	check_result(result, "dns_socketmgr_create");

	result = isc_timermgr_create(mctx, &timermgr);
	check_result(result, "dns_timermgr_create");

	result = isc_taskmgr_create(mctx, 1, 0, &taskmgr);
	check_result(result, "isc_taskmgr_create");

	result = isc_task_create(taskmgr, 0, &global_task);
	check_result(result, "isc_task_create");

	result = isc_task_onshutdown(global_task, shutdown_program, NULL);
	check_result(result, "isc_task_onshutdown");

	result = dst_lib_init(mctx, entp, 0);
	check_result(result, "dst_lib_init");
	is_dst_up = ISC_TRUE;

	attrmask = DNS_DISPATCHATTR_UDP | DNS_DISPATCHATTR_TCP;
	attrmask |= DNS_DISPATCHATTR_IPV4 | DNS_DISPATCHATTR_IPV6;

	if (have_ipv6) {
		attrs = DNS_DISPATCHATTR_UDP;
		attrs |= DNS_DISPATCHATTR_MAKEQUERY;
		attrs |= DNS_DISPATCHATTR_IPV6;
		isc_sockaddr_any6(&bind_any6);
		result = dns_dispatch_getudp(dispatchmgr, socketmgr, taskmgr,
					     &bind_any6, PACKETSIZE,
					     4, 2, 3, 5,
					     attrs, attrmask, &dispatchv6);
		check_result(result, "dns_dispatch_getudp (v6)");
	}

	if (have_ipv4) {
		attrs = DNS_DISPATCHATTR_UDP;
		attrs |= DNS_DISPATCHATTR_MAKEQUERY;
		attrs |= DNS_DISPATCHATTR_IPV4;
		isc_sockaddr_any(&bind_any);
		result = dns_dispatch_getudp(dispatchmgr, socketmgr, taskmgr,
					     &bind_any, PACKETSIZE,
					     4, 2, 3, 5,
					     attrs, attrmask, &dispatchv4);
		check_result(result, "dns_dispatch_getudp (v4)");
	}

	result = dns_requestmgr_create(mctx, timermgr,
				       socketmgr, taskmgr, dispatchmgr,
				       dispatchv4, dispatchv6, &requestmgr);
	check_result(result, "dns_requestmgr_create");

	if (lwconf->domainname != NULL) {
		dns_fixedname_init(&resolvdomain);
		isc_buffer_init(&buf, lwconf->domainname,
				strlen(lwconf->domainname));
		isc_buffer_add(&buf, strlen(lwconf->domainname));
		result = dns_name_fromtext(dns_fixedname_name(&resolvdomain),
					   &buf, dns_rootname, ISC_FALSE,
					   NULL);
		check_result(result, "dns_name_fromtext");
		origin = dns_fixedname_name(&resolvdomain);
	}
	else
		origin = dns_rootname;

	if (keystr != NULL)
		setup_keystr();
	else if (keyfile != NULL)
		setup_keyfile();
}

static void
get_address(char *host, in_port_t port, isc_sockaddr_t *sockaddr) {
	struct in_addr in4;
	struct in6_addr in6;
#ifdef USE_GETADDRINFO
	struct addrinfo *res = NULL, hints;
	int result;
#else
	struct hostent *he;
#endif

	ddebug("get_address()");

	/*
	 * Assume we have v4 if we don't have v6, since setup_libs
	 * fatal()'s out if we don't have either.
	 */
	if (have_ipv6 && inet_pton(AF_INET6, host, &in6) == 1)
		isc_sockaddr_fromin6(sockaddr, &in6, port);
	else if (inet_pton(AF_INET, host, &in4) == 1)
		isc_sockaddr_fromin(sockaddr, &in4, port);
	else {
#ifdef USE_GETADDRINFO
		memset(&hints, 0, sizeof(hints));
		if (!have_ipv6)
			hints.ai_family = PF_INET;
		else if (!have_ipv4)
			hints.ai_family = PF_INET6;
		else
			hints.ai_family = PF_UNSPEC;
		debug ("before getaddrinfo()");
		isc_app_block();
		result = getaddrinfo(host, NULL, &hints, &res);
		isc_app_unblock();
		if (result != 0) {
			fatal("couldn't find server '%s': %s",
			      host, gai_strerror(result));
		}
		memcpy(&sockaddr->type.sa,res->ai_addr, res->ai_addrlen);
		sockaddr->length = res->ai_addrlen;
		isc_sockaddr_setport(sockaddr, port);
		freeaddrinfo(res);
#else
		debug ("before gethostbyname()");
		isc_app_block();
		he = gethostbyname(host);
		isc_app_unblock();
		if (he == NULL)
		     fatal("couldn't find server '%s' (h_errno=%d)",
			   host, h_errno);
		INSIST(he->h_addrtype == AF_INET);
		isc_sockaddr_fromin(sockaddr,
				    (struct in_addr *)(he->h_addr_list[0]),
				    port);
#endif
	}
}

static void
parse_args(int argc, char **argv) {
	int ch;
	isc_result_t result;

	debug("parse_args");
	while ((ch = isc_commandline_parse(argc, argv, "dDMy:vk:")) != -1) {
		switch (ch) {
		case 'd':
			debugging = ISC_TRUE;
			break;
		case 'D': /* was -dd */
			debugging = ISC_TRUE;
			ddebugging = ISC_TRUE;
			break;
		case 'M': /* was -dm */
			debugging = ISC_TRUE;
			ddebugging = ISC_TRUE;
			memdebugging = ISC_TRUE;
			isc_mem_debugging = ISC_MEM_DEBUGTRACE |
					    ISC_MEM_DEBUGRECORD;
			break;
		case 'y':
			keystr = isc_commandline_argument;
			break;
		case 'v':
			usevc = ISC_TRUE;
			break;
		case 'k':
			keyfile = isc_commandline_argument;
			break;
		default:
			fprintf(stderr, "%s: invalid argument -%c\n",
				argv[0], ch);
			fprintf(stderr, "usage: nsupdate [-d] "
				"[-y keyname:secret | -k keyfile] [-v] "
				"[filename]\n");
			exit(1);
		}
	}
	if (keyfile != NULL && keystr != NULL) {
		fprintf(stderr, "%s: cannot specify both -k and -y\n",
			argv[0]);
		exit(1);
	}

	if (argv[isc_commandline_index] != NULL) {
		if (strcmp(argv[isc_commandline_index], "-") == 0) {
			input = stdin;
		} else {
			result = isc_stdio_open(argv[isc_commandline_index],
						"r", &input);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "could not open '%s': %s\n",
					argv[isc_commandline_index],
					isc_result_totext(result));
				exit(1);
			}
		}
		interactive = ISC_FALSE;
	}
}

static isc_uint16_t
parse_name(char **cmdlinep, dns_message_t *msg, dns_name_t **namep) {
	isc_result_t result;
	char *word;
	isc_buffer_t *namebuf = NULL;
	isc_buffer_t source;
	unsigned int dots;
	isc_boolean_t last;
	dns_name_t *rn;

	word = nsu_strsep(cmdlinep, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read owner name\n");
		return (STATUS_SYNTAX);
	}

	result = dns_message_gettempname(msg, namep);
	check_result(result, "dns_message_gettempname");
	result = isc_buffer_allocate(mctx, &namebuf, NAMEBUF);
	check_result(result, "isc_buffer_allocate");
	dns_name_init(*namep, NULL);
	dns_name_setbuffer(*namep, namebuf);
	dns_message_takebuffer(msg, &namebuf);
	isc_buffer_init(&source, word, strlen(word));
	isc_buffer_add(&source, strlen(word));
	dots = count_dots(word, &last);
	if (dots > lwconf->ndots || last)
		rn = dns_rootname;
	else if (userzone != NULL)
		rn = userzone;
	else
		rn = origin;
	result = dns_name_fromtext(*namep, &source, rn,
				   ISC_FALSE, NULL);
	check_result(result, "dns_name_fromtext");
	isc_buffer_invalidate(&source);
	return (STATUS_MORE);
}

static isc_uint16_t
parse_rdata(char **cmdlinep, dns_rdataclass_t rdataclass,
	    dns_rdatatype_t rdatatype, dns_message_t *msg,
	    dns_rdata_t *rdata)
{
	char *cmdline = *cmdlinep;
	isc_buffer_t source, *buf = NULL, *newbuf = NULL;
	isc_region_t r;
	isc_lex_t *lex = NULL;
	dns_rdatacallbacks_t callbacks;
	isc_result_t result;
	dns_name_t *rn;

	while (*cmdline != 0 && isspace((unsigned char)*cmdline))
		cmdline++;

	if (*cmdline != 0) {
		dns_rdatacallbacks_init(&callbacks);
		if (userzone != NULL)
			rn = userzone;
		else
			rn = origin;
		result = isc_lex_create(mctx, strlen(cmdline), &lex);
		check_result(result, "isc_lex_create");
		isc_buffer_init(&source, cmdline, strlen(cmdline));
		isc_buffer_add(&source, strlen(cmdline));
		result = isc_lex_openbuffer(lex, &source);
		check_result(result, "isc_lex_openbuffer");
		result = isc_buffer_allocate(mctx, &buf, MAXWIRE);
		check_result(result, "isc_buffer_allocate");
		result = dns_rdata_fromtext(rdata, rdataclass, rdatatype, lex,
					    rn, ISC_FALSE, mctx, buf,
					    &callbacks);
		isc_lex_destroy(&lex);
		if (result == ISC_R_SUCCESS) {
			isc_buffer_usedregion(buf, &r);
			result = isc_buffer_allocate(mctx, &newbuf, r.length);
			check_result(result, "isc_buffer_allocate");
			isc_buffer_putmem(newbuf, r.base, r.length);
			isc_buffer_usedregion(newbuf, &r);
			dns_rdata_fromregion(rdata, rdataclass, rdatatype, &r);
			isc_buffer_free(&buf);
			dns_message_takebuffer(msg, &newbuf);
		} else {
			fprintf(stderr, "invalid rdata format: %s\n",
				isc_result_totext(result));
			isc_buffer_free(&buf);
			return (STATUS_SYNTAX);
		}
	} else {
		rdata->flags = DNS_RDATA_UPDATE;
	}
	*cmdlinep = cmdline;
	return (STATUS_MORE);
}

static isc_uint16_t
make_prereq(char *cmdline, isc_boolean_t ispositive, isc_boolean_t isrrset) {
	isc_result_t result;
	char *word;
	dns_name_t *name = NULL;
	isc_textregion_t region;
	dns_rdataset_t *rdataset = NULL;
	dns_rdatalist_t *rdatalist = NULL;
	dns_rdataclass_t rdataclass;
	dns_rdatatype_t rdatatype;
	dns_rdata_t *rdata = NULL;
	isc_uint16_t retval;

	ddebug("make_prereq()");

	/*
	 * Read the owner name
	 */
	retval = parse_name(&cmdline, updatemsg, &name);
	if (retval != STATUS_MORE)
		return (retval);

	/*
	 * If this is an rrset prereq, read the class or type.
	 */
	if (isrrset) {
		word = nsu_strsep(&cmdline, " \t\r\n");
		if (*word == 0) {
			fprintf(stderr, "could not read class or type\n");
			goto failure;
		}
		region.base = word;
		region.length = strlen(word);
		result = dns_rdataclass_fromtext(&rdataclass, &region);
		if (result == ISC_R_SUCCESS) {
			/*
			 * Now read the type.
			 */
			word = nsu_strsep(&cmdline, " \t\r\n");
			if (*word == 0) {
				fprintf(stderr, "could not read type\n");
				goto failure;
			}
			region.base = word;
			region.length = strlen(word);
			result = dns_rdatatype_fromtext(&rdatatype, &region);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "invalid type: %s\n", word);
				goto failure;
			}
		} else {
			rdataclass = dns_rdataclass_in;
			result = dns_rdatatype_fromtext(&rdatatype, &region);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "invalid type: %s\n", word);
				goto failure;
			}
		}
	} else
		rdatatype = dns_rdatatype_any;

	result = dns_message_gettemprdata(updatemsg, &rdata);
	check_result(result, "dns_message_gettemprdata");

	rdata->data = NULL;
	rdata->length = 0;

	if (isrrset && ispositive) {
		retval = parse_rdata(&cmdline, rdataclass, rdatatype,
				     updatemsg, rdata);
		if (retval != STATUS_MORE)
			goto failure;
	} else
		rdata->flags = DNS_RDATA_UPDATE;

	result = dns_message_gettemprdatalist(updatemsg, &rdatalist);
	check_result(result, "dns_message_gettemprdatalist");
	result = dns_message_gettemprdataset(updatemsg, &rdataset);
	check_result(result, "dns_message_gettemprdataset");
	dns_rdatalist_init(rdatalist);
	rdatalist->type = rdatatype;
	if (ispositive) {
		if (isrrset && rdata->data != NULL)
			rdatalist->rdclass = rdataclass;
		else
			rdatalist->rdclass = dns_rdataclass_any;
	} else
		rdatalist->rdclass = dns_rdataclass_none;
	rdatalist->covers = 0;
	rdatalist->ttl = 0;
	rdata->rdclass = rdatalist->rdclass;
	rdata->type = rdatatype;
	ISC_LIST_INIT(rdatalist->rdata);
	ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	dns_rdataset_init(rdataset);
	dns_rdatalist_tordataset(rdatalist, rdataset);
	ISC_LIST_INIT(name->list);
	ISC_LIST_APPEND(name->list, rdataset, link);
	dns_message_addname(updatemsg, name, DNS_SECTION_PREREQUISITE);
	return (STATUS_MORE);

 failure:
	if (name != NULL)
		dns_message_puttempname(updatemsg, &name);
	return (STATUS_SYNTAX);
}

static isc_uint16_t
evaluate_prereq(char *cmdline) {
	char *word;
	isc_boolean_t ispositive, isrrset;

	ddebug("evaluate_prereq()");
	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read operation code\n");
		return (STATUS_SYNTAX);
	}
	if (strcasecmp(word, "nxdomain") == 0) {
		ispositive = ISC_FALSE;
		isrrset = ISC_FALSE;
	} else if (strcasecmp(word, "yxdomain") == 0) {
		ispositive = ISC_TRUE;
		isrrset = ISC_FALSE;
	} else if (strcasecmp(word, "nxrrset") == 0) {
		ispositive = ISC_FALSE;
		isrrset = ISC_TRUE;
	} else if (strcasecmp(word, "yxrrset") == 0) {
		ispositive = ISC_TRUE;
		isrrset = ISC_TRUE;
	} else {
		fprintf(stderr, "incorrect operation code: %s\n", word);
		return (STATUS_SYNTAX);
	}
	return (make_prereq(cmdline, ispositive, isrrset));
}

static isc_uint16_t
evaluate_server(char *cmdline) {
	char *word, *server;
	long port;

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read server name\n");
		return (STATUS_SYNTAX);
	}
	server = word;

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0)
		port = DNSDEFAULTPORT;
	else {
		char *endp;
		port = strtol(word, &endp, 10);
		if (*endp != 0) {
			fprintf(stderr, "port '%s' is not numeric\n", word);
			return (STATUS_SYNTAX);
		} else if (port < 1 || port > 65535) {
			fprintf(stderr, "port '%s' is out of range "
				"(1 to 65535)\n", word);
			return (STATUS_SYNTAX);
		}
	}

	if (userserver == NULL) {
		userserver = isc_mem_get(mctx, sizeof(isc_sockaddr_t));
		if (userserver == NULL)
			fatal("out of memory");
	}

	get_address(server, (in_port_t)port, userserver);

	return (STATUS_MORE);
}

static isc_uint16_t
evaluate_local(char *cmdline) {
	char *word, *local;
	long port;
	struct in_addr in4;
	struct in6_addr in6;

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read server name\n");
		return (STATUS_SYNTAX);
	}
	local = word;

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0)
		port = 0;
	else {
		char *endp;
		port = strtol(word, &endp, 10);
		if (*endp != 0) {
			fprintf(stderr, "port '%s' is not numeric\n", word);
			return (STATUS_SYNTAX);
		} else if (port < 1 || port > 65535) {
			fprintf(stderr, "port '%s' is out of range "
				"(1 to 65535)\n", word);
			return (STATUS_SYNTAX);
		}
	}

	if (localaddr == NULL) {
		localaddr = isc_mem_get(mctx, sizeof(isc_sockaddr_t));
		if (localaddr == NULL)
			fatal("out of memory");
	}

	if (have_ipv6 && inet_pton(AF_INET6, local, &in6) == 1)
		isc_sockaddr_fromin6(localaddr, &in6, (in_port_t)port);
	else if (have_ipv4 && inet_pton(AF_INET, local, &in4) == 1)
		isc_sockaddr_fromin(localaddr, &in4, (in_port_t)port);
	else {
		fprintf(stderr, "invalid address %s", local);
		return (STATUS_SYNTAX);
	}

	return (STATUS_MORE);
}

static isc_uint16_t
evaluate_key(char *cmdline) {
	char *namestr;
	char *secretstr;
	isc_buffer_t b;
	isc_result_t result;
	dns_fixedname_t fkeyname;
	dns_name_t *keyname;
	int secretlen;
	unsigned char *secret = NULL;
	isc_buffer_t secretbuf;

	namestr = nsu_strsep(&cmdline, " \t\r\n");
	if (*namestr == 0) {
		fprintf(stderr, "could not read key name\n");
		return (STATUS_SYNTAX);
	}

	dns_fixedname_init(&fkeyname);
	keyname = dns_fixedname_name(&fkeyname);

	isc_buffer_init(&b, namestr, strlen(namestr));
	isc_buffer_add(&b, strlen(namestr));
	result = dns_name_fromtext(keyname, &b, dns_rootname, ISC_FALSE, NULL);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not parse key name\n");
		return (STATUS_SYNTAX);
	}

	secretstr = nsu_strsep(&cmdline, "\r\n");
	if (*secretstr == 0) {
		fprintf(stderr, "could not read key secret\n");
		return (STATUS_SYNTAX);
	}
	secretlen = strlen(secretstr) * 3 / 4;
	secret = isc_mem_allocate(mctx, secretlen);
	if (secret == NULL)
		fatal("out of memory");
	
	isc_buffer_init(&secretbuf, secret, secretlen);
	result = isc_base64_decodestring(secretstr, &secretbuf);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not create key from %s: %s\n",
			secretstr, isc_result_totext(result));
		isc_mem_free(mctx, secret);
		return (STATUS_SYNTAX);
	}
	secretlen = isc_buffer_usedlength(&secretbuf);

	if (key != NULL)
		dns_tsigkey_detach(&key);
	result = dns_tsigkey_create(keyname, dns_tsig_hmacmd5_name,
                                    secret, secretlen, ISC_TRUE, NULL, 0, 0,
                                    mctx, NULL, &key);
	isc_mem_free(mctx, secret);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not create key from %s %s: %s\n",
			namestr, secretstr, dns_result_totext(result));
		return (STATUS_SYNTAX);
	}
	return (STATUS_MORE);
}

static isc_uint16_t
evaluate_zone(char *cmdline) {
	char *word;
	isc_buffer_t b;
	isc_result_t result;

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read zone name\n");
		return (STATUS_SYNTAX);
	}

	dns_fixedname_init(&fuserzone);
	userzone = dns_fixedname_name(&fuserzone);
	isc_buffer_init(&b, word, strlen(word));
	isc_buffer_add(&b, strlen(word));
	result = dns_name_fromtext(userzone, &b, dns_rootname, ISC_FALSE,
				   NULL);
	if (result != ISC_R_SUCCESS) {
		userzone = NULL; /* Lest it point to an invalid name */
		fprintf(stderr, "could not parse zone name\n");
		return (STATUS_SYNTAX);
	}

	return (STATUS_MORE);
}

static isc_uint16_t
update_addordelete(char *cmdline, isc_boolean_t isdelete) {
	isc_result_t result;
	dns_name_t *name = NULL;
	unsigned long ttl;
	char *word;
	dns_rdataclass_t rdataclass;
	dns_rdatatype_t rdatatype;
	dns_rdata_t *rdata = NULL;
	dns_rdatalist_t *rdatalist = NULL;
	dns_rdataset_t *rdataset = NULL;
	isc_textregion_t region;
	char *endp;
	isc_uint16_t retval;

	ddebug("update_addordelete()");

	/*
	 * Read the owner name.
	 */
	retval = parse_name(&cmdline, updatemsg, &name);
	if (retval != STATUS_MORE)
		return (retval);

	result = dns_message_gettemprdata(updatemsg, &rdata);
	check_result(result, "dns_message_gettemprdata");

	rdata->rdclass = 0;
	rdata->type = 0;
	rdata->data = NULL;
	rdata->length = 0;

	/*
	 * If this is an add, read the TTL and verify that it's in range.
	 * If it's a delete, ignore a TTL if present (for compatibility).
	 */
	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		if (!isdelete) {
			fprintf(stderr, "could not read owner ttl\n");
			goto failure;
		}
		else {
			ttl = 0;
			rdataclass = dns_rdataclass_any;
			rdatatype = dns_rdatatype_any;
			rdata->flags = DNS_RDATA_UPDATE;
			goto doneparsing;
		}
	}
	ttl = strtoul(word, &endp, 10);
	if (!isdigit((unsigned char)*word) || *endp != '\0') {
		if (isdelete) {
			ttl = 0;
			goto parseclass;
		} else {
			fprintf(stderr, "ttl '%s' is not legal\n", word);
			goto failure;
		}
	}

	if (isdelete)
		ttl = 0;
	else if (ttl > TTL_MAX) {
		fprintf(stderr, "ttl '%s' is out of range (0 to %d)\n",
			word, TTL_MAX);
		goto failure;
	}

	/*
	 * Read the class or type.
	 */
	word = nsu_strsep(&cmdline, " \t\r\n");
 parseclass:
	if (*word == 0) {
		if (isdelete) {
			rdataclass = dns_rdataclass_any;
			rdatatype = dns_rdatatype_any;
			rdata->flags = DNS_RDATA_UPDATE;
			goto doneparsing;
		} else {
			fprintf(stderr, "could not read class or type\n");
			goto failure;
		}
	}
	region.base = word;
	region.length = strlen(word);
	result = dns_rdataclass_fromtext(&rdataclass, &region);
	if (result == ISC_R_SUCCESS) {
		/*
		 * Now read the type.
		 */
		word = nsu_strsep(&cmdline, " \t\r\n");
		if (*word == 0) {
			if (isdelete) {
				rdataclass = dns_rdataclass_any;
				rdatatype = dns_rdatatype_any;
				rdata->flags = DNS_RDATA_UPDATE;
				goto doneparsing;
			} else {
				fprintf(stderr, "could not read type\n");
				goto failure;
			}
		}
		region.base = word;
		region.length = strlen(word);
		result = dns_rdatatype_fromtext(&rdatatype, &region);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "'%s' is not a valid type: %s\n",
				word, isc_result_totext(result));
			goto failure;
		}
	} else {
		rdataclass = dns_rdataclass_in;
		result = dns_rdatatype_fromtext(&rdatatype, &region);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "'%s' is not a valid class or type: "
				"%s\n", word, isc_result_totext(result));
			goto failure;
		}
	}

	retval = parse_rdata(&cmdline, rdataclass, rdatatype, updatemsg,
			     rdata);
	if (retval != STATUS_MORE)
		goto failure;

	if (isdelete) {
		if ((rdata->flags & DNS_RDATA_UPDATE) != 0)
			rdataclass = dns_rdataclass_any;
		else
			rdataclass = dns_rdataclass_none;
	} else {
		if ((rdata->flags & DNS_RDATA_UPDATE) != 0) {
			fprintf(stderr, "could not read rdata\n");
			goto failure;
		}
	}

 doneparsing:

	result = dns_message_gettemprdatalist(updatemsg, &rdatalist);
	check_result(result, "dns_message_gettemprdatalist");
	result = dns_message_gettemprdataset(updatemsg, &rdataset);
	check_result(result, "dns_message_gettemprdataset");
	dns_rdatalist_init(rdatalist);
	rdatalist->type = rdatatype;
	rdatalist->rdclass = rdataclass;
	rdatalist->covers = rdatatype;
	rdatalist->ttl = (dns_ttl_t)ttl;
	ISC_LIST_INIT(rdatalist->rdata);
	ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	dns_rdataset_init(rdataset);
	dns_rdatalist_tordataset(rdatalist, rdataset);
	ISC_LIST_INIT(name->list);
	ISC_LIST_APPEND(name->list, rdataset, link);
	dns_message_addname(updatemsg, name, DNS_SECTION_UPDATE);
	return (STATUS_MORE);

 failure:
	if (name != NULL)
		dns_message_puttempname(updatemsg, &name);
	if (rdata != NULL)
		dns_message_puttemprdata(updatemsg, &rdata);
	return (STATUS_SYNTAX);
}

static isc_uint16_t
evaluate_update(char *cmdline) {
	char *word;
	isc_boolean_t isdelete;

	ddebug("evaluate_update()");
	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read operation code\n");
		return (STATUS_SYNTAX);
	}
	if (strcasecmp(word, "delete") == 0)
		isdelete = ISC_TRUE;
	else if (strcasecmp(word, "add") == 0)
		isdelete = ISC_FALSE;
	else {
		fprintf(stderr, "incorrect operation code: %s\n", word);
		return (STATUS_SYNTAX);
	}
	return (update_addordelete(cmdline, isdelete));
}

static void
show_message(dns_message_t *msg) {
	isc_result_t result;
	isc_buffer_t *buf = NULL;
	int bufsz;

	ddebug("show_message()");
	bufsz = INITTEXT;
	do { 
		if (bufsz > MAXTEXT) {
			fprintf(stderr, "could not allocate large enough "
				"buffer to display message\n");
			exit(1);
		}
		if (buf != NULL)
			isc_buffer_free(&buf);
		result = isc_buffer_allocate(mctx, &buf, bufsz);
		check_result(result, "isc_buffer_allocate");
		result = dns_message_totext(msg, style, 0, buf);
		bufsz *= 2;
	} while (result == ISC_R_NOSPACE);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not convert message to text format.\n");
		isc_buffer_free(&buf);
		return;
	}
	printf("Outgoing update query:\n%.*s",
	       (int)isc_buffer_usedlength(buf),
	       (char*)isc_buffer_base(buf));
	isc_buffer_free(&buf);
}


static isc_uint16_t
get_next_command(void) {
	char cmdlinebuf[MAXCMD];
	char *cmdline;
	char *word;

	ddebug("get_next_command()");
	if (interactive)
		fprintf(stdout, "> ");
	isc_app_block();
	cmdline = fgets(cmdlinebuf, MAXCMD, input);
	isc_app_unblock();
	if (cmdline == NULL)
		return (STATUS_QUIT);
	word = nsu_strsep(&cmdline, " \t\r\n");

	if (feof(input))
		return (STATUS_QUIT);
	if (*word == 0)
		return (STATUS_SEND);
	if (word[0] == ';')
		return (STATUS_MORE);
	if (strcasecmp(word, "quit") == 0)
		return (STATUS_QUIT);
	if (strcasecmp(word, "prereq") == 0)
		return (evaluate_prereq(cmdline));
	if (strcasecmp(word, "update") == 0)
		return (evaluate_update(cmdline));
	if (strcasecmp(word, "server") == 0)
		return (evaluate_server(cmdline));
	if (strcasecmp(word, "local") == 0)
		return (evaluate_local(cmdline));
	if (strcasecmp(word, "zone") == 0)
		return (evaluate_zone(cmdline));
	if (strcasecmp(word, "send") == 0)
		return (STATUS_SEND);
	if (strcasecmp(word, "show") == 0) {
		show_message(updatemsg);
		return (STATUS_MORE);
	}
	if (strcasecmp(word, "key") == 0)
		return (evaluate_key(cmdline));
	fprintf(stderr, "incorrect section name: %s\n", word);
	return (STATUS_SYNTAX);
}

static isc_boolean_t
user_interaction(void) {
	isc_uint16_t result = STATUS_MORE;

	ddebug("user_interaction()");
	while ((result == STATUS_MORE) || (result == STATUS_SYNTAX))
		result = get_next_command();
	if (result == STATUS_SEND)
		return (ISC_TRUE);
	return (ISC_FALSE);

}

static void
done_update(void) {
	isc_event_t *event = global_event;
	ddebug("done_update()");
	isc_task_send(global_task, &event);
}

static void
update_completed(isc_task_t *task, isc_event_t *event) {
	dns_requestevent_t *reqev = NULL;
	isc_result_t result;
	dns_message_t *rcvmsg = NULL;
	dns_request_t *request;

	UNUSED(task);

	ddebug("update_completed()");

	requests--;

	REQUIRE(event->ev_type == DNS_EVENT_REQUESTDONE);
	reqev = (dns_requestevent_t *)event;
	request = reqev->request;

	if (shuttingdown) {
		dns_request_destroy(&request);
		isc_event_free(&event);
		maybeshutdown();
		return;
	}

	if (reqev->result != ISC_R_SUCCESS) {
		fprintf(stderr, "; Communication with server failed: %s\n",
			isc_result_totext(reqev->result));
		seenerror = ISC_TRUE;
		goto done;
	}

	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &rcvmsg);
	check_result(result, "dns_message_create");
	result = dns_request_getresponse(request, rcvmsg,
					 DNS_MESSAGEPARSE_PRESERVEORDER);
	switch (result) {
	case ISC_R_SUCCESS:
		break;
	case DNS_R_CLOCKSKEW:
	case DNS_R_EXPECTEDTSIG:
	case DNS_R_TSIGERRORSET:
	case DNS_R_TSIGVERIFYFAILURE:
	case DNS_R_UNEXPECTEDTSIG:
		fprintf(stderr, "; TSIG error with server: %s\n",
			isc_result_totext(result));
		seenerror = ISC_TRUE;
		break;
	default:
		check_result(result, "dns_request_getresponse");
	}

	if (rcvmsg->rcode != dns_rcode_noerror)
		seenerror = ISC_TRUE;
	if (debugging) {
		isc_buffer_t *buf = NULL;
		int bufsz;

		bufsz = INITTEXT;
		do { 
			if (bufsz > MAXTEXT) {
				fprintf(stderr, "could not allocate large "
					"enough buffer to display message\n");
				exit(1);
			}
			if (buf != NULL)
				isc_buffer_free(&buf);
			result = isc_buffer_allocate(mctx, &buf, bufsz);
			check_result(result, "isc_buffer_allocate");
			result = dns_message_totext(rcvmsg, style, 0, buf);
			bufsz *= 2;
		} while (result == ISC_R_NOSPACE);
		check_result(result, "dns_message_totext");
		fprintf(stderr, "\nReply from update query:\n%.*s\n",
			(int)isc_buffer_usedlength(buf),
			(char*)isc_buffer_base(buf));
		isc_buffer_free(&buf);
	}
	dns_message_destroy(&rcvmsg);
 done:
	dns_request_destroy(&request);
	isc_event_free(&event);
	done_update();
}

static void
send_update(dns_name_t *zonename, isc_sockaddr_t *master,
	    isc_sockaddr_t *srcaddr)
{
	isc_result_t result;
	dns_request_t *request = NULL;
	dns_name_t *name = NULL;
	dns_rdataset_t *rdataset = NULL;
	unsigned int options = 0;

	ddebug("send_update()");

	result = dns_message_gettempname(updatemsg, &name);
	check_result(result, "dns_message_gettempname");
	dns_name_init(name, NULL);
	dns_name_clone(zonename, name);
	result = dns_message_gettemprdataset(updatemsg, &rdataset);
	check_result(result, "dns_message_gettemprdataset");
	dns_rdataset_makequestion(rdataset, dns_rdataclass_in,
				  dns_rdatatype_soa);
	ISC_LIST_INIT(name->list);
	ISC_LIST_APPEND(name->list, rdataset, link);
	dns_message_addname(updatemsg, name, DNS_SECTION_ZONE);

	if (usevc)
		options |= DNS_REQUESTOPT_TCP;
	result = dns_request_createvia(requestmgr, updatemsg, srcaddr,
				       master, options, key,
				       FIND_TIMEOUT, global_task,
				       update_completed, NULL, &request);
	check_result(result, "dns_request_createvia");
	requests++;
}

static void
recvsoa(isc_task_t *task, isc_event_t *event) {
	dns_requestevent_t *reqev = NULL;
	dns_request_t *request = NULL;
	isc_result_t result, eresult;
	dns_message_t *rcvmsg = NULL;
	dns_section_t section;
	dns_name_t *name = NULL;
	dns_rdataset_t *soaset = NULL;
	dns_rdata_soa_t soa;
	dns_rdata_t soarr = DNS_RDATA_INIT;
	int pass = 0;
	dns_name_t master;
	isc_sockaddr_t *serveraddr, tempaddr;
	dns_name_t *zonename;
	nsu_requestinfo_t *reqinfo;
	dns_message_t *soaquery = NULL;
	isc_sockaddr_t *addr;
	isc_boolean_t seencname = ISC_FALSE;

	UNUSED(task);

	ddebug("recvsoa()");

	requests--;
	
	REQUIRE(event->ev_type == DNS_EVENT_REQUESTDONE);
	reqev = (dns_requestevent_t *)event;
	request = reqev->request;
	eresult = reqev->result;
	reqinfo = reqev->ev_arg;
	soaquery = reqinfo->msg;
	addr = reqinfo->addr;

	if (shuttingdown) {
		dns_request_destroy(&request);
		dns_message_destroy(&soaquery);
		isc_mem_put(mctx, reqinfo, sizeof(nsu_requestinfo_t));
		isc_event_free(&event);
		maybeshutdown();
		return;
	}

	if (eresult != ISC_R_SUCCESS) {
		char addrbuf[ISC_SOCKADDR_FORMATSIZE];

		isc_sockaddr_format(addr, addrbuf, sizeof(addrbuf));
		fprintf(stderr, "; Communication with %s failed: %s\n",
		       addrbuf, isc_result_totext(eresult));
		if (userserver != NULL)
			fatal("could not talk to specified name server");
		else if (++ns_inuse >= lwconf->nsnext)
			fatal("could not talk to any default name server");
		ddebug("Destroying request [%p]", request);
		dns_request_destroy(&request);
		dns_message_renderreset(soaquery);
		sendrequest(localaddr, &servers[ns_inuse], soaquery, &request);
		isc_mem_put(mctx, reqinfo, sizeof(nsu_requestinfo_t));
		isc_event_free(&event);
		return;
	}
	isc_mem_put(mctx, reqinfo, sizeof(nsu_requestinfo_t));

	isc_event_free(&event);
	reqev = NULL;

	ddebug("About to create rcvmsg");
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &rcvmsg);
	check_result(result, "dns_message_create");
	result = dns_request_getresponse(request, rcvmsg,
					 DNS_MESSAGEPARSE_PRESERVEORDER);
	check_result(result, "dns_request_getresponse");
	section = DNS_SECTION_ANSWER;
	if (debugging) {
		isc_buffer_t *buf = NULL;
		int bufsz;
		bufsz = INITTEXT;
		do {
			if (buf != NULL)
				isc_buffer_free(&buf);
			if (bufsz > MAXTEXT) {
				fprintf(stderr, "could not allocate enough "
					 "space for debugging message\n");
				exit(1);
			}
			result = isc_buffer_allocate(mctx, &buf, bufsz);
			check_result(result, "isc_buffer_allocate");
			result = dns_message_totext(rcvmsg, style, 0, buf);
		} while (result == ISC_R_NOSPACE);
		check_result(result, "dns_message_totext");
		fprintf(stderr, "Reply from SOA query:\n%.*s\n",
			(int)isc_buffer_usedlength(buf),
			(char*)isc_buffer_base(buf));
		isc_buffer_free(&buf);
	}

	if (rcvmsg->rcode != dns_rcode_noerror &&
	    rcvmsg->rcode != dns_rcode_nxdomain)
		fatal("response to SOA query was unsuccessful");

 lookforsoa:
	if (pass == 0)
		section = DNS_SECTION_ANSWER;
	else if (pass == 1)
		section = DNS_SECTION_AUTHORITY;
	else
		fatal("response to SOA query didn't contain an SOA");


	result = dns_message_firstname(rcvmsg, section);
	if (result != ISC_R_SUCCESS) {
		pass++;
		goto lookforsoa;
	}
	while (result == ISC_R_SUCCESS) {
		name = NULL;
		dns_message_currentname(rcvmsg, section, &name);
		soaset = NULL;
		result = dns_message_findtype(name, dns_rdatatype_soa, 0,
					      &soaset);
		if (result == ISC_R_SUCCESS)
			break;
		if (section == DNS_SECTION_ANSWER) {
			dns_rdataset_t *tset = NULL;
			if (dns_message_findtype(name, dns_rdatatype_cname, 0,
						 &tset) == ISC_R_SUCCESS
			    ||
			    dns_message_findtype(name, dns_rdatatype_dname, 0,
						 &tset) == ISC_R_SUCCESS
			    )
			{
				seencname = ISC_TRUE;
				break;
			}
		}
				
		result = dns_message_nextname(rcvmsg, section);
	}

	if (soaset == NULL && !seencname) {
		pass++;
		goto lookforsoa;
	}

	if (seencname) {
		dns_name_t tname;
		unsigned int nlabels;

		result = dns_message_firstname(soaquery, DNS_SECTION_QUESTION);
		INSIST(result == ISC_R_SUCCESS);
		name = NULL;
		dns_message_currentname(soaquery, DNS_SECTION_QUESTION, &name);
		nlabels = dns_name_countlabels(name);
		if (nlabels == 1)
			fatal("could not find enclosing zone");
		dns_name_init(&tname, NULL);
		dns_name_getlabelsequence(name, 1, nlabels - 1, &tname);
		dns_name_clone(&tname, name);
		dns_request_destroy(&request);
		dns_message_renderreset(soaquery);
		if (userserver != NULL)
			sendrequest(localaddr, userserver, soaquery, &request);
		else
			sendrequest(localaddr, &servers[ns_inuse], soaquery,
				    &request);
		goto out;
	}

	if (debugging) {
		char namestr[DNS_NAME_FORMATSIZE];
		dns_name_format(name, namestr, sizeof(namestr));
		fprintf(stderr, "Found zone name: %s\n", namestr);
	}

	result = dns_rdataset_first(soaset);
	check_result(result, "dns_rdataset_first");

	dns_rdata_init(&soarr);
	dns_rdataset_current(soaset, &soarr);
	result = dns_rdata_tostruct(&soarr, &soa, NULL);
	check_result(result, "dns_rdata_tostruct");

	dns_name_init(&master, NULL);
	dns_name_clone(&soa.origin, &master);

	if (userzone != NULL)
		zonename = userzone;
	else
		zonename = name;

	if (debugging) {
		char namestr[DNS_NAME_FORMATSIZE];
		dns_name_format(&master, namestr, sizeof(namestr));
		fprintf(stderr, "The master is: %s\n", namestr);
	}

	if (userserver != NULL)
		serveraddr = userserver;
	else {
		char serverstr[DNS_NAME_MAXTEXT+1];
		isc_buffer_t buf;

		isc_buffer_init(&buf, serverstr, sizeof(serverstr));
		result = dns_name_totext(&master, ISC_TRUE, &buf);
		check_result(result, "dns_name_totext");
		serverstr[isc_buffer_usedlength(&buf)] = 0;
		get_address(serverstr, DNSDEFAULTPORT, &tempaddr);
		serveraddr = &tempaddr;
	}

	send_update(zonename, serveraddr, localaddr);

	dns_message_destroy(&soaquery);
	dns_request_destroy(&request);

 out:
	dns_rdata_freestruct(&soa);
	dns_message_destroy(&rcvmsg);
	ddebug("Out of recvsoa");
}

static void
sendrequest(isc_sockaddr_t *srcaddr, isc_sockaddr_t *destaddr,
	    dns_message_t *msg, dns_request_t **request)
{
	isc_result_t result;
	nsu_requestinfo_t *reqinfo;

	reqinfo = isc_mem_get(mctx, sizeof(nsu_requestinfo_t));
	if (reqinfo == NULL)
		fatal("out of memory");
	reqinfo->msg = msg;
	reqinfo->addr = destaddr;
	result = dns_request_createvia(requestmgr, msg, srcaddr, destaddr,
				       0, NULL, FIND_TIMEOUT, global_task,
				       recvsoa, reqinfo, request);
	check_result(result, "dns_request_createvia");
	requests++;
}

static void
start_update(void) {
	isc_result_t result;
	dns_rdataset_t *rdataset = NULL;
	dns_name_t *name = NULL;
	dns_request_t *request = NULL;
	dns_message_t *soaquery = NULL;
	dns_name_t *firstname;

	ddebug("start_update()");

	result = dns_message_firstname(updatemsg, DNS_SECTION_UPDATE);
	if (result != ISC_R_SUCCESS) {
		done_update();
		return;
	}

	if (userzone != NULL && userserver != NULL) {
		send_update(userzone, userserver, localaddr);
		return;
	}

	result = dns_message_create(mctx, DNS_MESSAGE_INTENTRENDER,
				    &soaquery);
	check_result(result, "dns_message_create");

	soaquery->flags |= DNS_MESSAGEFLAG_RD;

	result = dns_message_gettempname(soaquery, &name);
	check_result(result, "dns_message_gettempname");

	result = dns_message_gettemprdataset(soaquery, &rdataset);
	check_result(result, "dns_message_gettemprdataset");

	dns_rdataset_makequestion(rdataset, dns_rdataclass_in,
				  dns_rdatatype_soa);

	firstname = NULL;
	dns_message_currentname(updatemsg, DNS_SECTION_UPDATE, &firstname);
	dns_name_init(name, NULL);
	dns_name_clone(firstname, name);

	ISC_LIST_INIT(name->list);
	ISC_LIST_APPEND(name->list, rdataset, link);
	dns_message_addname(soaquery, name, DNS_SECTION_QUESTION);

	if (userserver != NULL)
		sendrequest(localaddr, userserver, soaquery, &request);
	else {
		ns_inuse = 0;
		sendrequest(localaddr, &servers[ns_inuse], soaquery, &request);
	}
}

static void
cleanup(void) {
	ddebug("cleanup()");

	ddebug("Shutting down task manager");
	isc_taskmgr_destroy(&taskmgr);

	ddebug("Destroying event");
	isc_event_free(&global_event);

	ddebug("Shutting down socket manager");
	isc_socketmgr_destroy(&socketmgr);

	ddebug("Shutting down timer manager");
	isc_timermgr_destroy(&timermgr);

	ddebug("Destroying memory context");
	if (memdebugging)
		isc_mem_stats(mctx, stderr);
	isc_mem_destroy(&mctx);
}

static void
getinput(isc_task_t *task, isc_event_t *event) {
	isc_boolean_t more;

	UNUSED(task);

	if (shuttingdown) {
		maybeshutdown();
		return;
	}

	if (global_event == NULL)
		global_event = event;

	reset_system();
	more = user_interaction();
	if (!more) {
		isc_app_shutdown();
		return;
	}
	start_update();
	return;
}

int
main(int argc, char **argv) {
	isc_result_t result;
	style = &dns_master_style_debug;

	input = stdin;

	isc_app_start();

	parse_args(argc, argv);

	setup_system();

	result = isc_app_onrun(mctx, global_task, getinput, NULL);
	check_result(result, "isc_app_onrun");

	(void)isc_app_run();

	cleanup();

	isc_app_finish();

	if (seenerror)
		return (2);
	else
		return (0);
}
