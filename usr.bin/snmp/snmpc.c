/*	$OpenBSD: snmpc.c,v 1.17 2019/10/26 19:34:15 martijn Exp $	*/

/*
 * Copyright (c) 2019 Martijn van Duren <martijn@openbsd.org>
 * Copyright (c) 2013 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <arpa/inet.h>
#include <openssl/evp.h>

#include <ber.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smi.h"
#include "snmp.h"
#include "usm.h"

#define GETOPT_COMMON		"A:a:c:E:e:K:k:l:n:O:r:t:u:v:X:x:Z:"

int snmpc_get(int, char *[]);
int snmpc_walk(int, char *[]);
int snmpc_set(int, char *[]);
int snmpc_trap(int, char *[]);
int snmpc_mibtree(int, char *[]);
struct snmp_agent *snmpc_connect(char *, char *);
int snmpc_parseagent(char *, char *);
int snmpc_print(struct ber_element *);
__dead void snmpc_printerror(enum snmp_error, struct ber_element *, int,
    const char *);
char *snmpc_hex2bin(char *, size_t *);
struct ber_element *snmpc_varbindparse(int, char *[]);
void usage(void);

struct snmp_app {
	const char *name;
	const int usecommonopt;
	const char *optstring;
	const char *usage;
	int (*exec)(int, char *[]);
};

struct snmp_app snmp_apps[] = {
	{ "get", 1, NULL, "agent oid ...", snmpc_get },
	{ "getnext", 1, NULL, "agent oid ...", snmpc_get },
	{ "walk", 1, "C:", "[-C cIipt] [-C E endoid] agent [oid]", snmpc_walk },
	{ "bulkget", 1, "C:", "[-C n<nonrep>r<maxrep>] agent oid ...", snmpc_get },
	{ "bulkwalk", 1, "C:", "[-C cipn<nonrep>r<maxrep>] agent [oid]", snmpc_walk },
	{ "set", 1, NULL, "agent oid type value [oid type value] ...", snmpc_set },
	{ "trap", 1, NULL, "agent uptime oid [oid type value] ...", snmpc_trap },
	{ "mibtree", 0, "O:", "[-O fnS]", snmpc_mibtree }
};
struct snmp_app *snmp_app = NULL;

char *community = "public";
struct snmp_v3 *v3;
char *mib = "mib_2";
int retries = 5;
int timeout = 1;
enum snmp_version version = SNMP_V2C;
int print_equals = 1;
int print_varbind_only = 0;
int print_summary = 0;
int print_time = 0;
int walk_check_increase = 1;
int walk_fallback_oid = 1;
int walk_include_oid = 0;
int smi_print_hint = 1;
int non_repeaters = 0;
int max_repetitions = 10;
struct ber_oid walk_end = {{0}, 0};
enum smi_oid_lookup oid_lookup = smi_oidl_short;
enum smi_output_string output_string = smi_os_default;

int
main(int argc, char *argv[])
{
	const EVP_MD *md = NULL;
	const EVP_CIPHER *cipher = NULL;
	struct snmp_sec *sec;
	char *user = NULL;
	enum usm_key_level authkeylevel;
	char *authkey = NULL;
	size_t authkeylen = 0;
	enum usm_key_level privkeylevel;
	char *privkey = NULL;
	size_t privkeylen = 0;
	int seclevel = SNMP_MSGFLAG_REPORT;
	char *ctxname = NULL;
	char *ctxengineid = NULL, *secengineid = NULL;
	size_t ctxengineidlen, secengineidlen;
	int zflag = 0;
	long long boots, time;
	char optstr[BUFSIZ];
	const char *errstr;
	char *strtolp;
	int ch;
	size_t i;

	if (pledge("stdio inet dns", NULL) == -1)
		err(1, "pledge");

	if (argc <= 1)
		usage();

	optstr[0] = '\0';
	for (i = 0; i < sizeof(snmp_apps)/sizeof(*snmp_apps); i++) {
		if (strcmp(snmp_apps[i].name, argv[1]) == 0) {
			snmp_app = &snmp_apps[i];
			if (snmp_app->optstring != NULL) {
				if (strlcpy(optstr, snmp_app->optstring,
				    sizeof(optstr)) > sizeof(optstr))
					errx(1, "strlcat");
			}
			break;
		}
	}
	if (snmp_app == NULL)
		usage();

	if (snmp_app->usecommonopt) {
		if (strlcat(optstr, GETOPT_COMMON, sizeof(optstr)) >
		    sizeof(optstr))
			errx(1, "strlcpy");
	}

	argc--;
	argv++;

	smi_init();

	while ((ch = getopt(argc, argv, optstr)) != -1) {
		switch (ch) {
		case 'A':
			authkey = optarg;
			authkeylen = strlen(authkey);
			authkeylevel = USM_KEY_PASSWORD;
			break;
		case 'a':
			if (strcasecmp(optarg, "MD5") == 0)
				md = EVP_md5();
			else if (strcasecmp(optarg, "SHA") == 0)
				md = EVP_sha1();
			else if (strcasecmp(optarg, "SHA-224") == 0)
				md = EVP_sha224();
			else if (strcasecmp(optarg, "SHA-256") == 0)
				md = EVP_sha256();
			else if (strcasecmp(optarg, "SHA-384") == 0)
				md = EVP_sha384();
			else if (strcasecmp(optarg, "SHA-512") == 0)
				md = EVP_sha512();
			else
				errx(1, "Invalid authentication protocol "
				    "specified after -a flag: %s", optarg);
			break;
		case 'c':
			community = optarg;
			break;
		case 'E':
			ctxengineid = snmpc_hex2bin(optarg,
			    &ctxengineidlen);
			if (ctxengineid == NULL) {
				if (errno == EINVAL)
					errx(1, "Bad engine ID value "
					    "after -3E flag.");
				err(1, "-3E");
			}
			break;
		case 'e':
			secengineid = snmpc_hex2bin(optarg,
			    &secengineidlen);
			if (secengineid == NULL) {
				if (errno == EINVAL)
					errx(1, "Bad engine ID value "
					    "after -3e flag.");
				err(1, "-3e");
			}
			break;
		case 'K':
			privkey = snmpc_hex2bin(optarg, &privkeylen);
			if (privkey == NULL) {
				if (errno == EINVAL)
					errx(1, "Bad key value after "
					    "-3K flag.");
				errx(1, "-3K");
			}
			privkeylevel = USM_KEY_LOCALIZED;
				break;
		case 'k':
			authkey = snmpc_hex2bin(optarg, &authkeylen);
			if (authkey == NULL) {
				if (errno == EINVAL)
					errx(1, "Bad key value after -k flag.");
				err(1, "-k");
			}
			authkeylevel = USM_KEY_LOCALIZED;
			break;
		case 'l':
			if (strcasecmp(optarg, "noAuthNoPriv") == 0)
				seclevel = SNMP_MSGFLAG_REPORT;
			else if (strcasecmp(optarg, "authNoPriv") == 0)
				seclevel = SNMP_MSGFLAG_AUTH |
				    SNMP_MSGFLAG_REPORT;
			else if (strcasecmp(optarg, "authPriv") == 0)
				seclevel = SNMP_MSGFLAG_AUTH |
				    SNMP_MSGFLAG_PRIV | SNMP_MSGFLAG_REPORT;
			else
				errx(1, "Invalid security level specified "
				    "after -l flag: %s", optarg);
			break;
		case 'n':
			ctxname = optarg;
			break;
		case 'r':
			if ((retries = strtonum(optarg, 0, INT_MAX,
			    &errstr)) == 0) {
				if (errstr != NULL)
					errx(1, "-r: %s argument", errstr);
			}
			break;
		case 't':
			if ((timeout = strtonum(optarg, 1, INT_MAX,
			    &errstr)) == 0) {
				if (errstr != NULL)
					errx(1, "-t: %s argument", errstr);
			}
			break;
		case 'u':
			user = optarg;
			break;
		case 'v':
			if (strcmp(optarg, "1") == 0)
				version = SNMP_V1;
			else if (strcmp(optarg, "2c") == 0)
				version = SNMP_V2C;
			else if (strcmp(optarg, "3") == 0)
				version = SNMP_V3;
			else
				errc(1, EINVAL, "-v");
			break;
		case 'C':
			for (i = 0; i < strlen(optarg); i++) {
				switch (optarg[i]) {
				case 'c':
					if (strcmp(snmp_app->name, "walk") &&
					    strcmp(snmp_app->name, "bulkwalk"))
						usage();
					walk_check_increase = 0;
					break;
				case 'i':
					if (strcmp(snmp_app->name, "walk") &&
					    strcmp(snmp_app->name, "bulkwalk"))
						usage();
					walk_include_oid = 1;
					break;
				case 'n':
					if (strcmp(snmp_app->name, "bulkget") &&
					    strcmp(snmp_app->name, "bulkwalk"))
						usage();
					errno = 0;
					non_repeaters = strtol(&optarg[i + 1],
					    &strtolp, 10);
					if (non_repeaters < 0 ||
					    errno == ERANGE) {
						if (non_repeaters < 0)
							errx(1, "%s%s",
							    "-Cn: too small ",
							    "argument");
						else
							errx(1, "%s%s",
							    "-Cn: too large",
							    "argument");
					} else if (&optarg[i + 1] == strtolp)
						errx(1, "-Cn invalid argument");
					i = strtolp - optarg - 1;
					break;
				case 'p':
					if (strcmp(snmp_app->name, "walk") &&
					    strcmp(snmp_app->name, "bulkwalk"))
						usage();
					print_summary = 1;
					break;
				case 'r':
					if (strcmp(snmp_app->name, "bulkget") &&
					    strcmp(snmp_app->name, "bulkwalk"))
						usage();
					errno = 0;
					max_repetitions = strtol(&optarg[i + 1],
					    &strtolp, 10);
					if (max_repetitions < 0 ||
					    errno == ERANGE) {
						if (max_repetitions < 0)
							errx(1, "%s%s",
							    "-Cr: too small ",
							    "argument");
						else
							errx(1, "%s%s",
							    "-Cr: too large",
							    "argument");
					} else if (&optarg[i + 1] == strtolp)
						errx(1, "-Cr invalid argument");
					i = strtolp - optarg - 1;
					break;
				case 't':
					if (strcmp(snmp_app->name, "walk"))
						usage();
					print_time = 1;
					break;
				case 'E':
					if (strcmp(snmp_app->name, "walk"))
						usage();
					if (smi_string2oid(argv[optind],
					    &walk_end) != 0)
						errx(1, "%s: %s",
						    "Unknown Object Identifier",
						    argv[optind]);
					optind++;
					continue;
				case 'I':
					if (strcmp(snmp_app->name, "walk"))
						usage();
					walk_fallback_oid = 0;
					break;
				default:
					usage();
				}
				if (optarg[i] == 'E')
					break;
			}
			break;
		case 'O':
			for (i = 0; i < strlen(optarg); i++) {
				if (strcmp(snmp_app->name, "mibtree") == 0 &&
				    optarg[i] != 'f' && optarg[i] != 'n' &&
				    optarg[i] != 'S')
						usage();
				switch (optarg[i]) {
				case 'a':
					output_string = smi_os_ascii;
					break;
				case 'f':
					oid_lookup = smi_oidl_full;
					break;
				case 'n':
					oid_lookup = smi_oidl_numeric;
					break;
				case 'q':
					print_equals = 0;
					smi_print_hint = 0;
					break;
				case 'v':
					print_varbind_only = 1;
					break;
				case 'x':
					output_string = smi_os_hex;
					break;
				case 'S':
					oid_lookup = smi_oidl_short;
					break;
				case 'Q':
					smi_print_hint = 0;
					break;
				default:
					usage();
				}
			}
			break;
		case 'X':
			privkey = optarg;
			privkeylen = strlen(privkey);
			privkeylevel = USM_KEY_PASSWORD;
			break;
		case 'x':
			if (strcasecmp(optarg, "DES") == 0)
				cipher = EVP_des_cbc();
			else if (strcasecmp(optarg, "AES") == 0)
				cipher = EVP_aes_128_cfb128();
			else
				errx(1, "Invalid privacy protocol "
				    "specified after -3x flag: %s",
				    optarg);
			break;
		case 'Z':
			boots = strtoll(optarg, &strtolp, 10);
			if (boots < 0 || strtolp == optarg || strtolp[0] != ',')
				usage();
			strtolp++;
			while (strtolp[0] == ' ' && strtolp[0] == '\t')
				strtolp++;
			time = strtoll(strtolp, &strtolp, 10);
			if (boots < 0 || strtolp == optarg)
				usage();
			zflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (version == SNMP_V3) {
		/* Setup USM */
		if (user == NULL || user[0] == '\0')
			errx(1, "No securityName specified");
		if ((sec = usm_init(user, strlen(user))) == NULL)
			err(1, "usm_init");
		if (seclevel & SNMP_MSGFLAG_AUTH) {
			if (md == NULL)
				md = EVP_md5();
			if (authkey == NULL)
				errx(1, "No authKey or authPassword specified");
			if (usm_setauth(sec, md, authkey, authkeylen,
			    authkeylevel) == -1)
				err(1, "Can't set authkey");
		}
		if (seclevel & SNMP_MSGFLAG_PRIV) {
			if (cipher == NULL)
				cipher = EVP_des_cbc();
			if (privkey == NULL)
				errx(1, "No privKey or privPassword specified");
			if (usm_setpriv(sec, cipher, privkey, privkeylen,
			    privkeylevel) == -1)
				err(1, "Can't set authkey");
		}
		if (secengineid != NULL) {
			if (usm_setengineid(sec, secengineid,
			    secengineidlen) == -1)
				err(1, "Can't set secengineid");
		}
		if (zflag)
			if (usm_setbootstime(sec, boots, time) == -1)
				err(1, "Can't set boots/time");
		v3 = snmp_v3_init(seclevel, ctxname, ctxname == NULL ? 0 :
		    strlen(ctxname), sec);
		if (v3 == NULL)
			err(1, "snmp_v3_init");
		if (ctxengineid != NULL) {
			if (snmp_v3_setengineid(v3, ctxengineid,
			    ctxengineidlen) == -1)
				err(1, "Can't set ctxengineid");
		}
	}


	return snmp_app->exec(argc, argv);
}

