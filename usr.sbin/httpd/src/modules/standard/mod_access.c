/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2003 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

/*
 * Security options etc.
 * 
 * Module derived from code originally written by Rob McCool
 * 
 */

#include "httpd.h"
#include "http_core.h"
#include "http_config.h"
#include "http_log.h"
#include "http_request.h"

enum allowdeny_type {
    T_ENV,
    T_ALL,
    T_IP,
    T_HOST,
    T_FAIL,
    T_IP6,
};

typedef struct {
    int limited;
    union {
	char *from;
	struct {
	    struct in_addr net;
	    struct in_addr mask;
	} ip;
	struct {
	    struct in6_addr net6;
	    struct in6_addr mask6;
	} ip6;
    } x;
    enum allowdeny_type type;
} allowdeny;

/* things in the 'order' array */
#define DENY_THEN_ALLOW 0
#define ALLOW_THEN_DENY 1
#define MUTUAL_FAILURE 2

typedef struct {
    int order[METHODS];
    array_header *allows;
    array_header *denys;
} access_dir_conf;

module MODULE_VAR_EXPORT access_module;

static void *create_access_dir_config(pool *p, char *dummy)
{
    access_dir_conf *conf =
    (access_dir_conf *) ap_pcalloc(p, sizeof(access_dir_conf));
    int i;

    for (i = 0; i < METHODS; ++i)
	conf->order[i] = DENY_THEN_ALLOW;
    conf->allows = ap_make_array(p, 1, sizeof(allowdeny));
    conf->denys = ap_make_array(p, 1, sizeof(allowdeny));

    return (void *) conf;
}

static const char *order(cmd_parms *cmd, void *dv, char *arg)
{
    access_dir_conf *d = (access_dir_conf *) dv;
    int i, o;

    if (!strcasecmp(arg, "allow,deny"))
	o = ALLOW_THEN_DENY;
    else if (!strcasecmp(arg, "deny,allow"))
	o = DENY_THEN_ALLOW;
    else if (!strcasecmp(arg, "mutual-failure"))
	o = MUTUAL_FAILURE;
    else
	return "unknown order";

    for (i = 0; i < METHODS; ++i)
	if (cmd->limited & (1 << i))
	    d->order[i] = o;

    return NULL;
}

static int is_ip(const char *host)
{
    while ((*host == '.') || ap_isdigit(*host))
	host++;
    return (*host == '\0');
}

