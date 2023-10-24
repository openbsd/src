#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>

#include <ber.h>
#include <err.h>
#include <inttypes.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "regress.h"

#include <stdio.h>

#define SNMP_HOSTNAME "127.0.0.1"
#define SNMP_PORT "161"

#define SNMP_R_COMMUNITY "public"

enum snmp_application {
        APPLICATION_IPADDR = 0,
        APPLICATION_COUNTER32 = 1,
        APPLICATION_GAUGE32 = 2,
        APPLICATION_UNSIGNED32 = 2,
        APPLICATION_TIMETICKS = 3,
        APPLICATION_OPAQUE = 4,
        APPLICATION_NSAPADDR = 5,
        APPLICATION_COUNTER64 = 6,
};

enum snmp_exception {
	EXCEPTION_NOSUCHOBJECT = 0,
	EXCEPTION_NOSUCHINSTANCE = 1,
	EXCEPTION_ENDOFMIBVIEW = 2
};

enum snmp_request {
	REQUEST_GET = 0,
	REQUEST_GETNEXT = 1,
	REQUEST_RESPONSE = 2,
	REQUEST_SET = 3,
	REQUEST_TRAP = 4,
	REQUEST_GETBULK = 5,
	REQUEST_INFORM = 6,
	REQUEST_TRAPV2 = 7,
	REQUEST_REPORT = 8
};

int32_t snmpv2_send(int, const char *, enum snmp_request, int32_t, int32_t,
    int32_t, struct varbind *, size_t);
struct ber_element *snmp_v2_recv(int, int);
struct ber_element *snmp_pdu(enum snmp_request, int32_t, int32_t, int32_t,
    struct varbind *, size_t);
struct ber_element *snmp_varbindlist(struct varbind *, size_t);
struct ber_oid *snmp_oid2ber_oid(struct oid *, struct ber_oid *);
struct ber_element *snmp_data2ber_element(enum type, union data *);
unsigned int smi_application(struct ber_element *);
char *smi_oid2string(struct ber_oid *, char *, size_t);
char *smi_print_element(struct ber_element *);
void smi_debug_elements(struct ber_element *);
struct ber_element *v2cmps(struct ber_element *, const char *);
void snmp_pdu_validate(struct ber_element *, enum snmp_request, int32_t,
    int32_t, int32_t, struct varbind *, size_t);

socklen_t
snmp_resolve(int type, const char *hostname, const char *servname, struct sockaddr *sa)
{
	struct addrinfo hints, *res;
	socklen_t salen;
	int error;

	if (hostname == NULL)
		hostname = SNMP_HOSTNAME;
	if (servname == NULL)
		servname = SNMP_PORT;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = type;

	if ((error = getaddrinfo(hostname, servname, &hints, &res)) != 0)
		errx(1, "getaddrinfo(%s, %s): %s", hostname, servname,
		    gai_strerror(error));

	memcpy(sa, res->ai_addr, res->ai_addrlen);
	salen = res->ai_addrlen;

	freeaddrinfo(res);
	return salen;
}

int
snmp_connect(int type, struct sockaddr *sa, socklen_t salen)
{
	int s;

	if ((s = socket(sa->sa_family, type, 0)) == -1)
		err(1, "socket");
	if (connect(s, sa, salen) == -1)
		err(1, "connect");
	return s;
}

int32_t
snmpv2_get(int s, const char *community, int32_t requestid,
    struct varbind *varbindlist, size_t nvarbind)
{
	return snmpv2_send(s, community, REQUEST_GET, requestid, 0, 0,
	    varbindlist, nvarbind);
}

int32_t
snmpv2_getnext(int s, const char *community, int32_t requestid,
    struct varbind *varbindlist, size_t nvarbind)
{
	return snmpv2_send(s, community, REQUEST_GETNEXT, requestid, 0, 0,
	    varbindlist, nvarbind);
}

int32_t
snmpv2_getbulk(int s, const char *community, int32_t requestid, int32_t nonrep,
    int32_t maxrep, struct varbind *varbindlist, size_t nvarbind)
{
	return snmpv2_send(s, community, REQUEST_GETBULK, requestid, nonrep,
	    maxrep, varbindlist, nvarbind);
}