int
snmpc_get(int argc, char *argv[])
{
	struct ber_oid *oid;
	struct ber_element *pdu, *varbind;
	struct snmp_agent *agent;
	int errorstatus, errorindex;
	int i;
	int class;
	unsigned type;
	char *hint = NULL;

	if (argc < 2)
		usage();

	if ((agent = snmpc_connect(argv[0], "161")) == NULL)
		err(1, "%s", snmp_app->name);
	agent->timeout = timeout;
	agent->retries = retries;

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");
	argc--;
	argv++;

	oid = reallocarray(NULL, argc, sizeof(*oid));
	if (oid == NULL)
		err(1, "malloc");
	for (i = 0; i < argc; i++) {
		if (smi_string2oid(argv[i], &oid[i]) == -1)
			errx(1, "%s: Unknown object identifier", argv[i]);
	}
	if (strcmp(snmp_app->name, "getnext") == 0) {
		if ((pdu = snmp_getnext(agent, oid, argc)) == NULL)
			err(1, "getnext");
	} else if (strcmp(snmp_app->name, "bulkget") == 0) {
		if (version < SNMP_V2C)
			errx(1, "Cannot send V2 PDU on V1 session");
		if (non_repeaters > argc)
			errx(1, "need more objects than -Cn<num>");
		if ((pdu = snmp_getbulk(agent, oid, argc, non_repeaters,
		    max_repetitions)) == NULL)
			err(1, "bulkget");
	} else {
		if ((pdu = snmp_get(agent, oid, argc)) == NULL)
			err(1, "get");
	}

	(void) ober_scanf_elements(pdu, "t{Sdd{e", &class, &type, &errorstatus,
	    &errorindex, &varbind);
	if (errorstatus != 0) {
		if (errorindex >= 1 && errorindex <= argc)
			hint = argv[errorindex - 1];
		snmpc_printerror((enum snmp_error) errorstatus, varbind,
		    errorindex, hint);
	}

	if (class == BER_CLASS_CONTEXT && type == SNMP_C_REPORT)
		printf("Received report:\n");
	for (; varbind != NULL; varbind = varbind->be_next) {
		if (!snmpc_print(varbind))
			err(1, "Can't print response");
	}
	ober_free_elements(pdu);
	snmp_free_agent(agent);
	return 0;
}

