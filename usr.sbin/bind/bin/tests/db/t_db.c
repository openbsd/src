/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

/* $ISC: t_db.c,v 1.29 2001/08/08 22:54:30 gson Exp $ */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>

#include <isc/mem.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/fixedname.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdatatype.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/result.h>

#include <tests/t_api.h>

static isc_result_t
t_create(const char *db_type, const char *origin, const char *class,
	 const char *model, isc_mem_t *mctx, dns_db_t **db)
{
	int			len;
	isc_result_t		dns_result;
	dns_dbtype_t		dbtype;
	isc_constregion_t	region;
	isc_buffer_t		origin_buffer;
	dns_fixedname_t		dns_origin;
	dns_rdataclass_t	rdataclass;


	dbtype = dns_dbtype_zone;
	if (strcasecmp(model, "cache") == 0)
		dbtype = dns_dbtype_cache;

	dns_fixedname_init(&dns_origin);
	len = strlen(origin);
	isc_buffer_init(&origin_buffer, origin, len);
	isc_buffer_add(&origin_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_origin),
				       &origin_buffer, NULL, ISC_FALSE, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
		       dns_result_totext(dns_result));
		return(dns_result);
	}

	region.base = class;
	region.length = strlen(class);
	dns_result = dns_rdataclass_fromtext(&rdataclass,
					     (isc_textregion_t *)&region);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdataclass_fromtext failed %s\n",
		       dns_result_totext(dns_result));
		return(dns_result);
	}

	dns_result = dns_db_create(mctx, db_type,
				   dns_fixedname_name(&dns_origin),
				   dbtype, rdataclass, 0, NULL, db);
	if (dns_result != ISC_R_SUCCESS)
		t_info("dns_db_create failed %s\n",
		       dns_result_totext(dns_result));

	return(dns_result);

}

static int
t_dns_db_load(char **av) {
	char			*filename;
	char			*db_type;
	char			*origin;
	char			*model;
	char			*class;
	char			*expected_load_result;
	char			*findname;
	char			*find_type;
	char			*expected_find_result;

	int			result;
	int			len;
	dns_db_t		*db;
	isc_result_t		dns_result;
	isc_result_t		isc_result;
	isc_mem_t		*mctx;
	dns_dbnode_t		*nodep;
	isc_textregion_t	textregion;
	isc_buffer_t		findname_buffer;
	dns_fixedname_t		dns_findname;
	dns_fixedname_t		dns_foundname;
	dns_rdataset_t		rdataset;
	dns_rdatatype_t		rdatatype;
	dns_dbversion_t		*versionp;
	isc_result_t		exp_load_result;
	isc_result_t		exp_find_result;

	result = T_UNRESOLVED;
	db = NULL;
	mctx = NULL;
	filename = T_ARG(0);
	db_type = T_ARG(1);
	origin = T_ARG(2);
	model = T_ARG(3);
	class = T_ARG(4);
	expected_load_result = T_ARG(5);
	findname = T_ARG(6);
	find_type = T_ARG(7);
	expected_find_result = T_ARG(8);

	t_info("testing using file %s and name %s\n", filename, findname);

	exp_load_result = t_dns_result_fromtext(expected_load_result);
	exp_find_result = t_dns_result_fromtext(expected_find_result);

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
				isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	dns_result = t_create(db_type, origin, class, model, mctx, &db);
	if (dns_result != ISC_R_SUCCESS) {
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_load(db, filename);
	if (dns_result != exp_load_result) {
		t_info("dns_db_load returned %s, expected %s\n",
				dns_result_totext(dns_result),
				dns_result_totext(exp_load_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_FAIL);
	}
	if (dns_result != ISC_R_SUCCESS) {
		result = T_PASS;
		goto cleanup_db;
	}

	dns_fixedname_init(&dns_findname);
	len = strlen(findname);
	isc_buffer_init(&findname_buffer, findname, len);
	isc_buffer_add(&findname_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_findname),
				&findname_buffer, NULL, ISC_FALSE, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
			dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	textregion.base = find_type;
	textregion.length = strlen(find_type);
	dns_result = dns_rdatatype_fromtext(&rdatatype, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdatatype_fromtext %s failed %s\n",
				find_type,
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	versionp = NULL;
	dns_fixedname_init(&dns_foundname);
	dns_rdataset_init(&rdataset);
	if (dns_db_iszone(db))
		dns_db_currentversion(db, &versionp);
	nodep = NULL;

	dns_result = dns_db_find(db,
			dns_fixedname_name(&dns_findname),
			versionp,
			rdatatype,
			DNS_DBFIND_GLUEOK,
			0,
			&nodep,
			dns_fixedname_name(&dns_foundname),
			&rdataset, NULL);

	if (dns_result != exp_find_result) {
		t_info("dns_db_find returned %s, expected %s\n",
				dns_result_totext(dns_result),
				dns_result_totext(exp_find_result));
		result = T_FAIL;
	} else {
		result = T_PASS;
	}

	if (dns_result != ISC_R_NOTFOUND) {
		dns_db_detachnode(db, &nodep);
		if (dns_rdataset_isassociated(&rdataset))
			dns_rdataset_disassociate(&rdataset);
	}

	if (dns_db_iszone(db))
		dns_db_closeversion(db, &versionp, ISC_FALSE);
 cleanup_db:
	dns_db_detach(&db);
	isc_mem_destroy(&mctx);
	return(result);
}

static const char *a1 =
	"A call to dns_db_load(db, filename) loads the contents of "
	"the database in filename into db.";

static void
t1(void) {
	int	result;

	t_assert("dns_db_load", 1, T_REQUIRED, a1);
	result = t_eval("dns_db_load_data", t_dns_db_load, 9);
	t_result(result);
}


static const char *a2 =
	"When the database db has cache semantics, a call to "
	"dns_db_iscache(db) returns ISC_TRUE.";

static int
t_dns_db_zc_x(char *filename, char *db_type, char *origin, char *class,
	      dns_dbtype_t dbtype, isc_boolean_t(*cf)(dns_db_t *),
	      isc_boolean_t exp_result)
{
	int			result;
	int			len;
	dns_db_t		*db;
	isc_result_t		dns_result;
	isc_result_t		isc_result;
	isc_mem_t		*mctx;
	dns_rdataclass_t	rdataclass;
	isc_textregion_t	textregion;
	isc_buffer_t		origin_buffer;
	dns_fixedname_t		dns_origin;

	result = T_UNRESOLVED;

	db = NULL;
	mctx = NULL;

	t_info("testing using file %s\n", filename);

	dns_fixedname_init(&dns_origin);
	len = strlen(origin);
	isc_buffer_init(&origin_buffer, origin, len);
	isc_buffer_add(&origin_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_origin),
				       &origin_buffer, NULL, ISC_FALSE, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
		       dns_result_totext(dns_result));
		return(T_UNRESOLVED);
	}

	textregion.base = class;
	textregion.length = strlen(class);
	dns_result = dns_rdataclass_fromtext(&rdataclass, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdataclass_fromtext failed %s\n",
		       dns_result_totext(dns_result));
		return(T_UNRESOLVED);
	}

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
		       isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_create(mctx, db_type,
				   dns_fixedname_name(&dns_origin),
				   dbtype, rdataclass, 0, NULL, &db);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_create failed %s\n",
		       dns_result_totext(dns_result));
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_load(db, filename);
	if (dns_result == ISC_R_SUCCESS) {
		if ((*cf)(db) == exp_result)
			result = T_PASS;
		else
			result = T_FAIL;
	} else {
		t_info("dns_db_load failed %s\n",
		       dns_result_totext(dns_result));
		result = T_FAIL;
	}

	dns_db_detach(&db);
	isc_mem_destroy(&mctx);
	return(result);
}