static const char *allow_cmd(cmd_parms *cmd, void *dv, char *from, char *where)
{
    access_dir_conf *d = (access_dir_conf *) dv;
    allowdeny *a;
    char *s;

    if (strcasecmp(from, "from"))
	return "allow and deny must be followed by 'from'";

    a = (allowdeny *) ap_push_array(cmd->info ? d->allows : d->denys);
    a->x.from = where;
    a->limited = cmd->limited;

    if (!strncasecmp(where, "env=", 4)) {
	a->type = T_ENV;
	a->x.from += 4;

    }
    else if (!strcasecmp(where, "all")) {
	a->type = T_ALL;

    }
    else if ((s = strchr(where, '/'))) {
	struct addrinfo hints, *resnet, *resmask;
	struct sockaddr_storage net, mask;
	int error;
	char *p;
	int justdigits;

	a->type = T_FAIL;	/*just in case*/
	/* trample on where, we won't be using it any more */
	*s++ = '\0';

	justdigits = 0;
	for (p = s; *p; p++) {
	    if (!isdigit(*p))
		break;
	}
	if (!*p)
	    justdigits++;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;	/*dummy*/
#ifdef AI_NUMERICHOST
	hints.ai_flags = AI_NUMERICHOST;	/*don't resolve*/
#endif
	resnet = NULL;
	error = getaddrinfo(where, NULL, &hints, &resnet);
	if (error || !resnet) {
	    if (resnet)
		freeaddrinfo(resnet);
	    a->type = T_FAIL;
	    return "syntax error in network portion of network/netmask";
	}
	if (resnet->ai_next) {
	    freeaddrinfo(resnet);
	    a->type = T_FAIL;
	    return "network/netmask resolved to multiple addresses";
	}
	memcpy(&net, resnet->ai_addr, resnet->ai_addrlen);
	freeaddrinfo(resnet);

	switch (net.ss_family) {
	case AF_INET:
	    a->type = T_IP;
	    a->x.ip.net.s_addr = ((struct sockaddr_in *)&net)->sin_addr.s_addr;
	    break;
	case AF_INET6:
	    a->type = T_IP6;
	    memcpy(&a->x.ip6.net6, &((struct sockaddr_in6 *)&net)->sin6_addr,
		sizeof(a->x.ip6.net6));
	    break;
	default:
	    a->type = T_FAIL;
	    return "unknown address family for network";
	}

	if (!justdigits) {
	    memset(&hints, 0, sizeof(hints));
	    hints.ai_family = PF_UNSPEC;
	    hints.ai_socktype = SOCK_STREAM;	/*dummy*/ 
#ifdef AI_NUMERICHOST
	    hints.ai_flags = AI_NUMERICHOST;	/*don't resolve*/
#endif
	    resmask = NULL;
	    error = getaddrinfo(s, NULL, &hints, &resmask);
	    if (error || !resmask) {
		if (resmask)
		    freeaddrinfo(resmask);
		a->type = T_FAIL;
		return "syntax error in mask portion of network/netmask";
	    }
	    if (resmask->ai_next) {
		freeaddrinfo(resmask);
		a->type = T_FAIL;
		return "network/netmask resolved to multiple addresses";
	    }
	    memcpy(&mask, resmask->ai_addr, resmask->ai_addrlen);
	    freeaddrinfo(resmask);

	    if (net.ss_family != mask.ss_family) {
		a->type = T_FAIL;
		return "network/netmask resolved to different address family";
	    }

	    switch (a->type) {
	    case T_IP:
		a->x.ip.mask.s_addr =
		    ((struct sockaddr_in *)&mask)->sin_addr.s_addr;
		break;
	    case T_IP6:
		memcpy(&a->x.ip6.mask6,
		    &((struct sockaddr_in6 *)&mask)->sin6_addr,
		    sizeof(a->x.ip6.mask6));
		break;
	    }
	} else {
	    int mask;
	    mask = atoi(s);
	    switch (a->type) {
	    case T_IP:
		if (mask < 0 || 32 < mask) {
		    a->type = T_FAIL;
		    return "netmask out of range";
		}
		a->x.ip.mask.s_addr = htonl(0xFFFFFFFFUL << (32 - mask));
		break;
	    case T_IP6:
	      {
		int i;
		if (mask < 0 || 128 < mask) {
		    a->type = T_FAIL;
		    return "netmask out of range";
		}
		for (i = 0; i < mask / 8; i++) {
		    a->x.ip6.mask6.s6_addr[i] = 0xff;
		}
		if (mask % 8)
		    a->x.ip6.mask6.s6_addr[i] = 0xff << (8 - (mask % 8));
		break;
	      }
	    }
	}
    }
    else {
	struct addrinfo hints, *res;
	struct sockaddr_storage ss;
	int error;

	a->type = T_FAIL;	/*just in case*/

	/* First, try using the old apache code to match */
	/* legacy syntax for ip addrs: a.b.c. ==> a.b.c.0/24 for example */
	if (ap_isdigit(*where) && is_ip(where)) {
	    int shift;
	    char *t;
	    int octet;

	    a->type = T_IP;
	    /* parse components */
	    s = where;
	    a->x.ip.net.s_addr = 0;
	    a->x.ip.mask.s_addr = 0;
	    shift = 24;
	    while (*s) {
		t = s;
		if (!ap_isdigit(*t)) {
		    a->type = T_FAIL;
		    return "invalid ip address";
		}
		while (ap_isdigit(*t)) {
		    ++t;
		}
		if (*t == '.') {
		    *t++ = 0;
		}
		else if (*t) {
		    a->type = T_FAIL;
		    return "invalid ip address";
		}
		if (shift < 0) {
		    return "invalid ip address, only 4 octets allowed";
		}
		octet = atoi(s);
		if (octet < 0 || octet > 255) {
		    a->type = T_FAIL;
		    return "each octet must be between 0 and 255 inclusive";
		}
		a->x.ip.net.s_addr |= octet << shift;
		a->x.ip.mask.s_addr |= 0xFFUL << shift;
		s = t;
		shift -= 8;
	    }
	    a->x.ip.net.s_addr = ntohl(a->x.ip.net.s_addr);
	    a->x.ip.mask.s_addr = ntohl(a->x.ip.mask.s_addr);

	    return NULL;
	}

	/* IPv4/v6 numeric address */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;	/*dummy*/
#ifdef AI_NUMERICHOST
	hints.ai_flags = AI_NUMERICHOST;	/*don't resolve*/
#endif
	res = NULL;
	error = getaddrinfo(where, NULL, &hints, &res);
	if (error || !res) {
	    if (res)
		freeaddrinfo(res);
	    a->type = T_HOST;
	    return NULL;
	}
	if (res->ai_next) {
	    freeaddrinfo(res);
	    a->type = T_FAIL;
	    return "network/netmask resolved to multiple addresses";
	}
	memcpy(&ss, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);

	switch (ss.ss_family) {
	case AF_INET:
	    a->type = T_IP;
	    a->x.ip.net.s_addr = ((struct sockaddr_in *)&ss)->sin_addr.s_addr;
	    memset(&a->x.ip.mask, 0xff, sizeof(a->x.ip.mask));
	    break;
	case AF_INET6:
	    a->type = T_IP6;
	    memcpy(&a->x.ip6.net6, &((struct sockaddr_in6 *)&ss)->sin6_addr,
		sizeof(a->x.ip6.net6));
	    memset(&a->x.ip6.mask6, 0xff, sizeof(a->x.ip6.mask6));
	    break;
	default:
	    a->type = T_FAIL;
	    return "unknown address family for network";
	}
    }

    return NULL;
}