int
snmpc_walk(int argc, char *argv[])
{
	struct ber_oid oid, loid, noid;
	struct ber_element *pdu, *varbind, *value;
	struct timespec start, finish;
	struct snmp_agent *agent;
	const char *oids;
	int n = 0, prev_cmp;
	int errorstatus, errorindex;
	int class;
	unsigned type;

	if (strcmp(snmp_app->name, "bulkwalk") == 0 && version < SNMP_V2C)
		errx(1, "Cannot send V2 PDU on V1 session");
	if (argc < 1 || argc > 2)
		usage();
	oids = argc == 1 ? mib : argv[1];

	if ((agent = snmpc_connect(argv[0], "161"))== NULL)
		err(1, "%s", snmp_app->name);
	agent->timeout = timeout;
	agent->retries = retries;
	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	if (smi_string2oid(oids, &oid) == -1)
		errx(1, "%s: Unknown object identifier", oids);
	bcopy(&oid, &noid, sizeof(noid));
	if (print_time)
		clock_gettime(CLOCK_MONOTONIC, &start);

	if (walk_include_oid) {
		if ((pdu = snmp_get(agent, &oid, 1)) == NULL)
			err(1, "%s", snmp_app->name);

		(void) ober_scanf_elements(pdu, "t{Sdd{e", &class, &type,
		    &errorstatus, &errorindex, &varbind);
		if (errorstatus != 0)
			snmpc_printerror((enum snmp_error) errorstatus, varbind,
			    errorindex, oids);

		if (class == BER_CLASS_CONTEXT && type == SNMP_C_REPORT)
			printf("Received report:\n");
		if (!snmpc_print(varbind))
			err(1, "Can't print response");
		ober_free_element(pdu);
		if (class == BER_CLASS_CONTEXT && type == SNMP_C_REPORT)
			return 1;
		n++;
	}
	while (1) {
		bcopy(&noid, &loid, sizeof(loid));
		if (strcmp(snmp_app->name, "bulkwalk") == 0) {
			if ((pdu = snmp_getbulk(agent, &noid, 1,
			    non_repeaters, max_repetitions)) == NULL)
				err(1, "bulkwalk");
		} else {
			if ((pdu = snmp_getnext(agent, &noid, 1)) == NULL)
				err(1, "walk");
		}

		(void) ober_scanf_elements(pdu, "t{Sdd{e", &class, &type,
		    &errorstatus, &errorindex, &varbind);
		if (errorstatus != 0) {
			snmpc_printerror((enum snmp_error) errorstatus, varbind,
			    errorindex, NULL);
		}

		if (class == BER_CLASS_CONTEXT && type == SNMP_C_REPORT)
			printf("Received report:\n");
		for (; varbind != NULL; varbind = varbind->be_next) {
			(void) ober_scanf_elements(varbind, "{oe}", &noid,
			    &value);
			if (value->be_class == BER_CLASS_CONTEXT &&
			    value->be_type == BER_TYPE_EOC)
				break;
			prev_cmp = ober_oid_cmp(&loid, &noid);
			if (walk_check_increase && prev_cmp == -1)
				errx(1, "OID not increasing");
			if (prev_cmp == 0 || ober_oid_cmp(&oid, &noid) != 2)
				break;
			if (walk_end.bo_n != 0 &&
			    ober_oid_cmp(&walk_end, &noid) != -1)
				break;

			if (!snmpc_print(varbind))
				err(1, "Can't print response");
			n++;
		}
		ober_free_elements(pdu);
		if (class == BER_CLASS_CONTEXT && type == SNMP_C_REPORT)
			return 1;
		if (varbind != NULL)
			break;
	}
	if (walk_fallback_oid && n == 0) {
		if ((pdu = snmp_get(agent, &oid, 1)) == NULL)
			err(1, "%s", snmp_app->name);

		(void) ober_scanf_elements(pdu, "t{Sdd{e", &class, &type,
		    &errorstatus, &errorindex, &varbind);
		if (errorstatus != 0)
			snmpc_printerror((enum snmp_error) errorstatus, varbind,
			    errorindex, oids);

		if (class == BER_CLASS_CONTEXT && type == SNMP_C_REPORT)
			printf("Received report:\n");
		if (!snmpc_print(varbind))
			err(1, "Can't print response");
		ober_free_element(pdu);
		if (class == BER_CLASS_CONTEXT && type == SNMP_C_REPORT)
			return 1;
		n++;
	}
	if (print_time)
		clock_gettime(CLOCK_MONOTONIC, &finish);
	if (print_summary)
		printf("Variables found: %d\n", n);
	if (print_time) {
		if ((finish.tv_nsec -= start.tv_nsec) < 0) {
			finish.tv_sec -= 1;
			finish.tv_nsec += 1000000000;
		}
		finish.tv_sec -= start.tv_sec;
		fprintf(stderr, "Total traversal time: %lld.%09ld seconds\n",
		    finish.tv_sec, finish.tv_nsec);
	}
	snmp_free_agent(agent);
	return 0;
}

