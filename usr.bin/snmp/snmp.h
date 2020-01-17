/*	$OpenBSD: snmp.h,v 1.7 2020/01/17 09:52:44 martijn Exp $	*/

/*
 * Copyright (c) 2019 Martijn van Duren <martijn@openbsd.org>
 * Copyright (c) 2007, 2008, 2012 Reyk Floeter <reyk@openbsd.org>
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

#ifndef SNMPD_SNMP_H
#define SNMPD_SNMP_H

#include <sys/types.h>
#include <sys/queue.h>
#include <endian.h>

#include <time.h>

#define READ_BUF_SIZE 65535

#define SNMP_MAX_OID_STRLEN	128	/* max size of the OID _string_ */

/*
 * SNMP BER types
 */

enum snmp_version {
	SNMP_V1			= 0,
	SNMP_V2C		= 1,	/* SNMPv2c */
	SNMP_V3			= 3
};

enum snmp_context {
	SNMP_C_GETREQ		= 0,
	SNMP_C_GETNEXTREQ	= 1,
	SNMP_C_GETRESP		= 2,
	SNMP_C_SETREQ		= 3,
	SNMP_C_TRAP		= 4,

	/* SNMPv2 */
	SNMP_C_GETBULKREQ	= 5,
	SNMP_C_INFORMREQ	= 6,
	SNMP_C_TRAPV2		= 7,
	SNMP_C_REPORT		= 8
};

enum snmp_application {
	SNMP_T_IPADDR		= 0,
	SNMP_T_COUNTER32	= 1,
	SNMP_T_GAUGE32		= 2,
	SNMP_T_UNSIGNED32	= 2,
	SNMP_T_TIMETICKS	= 3,
	SNMP_T_OPAQUE		= 4,
	SNMP_T_NSAPADDR		= 5,
	SNMP_T_COUNTER64	= 6,
	SNMP_T_UINTEGER32	= 7
};

enum snmp_generic_trap {
	SNMP_TRAP_COLDSTART	= 0,
	SNMP_TRAP_WARMSTART	= 1,
	SNMP_TRAP_LINKDOWN	= 2,
	SNMP_TRAP_LINKUP	= 3,
	SNMP_TRAP_AUTHFAILURE	= 4,
	SNMP_TRAP_EGPNEIGHLOSS	= 5,
	SNMP_TRAP_ENTERPRISE	= 6
};

enum snmp_error {
	SNMP_ERROR_NONE		= 0,
	SNMP_ERROR_TOOBIG	= 1,
	SNMP_ERROR_NOSUCHNAME	= 2,
	SNMP_ERROR_BADVALUE	= 3,
	SNMP_ERROR_READONLY	= 4,
	SNMP_ERROR_GENERR	= 5,

	/* SNMPv2 */
	SNMP_ERROR_NOACCESS	= 6,
	SNMP_ERROR_WRONGTYPE	= 7,
	SNMP_ERROR_WRONGLENGTH	= 8,
	SNMP_ERROR_WRONGENC	= 9,
	SNMP_ERROR_WRONGVALUE	= 10,
	SNMP_ERROR_NOCREATION	= 11,
	SNMP_ERROR_INCONVALUE	= 12,
	SNMP_ERROR_RESUNAVAIL	= 13, /* EGAIN */
	SNMP_ERROR_COMMITFAILED	= 14,
	SNMP_ERROR_UNDOFAILED	= 15,
	SNMP_ERROR_AUTHERROR	= 16,
	SNMP_ERROR_NOTWRITABLE	= 17,
	SNMP_ERROR_INCONNAME	= 18
};

enum snmp_security_model {
	SNMP_SEC_ANY		= 0,
	SNMP_SEC_SNMPv1		= 1,
	SNMP_SEC_SNMPv2c	= 2,
	SNMP_SEC_USM		= 3,
	SNMP_SEC_TSM		= 4
};

enum snmp_application_exception {
	SNMP_E_NOSUCHOBJECT	= 0,
	SNMP_E_NOSUCHINSTANCE	= 1,
	SNMP_E_ENDOFMIB		= 2
};

struct snmp_agent;

struct snmp_sec {
	enum snmp_security_model model;
	int (*init)(struct snmp_agent *);
	char *(*genparams)(struct snmp_agent *, size_t *, void **);
	struct ber_element *(*encpdu)(struct snmp_agent *,
	    struct ber_element *, void *);
	int (*finalparams)(struct snmp_agent *, char *, size_t, size_t, void *);
	int (*parseparams)(struct snmp_agent *, char *, size_t, off_t, char *,
	    size_t, uint8_t, void **);
	struct ber_element *(*decpdu)(struct snmp_agent *, char *, size_t,
	    void *);
	void (*free)(void *);
	void (*freecookie)(void *);
	void *data;
};

struct snmp_v3 {
	uint8_t level;
	char *ctxname;
	size_t ctxnamelen;
	int engineidset;
	char *engineid;
	size_t engineidlen;
	struct snmp_sec *sec;
};

struct snmp_agent {
	int fd;
	int timeout;
	int retries;
	enum snmp_version version;
/* SNMP_V1 & SNMP_V2C */
	char *community;
/* SNMP_V3 */
	struct snmp_v3 *v3;
};

#define SNMP_MSGFLAG_AUTH	0x01
#define SNMP_MSGFLAG_PRIV	0x02
#define SNMP_MSGFLAG_SECMASK	(SNMP_MSGFLAG_AUTH | SNMP_MSGFLAG_PRIV)
#define SNMP_MSGFLAG_REPORT	0x04

#define SNMP_MAX_TIMEWINDOW	150	/* RFC3414 */

struct snmp_v3 *snmp_v3_init(int, const char *, size_t, struct snmp_sec *);
int snmp_v3_setengineid(struct snmp_v3 *, char *, size_t);
struct snmp_agent *snmp_connect_v12(int, enum snmp_version, const char *);
struct snmp_agent *snmp_connect_v3(int, struct snmp_v3 *);
void snmp_free_agent(struct snmp_agent *);
struct ber_element *
    snmp_get(struct snmp_agent *agent, struct ber_oid *oid, size_t len);
struct ber_element *snmp_getnext(struct snmp_agent *, struct ber_oid *, size_t);
struct ber_element *
    snmp_getbulk(struct snmp_agent *, struct ber_oid *, size_t, int, int);
struct ber_element *snmp_set(struct snmp_agent *, struct ber_element *);
int snmp_trap(struct snmp_agent *, struct timespec *, struct ber_oid *,
    struct ber_element *);

ssize_t ber_copy_writebuf(struct ber *, void **);

#endif /* SNMPD_SNMP_H */
