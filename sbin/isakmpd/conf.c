/* $OpenBSD: conf.c,v 1.65 2004/04/15 20:20:55 deraadt Exp $	 */
/* $EOM: conf.c,v 1.48 2000/12/04 02:04:29 angelos Exp $	 */

/*
 * Copyright (c) 1998, 1999, 2000, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2000, 2001, 2002 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "sysdep.h"

#include "app.h"
#include "conf.h"
#include "log.h"
#include "monitor.h"
#include "util.h"

static char    *conf_get_trans_str(int, char *, char *);
static void     conf_load_defaults(int);
#if 0
static int      conf_find_trans_xf(int, char *);
#endif

struct conf_trans {
	TAILQ_ENTRY(conf_trans) link;
	int             trans;
	enum conf_op {
		CONF_SET, CONF_REMOVE, CONF_REMOVE_SECTION
	}               op;
	char           *section;
	char           *tag;
	char           *value;
	int             override;
	int             is_default;
};

TAILQ_HEAD(conf_trans_head, conf_trans) conf_trans_queue;

/*
 * Radix-64 Encoding.
 */
const u_int8_t  bin2asc[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const u_int8_t  asc2bin[] =
{
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 62, 255, 255, 255, 63,
	52, 53, 54, 55, 56, 57, 58, 59,
	60, 61, 255, 255, 255, 255, 255, 255,
	255, 0, 1, 2, 3, 4, 5, 6,
	7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22,
	23, 24, 25, 255, 255, 255, 255, 255,
	255, 26, 27, 28, 29, 30, 31, 32,
	33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 51, 255, 255, 255, 255, 255
};

struct conf_binding {
	LIST_ENTRY(conf_binding) link;
	char           *section;
	char           *tag;
	char           *value;
	int             is_default;
};

char           *conf_path = CONFIG_FILE;
LIST_HEAD(conf_bindings, conf_binding) conf_bindings[256];

static char    *conf_addr;
static __inline__ u_int8_t
conf_hash(char *s)
{
	u_int8_t        hash = 0;

	while (*s) {
		hash = ((hash << 1) | (hash >> 7)) ^ tolower(*s);
		s++;
	}
	return hash;
}

/*
 * Insert a tag-value combination from LINE (the equal sign is at POS)
 */
static int
conf_remove_now(char *section, char *tag)
{
	struct conf_binding *cb, *next;

	for (cb = LIST_FIRST(&conf_bindings[conf_hash(section)]); cb; cb = next) {
		next = LIST_NEXT(cb, link);
		if (strcasecmp(cb->section, section) == 0
		    && strcasecmp(cb->tag, tag) == 0) {
			LIST_REMOVE(cb, link);
			LOG_DBG((LOG_MISC, 95, "[%s]:%s->%s removed", section, tag,
			    cb->value));
			free(cb->section);
			free(cb->tag);
			free(cb->value);
			free(cb);
			return 0;
		}
	}
	return 1;
}

static int
conf_remove_section_now(char *section)
{
	struct conf_binding *cb, *next;
	int             unseen = 1;

	for (cb = LIST_FIRST(&conf_bindings[conf_hash(section)]); cb; cb = next) {
		next = LIST_NEXT(cb, link);
		if (strcasecmp(cb->section, section) == 0) {
			unseen = 0;
			LIST_REMOVE(cb, link);
			LOG_DBG((LOG_MISC, 95, "[%s]:%s->%s removed", section, cb->tag,
			    cb->value));
			free(cb->section);
			free(cb->tag);
			free(cb->value);
			free(cb);
		}
	}
	return unseen;
}

/*
 * Insert a tag-value combination from LINE (the equal sign is at POS)
 * into SECTION of our configuration database.
 */
static int
conf_set_now(char *section, char *tag, char *value, int override,
	     int is_default)
{
	struct conf_binding *node = 0;

	if (override)
		conf_remove_now(section, tag);
	else if (conf_get_str(section, tag)) {
		if (!is_default)
			log_print("conf_set_now: duplicate tag [%s]:%s, ignoring...\n",
			    section, tag);
		return 1;
	}
	node = calloc(1, sizeof *node);
	if (!node) {
		log_error("conf_set_now: calloc (1, %lu) failed", (unsigned long) sizeof
		    *node);
		return 1;
	}
	node->section = strdup(section);
	node->tag = strdup(tag);
	node->value = strdup(value);
	node->is_default = is_default;

	LIST_INSERT_HEAD(&conf_bindings[conf_hash(section)], node, link);
	LOG_DBG((LOG_MISC, 95, "conf_set_now: [%s]:%s->%s", node->section, node->tag,
	    node->value));
	return 0;
}

/*
 * Parse the line LINE of SZ bytes.  Skip Comments, recognize section
 * headers and feed tag-value pairs into our configuration database.
 */
static void
conf_parse_line(int trans, char *line, size_t sz)
{
	char           *val;
	size_t          i;
	int             j;
	static char    *section = 0;
	static int      ln = 0;

	ln++;

	/* Lines starting with '#' or ';' are comments.  */
	if (*line == '#' || *line == ';')
		return;

	/* '[section]' parsing...  */
	if (*line == '[') {
		for (i = 1; i < sz; i++)
			if (line[i] == ']')
				break;
		if (section)
			free(section);
		if (i == sz) {
			log_print("conf_parse_line: %d:"
			    "non-matched ']', ignoring until next section", ln);
			section = 0;
			return;
		}
		section = malloc(i);
		if (!section) {
			log_print("conf_parse_line: %d: malloc (%lu) failed", ln,
			    (unsigned long) i);
			return;
		}
		strlcpy(section, line + 1, i);
		return;
	}
	/* Deal with assignments.  */
	for (i = 0; i < sz; i++)
		if (line[i] == '=') {
			/* If no section, we are ignoring the lines.  */
			if (!section) {
				log_print("conf_parse_line: %d: ignoring line "
				    "due to no section", ln);
				return;
			}
			line[strcspn(line, " \t=")] = '\0';
			val = line + i + 1 + strspn(line + i + 1, " \t");
			/* Skip trailing whitespace, if any */
			for (j = sz - (val - line) - 1; j > 0 && isspace(val[j]); j--)
				val[j] = '\0';
			/* XXX Perhaps should we not ignore errors?  */
			conf_set(trans, section, line, val, 0, 0);
			return;
		}
	/* Other non-empty lines are weird.  */
	i = strspn(line, " \t");
	if (line[i])
		log_print("conf_parse_line: %d: syntax error", ln);
}

/* Parse the mapped configuration file.  */
static void
conf_parse(int trans, char *buf, size_t sz)
{
	char           *cp = buf;
	char           *bufend = buf + sz;
	char           *line;

	line = cp;
	while (cp < bufend) {
		if (*cp == '\n') {
			/* Check for escaped newlines.  */
			if (cp > buf && *(cp - 1) == '\\')
				*(cp - 1) = *cp = ' ';
			else {
				*cp = '\0';
				conf_parse_line(trans, line, cp - line);
				line = cp + 1;
			}
		}
		cp++;
	}
	if (cp != line)
		log_print("conf_parse: last line non-terminated, ignored.");
}

/*
 * Auto-generate default configuration values for the transforms and
 * suites the user wants.
 *
 * Resulting section names can be:
 *  For main mode:
 *     {DES,BLF,3DES,CAST,AES}-{MD5,SHA}[-GRP{1,2,5,14}][-{DSS,RSA_SIG}]
 *  For quick mode:
 *     QM-{proto}[-TRP]-{cipher}[-{hash}][-PFS[-{group}]]-SUITE
 *     where
 *       {proto}  = ESP, AH
 *       {cipher} = DES, 3DES, CAST, BLF, AES
 *       {hash}   = MD5, SHA, RIPEMD, SHA2-{-256,384,512}
 *       {group}  = GRP1, GRP2, GRP5, GRP14
 *
 * DH group defaults to MODP_1024.
 *
 * XXX We may want to support USE_BLOWFISH, USE_TRIPLEDES, etc...
 * XXX No EC2N DH support here yet.
 */

/* Find the value for a section+tag in the transaction list.  */
static char    *
conf_get_trans_str(int trans, char *section, char *tag)
{
	struct conf_trans *node, *nf = 0;

	for (node = TAILQ_FIRST(&conf_trans_queue); node;
	     node = TAILQ_NEXT(node, link))
		if (node->trans == trans && strcasecmp(section, node->section) == 0
		    && strcasecmp(tag, node->tag) == 0) {
			if (!nf)
				nf = node;
			else if (node->override)
				nf = node;
		}
	return nf ? nf->value : 0;
}

#if 0
/* XXX Currently unused.  */
static int
conf_find_trans_xf(int phase, char *xf)
{
	struct conf_trans *node;
	char           *p;

	/* Find the relevant transforms and suites, if any.  */
	for (node = TAILQ_FIRST(&conf_trans_queue); node;
	     node = TAILQ_NEXT(node, link))
		if ((phase == 1 && strcmp("Transforms", node->tag) == 0) ||
		    (phase == 2 && strcmp("Suites", node->tag) == 0)) {
			p = node->value;
			while ((p = strstr(p, xf)) != NULL)
				if (*(p + strlen(p)) && *(p + strlen(p)) != ',')
					p += strlen(p);
				else
					return 1;
		}
	return 0;
}
#endif

static void
conf_load_defaults(int tr)
{
#define CONF_MAX 256
	int             enc, auth, hash, group, group_max, proto, mode,
	                pfs;
	char            sect[CONF_MAX], *dflt;

	char           *mm_auth[] = {"PRE_SHARED", "DSS", "RSA_SIG", 0};
	char           *mm_hash[] = {"MD5", "SHA", 0};
	char           *mm_enc[] = {"DES_CBC", "BLOWFISH_CBC", "3DES_CBC",
	"CAST_CBC", "AES_CBC", 0};
	char           *dh_group[] = {"MODP_768", "MODP_1024", "MODP_1536", "MODP_2048", 0};
	char           *qm_enc[] = {"DES", "3DES", "CAST", "BLOWFISH", "AES", 0};
	char           *qm_hash[] = {"HMAC_MD5", "HMAC_SHA", "HMAC_RIPEMD",
		"HMAC_SHA2_256", "HMAC_SHA2_384", "HMAC_SHA2_512",
	"NONE", 0};

	/* Abbreviations to make section names a bit shorter.  */
	char           *mm_auth_p[] = {"", "-DSS", "-RSA_SIG", 0};
	char           *mm_enc_p[] = {"DES", "BLF", "3DES", "CAST", "AES", 0};
	char           *dh_group_p[] = {"-GRP1", "-GRP2", "-GRP5", "-GRP14", "", 0};
	char           *qm_enc_p[] = {"-DES", "-3DES", "-CAST", "-BLF", "-AES", 0};
	char           *qm_hash_p[] = {"-MD5", "-SHA", "-RIPEMD",
		"-SHA2-256", "-SHA2-384", "-SHA2-512",
	"", 0};

	/* Helper #defines, incl abbreviations.  */
#define PROTO(x)  ((x) ? "AH" : "ESP")
#define PFS(x)    ((x) ? "-PFS" : "")
#define MODE(x)   ((x) ? "TRANSPORT" : "TUNNEL")
#define MODE_p(x) ((x) ? "-TRP" : "")
	group_max = sizeof dh_group / sizeof *dh_group - 1;

	/* General and X509 defaults */
	conf_set(tr, "General", "Retransmits", CONF_DFLT_RETRANSMITS, 0, 1);
	conf_set(tr, "General", "Exchange-max-time", CONF_DFLT_EXCH_MAX_TIME, 0, 1);
	conf_set(tr, "General", "Policy-file", CONF_DFLT_POLICY_FILE, 0, 1);
	conf_set(tr, "General", "Pubkey-directory", CONF_DFLT_PUBKEY_DIR, 0, 1);

#ifdef USE_X509
	conf_set(tr, "X509-certificates", "CA-directory", CONF_DFLT_X509_CA_DIR, 0,
		 1);
	conf_set(tr, "X509-certificates", "Cert-directory", CONF_DFLT_X509_CERT_DIR,
		 0, 1);
	conf_set(tr, "X509-certificates", "Private-key", CONF_DFLT_X509_PRIVATE_KEY,
		 0, 1);
	conf_set(tr, "X509-certificates", "CRL-directory", CONF_DFLT_X509_CRL_DIR,
		 0, 1);
#endif

#ifdef USE_KEYNOTE
	conf_set(tr, "KeyNote", "Credential-directory", CONF_DFLT_KEYNOTE_CRED_DIR,
		 0, 1);
#endif

	/* Lifetimes. XXX p1/p2 vs main/quick mode may be unclear.  */
	dflt = conf_get_trans_str(tr, "General", "Default-phase-1-lifetime");
	conf_set(tr, CONF_DFLT_TAG_LIFE_MAIN_MODE, "LIFE_TYPE",
		 CONF_DFLT_TYPE_LIFE_MAIN_MODE, 0, 1);
	conf_set(tr, CONF_DFLT_TAG_LIFE_MAIN_MODE, "LIFE_DURATION",
		 (dflt ? dflt : CONF_DFLT_VAL_LIFE_MAIN_MODE), 0, 1);

	dflt = conf_get_trans_str(tr, "General", "Default-phase-2-lifetime");
	conf_set(tr, CONF_DFLT_TAG_LIFE_QUICK_MODE, "LIFE_TYPE",
		 CONF_DFLT_TYPE_LIFE_QUICK_MODE, 0, 1);
	conf_set(tr, CONF_DFLT_TAG_LIFE_QUICK_MODE, "LIFE_DURATION",
		 (dflt ? dflt : CONF_DFLT_VAL_LIFE_QUICK_MODE), 0, 1);

	/* Default Phase-1 Configuration section */
	conf_set(tr, CONF_DFLT_TAG_PHASE1_CONFIG, "EXCHANGE_TYPE",
		 CONF_DFLT_PHASE1_EXCH_TYPE, 0, 1);
	conf_set(tr, CONF_DFLT_TAG_PHASE1_CONFIG, "Transforms",
		 CONF_DFLT_PHASE1_TRANSFORMS, 0, 1);

	/* Main modes */
	for (enc = 0; mm_enc[enc]; enc++) {
		for (hash = 0; mm_hash[hash]; hash++) {
			for (auth = 0; mm_auth[auth]; auth++) {
				for (group = 0; dh_group_p[group]; group++) {
					/* special */
					snprintf(sect, sizeof sect, "%s-%s%s%s",
					    mm_enc_p[enc], mm_hash[hash],
					    dh_group_p[group], mm_auth_p[auth]);

#if 0
					if (!conf_find_trans_xf(1, sect))
						continue;
#endif

					LOG_DBG((LOG_MISC, 90,
					    "conf_load_defaults : main mode %s",
					    sect));

					conf_set(tr, sect, "ENCRYPTION_ALGORITHM",
					    mm_enc[enc], 0, 1);
					if (strcmp(mm_enc[enc], "BLOWFISH_CBC") == 0)
						conf_set(tr, sect, "KEY_LENGTH",
						    CONF_DFLT_VAL_BLF_KEYLEN, 0, 1);

					conf_set(tr, sect, "HASH_ALGORITHM",
					    mm_hash[hash], 0, 1);
					conf_set(tr, sect, "AUTHENTICATION_METHOD",
					    mm_auth[auth], 0, 1);

					/* XXX Always DH group 2 (MODP_1024) */
					conf_set(tr, sect, "GROUP_DESCRIPTION",
					    dh_group[group < group_max ? group : 1],
					    0, 1);

					conf_set(tr, sect, "Life",
					    CONF_DFLT_TAG_LIFE_MAIN_MODE, 0, 1);
				}
			}
		}
	}

	/* Setup a default Phase 1 entry */
	conf_set(tr, "Phase 1", "Default", "Default-phase-1", 0, 1);

	conf_set(tr, "Default-phase-1", "Phase", "1", 0, 1);
	conf_set(tr, "Default-phase-1", "Configuration",
		 "Default-phase-1-configuration", 0, 1);
	dflt = conf_get_trans_str(tr, "General", "Default-phase-1-ID");
	if (dflt)
		conf_set(tr, "Default-phase-1", "ID", dflt, 0, 1);

	/* Quick modes */
	for (enc = 0; qm_enc[enc]; enc++) {
		for (proto = 0; proto < 2; proto++) {
			for (mode = 0; mode < 2; mode++) {
				for (pfs = 0; pfs < 2; pfs++) {
					for (hash = 0; qm_hash[hash]; hash++) {
						for (group = 0; dh_group_p[group];
						    group++) {
							char tmp[CONF_MAX];

							if ((proto == 1 &&
							    strcmp(qm_hash[hash],
							    "NONE") == 0)) /* AH */
								continue;

							snprintf(tmp, sizeof tmp,
							    "QM-%s%s%s%s%s%s",
							    PROTO(proto),
							    MODE_p(mode),
							    qm_enc_p[enc],
							    qm_hash_p[hash],
							    PFS(pfs),
							    dh_group_p[group]);

							strlcpy(sect, tmp, CONF_MAX);
							strlcat(sect, "-SUITE",
							    CONF_MAX);

#if 0
							if (!conf_find_trans_xf(2, sect))
								continue;
#endif

							LOG_DBG((LOG_MISC, 90,
							    "conf_load_defaults : quick mode %s",
							    sect));

							conf_set(tr, sect, "Protocols",
							    tmp, 0, 1);

							snprintf(sect, sizeof sect,
							    "IPSEC_%s", PROTO(proto));
							conf_set(tr, tmp, "PROTOCOL_ID",
							    sect, 0, 1);

							strlcpy(sect, tmp, CONF_MAX);
							strlcat(sect, "-XF", CONF_MAX);
							conf_set(tr, tmp, "Transforms",
							    sect, 0, 1);

							/*
							 * XXX For now, defaults
							 * contain one xf per protocol.
							 */

							conf_set(tr, sect,
							    "TRANSFORM_ID",
							    qm_enc[enc], 0, 1);

							if (strcmp(qm_enc[enc],
							    "BLOWFISH") == 0)
								conf_set(tr, sect,
								    "KEY_LENGTH",
								    CONF_DFLT_VAL_BLF_KEYLEN,
								    0, 1);

							conf_set(tr, sect,
							    "ENCAPSULATION_MODE",
							    MODE(mode), 0, 1);

							if (strcmp(qm_hash[hash], "NONE")) {
								conf_set(tr, sect, "AUTHENTICATION_ALGORITHM",
								    qm_hash[hash], 0, 1);

								/*
								 * XXX
								 *
								 * Another shortcut:
								 * to keep length down
								 */
								if (pfs)
									conf_set(tr, sect, "GROUP_DESCRIPTION",
									    dh_group[group < group_max ? group : 1],
									    0, 1);
							}
							/*
							 * XXX
							 * Lifetimes depending
							 * on enc/auth strength?
							 */
							conf_set(tr, sect, "Life", CONF_DFLT_TAG_LIFE_QUICK_MODE, 0,
							    1);

						}
					}
				}
			}
		}
	}
}

void
conf_init(void)
{
	unsigned int    i;

	for (i = 0; i < sizeof conf_bindings / sizeof conf_bindings[0]; i++)
		LIST_INIT(&conf_bindings[i]);
	TAILQ_INIT(&conf_trans_queue);
	conf_reinit();
}

/* Open the config file and map it into our address space, then parse it.  */
void
conf_reinit(void)
{
	struct conf_binding *cb = 0;
	int             fd, trans;
	unsigned int    i;
	size_t          sz;
	char           *new_conf_addr = 0;
	struct stat     sb;

	if ((monitor_stat(conf_path, &sb) == 0) || (errno != ENOENT)) {
		if (check_file_secrecy(conf_path, &sz))
			return;

		fd = monitor_open(conf_path, O_RDONLY, 0);
		if (fd == -1) {
			log_error("conf_reinit: open (\"%s\", O_RDONLY) failed", conf_path);
			return;
		}
		new_conf_addr = malloc(sz);
		if (!new_conf_addr) {
			log_error("conf_reinit: malloc (%lu) failed", (unsigned long) sz);
			goto fail;
		}
		/* XXX I assume short reads won't happen here.  */
		if (read(fd, new_conf_addr, sz) != (int) sz) {
			log_error("conf_reinit: read (%d, %p, %lu) failed",
				  fd, new_conf_addr, (unsigned long) sz);
			goto fail;
		}
		close(fd);

		trans = conf_begin();

		/* XXX Should we not care about errors and rollback?  */
		conf_parse(trans, new_conf_addr, sz);
	} else
		trans = conf_begin();

	/* Load default configuration values.  */
	conf_load_defaults(trans);

	/* Free potential existing configuration.  */
	if (conf_addr) {
		for (i = 0; i < sizeof conf_bindings / sizeof conf_bindings[0]; i++)
			for (cb = LIST_FIRST(&conf_bindings[i]); cb;
			     cb = LIST_FIRST(&conf_bindings[i]))
				conf_remove_now(cb->section, cb->tag);
		free(conf_addr);
	}
	conf_end(trans, 1);
	conf_addr = new_conf_addr;
	return;

fail:
	if (new_conf_addr)
		free(new_conf_addr);
	close(fd);
}

/*
 * Return the numeric value denoted by TAG in section SECTION or DEF
 * if that tag does not exist.
 */
int
conf_get_num(char *section, char *tag, int def)
{
	char           *value = conf_get_str(section, tag);

	if (value)
		return atoi(value);
	return def;
}

/*
 * Return the socket endpoint address denoted by TAG in SECTION as a
 * struct sockaddr.  It is the callers responsibility to deallocate
 * this structure when it is finished with it.
 */
struct sockaddr *
conf_get_address(char *section, char *tag)
{
	char           *value = conf_get_str(section, tag);
	struct sockaddr *sa;

	if (!value)
		return 0;
	if (text2sockaddr(value, 0, &sa) == -1)
		return 0;
	return sa;
}

/* Validate X according to the range denoted by TAG in section SECTION.  */
int
conf_match_num(char *section, char *tag, int x)
{
	char           *value = conf_get_str(section, tag);
	int             val, min, max, n;

	if (!value)
		return 0;
	n = sscanf(value, "%d,%d:%d", &val, &min, &max);
	switch (n) {
	case 1:
		LOG_DBG((LOG_MISC, 90, "conf_match_num: %s:%s %d==%d?", section, tag,
			 val, x));
		return x == val;
	case 3:
		LOG_DBG((LOG_MISC, 90, "conf_match_num: %s:%s %d<=%d<=%d?", section,
			 tag, min, x, max));
		return min <= x && max >= x;
	default:
		log_error("conf_match_num: section %s tag %s: invalid number spec %s",
			  section, tag, value);
	}
	return 0;
}

/* Return the string value denoted by TAG in section SECTION.  */
char           *
conf_get_str(char *section, char *tag)
{
	struct conf_binding *cb;

	for (cb = LIST_FIRST(&conf_bindings[conf_hash(section)]); cb;
	     cb = LIST_NEXT(cb, link))
		if (strcasecmp(section, cb->section) == 0
		    && strcasecmp(tag, cb->tag) == 0) {
			LOG_DBG((LOG_MISC, 95, "conf_get_str: [%s]:%s->%s", section,
				 tag, cb->value));
			return cb->value;
		}
	LOG_DBG((LOG_MISC, 95,
	     "conf_get_str: configuration value not found [%s]:%s", section,
		 tag));
	return 0;
}

/*
 * Build a list of string values out of the comma separated value denoted by
 * TAG in SECTION.
 */
struct conf_list *
conf_get_list(char *section, char *tag)
{
	char           *liststr = 0, *p, *field, *t;
	struct conf_list *list = 0;
	struct conf_list_node *node;

	list = malloc(sizeof *list);
	if (!list)
		goto cleanup;
	TAILQ_INIT(&list->fields);
	list->cnt = 0;
	liststr = conf_get_str(section, tag);
	if (!liststr)
		goto cleanup;
	liststr = strdup(liststr);
	if (!liststr)
		goto cleanup;
	p = liststr;
	while ((field = strsep(&p, ",")) != NULL) {
		/* Skip leading whitespace */
		while (isspace(*field))
			field++;
		/* Skip trailing whitespace */
		if (p)
			for (t = p - 1; t > field && isspace(*t); t--)
				*t = '\0';
		if (*field == '\0') {
			log_print("conf_get_list: empty field, ignoring...");
			continue;
		}
		list->cnt++;
		node = calloc(1, sizeof *node);
		if (!node)
			goto cleanup;
		node->field = strdup(field);
		if (!node->field)
			goto cleanup;
		TAILQ_INSERT_TAIL(&list->fields, node, link);
	}
	free(liststr);
	return list;

cleanup:
	if (list)
		conf_free_list(list);
	if (liststr)
		free(liststr);
	return 0;
}

struct conf_list *
conf_get_tag_list(char *section)
{
	struct conf_list *list = 0;
	struct conf_list_node *node;
	struct conf_binding *cb;

	list = malloc(sizeof *list);
	if (!list)
		goto cleanup;
	TAILQ_INIT(&list->fields);
	list->cnt = 0;
	for (cb = LIST_FIRST(&conf_bindings[conf_hash(section)]); cb;
	     cb = LIST_NEXT(cb, link))
		if (strcasecmp(section, cb->section) == 0) {
			list->cnt++;
			node = calloc(1, sizeof *node);
			if (!node)
				goto cleanup;
			node->field = strdup(cb->tag);
			if (!node->field)
				goto cleanup;
			TAILQ_INSERT_TAIL(&list->fields, node, link);
		}
	return list;

cleanup:
	if (list)
		conf_free_list(list);
	return 0;
}

/* Decode a PEM encoded buffer.  */
int
conf_decode_base64(u_int8_t * out, u_int32_t * len, u_char * buf)
{
	u_int32_t       c = 0;
	u_int8_t        c1, c2, c3, c4;

	while (*buf) {
		if (*buf > 127 || (c1 = asc2bin[*buf]) == 255)
			return 0;
		buf++;

		if (*buf > 127 || (c2 = asc2bin[*buf]) == 255)
			return 0;
		buf++;

		if (*buf == '=') {
			c3 = c4 = 0;
			c++;

			/* Check last four bit */
			if (c2 & 0xF)
				return 0;

			if (strcmp((char *) buf, "==") == 0)
				buf++;
			else
				return 0;
		} else if (*buf > 127 || (c3 = asc2bin[*buf]) == 255)
			return 0;
		else {
			if (*++buf == '=') {
				c4 = 0;
				c += 2;

				/* Check last two bit */
				if (c3 & 3)
					return 0;

				if (strcmp((char *) buf, "="))
					return 0;

			} else if (*buf > 127 || (c4 = asc2bin[*buf]) == 255)
				return 0;
			else
				c += 3;
		}

		buf++;
		*out++ = (c1 << 2) | (c2 >> 4);
		*out++ = (c2 << 4) | (c3 >> 2);
		*out++ = (c3 << 6) | c4;
	}

	*len = c;
	return 1;

}

void
conf_free_list(struct conf_list * list)
{
	struct conf_list_node *node = TAILQ_FIRST(&list->fields);

	while (node) {
		TAILQ_REMOVE(&list->fields, node, link);
		if (node->field)
			free(node->field);
		free(node);
		node = TAILQ_FIRST(&list->fields);
	}
	free(list);
}

int
conf_begin(void)
{
	static int      seq = 0;

	return ++seq;
}

static struct conf_trans *
conf_trans_node(int transaction, enum conf_op op)
{
	struct conf_trans *node;

	node = calloc(1, sizeof *node);
	if (!node) {
		log_error("conf_trans_node: calloc (1, %lu) failed",
			  (unsigned long) sizeof *node);
		return 0;
	}
	node->trans = transaction;
	node->op = op;
	TAILQ_INSERT_TAIL(&conf_trans_queue, node, link);
	return node;
}

/* Queue a set operation.  */
int
conf_set(int transaction, char *section, char *tag, char *value, int override,
	 int is_default)
{
	struct conf_trans *node;

	node = conf_trans_node(transaction, CONF_SET);
	if (!node)
		return 1;
	node->section = strdup(section);
	if (!node->section) {
		log_error("conf_set: strdup (\"%s\") failed", section);
		goto fail;
	}
	node->tag = strdup(tag);
	if (!node->tag) {
		log_error("conf_set: strdup (\"%s\") failed", tag);
		goto fail;
	}
	node->value = strdup(value);
	if (!node->value) {
		log_error("conf_set: strdup (\"%s\") failed", value);
		goto fail;
	}
	node->override = override;
	node->is_default = is_default;
	return 0;

fail:
	if (node->tag)
		free(node->tag);
	if (node->section)
		free(node->section);
	if (node)
		free(node);
	return 1;
}

/* Queue a remove operation.  */
int
conf_remove(int transaction, char *section, char *tag)
{
	struct conf_trans *node;

	node = conf_trans_node(transaction, CONF_REMOVE);
	if (!node)
		goto fail;
	node->section = strdup(section);
	if (!node->section) {
		log_error("conf_remove: strdup (\"%s\") failed", section);
		goto fail;
	}
	node->tag = strdup(tag);
	if (!node->tag) {
		log_error("conf_remove: strdup (\"%s\") failed", tag);
		goto fail;
	}
	return 0;

fail:
	if (node->section)
		free(node->section);
	if (node)
		free(node);
	return 1;
}

/* Queue a remove section operation.  */
int
conf_remove_section(int transaction, char *section)
{
	struct conf_trans *node;

	node = conf_trans_node(transaction, CONF_REMOVE_SECTION);
	if (!node)
		goto fail;
	node->section = strdup(section);
	if (!node->section) {
		log_error("conf_remove_section: strdup (\"%s\") failed", section);
		goto fail;
	}
	return 0;

fail:
	if (node)
		free(node);
	return 1;
}

/* Execute all queued operations for this transaction.  Cleanup.  */
int
conf_end(int transaction, int commit)
{
	struct conf_trans *node, *next;

	for (node = TAILQ_FIRST(&conf_trans_queue); node; node = next) {
		next = TAILQ_NEXT(node, link);
		if (node->trans == transaction) {
			if (commit)
				switch (node->op) {
				case CONF_SET:
					conf_set_now(node->section, node->tag,
					    node->value, node->override,
					    node->is_default);
					break;
				case CONF_REMOVE:
					conf_remove_now(node->section, node->tag);
					break;
				case CONF_REMOVE_SECTION:
					conf_remove_section_now(node->section);
					break;
				default:
					log_print("conf_end: unknown operation: %d",
					    node->op);
				}
			TAILQ_REMOVE(&conf_trans_queue, node, link);
			if (node->section)
				free(node->section);
			if (node->tag)
				free(node->tag);
			if (node->value)
				free(node->value);
			free(node);
		}
	}
	return 0;
}

/*
 * Dump running configuration upon SIGUSR1.
 * Configuration is "stored in reverse order", so reverse it again.
 */
struct dumper {
	char           *s, *v;
	struct dumper  *next;
};

static void
conf_report_dump(struct dumper * node)
{
	/* Recursive, cleanup when we're done.  */

	if (node->next)
		conf_report_dump(node->next);

	if (node->v)
		LOG_DBG((LOG_REPORT, 0, "%s=\t%s", node->s, node->v));
	else if (node->s) {
		LOG_DBG((LOG_REPORT, 0, "%s", node->s));
		if (strlen(node->s) > 0)
			free(node->s);
	}
	free(node);
}

void
conf_report(void)
{
	struct conf_binding *cb, *last = 0;
	unsigned int    i, len;
	char           *current_section = (char *) 0;
	struct dumper  *dumper, *dnode;

	dumper = dnode = (struct dumper *) calloc(1, sizeof *dumper);
	if (!dumper)
		goto mem_fail;

	LOG_DBG((LOG_REPORT, 0, "conf_report: dumping running configuration"));

	for (i = 0; i < sizeof conf_bindings / sizeof conf_bindings[0]; i++)
		for (cb = LIST_FIRST(&conf_bindings[i]); cb;
		     cb = LIST_NEXT(cb, link)) {
			if (!cb->is_default) {
				/* Dump this entry.  */
				if (!current_section ||
				    strcmp(cb->section, current_section)) {
					if (current_section) {
						len = strlen(current_section) + 3;
						dnode->s = malloc(len);
						if (!dnode->s)
							goto mem_fail;

						snprintf(dnode->s, len, "[%s]",
						    current_section);
						dnode->next = (struct dumper *)
						    calloc(1, sizeof(struct dumper));
						dnode = dnode->next;
						if (!dnode)
							goto mem_fail;

						dnode->s = "";
						dnode->next = (struct dumper *)
						    calloc(1, sizeof(struct dumper));
						dnode = dnode->next;
						if (!dnode)
							goto mem_fail;
					}
					current_section = cb->section;
				}
				dnode->s = cb->tag;
				dnode->v = cb->value;
				dnode->next = (struct dumper *)
				    calloc(1, sizeof(struct dumper));
				dnode = dnode->next;
				if (!dnode)
					goto mem_fail;
				last = cb;
			}
		}

	if (last) {
		len = strlen(last->section) + 3;
		dnode->s = malloc(len);
		if (!dnode->s)
			goto mem_fail;
		snprintf(dnode->s, len, "[%s]", last->section);
	}
	conf_report_dump(dumper);

	return;

mem_fail:
	log_error("conf_report: malloc/calloc failed");
	while ((dnode = dumper) != 0) {
		dumper = dumper->next;
		if (dnode->s)
			free(dnode->s);
		free(dnode);
	}
}