int32_t
snmpv2_send(int s, const char *community, enum snmp_request request,
    int32_t requestid, int32_t error, int32_t index,
    struct varbind *varbindlist, size_t nvarbind)
{
	struct ber_element *message;
	struct ber ber = {};
	void *buf;
	ssize_t buflen, writelen;

	if (community == NULL)
		community = SNMP_R_COMMUNITY;
	while (requestid == 0) 
		requestid = arc4random();
	message = ober_printf_elements(NULL, "{dse}", 1, community,
	    snmp_pdu(request, requestid, error, index, varbindlist, nvarbind));
	if (message == NULL)
		err(1, NULL);

	if (ober_write_elements(&ber, message) == -1)
		err(1, "ober_write_elements");

	buflen = ober_get_writebuf(&ber, &buf);
	if ((writelen = write(s, buf, buflen)) == -1)
		err(1, "write");
	if (writelen != buflen)
		errx(1, "write: short write");

	if (verbose) {
		printf("SNMP send(%d):\n", s);
		smi_debug_elements(message);
	}
	ober_free_elements(message);
	ober_free(&ber);

	return requestid;
}

void
snmpv2_response_validate(int s, int timeout, const char *community,
    int32_t requestid, int32_t error, int32_t index,
    struct varbind *varbindlist, size_t nvarbind)
{
	struct ber_element *message, *pdu;

	message = snmp_v2_recv(s, timeout);

	if (community == NULL)
		community = SNMP_R_COMMUNITY;
	snmp_pdu_validate(v2cmps(message, community), REQUEST_RESPONSE,
	    requestid, error, index, varbindlist, nvarbind);

	ober_free_elements(message);
}

struct ber_element *
snmp_v2_recv(int s, int timeout)
{
	struct pollfd pfd = {
		.fd = s,
		.events = POLLIN
	};
	char buf[1024];
	struct ber ber = {};
	struct ber_element *message;
	ssize_t n;
	int ret;

	if ((ret = poll(&pfd, 1, timeout)) == -1)
		err(1, "poll");
	if (ret == 0)
		errx(1, "%s: timeout", __func__);
	if ((n = read(s, buf, sizeof(buf))) == -1)
		err(1, "agentx read");

	ober_set_application(&ber, smi_application);
	ober_set_readbuf(&ber, buf, n);

	message = ober_read_elements(&ber, NULL);
	if (verbose) {
		printf("SNMP received(%d):\n", s);
		smi_debug_elements(message);
	}

	ober_free(&ber);

	return message;
}

void
snmp_timeout(int s, int timeout)
{
	int ret;
	struct pollfd pfd = {
		.fd = s,
		.events = POLLIN
	};
	
	if ((ret = poll(&pfd, 1, timeout)) == -1)
		err(1, "poll");
	if (ret != 0)
		errx(1, "%s: unexpected snmp data", __func__);
}

struct ber_element *
snmp_pdu(enum snmp_request request, int32_t requestid, int32_t error,
    int32_t index, struct varbind *varbindlist, size_t nvarbind)
{
	struct ber_element *pdu;

	if ((pdu = ober_printf_elements(NULL, "{tddde}", BER_CLASS_CONTEXT,
	    request, requestid, error, index,
	    snmp_varbindlist(varbindlist, nvarbind))) == NULL)
		err(1, NULL);
	return pdu;
}

struct ber_element *
snmp_varbindlist(struct varbind *varbindlist, size_t nvarbind)
{
	struct ber_element *vblist, *prev = NULL;
	struct ber_oid oid;
	size_t i;

	if ((vblist = prev = ober_add_sequence(NULL)) == NULL)
		err(1, NULL);
	for (i = 0; i < nvarbind; i++) {
		if ((prev = ober_printf_elements(prev, "{Oe}",
		    snmp_oid2ber_oid(&varbindlist[i].name, &oid),
		    snmp_data2ber_element(varbindlist[i].type,
		    &varbindlist[i].data))) == NULL)
			err(1, NULL);
	}

	return vblist;
}

struct ber_oid *
snmp_oid2ber_oid(struct oid *oid, struct ber_oid *boid)
{
	size_t i;

	for (i = 0; i < oid->n_subid; i++)
		boid->bo_id[i] = oid->subid[i];
	boid->bo_n = oid->n_subid;

	return boid;
}