static int
test_dns_db_zc_x(const char *filename, dns_dbtype_t dbtype,
		 isc_boolean_t(*cf)(dns_db_t *), isc_boolean_t exp_result)
{

	FILE		*fp;
	char		*p;
	int		line;
	int		cnt;
	int		result;
	int		nfails;
	int		nprobs;
	char		*tokens[T_MAXTOKS];

	nfails = 0;
	nprobs = 0;

	fp = fopen(filename, "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = t_bustline(p, tokens);
			if (cnt == 4) {
				result = t_dns_db_zc_x(tokens[0], /* file */
						       tokens[1], /* type */
						       tokens[2], /* origin */
						       tokens[3], /* class */
						       dbtype,     /* cache */
						       cf,     /* check func */
						       exp_result);/* expect */
				if (result != T_PASS) {
					if (result == T_FAIL)
						++nfails;
					else
						++nprobs;
				}
			} else {
				t_info("bad format in %s at line %d\n",
				       filename, line);
				++nprobs;
			}

			(void)free(p);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile %s\n", filename);
		++nprobs;
	}

	result = T_UNRESOLVED;

	if (nfails == 0 && nprobs == 0)
		result = T_PASS;
	else if (nfails)
		result = T_FAIL;

	return(result);
}

static void
t2(void) {
	int	result;

	t_assert("dns_db_iscache", 2, T_REQUIRED, a2);
	result = test_dns_db_zc_x("dns_db_iscache_1_data",
				  dns_dbtype_cache, dns_db_iscache, ISC_TRUE);
	t_result(result);
}


static const char *a3 =
	"When the database db has zone semantics, a call to "
	"dns_db_iscache(db) returns ISC_FALSE.";


static void
t3(void) {
	int	result;

	t_assert("dns_db_iscache", 3, T_REQUIRED, a3);
	result = test_dns_db_zc_x("dns_db_iscache_2_data",
				  dns_dbtype_zone, dns_db_iscache, ISC_FALSE);
	t_result(result);
}


static const char *a4 =
	"When the database db has zone semantics, a call to "
	"dns_db_iszone(db) returns ISC_TRUE.";


static void
t4(void) {
	int	result;

	t_assert("dns_db_iszone", 4, T_REQUIRED, a4);
	result = test_dns_db_zc_x("dns_db_iszone_1_data",
				  dns_dbtype_zone, dns_db_iszone, ISC_TRUE);
	t_result(result);
}


static const char *a5 =
	"When the database db has cache semantics, a call to "
	"dns_db_iszone(db) returns ISC_FALSE.";

static void
t5(void) {
	int	result;

	t_assert("dns_db_iszone", 5, T_REQUIRED, a5);
	result = test_dns_db_zc_x("dns_db_iszone_2_data",
				  dns_dbtype_cache, dns_db_iszone, ISC_FALSE);
	t_result(result);
}

static int
t_dns_db_origin(char **av) {

	char			*filename;
	char			*origin;

	int			result;
	int			len;
	int			order;
	isc_result_t		dns_result;
	isc_result_t		isc_result;
	isc_mem_t		*mctx;
	dns_db_t		*db;
	dns_fixedname_t		dns_origin;
	dns_fixedname_t		dns_dborigin;
	isc_buffer_t		origin_buffer;

	db = NULL;
	mctx = NULL;


	filename = T_ARG(0);
	origin = T_ARG(1);

	t_info("testing with database %s and origin %s\n",
			filename, origin);

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
			isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	dns_result = t_create("rbt", origin, "in", "isc_true", mctx, &db);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("t_create failed %s\n",
			dns_result_totext(dns_result));
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}
	dns_fixedname_init(&dns_origin);
	dns_fixedname_init(&dns_dborigin);

	len = strlen(origin);
	isc_buffer_init(&origin_buffer, origin, len);
	isc_buffer_add(&origin_buffer, len);

	dns_result = dns_db_load(db, filename);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_load failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_origin),
				&origin_buffer, NULL, ISC_FALSE, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}
	order = dns_name_compare(dns_fixedname_name(&dns_origin),
				 dns_db_origin(db));
	if (order == 0) {
		result = T_PASS;
	} else {
		t_info("dns_name_compare returned %d\n", order);
		result = T_FAIL;
	}

	dns_db_detach(&db);
	isc_mem_destroy(&mctx);
	return(result);

}

static const char *a6 =
	"A call to dns_db_origin(db) returns the origin of the database.";

static void
t6(void) {
	int	result;

	t_assert("dns_db_origin", 6, T_REQUIRED, a6);
	result = t_eval("dns_db_origin_data", t_dns_db_origin, 2);
	t_result(result);
}


static const char *a7 =
	"A call to dns_db_class(db) returns the class of the database.";


#define	CLASSBUFLEN	256

static int
t_dns_db_class(char **av) {

	char			*filename;
	char			*class;

	int			result;
	isc_result_t		dns_result;
	isc_result_t		isc_result;
	isc_mem_t		*mctx;
	dns_db_t		*db;
	dns_rdataclass_t	rdataclass;
	dns_rdataclass_t	db_rdataclass;
	isc_textregion_t	textregion;

	filename = T_ARG(0);
	class = T_ARG(1);
	db = NULL;
	mctx = NULL;


	t_info("testing with database %s and class %s\n",
			filename, class);

	textregion.base = class;
	textregion.length = strlen(class);
	dns_result = dns_rdataclass_fromtext(&rdataclass, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdataclass_fromtext failed %s\n",
				dns_result_totext(dns_result));
		return(T_UNRESOLVED);
	}

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
			isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	dns_result = t_create("rbt", ".", class, "isc_true", mctx, &db);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("t_create failed %s\n",
			dns_result_totext(dns_result));
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_load(db, filename);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_load failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	db_rdataclass = dns_db_class(db);
	if (db_rdataclass == rdataclass)
		result = T_PASS;
	else {
		char classbuf[DNS_RDATACLASS_FORMATSIZE];
		dns_rdataclass_format(db_rdataclass,
				      classbuf, sizeof(classbuf));
		t_info("dns_db_class returned %s, expected %s\n",
		       classbuf, class);
		result = T_FAIL;
	}

	dns_db_detach(&db);
	isc_mem_destroy(&mctx);
	return(result);

}
static void
t7(void) {
	int	result;

	t_assert("dns_db_class", 7, T_REQUIRED, a7);
	result = t_eval("dns_db_class_data", t_dns_db_class, 2);
	t_result(result);
}


static const char *a8 =
	"A call to dns_db_currentversion() opens the current "
	"version for reading.";