int
snmpc_set(int argc, char *argv[])
{
	struct snmp_agent *agent;
	struct ber_element *pdu, *varbind;
	int errorstatus, errorindex;
	int class;
	unsigned type;
	char *hint = NULL;

	if (argc < 4)
		usage();
	if ((agent = snmpc_connect(argv[0], "161")) == NULL)
		err(1, "%s", snmp_app->name);
	argc--;
	argv++;

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	if ((pdu = snmp_set(agent, snmpc_varbindparse(argc, argv))) == NULL)
		err(1, "set");

	(void) ober_scanf_elements(pdu, "t{Sdd{e", &class, &type, &errorstatus,
	    &errorindex, &varbind);
	if (errorstatus != 0) {
		if (errorindex >= 1 && errorindex <= argc / 3)
			hint = argv[(errorindex - 1) * 3];
		snmpc_printerror((enum snmp_error) errorstatus, varbind,
		    errorindex, hint);
	}

	if (class == BER_CLASS_CONTEXT && type == SNMP_C_REPORT)
		printf("Received report:\n");
	for (; varbind != NULL; varbind = varbind->be_next) {
		if (!snmpc_print(varbind))
			err(1, "Can't print response");
	}
	ober_free_elements(pdu);
	snmp_free_agent(agent);
	return 0;
}

