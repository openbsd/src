/*	$OpenBSD: smi.c,v 1.15 2021/10/21 08:17:34 martijn Exp $	*/

/*
 * Copyright (c) 2019 Martijn van Duren <martijn@openbsd.org>
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/limits.h>
#include <sys/tree.h>
#include <sys/queue.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <wctype.h>

#include "ber.h"
#include "mib.h"
#include "snmp.h"
#include "smi.h"

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

char *smi_displayhint_os(struct textconv *, int, const char *, size_t, int);
char *smi_displayhint_int(struct textconv*, int, long long);

int smi_oid_cmp(struct oid *, struct oid *);
int smi_key_cmp(struct oid *, struct oid *);
int smi_textconv_cmp(struct textconv *, struct textconv *);
struct oid * smi_findkey(char *);

RB_HEAD(oidtree, oid);
RB_PROTOTYPE(oidtree, oid, o_element, smi_oid_cmp)
struct oidtree smi_oidtree;

RB_HEAD(keytree, oid);
RB_PROTOTYPE(keytree, oid, o_keyword, smi_key_cmp)
struct keytree smi_keytree;

RB_HEAD(textconvtree, textconv);
RB_PROTOTYPE(textconvtree, textconv, tc_entry, smi_textconv_cmp);
struct textconvtree smi_tctree;

int
smi_init(void)
{
	/* Initialize the Structure of Managed Information (SMI) */
	RB_INIT(&smi_oidtree);
	mib_init();
	return (0);
}