static int
t_dns_db_currentversion(char **av) {
	char			*filename;
	char			*db_type;
	char			*origin;
	char			*class;
	char			*model;
	char			*findname;
	char			*findtype;

	int			result;
	int			len;
	dns_db_t		*db;
	isc_result_t		dns_result;
	isc_result_t		isc_result;
	isc_mem_t		*mctx;
	dns_dbnode_t		*nodep;
	isc_textregion_t	textregion;
	isc_buffer_t		findname_buffer;
	dns_fixedname_t		dns_findname;
	dns_fixedname_t		dns_foundname;
	dns_rdataset_t		rdataset;
	dns_rdatatype_t		rdatatype;
	dns_dbversion_t		*cversionp;
	dns_dbversion_t		*nversionp;

	result = T_UNRESOLVED;

	filename = T_ARG(0);
	db_type = T_ARG(1);
	origin = T_ARG(2);
	class = T_ARG(3);
	model = T_ARG(4);
	findname = T_ARG(5);
	findtype = T_ARG(6);
	db = NULL;
	mctx = NULL;

	t_info("testing using file %s and name %s\n", filename, findname);

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
				isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	dns_result = t_create(db_type, origin, class, model, mctx, &db);
	if (dns_result != ISC_R_SUCCESS) {
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_load(db, filename);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_load returned %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_fixedname_init(&dns_findname);
	len = strlen(findname);
	isc_buffer_init(&findname_buffer, findname, len);
	isc_buffer_add(&findname_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_findname),
				&findname_buffer, NULL, ISC_FALSE, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
			dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	textregion.base = findtype;
	textregion.length = strlen(findtype);
	dns_result = dns_rdatatype_fromtext(&rdatatype, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdatatype_fromtext %s failed %s\n",
				findtype,
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * find a name we know is there
	 */

	cversionp = NULL;
	dns_fixedname_init(&dns_foundname);
	dns_rdataset_init(&rdataset);
	dns_db_currentversion(db, &cversionp);
	nodep = NULL;

	dns_result = dns_db_find(db,
			dns_fixedname_name(&dns_findname),
			cversionp,
			rdatatype,
			0,
			0,
			&nodep,
			dns_fixedname_name(&dns_foundname),
			&rdataset, NULL);

	if (dns_result != ISC_R_SUCCESS) {
		t_info("unable to find %s using current version\n", findname);
		dns_db_closeversion(db, &cversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * create a new version
	 * delete the found rdataset in the new version
	 * attempt to find the rdataset again and expect the find to fail
	 * close/commit the new version
	 * attempt to find the rdataset in the current version and
	 * expect the find to succeed
	 */

	nversionp = NULL;
	dns_result = dns_db_newversion(db, &nversionp);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_newversion failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_rdataset_disassociate(&rdataset);
		dns_db_closeversion(db, &cversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * Delete the found rdataset in the new version.
	 */
	dns_result = dns_db_deleterdataset(db, nodep, nversionp, rdatatype, 0);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_deleterdataset failed %s\n",
				dns_result_totext(dns_result));
		dns_rdataset_disassociate(&rdataset);
		dns_db_detachnode(db, &nodep);
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_closeversion(db, &cversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * Don't need these now.
	 */
	dns_rdataset_disassociate(&rdataset);
	dns_db_detachnode(db, &nodep);
	nodep = NULL;

	/*
	 * Find the deleted rdataset and expect it to fail.
	 */
	dns_result = dns_db_find(db,
			dns_fixedname_name(&dns_findname),
			nversionp,
			rdatatype,
			0,
			0,
			&nodep,
			dns_fixedname_name(&dns_foundname),
			&rdataset, NULL);

	if ((dns_result != ISC_R_NOTFOUND) && (dns_result != DNS_R_NXDOMAIN)) {
		t_info("unexpectedly found %s using current version\n",
		       findname);
		dns_db_closeversion(db, &cversionp, ISC_FALSE);
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_FAIL);
	}

	/*
	 * Close/commit the new version.
	 */
	dns_db_closeversion(db, &nversionp, ISC_TRUE);

	/*
	 * Find the deleted rdata in the current version.
	 */
	dns_result = dns_db_find(db, dns_fixedname_name(&dns_findname),
				 cversionp, rdatatype, DNS_DBFIND_GLUEOK,
				 0, &nodep, dns_fixedname_name(&dns_foundname),
				 &rdataset, NULL);

	/*
	 * And expect it to succeed.
	 */
	if (dns_result == ISC_R_SUCCESS) {
		result = T_PASS;
	} else {
		t_info("cound not find %s using current version\n", findname);
		dns_db_closeversion(db, &cversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		result = T_FAIL;
	}

	dns_db_detachnode(db, &nodep);
	dns_rdataset_disassociate(&rdataset);

	dns_db_closeversion(db, &cversionp, ISC_FALSE);
	dns_db_detach(&db);
	isc_mem_destroy(&mctx);

	return(result);
}

static void
t8(void) {
	int	result;

	t_assert("dns_db_currentversion", 8, T_REQUIRED, a8);
	result = t_eval("dns_db_currentversion_data",
			t_dns_db_currentversion, 7);
	t_result(result);
}

static const char *a9 =
	"A call to dns_db_newversion() opens a new version for "
	"reading and writing.";

static int
t_dns_db_newversion(char **av) {

	char			*filename;
	char			*db_type;
	char			*origin;
	char			*class;
	char			*model;
	char			*newname;
	char			*newtype;

	int			result;
	int			len;
	int			rval;
	dns_db_t		*db;
	isc_result_t		dns_result;
	isc_result_t		isc_result;
	isc_mem_t		*mctx;
	dns_dbnode_t		*nodep;
	dns_dbnode_t		*found_nodep;
	isc_textregion_t	textregion;
	isc_buffer_t		newname_buffer;
	dns_fixedname_t		dns_newname;
	dns_fixedname_t		dns_foundname;
	dns_rdata_t		added_rdata = DNS_RDATA_INIT;
	const char *		added_rdata_data;
	dns_rdataset_t		added_rdataset;
	dns_rdata_t		found_rdata = DNS_RDATA_INIT;
	dns_rdataset_t		found_rdataset;
	dns_rdatatype_t		rdatatype;
	dns_rdataclass_t	rdataclass;
	dns_dbversion_t		*nversionp;
	dns_rdatalist_t		rdatalist;

	result = T_UNRESOLVED;

	filename = T_ARG(0);
	db_type = T_ARG(1);
	origin = T_ARG(2);
	class = T_ARG(3);
	model = T_ARG(4);
	newname = T_ARG(5);
	newtype = T_ARG(6);
	db = NULL;
	mctx = NULL;

	/*
	 * Open a new version, add some data, commit it,
	 * close it, open a new version, and check that changes
	 * are present.
	 */

	t_info("testing using file %s and name %s\n", filename, newname);

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
				isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	dns_result = t_create(db_type, origin, class, model, mctx, &db);
	if (dns_result != ISC_R_SUCCESS) {
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_load(db, filename);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_load returned %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * Add a new name.
	 */

	dns_fixedname_init(&dns_newname);
	len = strlen(newname);
	isc_buffer_init(&newname_buffer, newname, len);
	isc_buffer_add(&newname_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_newname),
				&newname_buffer, NULL, ISC_FALSE, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
			dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	nodep = NULL;
	dns_result = dns_db_findnode(db, dns_fixedname_name(&dns_newname),
				ISC_TRUE, &nodep);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_findnode failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * Open a new version and associate some rdata with the new name.
	 */

	textregion.base = newtype;
	textregion.length = strlen(newtype);
	dns_result = dns_rdatatype_fromtext(&rdatatype, &textregion);

	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdatatype_fromtext %s failed %s\n",
				newtype,
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	textregion.base = class;
	textregion.length = strlen(class);
	dns_result = dns_rdataclass_fromtext(&rdataclass, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdataclass_fromtext failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_rdata_init(&added_rdata);
	added_rdata_data = "\x10\x00\x00\x01";
	DE_CONST(added_rdata_data, added_rdata.data);
	added_rdata.length = 4;
	added_rdata.rdclass = rdataclass;
	added_rdata.type = rdatatype;

	dns_rdataset_init(&added_rdataset);
	rdatalist.type = rdatatype;
	rdatalist.covers = 0;
	rdatalist.rdclass = rdataclass;
	rdatalist.ttl = 0;
	ISC_LIST_INIT(rdatalist.rdata);
	ISC_LIST_APPEND(rdatalist.rdata, &added_rdata, link);

	dns_result = dns_rdatalist_tordataset(&rdatalist, &added_rdataset);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdatalist_tordataset failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	nversionp = NULL;
	dns_result = dns_db_newversion(db, &nversionp);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_newversion failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_addrdataset(db, nodep, nversionp, 0,
				&added_rdataset, 0, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_addrdataset failed %s\n",
				dns_result_totext(dns_result));
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * Close and commit the version.
	 */
	dns_db_closeversion(db, &nversionp, ISC_TRUE);
	dns_db_detachnode(db, &nodep);
	if (dns_rdataset_isassociated(&added_rdataset))
		dns_rdataset_disassociate(&added_rdataset);
	nodep = NULL;

	/*
	 * Open a new version and find the data we added.
	 */
	dns_fixedname_init(&dns_foundname);
	dns_rdataset_init(&found_rdataset);
	nversionp = NULL;
	found_nodep = NULL;
	dns_db_newversion(db, &nversionp);

	/*
	 * Find the recently added name and rdata.
	 */
	dns_result = dns_db_find(db, dns_fixedname_name(&dns_newname),
				 nversionp, rdatatype, 0, 0, &found_nodep,
				 dns_fixedname_name(&dns_foundname),
				 &found_rdataset, NULL);

	if (dns_result != ISC_R_SUCCESS) {
		/* XXXWPK - NXRRSET ???  reference counts ??? */
		t_info("dns_db_find failed %s\n",
		       dns_result_totext(dns_result));
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detachnode(db, &found_nodep);
		if (dns_rdataset_isassociated(&found_rdataset))
			dns_rdataset_disassociate(&found_rdataset);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_FAIL);
	}

	dns_result = dns_rdataset_first(&found_rdataset);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdataset_first failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		if (dns_rdataset_isassociated(&found_rdataset))
			dns_rdataset_disassociate(&found_rdataset);
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_FAIL);
	}

	/*
	 * Now make sure its what we expect.
	 */
	dns_rdata_init(&found_rdata);
	dns_rdataset_current(&found_rdataset, &found_rdata);
	rval = dns_rdata_compare(&added_rdata, &found_rdata);
	if (rval == 0) {
		result = T_PASS;
	} else {
		t_info("dns_rdata_compare returned %d\n", rval);
		result = T_FAIL;
	}

	/*
	 * Don't need these now.
	 */
	dns_db_closeversion(db, &nversionp, ISC_FALSE);
	if (dns_rdataset_isassociated(&found_rdataset))
		dns_rdataset_disassociate(&found_rdataset);
	dns_db_detachnode(db, &found_nodep);
	dns_db_detach(&db);
	isc_mem_destroy(&mctx);

	return(result);
}

static void
t9(void) {
	int	result;

	t_assert("dns_db_newversion", 9, T_REQUIRED, a9);
	result = t_eval("dns_db_newversion_data", t_dns_db_newversion, 7);
	t_result(result);
}

static const char *a10 =
	"When versionp points to a read-write version and commit is "
	"ISC_TRUE, a call to dns_db_closeversion(db, versionp, commit) "
	"causes all changes made in the version to take effect, "
	"and returns ISC_R_SUCCESS.";

static int
t_dns_db_closeversion_1(char **av) {
	char			*filename;
	char			*db_type;
	char			*origin;
	char			*class;
	char			*model;
	char			*new_name;
	char			*new_type;
	char			*existing_name;
	char			*existing_type;

	int			result;
	int			len;
	int			rval;
	int			nfails;
	dns_db_t		*db;
	isc_result_t		dns_result;
	isc_result_t		isc_result;
	isc_mem_t		*mctx;
	dns_dbnode_t		*nodep;
	isc_textregion_t	textregion;
	isc_buffer_t		name_buffer;
	dns_fixedname_t		dns_newname;
	dns_fixedname_t		dns_foundname;
	dns_fixedname_t		dns_existingname;
	dns_rdata_t		added_rdata = DNS_RDATA_INIT;
	const char *		added_rdata_data;
	dns_rdataset_t		added_rdataset;
	dns_rdata_t		found_rdata = DNS_RDATA_INIT;
	dns_rdataset_t		found_rdataset;
	dns_rdatatype_t		new_rdatatype;
	dns_rdatatype_t		existing_rdatatype;
	dns_rdataclass_t	rdataclass;
	dns_dbversion_t		*nversionp;
	dns_dbversion_t		*cversionp;
	dns_rdatalist_t		rdatalist;

	filename = T_ARG(0);
	db_type = T_ARG(1);
	origin = T_ARG(2);
	class = T_ARG(3);
	model = T_ARG(4);
	new_name = T_ARG(5);
	new_type = T_ARG(6);
	existing_name = T_ARG(7);
	existing_type = T_ARG(8);

	nfails = 0;
	result = T_UNRESOLVED;
	db = NULL;
	mctx = NULL;

	/*
	 * Open a new version, add some data,
	 * remove some data, close with commit, open the current
	 * version and check that changes are present.
	 */

	t_info("testing using file %s and name %s\n", filename, new_name);

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
				isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	dns_result = t_create(db_type, origin, class, model, mctx, &db);
	if (dns_result != ISC_R_SUCCESS) {
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_load(db, filename);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_load returned %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * Remove all rdata for an existing name.
	 */

	dns_fixedname_init(&dns_existingname);
	len = strlen(existing_name);
	isc_buffer_init(&name_buffer, existing_name, len);
	isc_buffer_add(&name_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_existingname),
			&name_buffer, NULL, ISC_FALSE, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
			dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	textregion.base = existing_type;
	textregion.length = strlen(existing_type);
	dns_result = dns_rdatatype_fromtext(&existing_rdatatype, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdatatype_fromtext %s failed %s\n",
				existing_type,
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	nodep = NULL;
	dns_result = dns_db_findnode(db, dns_fixedname_name(&dns_existingname),
				ISC_FALSE, &nodep);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_findnode %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/* open a new version */
	nversionp = NULL;
	dns_result = dns_db_newversion(db, &nversionp);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_newversion failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_deleterdataset(db, nodep, nversionp,
					   existing_rdatatype, 0);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_deleterdataset failed %s\n",
				dns_result_totext(dns_result));
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * add a new name and associate some rdata with it
	 */

	dns_db_detachnode(db, &nodep);
	nodep = NULL;

	dns_fixedname_init(&dns_newname);
	len = strlen(new_name);
	isc_buffer_init(&name_buffer, new_name, len);
	isc_buffer_add(&name_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_newname),
				&name_buffer, NULL, ISC_FALSE, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
			dns_result_totext(dns_result));
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_findnode(db, dns_fixedname_name(&dns_newname),
				ISC_TRUE, &nodep);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_findnode failed %s\n",
				dns_result_totext(dns_result));
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * associate some rdata with the new name
	 */

	textregion.base = new_type;
	textregion.length = strlen(new_type);
	dns_result = dns_rdatatype_fromtext(&new_rdatatype, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdatatype_fromtext %s failed %s\n",
				new_type,
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	textregion.base = class;
	textregion.length = strlen(class);
	dns_result = dns_rdataclass_fromtext(&rdataclass, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdataclass_fromtext failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_rdata_init(&added_rdata);
	added_rdata_data = "\x10\x00\x00\x01";
	DE_CONST(added_rdata_data, added_rdata.data);
	added_rdata.length = 4;
	added_rdata.rdclass = rdataclass;
	added_rdata.type = new_rdatatype;

	dns_rdataset_init(&added_rdataset);
	rdatalist.type = new_rdatatype;
	rdatalist.covers = 0;
	rdatalist.rdclass = rdataclass;
	rdatalist.ttl = 0;
	ISC_LIST_INIT(rdatalist.rdata);
	ISC_LIST_APPEND(rdatalist.rdata, &added_rdata, link);

	dns_result = dns_rdatalist_tordataset(&rdatalist, &added_rdataset);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdatalist_tordataset failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_addrdataset(db, nodep, nversionp, 0,
				&added_rdataset, 0, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_addrdataset failed %s\n",
				dns_result_totext(dns_result));
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/* close and commit the version */
	dns_db_closeversion(db, &nversionp, ISC_TRUE);
	dns_db_detachnode(db, &nodep);
	nodep = NULL;

	/* open the current version and check changes */
	dns_fixedname_init(&dns_foundname);
	dns_rdataset_init(&found_rdataset);
	cversionp = NULL;
	dns_db_currentversion(db, &cversionp);

	/* find the recently added name and rdata */
	dns_result = dns_db_find(db,
			dns_fixedname_name(&dns_newname),
			cversionp,
			new_rdatatype,
			0,
			0,
			&nodep,
			dns_fixedname_name(&dns_foundname),
			&found_rdataset, NULL);

	if (dns_result != ISC_R_SUCCESS) {
		/* XXXWPK NXRRSET ??? reference counting ??? */
		t_info("dns_db_find failed %s\n",
				dns_result_totext(dns_result));
		dns_db_closeversion(db, &cversionp, ISC_FALSE);
		dns_db_detachnode(db, &nodep);
		if (dns_rdataset_isassociated(&found_rdataset))
			dns_rdataset_disassociate(&found_rdataset);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_FAIL);
	}

	dns_result = dns_rdataset_first(&found_rdataset);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdataset_first failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		if (dns_rdataset_isassociated(&found_rdataset))
			dns_rdataset_disassociate(&found_rdataset);
		dns_db_closeversion(db, &cversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_FAIL);
	}

	/*
	 * Now make sure its what we expect.
	 */
	dns_rdata_init(&found_rdata);
	dns_rdataset_current(&found_rdataset, &found_rdata);
	rval = dns_rdata_compare(&added_rdata, &found_rdata);
	if (rval != 0) {
		t_info("dns_rdata_compare returned %d\n", rval);
		++nfails;
	}

	/*
	 * Now check the rdata deletion.
	 */

	if (dns_rdataset_isassociated(&found_rdataset))
		dns_rdataset_disassociate(&found_rdataset);
	dns_rdataset_init(&found_rdataset);
	dns_db_detachnode(db, &nodep);
	nodep = NULL;
	dns_fixedname_init(&dns_foundname);

	dns_result = dns_db_find(db, dns_fixedname_name(&dns_existingname),
				 cversionp, existing_rdatatype,
				 0, 0, &nodep,
				 dns_fixedname_name(&dns_foundname),
				 &found_rdataset, NULL);


	if ((dns_result != ISC_R_NOTFOUND) && (dns_result != DNS_R_NXDOMAIN)) {
		dns_rdataset_disassociate(&found_rdataset);
		dns_db_detachnode(db, &nodep);
		t_info("dns_db_find %s returned %s\n", existing_name,
		       dns_result_totext(dns_result));
		++nfails;
	}

	dns_db_closeversion(db, &cversionp, ISC_FALSE);
	dns_db_detach(&db);
	isc_mem_destroy(&mctx);

	if (nfails == 0)
		result = T_PASS;
	else
		result = T_FAIL;

	return(result);
}

static void
t10(void) {
	int	result;

	t_assert("dns_db_closeversion", 10, T_REQUIRED, a10);
	result = t_eval("dns_db_closeversion_1_data",
			t_dns_db_closeversion_1, 9);
	t_result(result);
}

static const char *a11 =
	"When versionp points to a read-write version and commit is "
	"ISC_FALSE, a call to dns_db_closeversion(db, versionp, commit) "
	"causes all changes made in the version to to be rolled back, "
	"and returns ISC_R_SUCCESS.";

static int
t_dns_db_closeversion_2(char **av) {
	char			*filename;
	char			*db_type;
	char			*origin;
	char			*class;
	char			*model;
	char			*new_name;
	char			*new_type;
	char			*existing_name;
	char			*existing_type;

	int			result;
	int			len;
	int			rval;
	int			nfails;
	dns_db_t		*db;
	isc_result_t		dns_result;
	isc_result_t		isc_result;
	isc_mem_t		*mctx;
	dns_dbnode_t		*nodep;
	isc_textregion_t	textregion;
	isc_buffer_t		name_buffer;
	dns_fixedname_t		dns_newname;
	dns_fixedname_t		dns_foundname;
	dns_fixedname_t		dns_existingname;
	dns_rdata_t		added_rdata = DNS_RDATA_INIT;
	const char *		added_rdata_data;
	dns_rdataset_t		added_rdataset;
	dns_rdata_t		found_rdata = DNS_RDATA_INIT;
	dns_rdataset_t		found_rdataset;
	dns_rdatatype_t		new_rdatatype;
	dns_rdatatype_t		existing_rdatatype;
	dns_rdataclass_t	rdataclass;
	dns_dbversion_t		*nversionp;
	dns_dbversion_t		*cversionp;
	dns_rdatalist_t		rdatalist;

	filename = T_ARG(0);
	db_type = T_ARG(1);
	origin = T_ARG(2);
	class = T_ARG(3);
	model = T_ARG(4);
	new_name = T_ARG(5);
	new_type = T_ARG(6);
	existing_name = T_ARG(7);
	existing_type = T_ARG(8);

	nfails = 0;
	result = T_UNRESOLVED;
	db = NULL;
	mctx = NULL;

	/*
	 * Open a new version, add some data,
	 * remove some data, close with commit, open the current
	 * version and check that changes are present.
	 */

	t_info("testing using file %s and name %s\n", filename, new_name);

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
				isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	dns_result = t_create(db_type, origin, class, model, mctx, &db);
	if (dns_result != ISC_R_SUCCESS) {
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_load(db, filename);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_load returned %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * Remove all rdata for an existing name.
	 */

	dns_fixedname_init(&dns_existingname);
	len = strlen(existing_name);
	isc_buffer_init(&name_buffer, existing_name, len);
	isc_buffer_add(&name_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_existingname),
			&name_buffer, NULL, ISC_FALSE, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
			dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	textregion.base = existing_type;
	textregion.length = strlen(existing_type);
	dns_result = dns_rdatatype_fromtext(&existing_rdatatype, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdatatype_fromtext %s failed %s\n",
				existing_type,
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	nodep = NULL;
	dns_result = dns_db_findnode(db, dns_fixedname_name(&dns_existingname),
				ISC_FALSE, &nodep);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_findnode %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * Open a new version.
	 */
	nversionp = NULL;
	dns_result = dns_db_newversion(db, &nversionp);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_newversion failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_deleterdataset(db, nodep, nversionp,
					   existing_rdatatype, 0);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_deleterdataset failed %s\n",
				dns_result_totext(dns_result));
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * add a new name and associate some rdata with it
	 */

	dns_db_detachnode(db, &nodep);
	nodep = NULL;

	dns_fixedname_init(&dns_newname);
	len = strlen(new_name);
	isc_buffer_init(&name_buffer, new_name, len);
	isc_buffer_add(&name_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_newname),
				       &name_buffer, NULL, ISC_FALSE, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
		       dns_result_totext(dns_result));
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_findnode(db, dns_fixedname_name(&dns_newname),
				     ISC_TRUE, &nodep);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_findnode failed %s\n",
		       dns_result_totext(dns_result));
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	textregion.base = new_type;
	textregion.length = strlen(new_type);
	dns_result = dns_rdatatype_fromtext(&new_rdatatype, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdatatype_fromtext %s failed %s\n",
		       new_type, dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	textregion.base = class;
	textregion.length = strlen(class);
	dns_result = dns_rdataclass_fromtext(&rdataclass, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdataclass_fromtext failed %s\n",
		       dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_rdata_init(&added_rdata);
	added_rdata_data = "\x10\x00\x00\x01";
	DE_CONST(added_rdata_data, added_rdata.data);
	added_rdata.length = 4;
	added_rdata.rdclass = rdataclass;
	added_rdata.type = new_rdatatype;

	dns_rdataset_init(&added_rdataset);
	rdatalist.type = new_rdatatype;
	rdatalist.covers = 0;
	rdatalist.rdclass = rdataclass;
	rdatalist.ttl = 0;
	ISC_LIST_INIT(rdatalist.rdata);
	ISC_LIST_APPEND(rdatalist.rdata, &added_rdata, link);

	dns_result = dns_rdatalist_tordataset(&rdatalist, &added_rdataset);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdatalist_tordataset failed %s\n",
		       dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_addrdataset(db, nodep, nversionp, 0,
				&added_rdataset, 0, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_addrdataset failed %s\n",
		       dns_result_totext(dns_result));
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * Check that our changes took.
	 */
	dns_db_detachnode(db, &nodep);
	nodep = NULL;
	dns_fixedname_init(&dns_foundname);
	dns_rdataset_init(&found_rdataset);

	/*
	 * Find the recently added name and rdata.
	 */
	dns_result = dns_db_find(db, dns_fixedname_name(&dns_newname),
				 nversionp, new_rdatatype, 0, 0, &nodep,
				 dns_fixedname_name(&dns_foundname),
				 &found_rdataset, NULL);

	if ((dns_result == ISC_R_NOTFOUND) ||
	    (dns_result == DNS_R_NXDOMAIN) ||
	    (dns_result == DNS_R_NXRRSET)) {

		t_info("dns_db_find failed %s\n",
		       dns_result_totext(dns_result));
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detachnode(db, &nodep);
		if (dns_rdataset_isassociated(&found_rdataset))
			dns_rdataset_disassociate(&found_rdataset);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_FAIL);
	}

	dns_result = dns_rdataset_first(&found_rdataset);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdataset_first failed %s\n",
				dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		if (dns_rdataset_isassociated(&found_rdataset))
			dns_rdataset_disassociate(&found_rdataset);
		dns_db_closeversion(db, &nversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_FAIL);
	}

	/*
	 * Now make sure its what we expect.
	 */
	dns_rdata_init(&found_rdata);
	dns_rdataset_current(&found_rdataset, &found_rdata);
	rval = dns_rdata_compare(&added_rdata, &found_rdata);
	if (rval != 0) {
		t_info("dns_rdata_compare returned %d\n", rval);
		++nfails;
	}

	/*
	 * Now check the rdata deletion.
	 */
	if (dns_rdataset_isassociated(&found_rdataset))
		dns_rdataset_disassociate(&found_rdataset);
	dns_rdataset_init(&found_rdataset);
	dns_db_detachnode(db, &nodep);
	nodep = NULL;
	dns_fixedname_init(&dns_foundname);

	dns_result = dns_db_find(db,
			dns_fixedname_name(&dns_existingname),
			nversionp,
			existing_rdatatype,
			0,
			0,
			&nodep,
			dns_fixedname_name(&dns_foundname),
			&found_rdataset, NULL);


	if ((dns_result != ISC_R_NOTFOUND) && (dns_result != DNS_R_NXDOMAIN)) {
		t_info("dns_db_find %s returned %s\n", existing_name,
		       dns_result_totext(dns_result));
		if (dns_rdataset_isassociated(&found_rdataset))
			dns_rdataset_disassociate(&found_rdataset);
		dns_db_detachnode(db, &nodep);
		++nfails;
	}


	/*
	 * Close the version without a commit.
	 */
	dns_db_closeversion(db, &nversionp, ISC_FALSE);

	/*
	 * Open the current version and check changes.
	 */
	dns_fixedname_init(&dns_foundname);
	dns_rdataset_init(&found_rdataset);
	cversionp = NULL;
	dns_db_currentversion(db, &cversionp);

	/*
	 * Find the recently added name and rdata.
	 */
	dns_result = dns_db_find(db,
			dns_fixedname_name(&dns_newname),
			cversionp,
			new_rdatatype,
			0,
			0,
			&nodep,
			dns_fixedname_name(&dns_foundname),
			&found_rdataset, NULL);

	if ((dns_result != ISC_R_NOTFOUND) && (dns_result != DNS_R_NXDOMAIN)) {
		t_info("dns_db_find %s returned %s\n", new_name,
				dns_result_totext(dns_result));
		dns_rdataset_disassociate(&found_rdataset);
		dns_db_detachnode(db, &nodep);
		dns_db_closeversion(db, &cversionp, ISC_FALSE);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_FAIL);
	}

	/*
	 * Now check the rdata deletion.
	 */
	nodep = NULL;
	dns_rdataset_init(&found_rdataset);
	dns_fixedname_init(&dns_foundname);

	dns_result = dns_db_find(db, dns_fixedname_name(&dns_existingname),
				 cversionp, existing_rdatatype, 0, 0,
				 &nodep, dns_fixedname_name(&dns_foundname),
				 &found_rdataset, NULL);


	if ((dns_result == ISC_R_NOTFOUND) ||
	    (dns_result == DNS_R_NXDOMAIN) ||
	    (dns_result == DNS_R_NXRRSET)) {

		t_info("dns_db_find %s returned %s\n", existing_name,
		       dns_result_totext(dns_result));
		dns_rdataset_disassociate(&found_rdataset);
		dns_db_detachnode(db, &nodep);
		++nfails;
	}

	dns_db_detachnode(db, &nodep);
	dns_rdataset_disassociate(&found_rdataset);
	dns_db_closeversion(db, &cversionp, ISC_FALSE);
	dns_db_detach(&db);
	isc_mem_destroy(&mctx);

	if (nfails == 0)
		result = T_PASS;
	else
		result = T_FAIL;

	return(result);
}

static void
t11(void) {
	int	result;

	t_assert("dns_db_closeversion", 11, T_REQUIRED, a11);
	result = t_eval("dns_db_closeversion_2_data",
			t_dns_db_closeversion_2, 9);
	t_result(result);
}

static const char *a12 =
	"A call to dns_db_expirenode() marks as stale all records at node  "
	"which expire at or before 'now'. If 'now' is zero, then the current  "
	"time will be used.";

static int
t_dns_db_expirenode(char **av) {
	char			*filename;
	char			*db_type;
	char			*origin;
	char			*class;
	char			*existing_name;
	char			*node_xtime;
	char			*find_xtime;
	char			*exp_find_result;

	int			result;
	int			len;
	dns_db_t		*db;
	isc_result_t		dns_result;
	isc_result_t		exp_result;
	isc_result_t		isc_result;
	isc_mem_t		*mctx;
	dns_dbnode_t		*nodep;
	isc_buffer_t		name_buffer;
	dns_fixedname_t		dns_foundname;
	dns_fixedname_t		dns_existingname;
	isc_stdtime_t		node_expire_time;
	isc_stdtime_t		find_expire_time;
	isc_stdtime_t		now;
	dns_rdataset_t		rdataset;

	filename = T_ARG(0);
	db_type = T_ARG(1);
	origin = T_ARG(2);
	class = T_ARG(3);
	existing_name = T_ARG(4);
	node_xtime = T_ARG(5);
	find_xtime = T_ARG(6);
	exp_find_result = T_ARG(7);

	result = T_UNRESOLVED;

	/*
	 * Find a node, mark it as stale, do a dns_db_find on the name and
	 * expect it to fail.
	 */

	t_info("testing using file %s and name %s\n", filename, existing_name);

	node_expire_time = (isc_stdtime_t) strtol(node_xtime, NULL, 10);
	find_expire_time = (isc_stdtime_t) strtol(find_xtime, NULL, 10);
	exp_result = t_dns_result_fromtext(exp_find_result);

	isc_stdtime_get(&now);

	dns_fixedname_init(&dns_existingname);
	len = strlen(existing_name);
	isc_buffer_init(&name_buffer, existing_name, len);
	isc_buffer_add(&name_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_existingname),
				       &name_buffer, NULL, ISC_FALSE, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
		       dns_result_totext(dns_result));
		return(T_UNRESOLVED);
	}

	mctx = NULL;
	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
		       isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	db = NULL;
	dns_result = t_create(db_type, origin, class, "cache", mctx, &db);
	if (dns_result != ISC_R_SUCCESS) {
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_load(db, filename);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_load returned %s\n",
		       dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	nodep = NULL;

	/*
	 * Check that the node is there.
	 */
	dns_result = dns_db_findnode(db, dns_fixedname_name(&dns_existingname),
				     ISC_FALSE, &nodep);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("unable to find %s\n", existing_name);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * Expire it.
	 */
	if (node_expire_time != 0)
		node_expire_time += now;

	dns_result = dns_db_expirenode(db, nodep, node_expire_time);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_expirenode failed %s\n",
		       dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_FAIL);
	}

	dns_fixedname_init(&dns_foundname);
	dns_rdataset_init(&rdataset);
	dns_db_detachnode(db, &nodep);
	nodep = NULL;

	if (find_expire_time != 0)
		find_expire_time += now;

	dns_result = dns_db_find(db,
				 dns_fixedname_name(&dns_existingname),
				 NULL,
				 dns_rdatatype_any,
				 0,
				 find_expire_time,
				 &nodep,
				 dns_fixedname_name(&dns_foundname),
				 &rdataset, NULL);

	if (dns_result == exp_result) {
		result = T_PASS;
	} else {
		t_info("dns_db_find %s returned %s\n", existing_name,
		       dns_result_totext(dns_result));
		result = T_FAIL;
	}

	if ((dns_result != ISC_R_NOTFOUND) &&
	    (dns_result != DNS_R_NXDOMAIN) &&
	    (dns_result != DNS_R_NXRRSET)) {

		/*
		 * Don't need to disassociate the rdataset because
		 * we're searching with dns_rdatatype_any.
		 */
		dns_db_detachnode(db, &nodep);
	}


	dns_db_detach(&db);
	isc_mem_destroy(&mctx);

	return(result);
}