int
snmpc_trap(int argc, char *argv[])
{
	struct snmp_agent *agent;
	struct timespec ts;
	struct ber_oid trapoid;
	const char *errstr = NULL;
	long long lval;

	if (version == SNMP_V1)
		errx(1, "trap is not supported for snmp v1");

	if ((agent = snmpc_connect(argv[0], "162")) == NULL)
		err(1, "%s", snmp_app->name);

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	if (argv[1][0] == '\0') {
		if (clock_gettime(CLOCK_UPTIME, &ts) == -1)
			err(1, "clock_gettime");
	} else {
		lval = strtonum(argv[1], 0, LLONG_MAX, &errstr);
		if (errstr != NULL)
			errx(1, "Bad value notation (%s)", argv[1]);
		ts.tv_sec = lval / 100;
		ts.tv_nsec = (lval % 100) * 10000000;
	}
	if (smi_string2oid(argv[2], &trapoid) == -1)
		errx(1, "Invalid oid: %s\n", argv[2]);

	argc -= 3;
	argv += 3;

	snmp_trap(agent, &ts, &trapoid, snmpc_varbindparse(argc, argv));

	return 0;
}

int
snmpc_mibtree(int argc, char *argv[])
{
	struct oid *oid;
	char buf[BUFSIZ];

	for (oid = NULL; (oid = smi_foreach(oid, 0)) != NULL;) {
		smi_oid2string(&oid->o_id, buf, sizeof(buf), oid_lookup);
		printf("%s\n", buf);
	}
	return 0;
}

struct snmp_agent *
snmpc_connect(char *host, char *port)
{
	switch (version) {
	case SNMP_V1:
	case SNMP_V2C:
		return snmp_connect_v12(snmpc_parseagent(host, port), version,
		    community);
	case SNMP_V3:
		return snmp_connect_v3(snmpc_parseagent(host, port), v3);
	}
	return NULL;
}

int
snmpc_print(struct ber_element *elm)
{
	struct ber_oid oid;
	char oids[SNMP_MAX_OID_STRLEN];
	char *value;

	elm = elm->be_sub;
	if (ober_get_oid(elm, &oid) != 0) {
		errno = EINVAL;
		return 0;
	}

	elm = elm->be_next;
	value = smi_print_element(elm, smi_print_hint, output_string, oid_lookup);
	if (value == NULL)
		return 0;

	if (print_varbind_only)
		printf("%s\n", value);
	else if (print_equals) {
		smi_oid2string(&oid, oids, sizeof(oids), oid_lookup);
		printf("%s = %s\n", oids, value);
	} else {
		smi_oid2string(&oid, oids, sizeof(oids), oid_lookup);
		printf("%s %s\n", oids, value);
	}
	free(value);

	return 1;
}

