/*
 * Copyright (C) 1998-2001  Internet Software Consortium.
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

/* $ISC: t_master.c,v 1.30 2001/05/22 01:44:36 gson Exp $ */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/mem.h>
#include <isc/string.h>		/* Required for HP/UX (and others?) */
#include <isc/util.h>

#include <dns/callbacks.h>
#include <dns/master.h>
#include <dns/name.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/result.h>

#include <tests/t_api.h>

#define	BUFLEN		255
#define	BIGBUFLEN	(64 * 1024)

static isc_result_t
t1_add_callback(void *arg, dns_name_t *owner, dns_rdataset_t *dataset);

isc_mem_t	*T1_mctx;
char		*Tokens[T_MAXTOKS + 1];

static isc_result_t
t1_add_callback(void *arg, dns_name_t *owner, dns_rdataset_t *dataset) {
	char buf[BIGBUFLEN];
	isc_buffer_t target;
	isc_result_t result;

	UNUSED(arg);

	isc_buffer_init(&target, buf, BIGBUFLEN);
	result = dns_rdataset_totext(dataset, owner, ISC_FALSE, ISC_FALSE,
				     &target);
	if (result != ISC_R_SUCCESS)
		t_info("dns_rdataset_totext: %s\n", dns_result_totext(result));

	return(result);
}

static int
test_master(char *testfile, char *origin, char *class, isc_result_t exp_result)
{
	int			result;
	int			len;
	isc_result_t		isc_result;
	isc_result_t		dns_result;
	dns_name_t		dns_origin;
	isc_buffer_t		source;
	isc_buffer_t		target;
	unsigned char		name_buf[BUFLEN];
	dns_rdatacallbacks_t	callbacks;
	dns_rdataclass_t	rdataclass;
	isc_textregion_t	textregion;

	result = T_UNRESOLVED;
	if (T1_mctx == NULL)
		isc_result = isc_mem_create(0, 0, &T1_mctx);
	else
		isc_result = ISC_R_SUCCESS;
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %d\n", isc_result);
		return(T_UNRESOLVED);
	}

	len = strlen(origin);
	isc_buffer_init(&source, origin, len);
	isc_buffer_add(&source, len);
	isc_buffer_setactive(&source, len);
	isc_buffer_init(&target, name_buf, BUFLEN);
	dns_name_init(&dns_origin, NULL);
	dns_result = dns_name_fromtext(&dns_origin, &source, dns_rootname,
				   ISC_FALSE, &target);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
				dns_result_totext(dns_result));
		return(T_UNRESOLVED);
	}

	dns_rdatacallbacks_init_stdio(&callbacks);
	callbacks.add = t1_add_callback;

	textregion.base = class;
	textregion.length = strlen(class);

	dns_result = dns_rdataclass_fromtext(&rdataclass, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdataclass_fromtext failed %s\n",
				dns_result_totext(dns_result));
		return(T_UNRESOLVED);
	}

	dns_result = dns_master_loadfile(	testfile,
						&dns_origin,
						&dns_origin,
						rdataclass,
						ISC_TRUE,
						&callbacks,
						T1_mctx);

	if (dns_result == exp_result)
		result = T_PASS;
	else {
		t_info("dns_master_loadfile: got %s, expected %s\n",
			dns_result_totext(dns_result),
			dns_result_totext(exp_result));
		result = T_FAIL;
	}
	return(result);
}

static int
test_master_x(const char *filename) {
	FILE		*fp;
	char		*p;
	int		line;
	int		cnt;
	int		result;

	result = T_UNRESOLVED;

	fp = fopen(filename, "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace(*p & 0xff)) || (*p == '#'))
				continue;

			/*
			 * Name of data file, origin, zclass, expected result.
			 */
			cnt = t_bustline(p, Tokens);
			if (cnt == 4) {
				result = test_master(Tokens[0], Tokens[1],
					     Tokens[2],
					     t_dns_result_fromtext(Tokens[3]));
			} else {
				t_info("bad format in %s at line %d\n",
				       filename, line);
			}

			(void)free(p);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile %s\n", filename);
	}
	return(result);
}