static void
t12(void) {
	int	result;

	t_assert("dns_db_expirenode", 12, T_REQUIRED, a12);
	result = t_eval("dns_db_expirenode_data", t_dns_db_expirenode, 8);
	t_result(result);
}

static const char *a13 =
	"If the node name exists, then a call to "
	"dns_db_findnode(db, name, ISC_FALSE, nodep) initializes nodep "
	"to point to the node and returns ISC_R_SUCCESS, otherwise "
	"it returns ISC_R_NOTFOUND.";

static int
t_dns_db_findnode_1(char **av) {
	char		*filename;
	char		*db_type;
	char		*origin;
	char		*class;
	char		*model;
	char		*find_name;
	char		*find_type;
	char		*expected_result;

	int			result;
	int			len;
	dns_db_t		*db;
	isc_result_t		dns_result;
	isc_result_t		isc_result;
	isc_mem_t		*mctx;
	dns_dbnode_t		*nodep;
	isc_buffer_t		name_buffer;
	dns_rdataset_t		rdataset;
	dns_rdatatype_t		rdatatype;
	isc_textregion_t	textregion;
	dns_fixedname_t		dns_name;
	dns_dbversion_t		*cversionp;
	isc_result_t		exp_result;

	filename = T_ARG(0);
	db_type = T_ARG(1);
	origin = T_ARG(2);
	class = T_ARG(3);
	model = T_ARG(4);
	find_name = T_ARG(5);
	find_type = T_ARG(6);
	expected_result = T_ARG(7);

	db = NULL;
	mctx = NULL;
	result = T_UNRESOLVED;

	t_info("testing using file %s and name %s\n", filename, find_name);

	exp_result = t_dns_result_fromtext(expected_result);

	textregion.base = find_type;
	textregion.length = strlen(find_type);
	dns_result = dns_rdatatype_fromtext(&rdatatype, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdatatype_fromtext %s failed %s\n",
				find_type,
				dns_result_totext(dns_result));
		return(T_UNRESOLVED);
	}

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
				isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	dns_result = t_create(db_type, origin, class, model, mctx, &db);
	if (dns_result != ISC_R_SUCCESS) {
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_load(db, filename);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_load returned %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	nodep = NULL;
	dns_fixedname_init(&dns_name);

	len = strlen(find_name);
	isc_buffer_init(&name_buffer, find_name, len);
	isc_buffer_add(&name_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_name),
				&name_buffer, NULL, ISC_FALSE, NULL);

	dns_result = dns_db_findnode(db, dns_fixedname_name(&dns_name),
				ISC_FALSE, &nodep);
	if (dns_result != exp_result) {
		t_info("dns_db_findnode failed %s\n",
				dns_result_totext(dns_result));
		if (dns_result == ISC_R_SUCCESS)
			dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_FAIL);
	}

	/*
	 * if we're expecting the find to succeed and it did,
	 * check that the node has been initialized
	 * by checking for the specified type of rdata
	 * and expecting the search to succeed
	 */

	if (dns_result == ISC_R_SUCCESS) {
		cversionp = NULL;
		dns_db_currentversion(db, &cversionp);
		dns_rdataset_init(&rdataset);

		dns_result = dns_db_findrdataset(db, nodep, cversionp,
						 rdatatype, 0,
						 0, &rdataset, NULL);
		if (dns_result == ISC_R_SUCCESS) {
			dns_rdataset_disassociate(&rdataset);
			result = T_PASS;
		} else {
			t_info("dns_db_findrdataset failed %s\n",
					dns_result_totext(dns_result));
			result = T_FAIL;
		}
		dns_db_closeversion(db, &cversionp, ISC_FALSE);
		dns_db_detachnode(db, &nodep);
	} else {
		result = T_PASS;
	}

	dns_db_detach(&db);
	isc_mem_destroy(&mctx);

	return(result);
}