__dead void
snmpc_printerror(enum snmp_error error, struct ber_element *varbind,
    int index, const char *hint)
{
	struct ber_oid hoid, vboid;
	char oids[SNMP_MAX_OID_STRLEN];
	const char *oid = NULL;
	int i;

	if (index >= 1) {
		/* Only print if the index is in the reply */
		for (i = 1; varbind != NULL && i <= index;
		    varbind = varbind->be_next)
			i++;
		if (varbind != NULL &&
		    ober_get_oid(varbind->be_sub, &vboid) == 0) {
			/* If user and reply conform print user input */
			if (hint != NULL &&
			    smi_string2oid(hint, &hoid) == 0 &&
			    ober_oid_cmp(&hoid, &vboid) == 0)
				oid = hint;
			else
				oid = smi_oid2string(&vboid, oids,
				    sizeof(oids), oid_lookup);
		}
	}
	if (oid == NULL)
		oid = "?";

	switch (error) {
	case SNMP_ERROR_NONE:
		errx(1, "No error, how did I get here?");
	case SNMP_ERROR_TOOBIG:
		errx(1, "Can't parse oid %s: Response too big", oid);
	case SNMP_ERROR_NOSUCHNAME:
		errx(1, "Can't parse oid %s: No such object", oid);
	case SNMP_ERROR_BADVALUE:
		errx(1, "Can't parse oid %s: Bad value", oid);
	case SNMP_ERROR_READONLY:
		errx(1, "Can't parse oid %s: Read only", oid);
	case SNMP_ERROR_GENERR:
		errx(1, "Can't parse oid %s: Generic error", oid);
	case SNMP_ERROR_NOACCESS:
		errx(1, "Can't parse oid %s: Access denied", oid);
	case SNMP_ERROR_WRONGTYPE:
		errx(1, "Can't parse oid %s: Wrong type", oid);
	case SNMP_ERROR_WRONGLENGTH:
		errx(1, "Can't parse oid %s: Wrong length", oid);
	case SNMP_ERROR_WRONGENC:
		errx(1, "Can't parse oid %s: Wrong encoding", oid);
	case SNMP_ERROR_WRONGVALUE:
		errx(1, "Can't parse oid %s: Wrong value", oid);
	case SNMP_ERROR_NOCREATION:
		errx(1, "Can't parse oid %s: Can't be created", oid);
	case SNMP_ERROR_INCONVALUE:
		errx(1, "Can't parse oid %s: Inconsistent value", oid);
	case SNMP_ERROR_RESUNAVAIL:
		errx(1, "Can't parse oid %s: Resource unavailable", oid);
	case SNMP_ERROR_COMMITFAILED:
		errx(1, "Can't parse oid %s: Commit failed", oid);
	case SNMP_ERROR_UNDOFAILED:
		errx(1, "Can't parse oid %s: Undo faild", oid);
	case SNMP_ERROR_AUTHERROR:
		errx(1, "Can't parse oid %s: Authorization error", oid);
	case SNMP_ERROR_NOTWRITABLE:
		errx(1, "Can't parse oid %s: Not writable", oid);
	case SNMP_ERROR_INCONNAME:
		errx(1, "Can't parse oid %s: Inconsistent name", oid);
	}
	errx(1, "Can't parse oid %s: Unknown error (%d)", oid, error);
}

int
snmpc_parseagent(char *agent, char *defaultport)
{
	struct addrinfo hints, *ai, *ai0 = NULL;
	struct sockaddr_un saddr;
	char *agentdup, *specifier, *hostname, *port = NULL;
	int error;
	int s;

	if ((agentdup = specifier = strdup(agent)) == NULL)
		err(1, NULL);

	bzero(&hints, sizeof(hints));
	if ((hostname = strchr(specifier, ':')) != NULL) {
		*hostname++ = '\0';
		if (strcasecmp(specifier, "udp") == 0) {
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
		} else if (strcasecmp(specifier, "tcp") == 0) {
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
		} else if (strcasecmp(specifier, "udp6") == 0 ||
		    strcasecmp(specifier, "udpv6") == 0 ||
		    strcasecmp(specifier, "udpipv6") == 0) {
			hints.ai_family = AF_INET6;
			hints.ai_socktype = SOCK_DGRAM;
		} else if (strcasecmp(specifier, "tcp6") == 0 ||
		    strcasecmp(specifier, "tcpv6") == 0 ||
		    strcasecmp(specifier, "tcpipv6") == 0) {
			hints.ai_family = AF_INET6;
			hints.ai_socktype = SOCK_STREAM;
		} else if (strcasecmp(specifier, "unix") == 0) {
			hints.ai_family = AF_UNIX;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_addr = (struct sockaddr *)&saddr;
			hints.ai_addrlen = sizeof(saddr);
			saddr.sun_len = sizeof(saddr);
			saddr.sun_family = AF_UNIX;
			if (strlcpy(saddr.sun_path, hostname,
			    sizeof(saddr.sun_path)) > sizeof(saddr.sun_path))
				errx(1, "Hostname path too long");
			ai = &hints;
		} else {
			port = hostname;
			hostname = specifier;
			specifier = NULL;
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
		}
		if (port == NULL) {
			if (hints.ai_family == AF_INET) {
				if ((port = strchr(hostname, ':')) != NULL)
					*port++ = '\0';
			} else if (hints.ai_family == AF_INET6) {
				if (hostname[0] == '[') {
					hostname++;
					if ((port = strchr(hostname, ']')) == NULL)
						errx(1, "invalid agent");
					*port++ = '\0';
					if (port[0] == ':')
						*port++ = '\0';
					else
						port = NULL;
				} else {
					if ((port = strrchr(hostname, ':')) == NULL)
						errx(1, "invalid agent");
					*port++ = '\0';
				}
			}
		}
	} else {
		hostname = specifier;
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
	}

	if (hints.ai_family != AF_UNIX) {
		if (port == NULL)
			port = defaultport;
		error = getaddrinfo(hostname, port, &hints, &ai0);
		if (error)
			errx(1, "%s", gai_strerror(error));
		s = -1;
		for (ai = ai0; ai != NULL; ai = ai->ai_next) {
			if ((s = socket(ai->ai_family, ai->ai_socktype,
			    ai->ai_protocol)) == -1)
				continue;
			break;
		}
	} else
		s = socket(hints.ai_family, hints.ai_socktype,
		    hints.ai_protocol);
	if (s == -1)
		err(1, "socket");

	if (connect(s, (struct sockaddr *)ai->ai_addr, ai->ai_addrlen) == -1)
		err(1, "Can't connect to %s", agent);

	if (ai0 != NULL)
		freeaddrinfo(ai0);
	free(agentdup);
	return s;
}