struct ber_element *
snmp_data2ber_element(enum type type, union data *data)
{
	struct ber_element *elm;
	struct ber_oid oid;

	switch (type) {
	case TYPE_INTEGER:
		elm = ober_add_integer(NULL, data->int32);
		break;
	case TYPE_OCTETSTRING:
		elm = ober_add_nstring(NULL, data->octetstring.string,
		    data->octetstring.len);
		break;
	case TYPE_NULL:
		elm = ober_add_null(NULL);
		break;
	case TYPE_OBJECTIDENTIFIER:
		elm = ober_add_oid(NULL, snmp_oid2ber_oid(&data->oid, &oid));
		break;
	case TYPE_IPADDRESS:
		if ((elm = ober_add_nstring(NULL, data->octetstring.string,
		    data->octetstring.len)) == NULL)
			err(1, NULL);
		ober_set_header(elm, BER_CLASS_APPLICATION, APPLICATION_IPADDR);
		break;
	case TYPE_COUNTER32:
		if ((elm = ober_add_integer(NULL, data->uint32)) == NULL)
			err(1, NULL);
		ober_set_header(elm, BER_CLASS_APPLICATION,
		    APPLICATION_COUNTER32);
		break;
	case TYPE_GAUGE32:
		if ((elm = ober_add_integer(NULL, data->uint32)) == NULL)
			err(1, NULL);
		ober_set_header(elm, BER_CLASS_APPLICATION,
		    APPLICATION_GAUGE32);
		break;
	case TYPE_TIMETICKS:
		if ((elm = ober_add_integer(NULL, data->uint32)) == NULL)
			err(1, NULL);
		ober_set_header(elm, BER_CLASS_APPLICATION,
		    APPLICATION_TIMETICKS);
		break;
	case TYPE_OPAQUE:
		if ((elm = ober_add_nstring(NULL, data->octetstring.string,
		    data->octetstring.len)) == NULL)
			err(1, NULL);
		ober_set_header(elm, BER_CLASS_APPLICATION, APPLICATION_OPAQUE);
		break;
	case TYPE_COUNTER64:
		if ((elm = ober_add_integer(NULL, data->uint64)) == NULL)
			err(1, NULL);
		ober_set_header(elm, BER_CLASS_APPLICATION,
		    APPLICATION_COUNTER64);
		break;
	case TYPE_NOSUCHOBJECT:
		if ((elm = ober_add_null(NULL)) == NULL)
			err(1, NULL);
		ober_set_header(elm, BER_CLASS_CONTEXT, EXCEPTION_NOSUCHOBJECT);
		break;
	case TYPE_NOSUCHINSTANCE:
		if ((elm = ober_add_null(NULL)) == NULL)
			err(1, NULL);
		ober_set_header(elm, BER_CLASS_CONTEXT,
		    EXCEPTION_NOSUCHINSTANCE);
		break;
	case TYPE_ENDOFMIBVIEW:
		if ((elm = ober_add_null(NULL)) == NULL)
			err(1, NULL);
		ober_set_header(elm, BER_CLASS_CONTEXT, EXCEPTION_ENDOFMIBVIEW);
		break;
	default:
		errx(1, "%s: unsupported type: %d", __func__, type);
	}
	if (elm == NULL)
		err(1, NULL);
	return elm;
}

unsigned int
smi_application(struct ber_element *elm)
{
	if (elm->be_class != BER_CLASS_APPLICATION)
		return -1;

	switch (elm->be_type) {
	case APPLICATION_IPADDR:
	case APPLICATION_OPAQUE:
		return BER_TYPE_OCTETSTRING;
	case APPLICATION_COUNTER32:
	case APPLICATION_GAUGE32:
	case APPLICATION_TIMETICKS:
	case APPLICATION_COUNTER64:
		return BER_TYPE_INTEGER;
	default:
		return -1;
	}
}

char *
smi_oid2string(struct ber_oid *oid, char *buf, size_t len)
{
	char digit[11];
	size_t i;

	buf[0] = '\0';
	for (i = 0; i < oid->bo_n; i++) {
		snprintf(digit, sizeof(digit), "%"PRIu32, oid->bo_id[i]);
		if (i > 0)
			strlcat(buf, ".", len);
		strlcat(buf, digit, len);
	}
	return buf;
}

