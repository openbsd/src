/*	$OpenBSD: update.c,v 1.6 2009/10/27 23:59:30 deraadt Exp $	*/

 /*
  * Routines for controlled update/initialization of request structures.
  * 
  * request_init() initializes its argument. Pointers and string-valued members
  * are initialized to zero, to indicate that no lookup has been attempted.
  * 
  * request_set() adds information to an already initialized request structure.
  * 
  * Both functions take a variable-length name-value list.
  * 
  * Diagnostics are reported through syslog(3).
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

/* System libraries */

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>

/* Local stuff. */

#include "tcpd.h"

/* request_fill - request update engine */

static struct request_info *request_fill(request, ap)
struct request_info *request;
va_list ap;
{
    int     key;
    char   *ptr;

    while ((key = va_arg(ap, int)) > 0) {
	switch (key) {
	default:
	    tcpd_warn("request_fill: invalid key: %d", key);
	    return (request);
	case RQ_FILE:
	    request->fd = va_arg(ap, int);
	    continue;
	case RQ_CLIENT_SIN:
	    request->client->sin = va_arg(ap, struct sockaddr *);
	    continue;
	case RQ_SERVER_SIN:
	    request->server->sin = va_arg(ap, struct sockaddr *);
	    continue;

	    /*
	     * All other fields are strings with the same maximal length.
	     */

	case RQ_DAEMON:
	    ptr = request->daemon;
	    break;
	case RQ_USER:
	    ptr = request->user;
	    break;
	case RQ_CLIENT_NAME:
	    ptr = request->client->name;
	    break;
	case RQ_CLIENT_ADDR:
	    ptr = request->client->addr;
	    break;
	case RQ_SERVER_NAME:
	    ptr = request->server->name;
	    break;
	case RQ_SERVER_ADDR:
	    ptr = request->server->addr;
	    break;
	}
	strlcpy(ptr, va_arg(ap, char *), STRING_LENGTH);
    }
    return (request);
}

/* request_init - initialize request structure */

struct request_info *request_init(struct request_info *request, ...)
{
    static struct request_info default_info;
    struct request_info *r;
    va_list ap;

    /*
     * Initialize data members. We do not assign default function pointer
     * members, to avoid pulling in the whole socket module when it is not
     * really needed.
     */
    va_start(ap, request);
    *request = default_info;
    request->fd = -1;
    strlcpy(request->daemon, unknown, sizeof(request->daemon));
    snprintf(request->pid, sizeof(request->pid), "%ld", (long)getpid());
    request->client->request = request;
    request->server->request = request;
    r = request_fill(request, ap);
    va_end(ap);
    return (r);
}

/* request_set - update request structure */

struct request_info *request_set(struct request_info *request, ...)
{
    struct request_info *r;
    va_list ap;

    va_start(ap, request);
    r = request_fill(request, ap);
    va_end(ap);
    return (r);
}