char *
snmpc_hex2bin(char *hexstr, size_t *binlen)
{
	char *decstr;

	if (hexstr[0] == '0' && hexstr[1] == 'x')
		hexstr += 2;
	while (hexstr[0] == ' ' || hexstr[0] == '\t')
		hexstr++;

	if ((decstr = malloc((strlen(hexstr) / 2) + 1)) == NULL)
		return NULL;

	for (*binlen = 0; hexstr[0] != '\0'; (*binlen)++) {
		hexstr[0] = toupper(hexstr[0]);
		hexstr[1] = toupper(hexstr[1]);
		if (hexstr[0] >= '0' && hexstr[0] <= '9')
			decstr[*binlen] = (hexstr[0] - '0') << 4;
		else if (hexstr[0] >= 'A' && hexstr[0] <= 'F')
			decstr[*binlen] = ((hexstr[0] - 'A') + 10) << 4;
		else
			goto fail;
		if (hexstr[1] >= '0' && hexstr[1] <= '9')
			decstr[*binlen] |= (hexstr[1] - '0');
		else if (hexstr[1] >= 'A' && hexstr[1] <= 'F')
			decstr[*binlen] |= (hexstr[1] - 'A') + 10;
		else
			goto fail;

		hexstr += 2;
		while (hexstr[0] == ' ' || hexstr[0] == '\t')
			hexstr++;
	}

	return decstr;
fail:
	errno = EINVAL;
	free(decstr);
	return NULL;
}