static void
t13(void) {
	int	result;

	t_assert("dns_db_findnode", 13, T_REQUIRED, a13);
	result = t_eval("dns_db_findnode_1_data", t_dns_db_findnode_1, 8);
	t_result(result);
}

static const char *a14 =
	"If the node name does not exist and create is ISC_TRUE, "
	"then a call to dns_db_findnode(db, name, create, nodep) "
	"creates the node, initializes nodep to point to the node, "
	"and returns ISC_R_SUCCESS.";

static int
t_dns_db_findnode_2(char **av) {
	char			*filename;
	char			*db_type;
	char			*origin;
	char			*class;
	char			*model;
	char			*newname;

	int			nfails;
	int			result;
	int			len;
	dns_db_t		*db;
	isc_result_t		dns_result;
	isc_result_t		isc_result;
	isc_mem_t		*mctx;
	dns_dbnode_t		*nodep;
	dns_dbnode_t		*newnodep;
	isc_buffer_t		name_buffer;
	dns_rdataset_t		rdataset;
	dns_fixedname_t		dns_name;
	dns_fixedname_t		dns_foundname;
	dns_dbversion_t		*cversionp;

	filename = T_ARG(0);
	db_type = T_ARG(1);
	origin = T_ARG(2);
	class = T_ARG(3);
	model = T_ARG(4);
	newname = T_ARG(5);

	result = T_UNRESOLVED;
	db = NULL;
	mctx = NULL;
	nfails = 0;

	t_info("testing using file %s and name %s\n", filename, newname);

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
				isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	dns_result = t_create(db_type, origin, class, model, mctx, &db);
	if (dns_result != ISC_R_SUCCESS) {
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_load(db, filename);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_load returned %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	nodep = NULL;
	dns_fixedname_init(&dns_name);

	/*
	 * Make sure the name isn't there
	 */
	len = strlen(newname);
	isc_buffer_init(&name_buffer, newname, len);
	isc_buffer_add(&name_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_name),
				       &name_buffer, NULL, ISC_FALSE, NULL);

	dns_result = dns_db_findnode(db, dns_fixedname_name(&dns_name),
				     ISC_FALSE, &nodep);
	if ((dns_result != ISC_R_NOTFOUND) &&
	    (dns_result != DNS_R_NXDOMAIN) &&
	    (dns_result != DNS_R_NXRRSET)) {
		t_info("dns_db_findnode %s\n",
		       dns_result_totext(dns_result));
		dns_db_detachnode(db, &nodep);
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	/*
	 * Add it.
	 */
	dns_result = dns_db_findnode(db, dns_fixedname_name(&dns_name),
				ISC_TRUE, &nodep);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_findnode %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_FAIL);
	}

	/*
	 * Check it.
	 */
	newnodep = NULL;
	dns_rdataset_init(&rdataset);
	dns_fixedname_init(&dns_foundname);
	cversionp = NULL;
	dns_db_currentversion(db, &cversionp);

	/*
	 * First try dns_db_find DNS_R_NXDOMAIN.
	 */
	dns_result = dns_db_find(db,
			dns_fixedname_name(&dns_name),
			cversionp,
			dns_rdatatype_any,
			0,
			0,
			&newnodep,
			dns_fixedname_name(&dns_foundname),
			&rdataset, NULL);
	if ((dns_result != ISC_R_NOTFOUND) && (dns_result != DNS_R_NXDOMAIN)) {
		dns_db_detachnode(db, &newnodep);
	}

	if (dns_result != DNS_R_NXDOMAIN) {
		t_info("dns_db_find %s\n",
				dns_result_totext(dns_result));
		++nfails;
	}

	/*
	 * Then try dns_db_findnode ISC_R_SUCCESS.
	 */
	dns_result = dns_db_findnode(db, dns_fixedname_name(&dns_name),
				     ISC_FALSE, &newnodep);
	t_info("dns_db_findnode %s\n", dns_result_totext(dns_result));
	if (dns_result == ISC_R_SUCCESS) {
		dns_db_detachnode(db, &newnodep);
	} else {
		t_info("dns_db_findnode %s failed %s\n", newname,
				dns_result_totext(dns_result));
		++nfails;
	}


	dns_db_detachnode(db, &nodep);
	dns_db_closeversion(db, &cversionp, ISC_FALSE);
	dns_db_detach(&db);
	isc_mem_destroy(&mctx);

	if (nfails == 0)
		result = T_PASS;
	else
		result = T_FAIL;

	return(result);
}