static const char *a1 =	"dns_master_loadfile loads a valid master file and "
			"returns ISC_R_SUCCESS";
static void
t1(void) {
	int	result;
	t_assert("dns_master_loadfile", 1, T_REQUIRED, a1);
	result = test_master_x("dns_master_load_1_data");
	t_result(result);
}

static const char *a2 =
	"dns_master_loadfile returns ISC_R_UNEXPECTEDEND when the "
	"masterfile input ends unexpectedly";

static void
t2(void) {
	int	result;
	t_assert("dns_master_loadfile", 2, T_REQUIRED, a2);
	result = test_master_x("dns_master_load_2_data");
	t_result(result);
}

static const char *a3 =	"dns_master_loadfile returns DNS_R_NOOWNER when the "
			"an ownername is not specified";

static void
t3() {
	int	result;
	t_assert("dns_master_loadfile", 3, T_REQUIRED, a3);
	result = test_master_x("dns_master_load_3_data");
	t_result(result);
}

static const char *a4 =	"dns_master_loadfile accepts broken zone files "
			"where the first record has an undefined TTL, "
			"as long as it is a SOA";

static void
t4() {
	int	result;
	t_assert("dns_master_loadfile", 4, T_REQUIRED, a4);
	result = test_master_x("dns_master_load_4_data");
	t_result(result);
}

static const char *a5 =	"dns_master_loadfile returns DNS_R_BADCLASS when the "
			"the record class did not match the zone class";

static void
t5() {
	int	result;

	t_assert("dns_master_loadfile", 5, T_REQUIRED, a5);
	result = test_master_x("dns_master_load_5_data");

	t_result(result);
}

static const char *a6 =
	"dns_master_loadfile understands KEY RR specifications "
	"containing key material";

static void
t6() {
	int	result;

	t_assert("dns_master_loadfile", 6, T_REQUIRED, a6);
	result = test_master_x("dns_master_load_6_data");

	t_result(result);
}

static const char *a7 =
	"dns_master_loadfile understands KEY RR specifications "
	"containing no key material";

static void
t7() {
	int	result;

	t_assert("dns_master_loadfile", 7, T_REQUIRED, a7);
	result = test_master_x("dns_master_load_7_data");

	t_result(result);
}

static const char *a8 =
	"dns_master_loadfile understands $INCLUDE";

static void
t8() {
	int	result;

	t_assert("dns_master_loadfile", 8, T_REQUIRED, a8);
	result = test_master_x("dns_master_load_8_data");

	t_result(result);
}

static const char *a9 =
	"dns_master_loadfile understands $INCLUDE with failure";

static void
t9() {
	int	result;

	t_assert("dns_master_loadfile", 9, T_REQUIRED, a9);
	result = test_master_x("dns_master_load_9_data");

	t_result(result);
}

static const char *a10 =
	"dns_master_loadfile non-empty blank lines";

static void
t10() {
	int	result;

	t_assert("dns_master_loadfile", 10, T_REQUIRED, a10);
	result = test_master_x("dns_master_load_10_data");

	t_result(result);
}

static const char *a11 =
	"dns_master_loadfile allow leading zeros in SOA";

static void
t11() {
	int	result;

	t_assert("dns_master_loadfile", 11, T_REQUIRED, a11);
	result = test_master_x("dns_master_load_11_data");

	t_result(result);
}


testspec_t	T_testlist[] = {
	{	t1,	"ISC_R_SUCCESS"		},
	{	t2,	"ISC_R_UNEXPECTEDEND"	},
	{	t3,	"DNS_NOOWNER"		},
	{	t4,	"DNS_NOTTL"		},
	{	t5,	"DNS_BADCLASS"		},
	{	t6,	"KEY RR 1"		},
	{	t7,	"KEY RR 2"		},
	{	t8,	"$INCLUDE"		},
	{	t9,	"$INCLUDE w/ DNS_BADCLASS"	},
	{	t10,	"non empty blank lines"	},
	{	t11,	"leading zeros in serial"	},
	{	NULL,	NULL			}
};