char *
smi_print_element(struct ber_element *root)
{
	char		*str = NULL, *buf, *p;
	long long	 v;
	struct ber_oid	 o;
	char		 strbuf[BUFSIZ];

	switch (root->be_class) {
	case BER_CLASS_UNIVERSAL:
		switch (root->be_type) {
		case BER_TYPE_INTEGER:
			if (ober_get_integer(root, &v) == -1)
				goto fail;
			if (asprintf(&str, "%lld", v) == -1)
				goto fail;
			break;
		case BER_TYPE_OBJECT:
			if (ober_get_oid(root, &o) == -1)
				goto fail;
			if (asprintf(&str, "%s", smi_oid2string(&o, strbuf,
			    sizeof(strbuf))) == -1)
				goto fail;
			break;
		case BER_TYPE_OCTETSTRING:
			if (ober_get_string(root, &buf) == -1)
				goto fail;
			p = reallocarray(NULL, 4, root->be_len + 1);
			if (p == NULL)
				goto fail;
			strvisx(p, buf, root->be_len, VIS_NL);
			if (asprintf(&str, "\"%s\"", p) == -1) {
				free(p);
				goto fail;
			}
			free(p);
			break;
		case BER_TYPE_NULL:
			if (asprintf(&str, "null") == -1)
				goto fail;
			break;
		default:
			/* Should not happen in a valid SNMP packet */
			if (asprintf(&str, "[U/%u]", root->be_type) == -1)
				goto fail;
			break;
		}
		break;
	case BER_CLASS_APPLICATION:
		switch (root->be_type) {
		case APPLICATION_IPADDR:
			if (ober_get_string(root, &buf) == -1)
				goto fail;
			if (asprintf(&str, "%s",
			    inet_ntoa(*(struct in_addr *)buf)) == -1)
					goto fail;
			break;
		case APPLICATION_COUNTER32:
			if (ober_get_integer(root, &v) == -1)
				goto fail;
			if (asprintf(&str, "%lld(c32)", v) == -1)
				goto fail;
			break;
		case APPLICATION_GAUGE32:
			if (ober_get_integer(root, &v) == -1)
				goto fail;
			if (asprintf(&str, "%lld(g32)", v) == -1)
				goto fail;
			break;
		case APPLICATION_TIMETICKS:
			if (ober_get_integer(root, &v) == -1)
				goto fail;
			if (asprintf(&str, "%lld.%llds", v/100, v%100) == -1)
				goto fail;
			break;
		case APPLICATION_OPAQUE:
			if (ober_get_string(root, &buf) == -1)
				goto fail;
			p = reallocarray(NULL, 4, root->be_len + 1);
			if (p == NULL)
				goto fail;
			strvisx(p, buf, root->be_len, VIS_NL);
			if (asprintf(&str, "\"%s\"(opaque)", p) == -1) {
				free(p);
				goto fail;
			}
			free(p);
			break;
		case APPLICATION_COUNTER64:
			if (ober_get_integer(root, &v) == -1)
				goto fail;
			if (asprintf(&str, "%lld(c64)", v) == -1)
				goto fail;
			break;
		default:
			/* Should not happen in a valid SNMP packet */
			if (asprintf(&str, "[A/%u]", root->be_type) == -1)
				goto fail;
			break;
		}
		break;
	case BER_CLASS_CONTEXT:
		switch (root->be_type) {
		case EXCEPTION_NOSUCHOBJECT:
			str = strdup("noSuchObject");
			break;
		case EXCEPTION_NOSUCHINSTANCE:
			str = strdup("noSuchInstance");
			break;
		case EXCEPTION_ENDOFMIBVIEW:
			str = strdup("endOfMibView");
			break;
		default:
			/* Should not happen in a valid SNMP packet */
			if (asprintf(&str, "[C/%u]", root->be_type) == -1)
				goto fail;
			break;
		}
		break;
	default:
		/* Should not happen in a valid SNMP packet */
		if (asprintf(&str, "[%hhu/%u]", root->be_class,
		    root->be_type) == -1)
			goto fail;
		break;
	}

	return (str);

 fail:
	free(str);
	return (NULL);
}