void
smi_debug_elements(struct ber_element *root, int utf8)
{
	static int	 indent = 0;
	char		*value;
	int		 constructed;

	/* calculate lengths */
	ober_calc_len(root);

	switch (root->be_encoding) {
	case BER_TYPE_SEQUENCE:
	case BER_TYPE_SET:
		constructed = root->be_encoding;
		break;
	default:
		constructed = 0;
		break;
	}

	fprintf(stderr, "%*slen %lu ", indent, "", root->be_len);
	switch (root->be_class) {
	case BER_CLASS_UNIVERSAL:
		fprintf(stderr, "class: universal(%u) type: ", root->be_class);
		switch (root->be_type) {
		case BER_TYPE_EOC:
			fprintf(stderr, "end-of-content");
			break;
		case BER_TYPE_INTEGER:
			fprintf(stderr, "integer");
			break;
		case BER_TYPE_BITSTRING:
			fprintf(stderr, "bit-string");
			break;
		case BER_TYPE_OCTETSTRING:
			fprintf(stderr, "octet-string");
			break;
		case BER_TYPE_NULL:
			fprintf(stderr, "null");
			break;
		case BER_TYPE_OBJECT:
			fprintf(stderr, "object");
			break;
		case BER_TYPE_ENUMERATED:
			fprintf(stderr, "enumerated");
			break;
		case BER_TYPE_SEQUENCE:
			fprintf(stderr, "sequence");
			break;
		case BER_TYPE_SET:
			fprintf(stderr, "set");
			break;
		}
		break;
	case BER_CLASS_APPLICATION:
		fprintf(stderr, "class: application(%u) type: ",
		    root->be_class);
		switch (root->be_type) {
		case SNMP_T_IPADDR:
			fprintf(stderr, "ipaddr");
			break;
		case SNMP_T_COUNTER32:
			fprintf(stderr, "counter32");
			break;
		case SNMP_T_GAUGE32:
			fprintf(stderr, "gauge32");
			break;
		case SNMP_T_TIMETICKS:
			fprintf(stderr, "timeticks");
			break;
		case SNMP_T_OPAQUE:
			fprintf(stderr, "opaque");
			break;
		case SNMP_T_COUNTER64:
			fprintf(stderr, "counter64");
			break;
		}
		break;
	case BER_CLASS_CONTEXT:
		fprintf(stderr, "class: context(%u) type: ",
		    root->be_class);
		switch (root->be_type) {
		case SNMP_C_GETREQ:
			fprintf(stderr, "getreq");
			break;
		case SNMP_C_GETNEXTREQ:
			fprintf(stderr, "nextreq");
			break;
		case SNMP_C_GETRESP:
			fprintf(stderr, "getresp");
			break;
		case SNMP_C_SETREQ:
			fprintf(stderr, "setreq");
			break;
		case SNMP_C_TRAP:
			fprintf(stderr, "trap");
			break;
		case SNMP_C_GETBULKREQ:
			fprintf(stderr, "getbulkreq");
			break;
		case SNMP_C_INFORMREQ:
			fprintf(stderr, "informreq");
			break;
		case SNMP_C_TRAPV2:
			fprintf(stderr, "trapv2");
			break;
		case SNMP_C_REPORT:
			fprintf(stderr, "report");
			break;
		}
		break;
	case BER_CLASS_PRIVATE:
		fprintf(stderr, "class: private(%u) type: ", root->be_class);
		break;
	default:
		fprintf(stderr, "class: <INVALID>(%u) type: ", root->be_class);
		break;
	}
	fprintf(stderr, "(%u) encoding %u ",
	    root->be_type, root->be_encoding);

	if ((value = smi_print_element(NULL, root, 1, smi_os_default,
	    smi_oidl_numeric, utf8)) == NULL)
		goto invalid;

	switch (root->be_encoding) {
	case BER_TYPE_INTEGER:
	case BER_TYPE_ENUMERATED:
		fprintf(stderr, "value %s", value);
		break;
	case BER_TYPE_BITSTRING:
		fprintf(stderr, "hexdump %s", value);
		break;
	case BER_TYPE_OBJECT:
		fprintf(stderr, "oid %s", value);
		break;
	case BER_TYPE_OCTETSTRING:
		if (root->be_class == BER_CLASS_APPLICATION &&
		    root->be_type == SNMP_T_IPADDR) {
			fprintf(stderr, "addr %s", value);
		} else {
			fprintf(stderr, "string %s", value);
		}
		break;
	case BER_TYPE_NULL:	/* no payload */
	case BER_TYPE_EOC:
	case BER_TYPE_SEQUENCE:
	case BER_TYPE_SET:
	default:
		fprintf(stderr, "%s", value);
		break;
	}

 invalid:
	if (value == NULL)
		fprintf(stderr, "<INVALID>");
	else
		free(value);
	fprintf(stderr, "\n");

	if (constructed)
		root->be_encoding = constructed;

	if (constructed && root->be_sub) {
		indent += 2;
		smi_debug_elements(root->be_sub, utf8);
		indent -= 2;
	}
	if (root->be_next)
		smi_debug_elements(root->be_next, utf8);
}

char *
smi_print_element(struct ber_oid *oid, struct ber_element *root, int print_hint,
    enum smi_output_string output_string, enum smi_oid_lookup lookup, int utf8)
{
	char		*str = NULL, *buf, *p;
	struct oid	 okey;
	struct oid	*object = NULL;
	struct textconv	 tckey;
	size_t		 len, i, slen;
	long long	 v, ticks;
	int		 is_hex = 0, ret;
	struct ber_oid	 o;
	char		 strbuf[BUFSIZ];
	char		*hint;
	int		 days, hours, min, sec, csec;

	if (oid != NULL) {
		bcopy(oid, &(okey.o_id), sizeof(okey));
		do {
			object = RB_FIND(oidtree, &smi_oidtree, &okey);
			okey.o_id.bo_n--;
		} while (object == NULL && okey.o_id.bo_n > 0);
		if (object != NULL && object->o_textconv == NULL &&
		    object->o_tcname != NULL) {
			tckey.tc_name = object->o_tcname;
			object->o_textconv = RB_FIND(textconvtree, &smi_tctree,
			    &tckey);
		}
	}