static char its_an_allow;

static const command_rec access_cmds[] =
{
    {"order", order, NULL, OR_LIMIT, TAKE1,
     "'allow,deny', 'deny,allow', or 'mutual-failure'"},
    {"allow", allow_cmd, &its_an_allow, OR_LIMIT, ITERATE2,
     "'from' followed by hostnames or IP-address wildcards"},
    {"deny", allow_cmd, NULL, OR_LIMIT, ITERATE2,
     "'from' followed by hostnames or IP-address wildcards"},
    {NULL}
};

static int in_domain(const char *domain, const char *what)
{
    int dl = strlen(domain);
    int wl = strlen(what);

    if ((wl - dl) >= 0) {
	if (strcasecmp(domain, &what[wl - dl]) != 0)
	    return 0;

	/* Make sure we matched an *entire* subdomain --- if the user
	 * said 'allow from good.com', we don't want people from nogood.com
	 * to be able to get in.
	 */

	if (wl == dl)
	    return 1;		/* matched whole thing */
	else
	    return (domain[0] == '.' || what[wl - dl - 1] == '.');
    }
    else
	return 0;
}

static int find_allowdeny(request_rec *r, array_header *a, int method)
{
    allowdeny *ap = (allowdeny *) a->elts;
    int mmask = (1 << method);
    int i;
    int gothost = 0;
    const char *remotehost = NULL;

    for (i = 0; i < a->nelts; ++i) {
	if (!(mmask & ap[i].limited))
	    continue;

	switch (ap[i].type) {
	case T_ENV:
	    if (ap_table_get(r->subprocess_env, ap[i].x.from)) {
		return 1;
	    }
	    break;

	case T_ALL:
	    return 1;

	case T_IP:
	    if (ap[i].x.ip.net.s_addr == INADDR_NONE)
		break;
	    switch (r->connection->remote_addr.ss_family) {
	    case AF_INET:
		if ((((struct sockaddr_in *)&r->connection->remote_addr)->sin_addr.s_addr
			& ap[i].x.ip.mask.s_addr) == ap[i].x.ip.net.s_addr) {
		    return 1;
		}
		break;
	    case AF_INET6:
		if (!IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)&r->connection->remote_addr)->sin6_addr))	/*XXX*/
		    break;
		if ((*(uint32_t *)&((struct sockaddr_in6 *)&r->connection->remote_addr)->sin6_addr.s6_addr[12]
			& ap[i].x.ip.mask.s_addr) == ap[i].x.ip.net.s_addr) {
		    return 1;
		}
		break;
	    }
	    break;

	case T_IP6:
	  {
	    struct in6_addr masked;
	    int j;
	    if (IN6_IS_ADDR_UNSPECIFIED(&ap[i].x.ip6.net6))
		break;
	    switch (r->connection->remote_addr.ss_family) {
	    case AF_INET:
		if (!IN6_IS_ADDR_V4MAPPED(&ap[i].x.ip6.net6))	/*XXX*/
		    break;
		memset(&masked, 0, sizeof(masked));
		masked.s6_addr[10] = masked.s6_addr[11] = 0xff;
		memcpy(&masked.s6_addr[12],
		    &((struct sockaddr_in *)&r->connection->remote_addr)->sin_addr.s_addr,
		    sizeof(in_addr_t));
		for (j = 0; j < sizeof(struct in6_addr); j++)
			masked.s6_addr[j] &= ap[i].x.ip6.mask6.s6_addr[j];
		if (memcmp(&masked, &ap[i].x.ip6.net6, sizeof(masked)) == 0)
		    return 1;
		break;
	    case AF_INET6:
		memset(&masked, 0, sizeof(masked));
		memcpy(&masked,
		    &((struct sockaddr_in6 *)&r->connection->remote_addr)->sin6_addr,
		    sizeof(masked));
		for (j = 0; j < sizeof(struct in6_addr); j++)
		    masked.s6_addr[j] &= ap[i].x.ip6.mask6.s6_addr[j];
		if (memcmp(&masked, &ap[i].x.ip6.net6, sizeof(masked)) == 0)
		    return 1;
		break;
	    }
	    break;
	  }

	case T_HOST:
	    if (!gothost) {
		remotehost = ap_get_remote_host(r->connection, r->per_dir_config,
					    REMOTE_DOUBLE_REV);

		if ((remotehost == NULL) || is_ip(remotehost))
		    gothost = 1;
		else
		    gothost = 2;
	    }

	    if ((gothost == 2) && in_domain(ap[i].x.from, remotehost))
		return 1;
	    break;

	case T_FAIL:
	    /* do nothing? */
	    break;
	}
    }

    return 0;
}