void
smi_debug_elements(struct ber_element *root)
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
		case APPLICATION_IPADDR:
			fprintf(stderr, "ipaddr");
			break;
		case APPLICATION_COUNTER32:
			fprintf(stderr, "counter32");
			break;
		case APPLICATION_GAUGE32:
			fprintf(stderr, "gauge32");
			break;
		case APPLICATION_TIMETICKS:
			fprintf(stderr, "timeticks");
			break;
		case APPLICATION_OPAQUE:
			fprintf(stderr, "opaque");
			break;
		case APPLICATION_COUNTER64:
			fprintf(stderr, "counter64");
			break;
		}
		break;
	case BER_CLASS_CONTEXT:
		fprintf(stderr, "class: context(%u) type: ",
		    root->be_class);
		switch (root->be_type) {
		case REQUEST_GET:
			fprintf(stderr, "getreq");
			break;
		case REQUEST_GETNEXT:
			fprintf(stderr, "getnextreq");
			break;
		case REQUEST_RESPONSE:
			fprintf(stderr, "response");
			break;
		case REQUEST_SET:
			fprintf(stderr, "setreq");
			break;
		case REQUEST_TRAP:
			fprintf(stderr, "trap");
			break;
		case REQUEST_GETBULK:
			fprintf(stderr, "getbulkreq");
			break;
		case REQUEST_INFORM:
			fprintf(stderr, "informreq");
			break;
		case REQUEST_TRAPV2:
			fprintf(stderr, "trapv2");
			break;
		case REQUEST_REPORT:
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

	if ((value = smi_print_element(root)) == NULL)
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
		    root->be_type == APPLICATION_IPADDR) {
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
		smi_debug_elements(root->be_sub);
		indent -= 2;
	}
	if (root->be_next)
		smi_debug_elements(root->be_next);
}

struct ber_element *
v2cmps(struct ber_element *message, const char *community)
{
	int version;
	const char *mcommunity;
	struct ber_element *pdu;

	if (ober_scanf_elements(message, "{dse}$", &version, &mcommunity, &pdu) == -1)
		err(1, "%s: ober_scanf_elements", __func__);

	if (strcmp(mcommunity, community) != 0)
		errx(1, "%s: invalid community (%s/%s)", __func__, mcommunity,
		    community);

	return pdu;
}