static void
t14(void) {
	int	result;

	t_assert("dns_db_findnode", 14, T_REQUIRED, a14);
	result = t_eval("dns_db_findnode_2_data", t_dns_db_findnode_2, 6);
	t_result(result);
}

static int
t_dns_db_find_x(char **av) {
	char			*dbfile;
	char			*dbtype;
	char			*dborigin;
	char			*dbclass;
	char			*dbmodel;
	char			*findname;
	char			*findtype;
	char			*findopts;
	char			*findtime;
	char			*expected_result;

	int			result;
	int			len;
	int			opts;
	dns_db_t		*db;
	isc_result_t		dns_result;
	isc_result_t		isc_result;
	isc_stdtime_t		ftime;
	isc_stdtime_t		now;
	isc_result_t		exp_result;
	isc_mem_t		*mctx;
	dns_dbnode_t		*nodep;
	isc_textregion_t	textregion;
	isc_buffer_t		findname_buffer;
	dns_fixedname_t		dns_findname;
	dns_fixedname_t		dns_foundname;
	dns_rdataset_t		rdataset;
	dns_rdatatype_t		rdatatype;
	dns_dbversion_t		*cversionp;

	result = T_UNRESOLVED;

	dbfile = T_ARG(0);
	dbtype = T_ARG(1);
	dborigin = T_ARG(2);
	dbclass = T_ARG(3);
	dbmodel = T_ARG(4);
	findname = T_ARG(5);
	findtype = T_ARG(6);
	findopts = T_ARG(7);
	findtime = T_ARG(8);
	expected_result = T_ARG(9);
	db = NULL;
	mctx = NULL;
	opts = 0;

	t_info("testing using %s, name %s, type %s\n", dbfile, findname,
	       findtype);

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
				isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	dns_result = t_create(dbtype, dborigin, dbclass, dbmodel, mctx, &db);
	if (dns_result != ISC_R_SUCCESS) {
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	dns_result = dns_db_load(db, dbfile);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_db_load returned %s\n",
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	exp_result = t_dns_result_fromtext(expected_result);

	dns_fixedname_init(&dns_findname);
	len = strlen(findname);
	isc_buffer_init(&findname_buffer, findname, len);
	isc_buffer_add(&findname_buffer, len);
	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_findname),
				&findname_buffer, NULL, ISC_FALSE, NULL);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
			dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	textregion.base = findtype;
	textregion.length = strlen(findtype);
	dns_result = dns_rdatatype_fromtext(&rdatatype, &textregion);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rdatatype_fromtext %s failed %s\n",
				findtype,
				dns_result_totext(dns_result));
		dns_db_detach(&db);
		isc_mem_destroy(&mctx);
		return(T_UNRESOLVED);
	}

	if (strstr(findopts, "DNS_DBFIND_GLUEOK"))
		opts |= DNS_DBFIND_GLUEOK;
	if (strstr(findopts, "DNS_DBFIND_VALIDATEGLUE"))
		opts |= DNS_DBFIND_VALIDATEGLUE;

	isc_stdtime_get(&now);

	ftime = strtol(findtime, NULL, 10);
	if (ftime != 0)
		ftime += now;

	cversionp = NULL;
	dns_fixedname_init(&dns_foundname);
	dns_rdataset_init(&rdataset);
	if (dns_db_iszone(db))
		dns_db_currentversion(db, &cversionp);
	nodep = NULL;

	dns_result = dns_db_find(db,
			dns_fixedname_name(&dns_findname),
			cversionp,
			rdatatype,
			opts,
			ftime,
			&nodep,
			dns_fixedname_name(&dns_foundname),
			&rdataset, NULL);

	if (dns_result != exp_result) {
		t_info("dns_db_find %s %s unexpectedly returned %s, "
		       "expected %s\n",
		       findname, findtype, dns_result_totext(dns_result),
		       dns_result_totext(exp_result));
		result = T_FAIL;
	} else {
		result = T_PASS;
	}

	if ((dns_result != ISC_R_NOTFOUND) && (dns_result != DNS_R_NXDOMAIN)) {

		if ((dns_result != DNS_R_NXRRSET) &&
		    (dns_result != DNS_R_ZONECUT))
			if (dns_rdataset_isassociated(&rdataset))
				dns_rdataset_disassociate(&rdataset);
		dns_db_detachnode(db, &nodep);
	}

	if (dns_db_iszone(db))
		dns_db_closeversion(db, &cversionp, ISC_FALSE);
	dns_db_detach(&db);
	isc_mem_destroy(&mctx);

	return(result);
}