	switch (root->be_encoding) {
	case BER_TYPE_INTEGER:
	case BER_TYPE_ENUMERATED:
		if (ober_get_integer(root, &v) == -1)
			goto fail;
		if (root->be_class == BER_CLASS_APPLICATION &&
		    root->be_type == SNMP_T_TIMETICKS) {
			ticks = v;
			days = ticks / (60 * 60 * 24 * 100);
			ticks %= (60 * 60 * 24 * 100);
			hours = ticks / (60 * 60 * 100);
			ticks %= (60 * 60 * 100);
			min = ticks / (60 * 100);
			ticks %= (60 * 100);
			sec = ticks / 100;
			ticks %= 100;
			csec = ticks;

			if (print_hint) {
				if (days == 0) {
					if (asprintf(&str,
					    "Timeticks: (%lld) "
					    "%d:%02d:%02d.%02d",
					    v, hours, min, sec, csec) == -1)
						goto fail;
				} else if (days == 1) {
					if (asprintf(&str,
					    "Timeticks: (%lld) "
					    "1 day %d:%02d:%02d.%02d",
					    v, hours, min, sec, csec) == -1)
						goto fail;
				} else {
					if (asprintf(&str,
					    "Timeticks: (%lld) "
					    "%d day %d:%02d:%02d.%02d",
					    v, days, hours, min, sec, csec) ==
					    -1)
						goto fail;
				}
			} else {
				if (days == 0) {
					if (asprintf(&str, "%d:%02d:%02d.%02d",
					    hours, min, sec, csec) == -1)
						goto fail;
				} else if (days == 1) {
					if (asprintf(&str,
					    "1 day %d:%02d:%02d.%02d",
					    hours, min, sec, csec) == -1)
						goto fail;
				} else {
					if (asprintf(&str,
					    "%d day %d:%02d:%02d.%02d",
					    days, hours, min, sec, csec) == -1)
						goto fail;
				}
			}
			break;
		}
		hint = "INTEGER: ";
		if (object != NULL && object->o_textconv != NULL &&
		    object->o_textconv->tc_syntax == root->be_encoding)
			return smi_displayhint_int(object->o_textconv,
			    print_hint, v);
		if (root->be_class == BER_CLASS_APPLICATION) {
			if (root->be_type == SNMP_T_COUNTER32)
				hint = "Counter32: ";
			else if (root->be_type == SNMP_T_GAUGE32)
				hint = "Gauge32: ";
			else if (root->be_type == SNMP_T_OPAQUE)
				hint = "Opaque: ";
			else if (root->be_type == SNMP_T_COUNTER64)
				hint = "Counter64: ";
		}
		if (asprintf(&str, "%s%lld", print_hint ? hint : "", v) == -1)
			goto fail;
		break;
	case BER_TYPE_BITSTRING:
		if (ober_get_bitstring(root, (void *)&buf, &len) == -1)
			goto fail;
		slen = len * 2 + 1 + sizeof("BITS: ");
		if ((str = calloc(1, slen)) == NULL)
			goto fail;
		p = str;
		if (print_hint) {
			strlcpy(str, "BITS: ", slen);
			p += sizeof("BITS: ");
		}
		for (i = 0; i < len; i++) {
			snprintf(p, 3, "%02x", buf[i]);
			p += 2;
		}
		break;
	case BER_TYPE_OBJECT:
		if (ober_get_oid(root, &o) == -1)
			goto fail;
		if (asprintf(&str, "%s%s",
		    print_hint ? "OID: " : "",
		    smi_oid2string(&o, strbuf, sizeof(strbuf), lookup)) == -1)
			goto fail;
		break;
	case BER_TYPE_OCTETSTRING:
		if (ober_get_string(root, &buf) == -1)
			goto fail;
		if (root->be_class == BER_CLASS_APPLICATION &&
		    root->be_type == SNMP_T_IPADDR) {
			if (asprintf(&str, "%s%s",
			    print_hint ? "IpAddress: " : "",
			    inet_ntoa(*(struct in_addr *)buf)) == -1)
				goto fail;
		} else if (root->be_class == BER_CLASS_CONTEXT) {
			if (root->be_type == SNMP_E_NOSUCHOBJECT)
				str = strdup("No Such Object available on this "
				    "agent at this OID");
			else if (root->be_type == SNMP_E_NOSUCHINSTANCE)
				str = strdup("No Such Instance currently "
				    "exists at this OID");
			else if (root->be_type == SNMP_E_ENDOFMIB)
				str = strdup("No more variables left in this "
				    "MIB View (It is past the end of the MIB "
				    "tree)");
			else
				str = strdup("Unknown status at this OID");
		} else {
			if (object != NULL && object->o_textconv != NULL &&
			    object->o_textconv->tc_syntax == root->be_encoding)
				return smi_displayhint_os(object->o_textconv,
				    print_hint, buf, root->be_len, utf8);
			for (i = 0; i < root->be_len; i++) {
				if (!isprint(buf[i])) {
					if (output_string == smi_os_default)
						output_string = smi_os_hex;
					else if (output_string == smi_os_ascii)
						is_hex = 1;
					break;
				}
			}
			/*
			 * hex is 3 * n (2 digits + n - 1 spaces + NUL-byte)
			 * ascii can be max (2 * n) + 2 quotes + NUL-byte
			 */
			len = output_string == smi_os_hex ? 3 : 2;
			p = str = reallocarray(NULL, root->be_len + 2, len);
			if (p == NULL)
				goto fail;
			len *= root->be_len + 2;
			if (is_hex) {
				*str++ = '"';
				len--;
			}
			for (i = 0; i < root->be_len; i++) {
				switch (output_string) {
				case smi_os_default:
					/* FALLTHROUGH */
				case smi_os_ascii:
					/*
					 * There's probably more edgecases here,
					 * not fully investigated
					 */
					if (len < 2)
						goto fail;
					if (is_hex && buf[i] == '\\') {
						*str++ = '\\';
						len--;
					}
					*str++ = isprint(buf[i]) ? buf[i] : '.';
					len--;
					break;
				case smi_os_hex:
					ret = snprintf(str, len, "%s%02hhX",
					    i == 0 ? "" :
					    i % 16 == 0 ? "\n" : " ", buf[i]);
					if (ret == -1 || ret > (int) len)
						goto fail;
					len -= ret;
					str += ret;
					break;
				}
			}
			if (is_hex) {
				if (len < 2)
					goto fail;
				*str++ = '"';
				len--;
			}
			if (len == 0)
				goto fail;
			*str = '\0';
			str = NULL;
			if (asprintf(&str, "%s%s",
			    print_hint ?
			    output_string == smi_os_hex ? "Hex-STRING: " :
			    "STRING: " :
			    "", p) == -1) {
				free(p);
				goto fail;
			}
			free(p);
		}
		break;
	case BER_TYPE_NULL:	/* no payload */
	case BER_TYPE_EOC:
	case BER_TYPE_SEQUENCE:
	case BER_TYPE_SET:
	default:
		str = strdup("");
		break;
	}