void
snmp_pdu_validate(struct ber_element *pdu, enum snmp_request request,
    int32_t requestid, int32_t error, int32_t index, struct varbind *varbindlist,
    size_t nvarbind)
{
	int32_t mrequestid, merror, mindex;
	int class;
	unsigned int type;
	struct ber_element *mvarbindlist, *varbind;
	struct ber_oid moid, oid;
	struct ber_element *value;
	char oidbuf1[512], oidbuf2[512];
	size_t i;

	if (ober_scanf_elements(pdu, "t{ddd{e}$}$", &class, &type, &mrequestid,
	    &merror, &mindex, &varbind) == -1)
		err(1, "%s: ober_scanf_elementsi", __func__);

	if (class != BER_CLASS_CONTEXT || type != request)
		errx(1, "%s: unexpected pdu type", __func__);
	if (mrequestid != requestid)
		errx(1, "%s: unexpected pdu requestid (%d/%d)",
		    __func__, mrequestid, requestid);
	if (error != merror)
		errx(1, "%s: unexpected pdu error (%d/%d)",
		    __func__, merror, error);
	if (index != mindex)
		errx(1, "%s: unepxected pdu index (%d/%d)",
		    __func__, mindex, index);

	for (i = 0; varbind != NULL; varbind = varbind->be_next, i++) {
		if (ober_scanf_elements(varbind, "{oeS$}", &moid, &value) == -1)
			err(1, "%s: ober_scanf_elements", __func__);
		if (i >= nvarbind)
			continue;
		snmp_oid2ber_oid(&varbindlist[i].name, &oid);
		if (ober_oid_cmp(&moid, &oid) != 0)
			errx(1, "%s: unexpected oid (%s/%s)", __func__,
			    smi_oid2string(&moid, oidbuf1, sizeof(oidbuf1)),
			    smi_oid2string(&oid, oidbuf2, sizeof(oidbuf2)));
		switch (varbindlist[i].type) {
		case TYPE_INTEGER:
			if (value->be_class != BER_CLASS_UNIVERSAL ||
			    value->be_type != BER_TYPE_INTEGER ||
			    value->be_numeric != varbindlist[i].data.int32)
				errx(1, "%s: unexpected value", __func__);
			break;
		case TYPE_OCTETSTRING:
			if (value->be_class != BER_CLASS_UNIVERSAL ||
			    value->be_type != BER_TYPE_OCTETSTRING ||
			    value->be_len !=
			    varbindlist[i].data.octetstring.len ||
			    memcmp(varbindlist[i].data.octetstring.string,
			    value->be_val, value->be_len) != 0)
				errx(1, "%s: unexpected value", __func__);
			break;
		case TYPE_NULL:
			if (value->be_class != BER_CLASS_UNIVERSAL ||
			    value->be_type != BER_TYPE_NULL)
				errx(1, "%s: unexpected value", __func__);
			break;
		case TYPE_OBJECTIDENTIFIER:
			if (value->be_class != BER_CLASS_UNIVERSAL ||
			    value->be_type != BER_TYPE_OBJECT)
				errx(1, "%s: unexpected value", __func__);
			if (ober_get_oid(value, &moid) == -1)
				errx(1, "%s: unexpected value", __func__);
			snmp_oid2ber_oid(&varbindlist[i].data.oid, &oid);
			if (ober_oid_cmp(&oid, &moid) != 0)
				errx(1, "%s: unexpected value", __func__);
			break;
		case TYPE_IPADDRESS:
			if (value->be_class != BER_CLASS_APPLICATION ||
			    value->be_type != APPLICATION_IPADDR ||
			    value->be_len !=
			    varbindlist[i].data.octetstring.len ||
			    memcmp(varbindlist[i].data.octetstring.string,
			    value->be_val, value->be_len) != 0)
				errx(1, "%s: unexpected value", __func__);
			break;
		case TYPE_COUNTER32:
			if (value->be_class != BER_CLASS_APPLICATION ||
			    value->be_type != APPLICATION_COUNTER32 ||
			    value->be_numeric != varbindlist[i].data.uint32)
				errx(1, "%s: unexpected value", __func__);
			break;
		case TYPE_GAUGE32:
			if (value->be_class != BER_CLASS_APPLICATION ||
			    value->be_type != APPLICATION_GAUGE32 ||
			    value->be_numeric != varbindlist[i].data.uint32)
				errx(1, "%s: unexpected value", __func__);
			break;
		case TYPE_TIMETICKS:
			if (value->be_class != BER_CLASS_APPLICATION ||
			    value->be_type != APPLICATION_TIMETICKS ||
			    value->be_numeric != varbindlist[i].data.uint32)
				errx(1, "%s: unexpected value", __func__);
			break;
		case TYPE_OPAQUE:
			if (value->be_class != BER_CLASS_APPLICATION ||
			    value->be_type != APPLICATION_OPAQUE ||
			    value->be_len !=
			    varbindlist[i].data.octetstring.len ||
			    memcmp(varbindlist[i].data.octetstring.string,
			    value->be_val, value->be_len) != 0)
				errx(1, "%s: unexpected value", __func__);
			break;
		case TYPE_COUNTER64:
			if (value->be_class != BER_CLASS_APPLICATION ||
			    value->be_type != APPLICATION_COUNTER64 ||
			    value->be_numeric != varbindlist[i].data.uint64)
				errx(1, "%s: unexpected value", __func__);
			break;
		case TYPE_NOSUCHOBJECT:
			if (value->be_class != BER_CLASS_CONTEXT ||
			    value->be_type != EXCEPTION_NOSUCHOBJECT)
				errx(1, "%s: unexpected value", __func__);
			break;
		case TYPE_NOSUCHINSTANCE:
			if (value->be_class != BER_CLASS_CONTEXT ||
			    value->be_type != EXCEPTION_NOSUCHINSTANCE)
				errx(1, "%s: unexpected value", __func__);
			break;
		case TYPE_ENDOFMIBVIEW:
			if (value->be_class != BER_CLASS_CONTEXT ||
			    value->be_type != EXCEPTION_ENDOFMIBVIEW)
				errx(1, "%s: unexpected value", __func__);
			break;
		default:
			errx(1, "%s: unexpected type", __func__);
		}
	}
	if (i != nvarbind)
		errx(1, "%s: unexpected amount of varbind (%zu/%zu)", __func__,
		    i, nvarbind);
}