static const char *a15 =
	"A call to dns_db_find(db, name, version, type, options, now, ...)  "
	"finds the best match for 'name' and 'type' in version 'version' "
	"of 'db'.";

static void
t15(void) {
	int	result;

	t_assert("dns_db_find", 15, T_REQUIRED, a15);
	result = t_eval("dns_db_find_1_data", t_dns_db_find_x, 10);
	t_result(result);
}


static const char *a16 =
	"When the desired node and type were found, but are glue, "
	"and the DNS_DBFIND_GLUEOK option is set, a call to "
	"dns_db_find(db, name, version, type, options, now, ...)  "
	"returns DNS_R_GLUE.";

static void
t16(void) {
	int	result;

	t_assert("dns_db_find", 16, T_REQUIRED, a16);
	result = t_eval("dns_db_find_2_data", t_dns_db_find_x, 10);
	t_result(result);
}

static const char *a17 =
	"A call to dns_db_find() returns DNS_R_DELEGATION when the data "
	"requested is beneath a zone cut.";

static void
t17(void) {
	int	result;

	t_assert("dns_db_find", 17, T_REQUIRED, a17);
	result = t_eval("dns_db_find_3_data", t_dns_db_find_x, 10);
	t_result(result);
}

static const char *a18 =
	"A call to dns_db_find() returns DNS_R_DELEGATION when type is "
	"dns_rdatatype_any and the desired node is a zone cut.";