	return (str);

 fail:
	free(str);
	return (NULL);
}

int
smi_string2oid(const char *oidstr, struct ber_oid *o)
{
	char			*sp, *p, str[BUFSIZ];
	const char		*errstr;
	struct oid		*oid;
	struct ber_oid		 ko;

	if (strlcpy(str, oidstr, sizeof(str)) >= sizeof(str))
		return (-1);
	bzero(o, sizeof(*o));

	/*
	 * Parse OID strings in the common form n.n.n or n-n-n.
	 * Based on ober_string2oid with additional support for symbolic names.
	 */
	p = sp = str[0] == '.' ? str + 1 : str;
	for (; p != NULL; sp = p) {
		if ((p = strpbrk(p, ".-")) != NULL)
			*p++ = '\0';
		if ((oid = smi_findkey(sp)) != NULL) {
			bcopy(&oid->o_id, &ko, sizeof(ko));
			if (o->bo_n && ober_oid_cmp(&ko, o) != 2)
				return (-1);
			bcopy(&ko, o, sizeof(*o));
			errstr = NULL;
		} else {
			o->bo_id[o->bo_n++] =
			    strtonum(sp, 0, UINT_MAX, &errstr);
		}
		if (errstr || o->bo_n > BER_MAX_OID_LEN)
			return (-1);
	}

	return (0);
}