static int check_dir_access(request_rec *r)
{
    int method = r->method_number;
    access_dir_conf *a =
    (access_dir_conf *)
    ap_get_module_config(r->per_dir_config, &access_module);
    int ret = OK;

    if (a->order[method] == ALLOW_THEN_DENY) {
	ret = FORBIDDEN;
	if (find_allowdeny(r, a->allows, method))
	    ret = OK;
	if (find_allowdeny(r, a->denys, method))
	    ret = FORBIDDEN;
    }
    else if (a->order[method] == DENY_THEN_ALLOW) {
	if (find_allowdeny(r, a->denys, method))
	    ret = FORBIDDEN;
	if (find_allowdeny(r, a->allows, method))
	    ret = OK;
    }
    else {
	if (find_allowdeny(r, a->allows, method)
	    && !find_allowdeny(r, a->denys, method))
	    ret = OK;
	else
	    ret = FORBIDDEN;
    }

    if (ret == FORBIDDEN
	&& (ap_satisfies(r) != SATISFY_ANY || !ap_some_auth_required(r))) {
	ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, r,
		  "client denied by server configuration: %s",
		  r->filename);
    }

    return ret;
}



module MODULE_VAR_EXPORT access_module =
{
    STANDARD_MODULE_STUFF,
    NULL,			/* initializer */
    create_access_dir_config,	/* dir config creater */
    NULL,			/* dir merger --- default is to override */
    NULL,			/* server config */
    NULL,			/* merge server config */
    access_cmds,
    NULL,			/* handlers */
    NULL,			/* filename translation */
    NULL,			/* check_user_id */
    NULL,			/* check auth */
    check_dir_access,		/* check access */
    NULL,			/* type_checker */
    NULL,			/* fixups */
    NULL,			/* logger */
    NULL,			/* header parser */
    NULL,			/* child_init */
    NULL,			/* child_exit */
    NULL			/* post read-request */
};