static void
t18(void) {
	int	result;

	t_assert("dns_db_find", 18, T_REQUIRED, a18);
	result = t_eval("dns_db_find_4_data", t_dns_db_find_x, 10);
	t_result(result);
}

static const char *a19 =
	"A call to dns_db_find() returns DNS_R_DNAME when the data "
	"requested is beneath a DNAME.";

static void
t19(void) {
	int	result;

	t_assert("dns_db_find", 19, T_REQUIRED, a19);
	result = t_eval("dns_db_find_5_data", t_dns_db_find_x, 10);
	t_result(result);
}

static const char *a20 =
	"A call to dns_db_find() returns DNS_R_CNAME when the requested "
	"rdataset was not found but there is a CNAME at the desired name.";

static void
t20(void) {
	int	result;

	t_assert("dns_db_find", 20, T_REQUIRED, a20);
	result = t_eval("dns_db_find_6_data", t_dns_db_find_x, 10);
	t_result(result);
}

static const char *a21 =
	"A call to dns_db_find() returns DNS_R_NXDOMAIN when name "
	"does not exist.";

static void
t21(void) {
	int	result;

	t_assert("dns_db_find", 21, T_REQUIRED, a21);
	result = t_eval("dns_db_find_7_data", t_dns_db_find_x, 10);
	t_result(result);
}

static const char *a22 =
	"A call to dns_db_find() returns DNS_R_NXRRSET when "
	"the desired name exists, but the desired type does not.";

static void
t22(void) {
	int	result;

	t_assert("dns_db_find", 22, T_REQUIRED, a22);
	result = t_eval("dns_db_find_8_data", t_dns_db_find_x, 10);
	t_result(result);
}

static const char *a23 =
	"When db is a cache database, a call to dns_db_find() "
	"returns ISC_R_NOTFOUND when the desired name does not exist, "
	"and no delegation could be found.";

static void
t23(void) {
	int	result;

	t_assert("dns_db_find", 23, T_REQUIRED, a23);
	result = t_eval("dns_db_find_9_data", t_dns_db_find_x, 10);
	t_result(result);
}

static const char *a24 =
	"When db is a cache database, an rdataset will be found only "
	"if at least one rdataset at the found node expires after 'now'.";

static void
t24(void) {
	int	result;

	t_assert("dns_db_find", 24, T_REQUIRED, a24);
	result = t_eval("dns_db_find_10_data", t_dns_db_find_x, 10);
	t_result(result);
}

static const char *a25 =
	"A call to dns_db_load(db, filename) returns DNS_R_NOTZONETOP "
	"when the zone data contains a SOA not at the zone apex.";

static void
t25(void) {
	int	result;

	t_assert("dns_db_load", 25, T_REQUIRED, a25);
	result = t_eval("dns_db_load_soa_not_top", t_dns_db_load, 9);
	t_result(result);
}

testspec_t	T_testlist[] = {
	{	t1,		"dns_db_load"		},
	{	t2,		"dns_db_iscache"	},
	{	t3,		"dns_db_iscache"	},
	{	t4,		"dns_db_iszone"		},
	{	t5,		"dns_db_iszone"		},
	{	t6,		"dns_db_origin"		},
	{	t7,		"dns_db_class"		},
	{	t8,		"dns_db_currentversion"	},
	{	t9,		"dns_db_newversion"	},
	{	t10,		"dns_db_closeversion"	},
	{	t11,		"dns_db_closeversion"	},
	{	t12,		"dns_db_expirenode"	},
	{	t13,		"dns_db_findnode"	},
	{	t14,		"dns_db_findnode"	},
	{	t15,		"dns_db_find"		},
	{	t16,		"dns_db_find"		},
	{	t17,		"dns_db_find"		},
	{	t18,		"dns_db_find"		},
	{	t19,		"dns_db_find"		},
	{	t20,		"dns_db_find"		},
	{	t21,		"dns_db_find"		},
	{	t22,		"dns_db_find"		},
	{	t23,		"dns_db_find"		},
	{	t24,		"dns_db_find"		},
	{	t25,		"dns_db_load"		},
	{	NULL,		NULL			}
};