struct ber_element *
snmpc_varbindparse(int argc, char *argv[])
{
	struct ber_oid oid, oidval;
	struct in_addr addr4;
	char *addr = (char *)&addr4;
	char *str = NULL, *tmpstr, *endstr;
	const char *errstr = NULL;
	struct ber_element *varbind = NULL, *vblist = NULL;
	int i, ret;
	size_t strl, byte;
	long long lval;

	if (argc % 3 != 0)
		usage();
	for (i = 0; i < argc; i += 3) {
		if (smi_string2oid(argv[i], &oid) == -1)
			errx(1, "Invalid oid: %s\n", argv[i]);
		switch (argv[i + 1][0]) {
		case 'a':
			ret = inet_pton(AF_INET, argv[i + 2], &addr4);
			if (ret == -1)
				err(1, "inet_pton");
			if (ret == 0)
				errx(1, "%s: Bad value notation (%s)", argv[i],
				    argv[i + 2]);
			if ((varbind = ober_printf_elements(varbind, "{Oxt}",
			    &oid, addr, sizeof(addr4), BER_CLASS_APPLICATION,
			    SNMP_T_IPADDR)) == NULL)
				err(1, "ober_printf_elements");
			break;
		case 'b':
			tmpstr = argv[i + 2];
			strl = 0;
			do {
				lval = strtoll(tmpstr, &endstr, 10);
				if (endstr[0] != ' ' && endstr[0] != '\t' &&
				    endstr[0] != ',' && endstr[0] != '\0')
					errx(1, "%s: Bad value notation (%s)",
					    argv[i], argv[i + 2]);
				if (tmpstr == endstr) {
					tmpstr++;
					continue;
				}
				if (lval < 0)
					errx(1, "%s: Bad value notation (%s)",
					    argv[i], argv[i + 2]);
				byte = lval / 8;
				if (byte >= strl) {
					if ((str = recallocarray(str, strl,
					    byte + 1, 1)) == NULL)
						err(1, "malloc");
					strl = byte + 1;
				}
				str[byte] |= 0x80 >> (lval % 8);
				tmpstr = endstr + 1;
			} while (endstr[0] != '\0');
			/*
			 * RFC3416 Section 2.5
			 * A BITS value is encoded as an OCTET STRING
			 */
			goto pastestring;
		case 'c':
			lval = strtonum(argv[i + 2], INT32_MIN, INT32_MAX,
			    &errstr);
			if (errstr != NULL)
				errx(1, "%s: Bad value notation (%s)", argv[i],
				    argv[i + 2]);
			if ((varbind = ober_printf_elements(varbind, "{Oit}",
			    &oid, lval, BER_CLASS_APPLICATION,
			    SNMP_T_COUNTER32)) == NULL)
				err(1, "ober_printf_elements");
			break;
		case 'd':
			/* String always shrinks */
			if ((str = malloc(strlen(argv[i + 2]))) == NULL)
				err(1, "malloc");
			tmpstr = argv[i + 2];
			strl = 0;
			do {
				lval = strtoll(tmpstr, &endstr, 10);
				if (endstr[0] != ' ' && endstr[0] != '\t' &&
				    endstr[0] != '\0')
					errx(1, "%s: Bad value notation (%s)",
					    argv[i], argv[i + 2]);
				if (tmpstr == endstr) {
					tmpstr++;
					continue;
				}
				if (lval < 0 || lval > 0xff)
					errx(1, "%s: Bad value notation (%s)",
					    argv[i], argv[i + 2]);
				str[strl++] = (unsigned char) lval;
				tmpstr = endstr + 1;
			} while (endstr[0] != '\0');
			goto pastestring;
		case 'u':
		case 'i':
			lval = strtonum(argv[i + 2], LLONG_MIN, LLONG_MAX,
			    &errstr);
			if (errstr != NULL)
				errx(1, "%s: Bad value notation (%s)", argv[i],
				    argv[i + 2]);
			if ((varbind = ober_printf_elements(varbind, "{Oi}",
			    &oid, lval)) == NULL)
				err(1, "ober_printf_elements");
			break;
		case 'n':
			if ((varbind = ober_printf_elements(varbind, "{O0}",
			    &oid)) == NULL)
				err(1, "ober_printf_elements");
			break;
		case 'o':
			if (smi_string2oid(argv[i + 2], &oidval) == -1)
				errx(1, "%s: Unknown Object Identifier (Sub-id "
				    "not found: (top) -> %s)", argv[i],
				    argv[i + 2]);
			if ((varbind = ober_printf_elements(varbind, "{OO}",
			    &oid, &oidval)) == NULL)
				err(1, "ober_printf_elements");
			break;
		case 's':
			if ((str = strdup(argv[i + 2])) == NULL)
				err(1, NULL);
			strl = strlen(argv[i + 2]);
pastestring:
			if ((varbind = ober_printf_elements(varbind, "{Ox}",
			    &oid, str, strl)) == NULL)
				err(1, "ober_printf_elements");
			free(str);
			break;
		case 't':
			lval = strtonum(argv[i + 2], LLONG_MIN, LLONG_MAX,
			    &errstr);
			if (errstr != NULL)
				errx(1, "%s: Bad value notation (%s)", argv[i],
				    argv[i + 2]);
			if ((varbind = ober_printf_elements(varbind, "{Oit}",
			    &oid, lval, BER_CLASS_APPLICATION,
			    SNMP_T_TIMETICKS)) == NULL)
				err(1, "ober_printf_elements");
			break;
		case 'x':
			/* String always shrinks */
			if ((str = malloc(strlen(argv[i + 2]))) == NULL)
				err(1, "malloc");
			tmpstr = argv[i + 2];
			strl = 0;
			do {
				lval = strtoll(tmpstr, &endstr, 16);
				if (endstr[0] != ' ' && endstr[0] != '\t' &&
				    endstr[0] != '\0')
					errx(1, "%s: Bad value notation (%s)",
					    argv[i], argv[i + 2]);
				if (tmpstr == endstr) {
					tmpstr++;
					continue;
				}
				if (lval < 0 || lval > 0xff)
					errx(1, "%s: Bad value notation (%s)",
					    argv[i], argv[i + 2]);
				str[strl++] = (unsigned char) lval;
				tmpstr = endstr + 1;
			} while (endstr[0] != '\0');
			goto pastestring;
		default:
			usage();
		}
		if (vblist == NULL)
			vblist = varbind;
	}

	return vblist;
}

__dead void
usage(void)
{
	size_t i;

	if (snmp_app != NULL) {
		fprintf(stderr, "usage: snmp %s%s%s\n",
		    snmp_app->name,
		    snmp_app->usecommonopt ?
		    " [-A authpass] [-a digest] [-c community] [-e secengineid]\n"
		    "            [-E ctxengineid] [-K localpriv] [-k localauth] [-l seclevel]\n"
		    "            [-n ctxname] [-O afnqvxSQ] [-r retries] [-t timeout] [-u user]\n"
		    "            [-v version] [-X privpass] [-x cipher] [-Z boots,time]\n"
		    "            " : "",
		    snmp_app->usage == NULL ? "" : snmp_app->usage);
		exit(1);
	}
	for (i = 0; i < (sizeof(snmp_apps)/sizeof(*snmp_apps)); i++) {
		if (i == 0)
			fprintf(stderr, "usage: ");
		else
			fprintf(stderr, "       ");
		fprintf(stderr, "snmp %s%s %s\n",
		    snmp_apps[i].name,
		    snmp_apps[i].usecommonopt ?
		    " [options]" : "",
		    snmp_apps[i].usage ? snmp_apps[i].usage : "");
	}
	exit(1);
}