unsigned int
smi_application(struct ber_element *elm)
{
	if (elm->be_class != BER_CLASS_APPLICATION)
		return (BER_TYPE_OCTETSTRING);

	switch (elm->be_type) {
	case SNMP_T_IPADDR:
		return (BER_TYPE_OCTETSTRING);
	case SNMP_T_COUNTER32:
	case SNMP_T_GAUGE32:
	case SNMP_T_TIMETICKS:
	case SNMP_T_OPAQUE:
	case SNMP_T_COUNTER64:
		return (BER_TYPE_INTEGER);
	default:
		break;
	}
	return (BER_TYPE_OCTETSTRING);

}

char *
smi_oid2string(struct ber_oid *o, char *buf, size_t len,
    enum smi_oid_lookup lookup)
{
	char		 str[256];
	struct oid	*value, key;
	size_t		 i;

	bzero(buf, len);
	bzero(&key, sizeof(key));
	bcopy(o, &key.o_id, sizeof(struct ber_oid));

	for (i = 0; i < o->bo_n; i++) {
		key.o_oidlen = i + 1;
		if (lookup != smi_oidl_numeric &&
		    (value = RB_FIND(oidtree, &smi_oidtree, &key)) != NULL) {
			snprintf(str, sizeof(str), "%s", value->o_name);
			if (lookup == smi_oidl_short && i + 1 < o->bo_n) {
				key.o_oidlen = i + 2;
				if (RB_FIND(oidtree, &smi_oidtree, &key) != NULL)
					continue;
			}
		} else
			snprintf(str, sizeof(str), "%u", key.o_oid[i]);
		if (*buf != '\0' || i == 0)
			strlcat(buf, ".", len);
		strlcat(buf, str, len);
	}

	return (buf);
}

void
smi_mibtree(struct oid *oids)
{
	size_t		 i;

	for (i = 0; oids[i].o_name != NULL; i++) {
		RB_INSERT(oidtree, &smi_oidtree, &(oids[i]));
		RB_INSERT(keytree, &smi_keytree, &(oids[i]));
	}
}

void
smi_textconvtree(struct textconv *textconvs)
{
	size_t		 i = 0;

	for (i = 0; textconvs[i].tc_name != NULL; i++)
		RB_INSERT(textconvtree, &smi_tctree, &(textconvs[i]));
}

struct oid *
smi_findkey(char *name)
{
	struct oid	oid;
	if (name == NULL)
		return (NULL);
	oid.o_name = name;
	return (RB_FIND(keytree, &smi_keytree, &oid));
}

struct oid *
smi_foreach(struct oid *oid)
{
	/*
	 * Traverse the tree of MIBs with the option to check
	 * for specific OID flags.
	 */
	if (oid == NULL)
		return RB_MIN(oidtree, &smi_oidtree);
	return RB_NEXT(oidtree, &smi_oidtree, oid);
}

char *
smi_displayhint_int(struct textconv *tc, int print_hint, long long v)
{
	size_t i;
	char *rbuf;

	for (i = 0; tc->tc_enum[i].tce_name != NULL; i++) {
		if (tc->tc_enum[i].tce_number == v) {
			if (print_hint) {
				if (asprintf(&rbuf, "INTEGER: %s(%lld)",
				    tc->tc_enum[i].tce_name, v) == -1)
					return NULL;
			} else {
				if (asprintf(&rbuf, "%s",
				    tc->tc_enum[i].tce_name) == -1)
					return NULL;
			}
			return rbuf;
		}
	}
	if (asprintf(&rbuf, "%s%lld", print_hint ? "INTEGER: " : "", v) == -1)
		return NULL;
	return rbuf;
}

#define REPLACEMENT "\357\277\275"
char *
smi_displayhint_os(struct textconv *tc, int print_hint, const char *src,
    size_t srclen, int utf8)
{
	size_t octetlength, i = 0, j = 0;
	size_t prefixlen;
	unsigned long ulval;
	int clen;
	char *displayformat;
	const char *prefix;
	char *rbuf, *dst;
	wchar_t wc;

	prefix = print_hint ? "STRING: " : "";
	prefixlen = strlen(prefix);

	errno = 0;
	ulval = strtoul(tc->tc_display_hint, &displayformat, 10);
	octetlength = ulval;
	if (!isdigit(tc->tc_display_hint[0]) ||
	    (errno != 0 && (ulval == 0 || ulval == ULONG_MAX)) ||
	    (unsigned long) octetlength != ulval) {
		errno = EINVAL;
		return NULL;
	}
		
	if (displayformat[0] == 't' || displayformat[0] == 'a') {
		if ((rbuf = malloc(prefixlen + octetlength + 1)) == NULL)
			return NULL;
		(void)strlcpy(rbuf, prefix, prefixlen + octetlength + 1);
		dst = rbuf + prefixlen;
		while (j < octetlength && i < srclen) {
			clen = mbtowc(&wc, &(src[i]), srclen - i);
			if (displayformat[0] == 'a' && clen > 1)
				clen = -1;
			switch (clen) {
			case 0:
				dst[j++] = '.';
				i++;
				break;
			case -1:
				mbtowc(NULL, NULL, MB_CUR_MAX);
				if (utf8) {
					if (octetlength - j <
					    sizeof(REPLACEMENT) - 1) {
						dst[j] = '\0';
						return rbuf;
					}
					memcpy(&(dst[j]), REPLACEMENT,
					    sizeof(REPLACEMENT) - 1);
					j += sizeof(REPLACEMENT) - 1;
				} else
					dst[j++] = '?';
				i++;
				break;
			default:
				if (!iswprint(wc) || (!utf8 && clen > 1))
					dst[j++] = '.';
				else if (octetlength - j < (size_t)clen) {
					dst[j] = '\0';
					return rbuf;
				} else {
					memcpy(&(dst[j]), &(src[i]), clen);
					j += clen;
				}
				i += clen;
				break;
			}
		}
		dst[j] = '\0';
		return rbuf;
	}
	errno = EINVAL;
	return NULL;
}

int
smi_oid_cmp(struct oid *a, struct oid *b)
{
	size_t	 i;

	for (i = 0; i < MINIMUM(a->o_oidlen, b->o_oidlen); i++) {
		if (a->o_oid[i] != b->o_oid[i])
			return (a->o_oid[i] - b->o_oid[i]);
	}

	return (a->o_oidlen - b->o_oidlen);
}

int
smi_key_cmp(struct oid *a, struct oid *b)
{
	if (a->o_name == NULL || b->o_name == NULL)
		return (-1);
	return (strcasecmp(a->o_name, b->o_name));
}

int
smi_textconv_cmp(struct textconv *a, struct textconv *b)
{
	return strcmp(a->tc_name, b->tc_name);
}

RB_GENERATE(oidtree, oid, o_element, smi_oid_cmp)
RB_GENERATE(keytree, oid, o_keyword, smi_key_cmp)
RB_GENERATE(textconvtree, textconv, tc_entry, smi_textconv_cmp);
