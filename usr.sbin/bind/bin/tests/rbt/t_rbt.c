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

/* $ISC: t_rbt.c,v 1.23 2001/01/09 21:42:11 bwelling Exp $ */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>

#include <isc/mem.h>
#include <isc/string.h>

#include <dns/fixedname.h>
#include <dns/rbt.h>
#include <dns/result.h>

#include <tests/t_api.h>

#define		BUFLEN	1024
#define		DNSNAMELEN 255

char		*progname;
char		*Tokens[T_MAXTOKS];

static int
t_dns_rbtnodechain_init(char *dbfile, char *findname,
			char *firstname, char *firstorigin,
			char *nextname, char *nextorigin,
			char *prevname, char *prevorigin,
			char *lastname, char *lastorigin);
static char *
fixedname_totext(dns_fixedname_t *name);

static int
fixedname_cmp(dns_fixedname_t *dns_name, char *txtname);

static char *
dnsname_totext(dns_name_t *name);

static int
t_namechk(isc_result_t dns_result, dns_fixedname_t *dns_name, char *exp_name,
	  dns_fixedname_t *dns_origin, char *exp_origin,
	  isc_result_t exp_result);

/*
 * Parts adapted from the original rbt_test.c.
 */
static int
fixedname_cmp(dns_fixedname_t *dns_name, char *txtname) {
	char	*name;

	name = dnsname_totext(dns_fixedname_name(dns_name));
	if (strcmp(txtname, "NULL") == 0) {
		if ((name == NULL) || (*name == '\0'))
			return(0);
		return(1);
	} else {
		return(strcmp(name, txtname));
	}
}

static char *
dnsname_totext(dns_name_t *name) {
	static char	buf[BUFLEN];
	isc_buffer_t	target;

	isc_buffer_init(&target, buf, BUFLEN);
	dns_name_totext(name, ISC_FALSE, &target);
	*((char *)(target.base) + target.used) = '\0';
	return(target.base);
}

static char *
fixedname_totext(dns_fixedname_t *name) {
	static char	buf[BUFLEN];
	isc_buffer_t	target;

	memset(buf, 0, BUFLEN);
	isc_buffer_init(&target, buf, BUFLEN);
	dns_name_totext(dns_fixedname_name(name), ISC_FALSE, &target);
	*((char *)(target.base) + target.used) = '\0';
	return(target.base);
}

#ifdef	NEED_PRINT_DATA

static isc_result_t
print_data(void *data) {
	isc_result_t	dns_result;
	isc_buffer_t	target;
	char		*buffer[DNSNAMELEN];

	isc_buffer_init(&target, buffer, sizeof(buffer));

	dns_result = dns_name_totext(data, ISC_FALSE, &target);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_totext failed %s\n",
				dns_result_totext(dns_result));
	}
	return(dns_result);
}

#endif	/* NEED_PRINT_DATA */

static int
create_name(char *s, isc_mem_t *mctx, dns_name_t **dns_name) {
	int		nfails;
	int		length;
	isc_result_t	result;
	isc_buffer_t	source;
	isc_buffer_t	target;

	nfails = 0;

	if (s && *s) {

		length = strlen(s);

		isc_buffer_init(&source, s, length);
		isc_buffer_add(&source, length);

		/*
		 * The buffer for the actual name will immediately follow the
		 * name structure.
		 */
		*dns_name = isc_mem_get(mctx, sizeof(**dns_name) + DNSNAMELEN);
		if (*dns_name == NULL) {
			t_info("isc_mem_get failed\n");
			++nfails;
		}

		dns_name_init(*dns_name, NULL);
		isc_buffer_init(&target, *dns_name + 1, DNSNAMELEN);

		result = dns_name_fromtext(*dns_name, &source, dns_rootname,
					   ISC_FALSE, &target);

		if (result != ISC_R_SUCCESS) {
			++nfails;
			t_info("dns_name_fromtext(%s) failed %s\n",
			       s, dns_result_totext(result));
		}
	} else {
		++nfails;
		t_info("create_name: empty name\n");
	}

	return(nfails);
}

static void
delete_name(void *data, void *arg) {
	isc_mem_put((isc_mem_t *)arg, data, sizeof(dns_name_t) + DNSNAMELEN);
}


/*
 * Adapted from the original rbt_test.c.
 */
static int
t1_add(char *name, dns_rbt_t *rbt, isc_mem_t *mctx, isc_result_t *dns_result) {
	int		nprobs;
	dns_name_t	*dns_name;

	nprobs = 0;
	if (name && dns_result) {
		*dns_result = create_name(name, mctx, &dns_name);
		if (*dns_result == ISC_R_SUCCESS) {
			if (T_debug)
				t_info("dns_rbt_addname succeeded\n");
			*dns_result = dns_rbt_addname(rbt, dns_name, dns_name);
		} else {
			t_info("dns_rbt_addname failed %s\n",
		       			dns_result_totext(*dns_result));
			delete_name(dns_name, mctx);
			++nprobs;
		}
	} else {
		++nprobs;
	}
	return(nprobs);
}

static int
t1_delete(char *name, dns_rbt_t *rbt, isc_mem_t *mctx,
	  isc_result_t *dns_result)
{
	int		nprobs;
	dns_name_t	*dns_name;

	nprobs = 0;
	if (name && dns_result) {
		*dns_result = create_name(name, mctx, &dns_name);
		if (*dns_result == ISC_R_SUCCESS) {
			*dns_result = dns_rbt_deletename(rbt, dns_name,
							 ISC_FALSE);
			delete_name(dns_name, mctx);
		} else {
			++nprobs;
		}
	} else {
		++nprobs;
	}
	return(nprobs);
}

static int
t1_search(char *name, dns_rbt_t *rbt, isc_mem_t *mctx,
	  isc_result_t *dns_result)
{
	int		nprobs;
	dns_name_t	*dns_searchname;
	dns_name_t	*dns_foundname;
	dns_fixedname_t	dns_fixedname;
	void		*data;

	nprobs = 0;
	if (name && dns_result) {
		*dns_result = create_name(name, mctx, &dns_searchname);
		if (*dns_result == ISC_R_SUCCESS) {
			dns_fixedname_init(&dns_fixedname);
			dns_foundname = dns_fixedname_name(&dns_fixedname);
			data = NULL;
			*dns_result = dns_rbt_findname(rbt, dns_searchname, 0,
						dns_foundname, &data);
			delete_name(dns_searchname, mctx);
		} else {
			++nprobs;
		}
	} else {
		++nprobs;
	}
	return(nprobs);
}

/*
 * Initialize a database from filename.
 */
static int
rbt_init(char *filename, dns_rbt_t **rbt, isc_mem_t *mctx) {
	int		rval;
	isc_result_t	dns_result;
	char		*p;
	FILE		*fp;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		t_info("No such file %s\n", filename);
		return(1);
	}

	dns_result = dns_rbt_create(mctx, delete_name, mctx, rbt);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rbt_create failed %s\n",
		       		dns_result_totext(dns_result));
		fclose(fp);
		return(1);
	}

	while ((p = t_fgetbs(fp)) != NULL) {

		/*
		 * Skip any comment lines.
		 */
		if ((*p == '#') || (*p == '\0') || (*p == ' ')) {
			free(p);
			continue;
		}

		if (T_debug)
			t_info("adding name %s to the rbt\n", p);

		rval = t1_add(p, *rbt, mctx, &dns_result);
		if ((rval != 0) || (dns_result != ISC_R_SUCCESS)) {
			t_info("add of %s failed\n", p);
			dns_rbt_destroy(rbt);
			fclose(fp);
			return(1);
		}
		(void) free(p);
	}
	fclose(fp);
	return(0);
}

static int
test_rbt_gen(char *filename, char *command, char *testname,
	     isc_result_t exp_result)
{
	int		rval;
	int		result;
	dns_rbt_t	*rbt;
	isc_result_t	isc_result;
	isc_result_t	dns_result;
	isc_mem_t	*mctx;
	dns_name_t	*dns_name;

	result = T_UNRESOLVED;

	if (strcmp(command, "create") != 0)
		t_info("testing using name %s\n", testname);

	mctx = NULL;
	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create: %s: exiting\n",
		       dns_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	rbt = NULL;
	if (rbt_init(filename, &rbt, mctx) != 0) {
		if (strcmp(command, "create") == 0)
			result = T_FAIL;
		isc_mem_destroy(&mctx);
		return(result);
	}

	/*
	 * Now try the database command.
	 */
	if (strcmp(command, "create") == 0) {
		result = T_PASS;
	} else if (strcmp(command, "add") == 0) {
		dns_result = create_name(testname, mctx, &dns_name);
		if (dns_result == ISC_R_SUCCESS) {
			dns_result = dns_rbt_addname(rbt, dns_name, dns_name);

			if (dns_result != ISC_R_SUCCESS)
				delete_name(dns_name, mctx);

			if (dns_result == exp_result) {
				if (dns_result == ISC_R_SUCCESS) {
					rval = t1_search(testname, rbt, mctx,
							 &dns_result);
					if (rval == 0) {
					     if (dns_result == ISC_R_SUCCESS) {
						     result = T_PASS;
					     } else {
						     result = T_FAIL;
					     }
					} else {
						t_info("t1_search failed\n");
						result = T_UNRESOLVED;
					}
				} else {
					result = T_PASS;
				}
			} else {
				t_info("dns_rbt_addname returned %s, "
				       "expected %s\n",
				       dns_result_totext(dns_result),
				       dns_result_totext(exp_result));
				result = T_FAIL;
			}
		} else {
			t_info("create_name failed %s\n",
				dns_result_totext(dns_result));
			result = T_UNRESOLVED;
		}
	} else if ((strcmp(command, "delete") == 0) ||
		   (strcmp(command, "nuke") == 0)) {
		rval = t1_delete(testname, rbt, mctx, &dns_result);
		if (rval == 0) {
			if (dns_result == exp_result) {
				rval = t1_search(testname, rbt, mctx,
						 &dns_result);
				if (rval == 0) {
					if (dns_result == ISC_R_SUCCESS) {
						t_info("dns_rbt_deletename "
						       "didn't delete "
						       "the name");
						result = T_FAIL;
					} else {
						result = T_PASS;
					}
				}
			} else {
				t_info("delete returned %s, expected %s\n",
					dns_result_totext(dns_result),
					dns_result_totext(exp_result));
				result = T_FAIL;
			}
		}
	} else if (strcmp(command, "search") == 0) {
		rval = t1_search(testname, rbt, mctx, &dns_result);
		if (rval == 0) {
			if (dns_result == exp_result) {
				result = T_PASS;
			} else {
				t_info("find returned %s, expected %s\n",
					dns_result_totext(dns_result),
					dns_result_totext(exp_result));
				result = T_FAIL;
			}
		}
	}

	dns_rbt_destroy(&rbt);
	isc_mem_destroy(&mctx);
	return(result);
}

static int
test_dns_rbt_x(const char *filename) {
	FILE		*fp;
	char		*p;
	int		line;
	int		cnt;
	int		result;
	int		nfails;
	int		nprobs;

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

			/*
			 * Name of db file, command, testname,
			 * expected result.
			 */
			cnt = t_bustline(p, Tokens);
			if (cnt == 4) {
				result = test_rbt_gen(Tokens[0], Tokens[1],
					     Tokens[2],
					     t_dns_result_fromtext(Tokens[3]));
				if (result != T_PASS)
					++nfails;
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
	if ((nfails == 0) && (nprobs == 0))
		result = T_PASS;
	else if (nfails)
		result = T_FAIL;

	return(result);
}


static const char *a1 =	"dns_rbt_create creates a rbt and returns "
			"ISC_R_SUCCESS on success";

static void
t1() {
	int	result;

	t_assert("dns_rbt_create", 1, T_REQUIRED, a1);
	result = test_dns_rbt_x("dns_rbt_create_1_data");
	t_result(result);
}

static const char *a2 =	"dns_rbt_addname adds a name to a database and "
			"returns ISC_R_SUCCESS on success";

static void
t2() {
	int	result;

	t_assert("dns_rbt_addname", 2, T_REQUIRED, a2);
	result = test_dns_rbt_x("dns_rbt_addname_1_data");
	t_result(result);
}

static const char *a3 =	"when name already exists, dns_rbt_addname() "
			"returns ISC_R_EXISTS";

static void
t3() {
	int	result;

	t_assert("dns_rbt_addname", 3, T_REQUIRED, a3);
	result = test_dns_rbt_x("dns_rbt_addname_2_data");
	t_result(result);
}

static const char *a4 =	"when name exists, dns_rbt_deletename() returns "
			"ISC_R_SUCCESS";

static void
t4() {
	int	result;

	t_assert("dns_rbt_deletename", 4, T_REQUIRED, a4);
	result = test_dns_rbt_x("dns_rbt_deletename_1_data");
	t_result(result);
}

static const char *a5 =	"when name does not exist, dns_rbt_deletename() "
			"returns ISC_R_NOTFOUND";
static void
t5() {
	int	result;

	t_assert("dns_rbt_deletename", 5, T_REQUIRED, a5);
	result = test_dns_rbt_x("dns_rbt_deletename_2_data");
	t_result(result);
}

static const char *a6 =	"when name exists and exactly matches the "
			"search name dns_rbt_findname() returns ISC_R_SUCCESS";

static void
t6() {
	int	result;

	t_assert("dns_rbt_findname", 6, T_REQUIRED, a6);
	result = test_dns_rbt_x("dns_rbt_findname_1_data");
	t_result(result);
}

static const char *a7 =	"when a name does not exist, "
			"dns_rbt_findname returns ISC_R_NOTFOUND";

static void
t7() {
	int	result;

	t_assert("dns_rbt_findname", 7, T_REQUIRED, a7);
	result = test_dns_rbt_x("dns_rbt_findname_2_data");
	t_result(result);
}

static const char *a8 =	"when a superdomain is found with data matching name, "
			"dns_rbt_findname returns DNS_R_PARTIALMATCH";

static void
t8() {
	int	result;

	t_assert("dns_rbt_findname", 8, T_REQUIRED, a8);
	result = test_dns_rbt_x("dns_rbt_findname_3_data");
	t_result(result);
}


static const char *a9 =	"a call to dns_rbtnodechain_init(chain, mctx) "
			"initializes chain";

static int
t9_walkchain(dns_rbtnodechain_t *chain, dns_rbt_t *rbt) {
	int		cnt;
	int		order;
	unsigned int	nlabels;
	unsigned int	nbits;
	int		nprobs;
	isc_result_t	dns_result;

	dns_fixedname_t	name;
	dns_fixedname_t	origin;
	dns_fixedname_t	fullname1;
	dns_fixedname_t	fullname2;

	cnt = 0;
	nprobs = 0;

	do {

		if (cnt == 0) {
			dns_fixedname_init(&name);
			dns_fixedname_init(&origin);
			dns_result = dns_rbtnodechain_first(chain, rbt,
						dns_fixedname_name(&name),
						dns_fixedname_name(&origin));
			if (dns_result != DNS_R_NEWORIGIN) {
				t_info("dns_rbtnodechain_first returned %s, "
				       "expecting DNS_R_NEWORIGIN\n",
				       dns_result_totext(dns_result));
				++nprobs;
				break;
			}
			t_info("first name:\t<%s>\n", fixedname_totext(&name));
			t_info("first origin:\t<%s>\n",
			       fixedname_totext(&origin));
		} else {
			dns_fixedname_init(&fullname1);
			dns_result = dns_name_concatenate(
			       dns_fixedname_name(&name),
			       dns_name_isabsolute(dns_fixedname_name(&name)) ?
					    NULL : dns_fixedname_name(&origin),
			       dns_fixedname_name(&fullname1), NULL);
			if (dns_result != ISC_R_SUCCESS) {
				t_info("dns_name_concatenate failed %s\n",
				       dns_result_totext(dns_result));
				++nprobs;
				break;
			}

			/*
			 * Expecting origin not to get touched if next
			 * doesn't return NEWORIGIN.
			 */
			dns_fixedname_init(&name);
			dns_result = dns_rbtnodechain_next(chain,
						  dns_fixedname_name(&name),
						  dns_fixedname_name(&origin));
			if ((dns_result != ISC_R_SUCCESS) &&
			    (dns_result != DNS_R_NEWORIGIN)) {
				if (dns_result != ISC_R_NOMORE) {
					t_info("dns_rbtnodechain_next "
					       "failed %s\n",
					       dns_result_totext(dns_result));
					++nprobs;
				}
				break;
			}

			t_info("next name:\t<%s>\n", fixedname_totext(&name));
			t_info("next origin:\t<%s>\n",
			       fixedname_totext(&origin));

			dns_fixedname_init(&fullname2);
			dns_result = dns_name_concatenate(
			       dns_fixedname_name(&name),
			       dns_name_isabsolute(dns_fixedname_name(&name)) ?
			                    NULL : dns_fixedname_name(&origin),
			       dns_fixedname_name(&fullname2), NULL);
			if (dns_result != ISC_R_SUCCESS) {
				t_info("dns_name_concatenate failed %s\n",
				       dns_result_totext(dns_result));
				++nprobs;
				break;
			}

			t_info("comparing\t<%s>\n",
			       fixedname_totext(&fullname1));
			t_info("\twith\t<%s>\n", fixedname_totext(&fullname2));

			(void)dns_name_fullcompare(
						dns_fixedname_name(&fullname1),
						dns_fixedname_name(&fullname2),
						&order, &nlabels, &nbits);

			if (order >= 0) {
			    t_info("unexpected order %s %s %s\n",
			       dnsname_totext(dns_fixedname_name(&fullname1)),
			       order == -1 ? "<" : (order == 0 ? "==" : ">"),
			       dnsname_totext(dns_fixedname_name(&fullname2)));
				++nprobs;
			}
		}

		++cnt;
	} while (1);

	return (nprobs);
}

/*
 * Test by exercising the first|last|next|prev funcs in useful ways.
 */

static int
t_namechk(isc_result_t dns_result, dns_fixedname_t *dns_name, char *exp_name,
	  dns_fixedname_t *dns_origin, char *exp_origin,
	  isc_result_t exp_result)
{
	int	nfails;

	nfails = 0;

	if (fixedname_cmp(dns_name, exp_name)) {
		t_info("\texpected name of %s, got %s\n",
				exp_name, fixedname_totext(dns_name));
		++nfails;
	}
	if (exp_origin != NULL) {
		t_info("checking for DNS_R_NEWORIGIN\n");
		if (dns_result == exp_result) {
			if (fixedname_cmp(dns_origin, exp_origin)) {
				t_info("\torigin %s, expected %s\n",
				       fixedname_totext(dns_origin),
				       exp_origin);
				++nfails;
			}
		} else {
			t_info("\tgot %s\n", dns_result_totext(dns_result));
			++nfails;
		}
	}
	return(nfails);
}

static int
t_dns_rbtnodechain_init(char *dbfile, char *findname,
			char *nextname, char *nextorigin,
			char *prevname, char *prevorigin,
			char *firstname, char *firstorigin,
			char *lastname, char *lastorigin)
{
	int			result;
	int			len;
	int			nfails;
	dns_rbt_t		*rbt;
	dns_rbtnode_t		*node;
	dns_rbtnodechain_t	chain;
	isc_mem_t		*mctx;
	isc_result_t		isc_result;
	isc_result_t		dns_result;
	dns_fixedname_t		dns_findname;
	dns_fixedname_t		dns_foundname;
	dns_fixedname_t		dns_firstname;
	dns_fixedname_t		dns_lastname;
	dns_fixedname_t		dns_prevname;
	dns_fixedname_t		dns_nextname;
	dns_fixedname_t		dns_origin;
	isc_buffer_t		isc_buffer;

	result = T_UNRESOLVED;

	nfails = 0;
	mctx = NULL;
	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
		       isc_result_totext(isc_result));
		return(result);
	}

	dns_rbtnodechain_init(&chain, mctx);

	rbt = NULL;
	if (rbt_init(dbfile, &rbt, mctx)) {
		t_info("rbt_init %s failed\n", dbfile);
		isc_mem_destroy(&mctx);
		return(result);
	}

	len = strlen(findname);
	isc_buffer_init(&isc_buffer, findname, len);
	isc_buffer_add(&isc_buffer, len);

	dns_fixedname_init(&dns_foundname);
	dns_fixedname_init(&dns_findname);
	dns_fixedname_init(&dns_firstname);
	dns_fixedname_init(&dns_origin);
	dns_fixedname_init(&dns_lastname);
	dns_fixedname_init(&dns_prevname);
	dns_fixedname_init(&dns_nextname);

	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_findname),
					&isc_buffer, NULL, ISC_FALSE, NULL);

	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
		       dns_result_totext(dns_result));
		return(result);
	}

	/*
	 * Set the starting node.
	 */
	node = NULL;
	dns_result = dns_rbt_findnode(rbt, dns_fixedname_name(&dns_findname),
				      dns_fixedname_name(&dns_foundname),
				      &node, &chain, DNS_RBTFIND_EMPTYDATA,
				      NULL, NULL);

	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rbt_findnode failed %s\n",
		       dns_result_totext(dns_result));
		return(result);
	}

	/*
	 * Check next.
	 */
	t_info("checking for next name of %s and new origin of %s\n",
	       nextname, nextorigin);
	dns_result = dns_rbtnodechain_next(&chain,
					   dns_fixedname_name(&dns_nextname),
					   dns_fixedname_name(&dns_origin));

	if ((dns_result != ISC_R_SUCCESS) &&
	    (dns_result != DNS_R_NEWORIGIN)) {
		t_info("dns_rbtnodechain_next unexpectedly returned %s\n",
		       dns_result_totext(dns_result));
	}

	nfails += t_namechk(dns_result, &dns_nextname, nextname, &dns_origin,
			    nextorigin, DNS_R_NEWORIGIN);

	/*
	 * Set the starting node again.
	 */
	node = NULL;
	dns_fixedname_init(&dns_foundname);
	dns_result = dns_rbt_findnode(rbt, dns_fixedname_name(&dns_findname),
				      dns_fixedname_name(&dns_foundname),
				      &node, &chain, DNS_RBTFIND_EMPTYDATA,
				      NULL, NULL);

	if (dns_result != ISC_R_SUCCESS) {
		t_info("\tdns_rbt_findnode failed %s\n",
		       dns_result_totext(dns_result));
		return(result);
	}

	/*
	 * Check previous.
	 */
	t_info("checking for previous name of %s and new origin of %s\n",
	       prevname, prevorigin);
	dns_fixedname_init(&dns_origin);
	dns_result = dns_rbtnodechain_prev(&chain,
					   dns_fixedname_name(&dns_prevname),
					   dns_fixedname_name(&dns_origin));

	if ((dns_result != ISC_R_SUCCESS) &&
	    (dns_result != DNS_R_NEWORIGIN)) {
		t_info("dns_rbtnodechain_prev unexpectedly returned %s\n",
		       dns_result_totext(dns_result));
	}
	nfails += t_namechk(dns_result, &dns_prevname, prevname, &dns_origin,
			    prevorigin, DNS_R_NEWORIGIN);

	/*
	 * Check first.
	 */
	t_info("checking for first name of %s and new origin of %s\n",
	       firstname, firstorigin);
	dns_result = dns_rbtnodechain_first(&chain, rbt,
					    dns_fixedname_name(&dns_firstname),
					    dns_fixedname_name(&dns_origin));

	if (dns_result != DNS_R_NEWORIGIN) {
		t_info("dns_rbtnodechain_first unexpectedly returned %s\n",
		       dns_result_totext(dns_result));
	}
	nfails += t_namechk(dns_result, &dns_firstname, firstname,
			    &dns_origin, firstorigin, DNS_R_NEWORIGIN);

	/*
	 * Check last.
	 */
	t_info("checking for last name of %s\n", lastname);
	dns_result = dns_rbtnodechain_last(&chain, rbt,
					   dns_fixedname_name(&dns_lastname),
					   dns_fixedname_name(&dns_origin));

	if (dns_result != DNS_R_NEWORIGIN) {
		t_info("dns_rbtnodechain_last unexpectedly returned %s\n",
		       dns_result_totext(dns_result));
	}
	nfails += t_namechk(dns_result, &dns_lastname, lastname, &dns_origin,
			    lastorigin, DNS_R_NEWORIGIN);

	/*
	 * Check node ordering.
	 */
	t_info("checking node ordering\n");
	nfails += t9_walkchain(&chain, rbt);

	if (nfails)
		result = T_FAIL;
	else
		result = T_PASS;

	dns_rbtnodechain_invalidate(&chain);
	dns_rbt_destroy(&rbt);

	isc_mem_destroy(&mctx);

	return(result);
}

static int
test_dns_rbtnodechain_init(const char *filename) {
	FILE		*fp;
	char		*p;
	int		line;
	int		cnt;
	int		result;
	int		nfails;
	int		nprobs;

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

			cnt = t_bustline(p, Tokens);
			if (cnt == 10) {
				result = t_dns_rbtnodechain_init(
						Tokens[0],  /* dbfile */
						Tokens[1],  /* startname */
						Tokens[2],  /* nextname */
						Tokens[3],  /* nextorigin */
						Tokens[4],  /* prevname */
						Tokens[5],  /* prevorigin */
						Tokens[6],  /* firstname */
						Tokens[7],  /* firstorigin */
						Tokens[8],  /* lastname */
						Tokens[9]); /* lastorigin */
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

	if ((nfails == 0) && (nprobs == 0))
		result = T_PASS;
	else if (nfails)
		result = T_FAIL;

	return(result);
}

static void
t9() {
	int	result;

	t_assert("dns_rbtnodechain_init", 9, T_REQUIRED, a9);
	result = test_dns_rbtnodechain_init("dns_rbtnodechain_init_data");
	t_result(result);
}

static int
t_dns_rbtnodechain_first(char *dbfile, char *expected_firstname,
				char *expected_firstorigin,
				char *expected_nextname,
				char *expected_nextorigin)
{
	int			result;
	int			nfails;
	dns_rbt_t		*rbt;
	dns_rbtnodechain_t	chain;
	isc_mem_t		*mctx;
	isc_result_t		isc_result;
	isc_result_t		dns_result;
	dns_fixedname_t		dns_name;
	dns_fixedname_t		dns_origin;
	isc_result_t		expected_result;

	result = T_UNRESOLVED;

	nfails = 0;
	mctx = NULL;

	dns_fixedname_init(&dns_name);
	dns_fixedname_init(&dns_origin);

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
		       isc_result_totext(isc_result));
		return(result);
	}

	dns_rbtnodechain_init(&chain, mctx);

	rbt = NULL;
	if (rbt_init(dbfile, &rbt, mctx)) {
		t_info("rbt_init %s failed\n", dbfile);
		isc_mem_destroy(&mctx);
		return(result);
	}

	t_info("testing for first name of %s, origin of %s\n",
	       expected_firstname, expected_firstorigin);

	dns_result = dns_rbtnodechain_first(&chain, rbt,
					    dns_fixedname_name(&dns_name),
					    dns_fixedname_name(&dns_origin));

	if (dns_result != DNS_R_NEWORIGIN)
		t_info("dns_rbtnodechain_first unexpectedly returned %s\n",
		       dns_result_totext(dns_result));

	nfails = t_namechk(dns_result, &dns_name, expected_firstname,
			   &dns_origin, expected_firstorigin, DNS_R_NEWORIGIN);

	dns_fixedname_init(&dns_name);
	dns_result = dns_rbtnodechain_next(&chain,
			dns_fixedname_name(&dns_name),
			dns_fixedname_name(&dns_origin));

	t_info("testing for next name of %s, origin of %s\n",
			expected_nextname, expected_nextorigin);

	if ((dns_result != ISC_R_SUCCESS) && (dns_result != DNS_R_NEWORIGIN))
		t_info("dns_rbtnodechain_next unexpectedly returned %s\n",
		       dns_result_totext(dns_result));

	if (strcasecmp(expected_firstorigin, expected_nextorigin) == 0)
		expected_result = ISC_R_SUCCESS;
	else
		expected_result = DNS_R_NEWORIGIN;
	nfails += t_namechk(dns_result, &dns_name, expected_nextname,
			    &dns_origin, expected_nextorigin, expected_result);

	if (nfails)
		result = T_FAIL;
	else
		result = T_PASS;

	dns_rbtnodechain_invalidate(&chain);

	dns_rbt_destroy(&rbt);
	isc_mem_destroy(&mctx);
	return(result);
}

static int
test_dns_rbtnodechain_first(const char *filename) {
	FILE		*fp;
	char		*p;
	int		line;
	int		cnt;
	int		result;
	int		nfails;
	int		nprobs;

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

			cnt = t_bustline(p, Tokens);
			if (cnt == 5) {
				result = t_dns_rbtnodechain_first(
						Tokens[0],  /* dbfile */
						Tokens[1],  /* firstname */
						Tokens[2],  /* firstorigin */
						Tokens[3],  /* nextname */
						Tokens[4]); /* nextorigin */
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

	if ((nfails == 0) && (nprobs == 0))
		result = T_PASS;
	else if (nfails)
		result = T_FAIL;

	return(result);
}

static const char *a10 = "a call to "
			"dns_rbtnodechain_first(chain, rbt, name, origin) "
			"sets name to point to the root of the tree, "
			"origin to point to the origin, "
			"and returns DNS_R_NEWORIGIN";

static void
t10() {
	int	result;

	t_assert("dns_rbtnodechain_first", 10, T_REQUIRED, a10);
	result = test_dns_rbtnodechain_first("dns_rbtnodechain_first_data");
	t_result(result);
}

static int
t_dns_rbtnodechain_last(char *dbfile, char *expected_lastname,
			char *expected_lastorigin,
			char *expected_prevname,
			char *expected_prevorigin)
{

	int			result;
	int			nfails;
	dns_rbt_t		*rbt;
	dns_rbtnodechain_t	chain;
	isc_mem_t		*mctx;
	isc_result_t		isc_result;
	isc_result_t		dns_result;
	dns_fixedname_t		dns_name;
	dns_fixedname_t		dns_origin;
	isc_result_t		expected_result;

	result = T_UNRESOLVED;

	nfails = 0;
	mctx = NULL;

	dns_fixedname_init(&dns_name);
	dns_fixedname_init(&dns_origin);

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
		       isc_result_totext(isc_result));
		return(result);
	}

	dns_rbtnodechain_init(&chain, mctx);

	rbt = NULL;
	if (rbt_init(dbfile, &rbt, mctx)) {
		t_info("rbt_init %s failed\n", dbfile);
		isc_mem_destroy(&mctx);
		return(result);
	}

	t_info("testing for last name of %s, origin of %s\n",
	       expected_lastname, expected_lastorigin);

	dns_result = dns_rbtnodechain_last(&chain, rbt,
					   dns_fixedname_name(&dns_name),
					   dns_fixedname_name(&dns_origin));

	if (dns_result != DNS_R_NEWORIGIN) {
		t_info("dns_rbtnodechain_last unexpectedly returned %s\n",
		       dns_result_totext(dns_result));
	}
	nfails = t_namechk(dns_result, &dns_name, expected_lastname,
			   &dns_origin, expected_lastorigin, DNS_R_NEWORIGIN);

	t_info("testing for previous name of %s, origin of %s\n",
	       expected_prevname, expected_prevorigin);

	dns_fixedname_init(&dns_name);
	dns_result = dns_rbtnodechain_prev(&chain,
					   dns_fixedname_name(&dns_name),
					   dns_fixedname_name(&dns_origin));

	if ((dns_result != ISC_R_SUCCESS) &&
	    (dns_result != DNS_R_NEWORIGIN)) {
		t_info("dns_rbtnodechain_prev unexpectedly returned %s\n",
		       dns_result_totext(dns_result));
	}
	if (strcasecmp(expected_lastorigin, expected_prevorigin) == 0)
		expected_result = ISC_R_SUCCESS;
	else
		expected_result = DNS_R_NEWORIGIN;
	nfails += t_namechk(dns_result, &dns_name, expected_prevname,
			    &dns_origin, expected_prevorigin, expected_result);

	if (nfails)
		result = T_FAIL;
	else
		result = T_PASS;

	dns_rbtnodechain_invalidate(&chain);
	dns_rbt_destroy(&rbt);

	isc_mem_destroy(&mctx);

	return(result);
}

static int
test_dns_rbtnodechain_last(const char *filename) {
	FILE		*fp;
	char		*p;
	int		line;
	int		cnt;
	int		result;
	int		nfails;
	int		nprobs;

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

			cnt = t_bustline(p, Tokens);
			if (cnt == 5) {
				result = t_dns_rbtnodechain_last(
						Tokens[0],     /* dbfile */
						Tokens[1],     /* lastname */
						Tokens[2],     /* lastorigin */
						Tokens[3],     /* prevname */
						Tokens[4]);    /* prevorigin */
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

	if ((nfails == 0) && (nprobs == 0))
		result = T_PASS;
	else if (nfails)
		result = T_FAIL;

	return(result);
}

static const char *a11 = "a call to "
			"dns_rbtnodechain_last(chain, rbt, name, origin) "
			"sets name to point to the last node of the megatree, "
			"origin to the name of the level above it, "
			"and returns DNS_R_NEWORIGIN";

static void
t11() {
	int	result;

	t_assert("dns_rbtnodechain_last", 11, T_REQUIRED, a11);
	result = test_dns_rbtnodechain_last("dns_rbtnodechain_last_data");
	t_result(result);
}

static int
t_dns_rbtnodechain_next(char *dbfile, char *findname,
			char *nextname, char *nextorigin)
{

	int			result;
	int			len;
	int			nfails;
	dns_rbt_t		*rbt;
	dns_rbtnode_t		*node;
	dns_rbtnodechain_t	chain;
	isc_mem_t		*mctx;
	isc_result_t		isc_result;
	isc_result_t		dns_result;
	dns_fixedname_t		dns_findname;
	dns_fixedname_t		dns_foundname;
	dns_fixedname_t		dns_nextname;
	dns_fixedname_t		dns_origin;
	isc_buffer_t		isc_buffer;

	result = T_UNRESOLVED;

	nfails = 0;
	mctx = NULL;
	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
		       isc_result_totext(isc_result));
		return(result);
	}

	dns_rbtnodechain_init(&chain, mctx);

	rbt = NULL;
	if (rbt_init(dbfile, &rbt, mctx)) {
		t_info("rbt_init %s failed\n", dbfile);
		isc_mem_destroy(&mctx);
		return(result);
	}

	len = strlen(findname);
	isc_buffer_init(&isc_buffer, findname, len);
	isc_buffer_add(&isc_buffer, len);

	dns_fixedname_init(&dns_foundname);
	dns_fixedname_init(&dns_findname);
	dns_fixedname_init(&dns_nextname);
	dns_fixedname_init(&dns_origin);

	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_findname),
				       &isc_buffer, NULL, ISC_FALSE, NULL);

	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
		       dns_result_totext(dns_result));
		return(result);
	}

	/*
	 * Set the starting node.
	 */
	node = NULL;
	dns_result = dns_rbt_findnode(rbt, dns_fixedname_name(&dns_findname),
				      dns_fixedname_name(&dns_foundname),
				      &node, &chain, DNS_RBTFIND_EMPTYDATA,
				      NULL, NULL);

	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rbt_findnode failed %s\n",
		       dns_result_totext(dns_result));
		return(result);
	}

	/*
	 * Check next.
	 */
	t_info("checking for next name of %s and new origin of %s\n",
	       nextname, nextorigin);
	dns_result = dns_rbtnodechain_next(&chain,
					   dns_fixedname_name(&dns_nextname),
					   dns_fixedname_name(&dns_origin));

	if ((dns_result != ISC_R_SUCCESS) && (dns_result != DNS_R_NEWORIGIN)) {
		t_info("dns_rbtnodechain_next unexpectedly returned %s\n",
		       dns_result_totext(dns_result));
	}

	nfails += t_namechk(dns_result, &dns_nextname, nextname, &dns_origin,
			    nextorigin, DNS_R_NEWORIGIN);

	if (nfails)
		result = T_FAIL;
	else
		result = T_PASS;

	dns_rbtnodechain_invalidate(&chain);
	dns_rbt_destroy(&rbt);

	isc_mem_destroy(&mctx);

	return(result);
}

static int
test_dns_rbtnodechain_next(const char *filename) {
	FILE		*fp;
	char		*p;
	int		line;
	int		cnt;
	int		result;
	int		nfails;
	int		nprobs;

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

			cnt = t_bustline(p, Tokens);
			if (cnt == 4) {
				result = t_dns_rbtnodechain_next(
						Tokens[0],     /* dbfile */
						Tokens[1],     /* findname */
						Tokens[2],     /* nextname */
						Tokens[3]);    /* nextorigin */
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

	if ((nfails == 0) && (nprobs == 0))
		result = T_PASS;
	else if (nfails)
		result = T_FAIL;

	return(result);
}

static const char *a12 = "a call to "
			"dns_rbtnodechain_next(chain, name, origin) "
			"sets name to point to the next node of the tree "
			"and returns ISC_R_SUCCESS or "
			"DNS_R_NEWORIGIN on success";


static void
t12() {
	int	result;

	t_assert("dns_rbtnodechain_next", 12, T_REQUIRED, a12);
	result = test_dns_rbtnodechain_next("dns_rbtnodechain_next_data");
	t_result(result);
}

static int
t_dns_rbtnodechain_prev(char *dbfile, char *findname, char *prevname,
			char *prevorigin)
{
	int			result;
	int			len;
	int			nfails;
	dns_rbt_t		*rbt;
	dns_rbtnode_t		*node;
	dns_rbtnodechain_t	chain;
	isc_mem_t		*mctx;
	isc_result_t		isc_result;
	isc_result_t		dns_result;
	dns_fixedname_t		dns_findname;
	dns_fixedname_t		dns_foundname;
	dns_fixedname_t		dns_prevname;
	dns_fixedname_t		dns_origin;
	isc_buffer_t		isc_buffer;

	result = T_UNRESOLVED;

	nfails = 0;
	mctx = NULL;
	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
		       isc_result_totext(isc_result));
		return(result);
	}

	dns_rbtnodechain_init(&chain, mctx);

	rbt = NULL;
	if (rbt_init(dbfile, &rbt, mctx)) {
		t_info("rbt_init %s failed\n", dbfile);
		isc_mem_destroy(&mctx);
		return(result);
	}

	len = strlen(findname);
	isc_buffer_init(&isc_buffer, findname, len);
	isc_buffer_add(&isc_buffer, len);

	dns_fixedname_init(&dns_foundname);
	dns_fixedname_init(&dns_findname);
	dns_fixedname_init(&dns_prevname);
	dns_fixedname_init(&dns_origin);

	dns_result = dns_name_fromtext(dns_fixedname_name(&dns_findname),
				       &isc_buffer, NULL, ISC_FALSE, NULL);

	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
		       dns_result_totext(dns_result));
		return(result);
	}

	/*
	 * Set the starting node.
	 */
	node = NULL;
	dns_result = dns_rbt_findnode(rbt, dns_fixedname_name(&dns_findname),
				      dns_fixedname_name(&dns_foundname),
				      &node, &chain, DNS_RBTFIND_EMPTYDATA,
				      NULL, NULL);

	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_rbt_findnode failed %s\n",
		       dns_result_totext(dns_result));
		return(result);
	}

	/*
	 * Check next.
	 */
	t_info("checking for next name of %s and new origin of %s\n",
	       prevname, prevorigin);
	dns_result = dns_rbtnodechain_prev(&chain,
					   dns_fixedname_name(&dns_prevname),
					   dns_fixedname_name(&dns_origin));

	if ((dns_result != ISC_R_SUCCESS) && (dns_result != DNS_R_NEWORIGIN)) {
		t_info("dns_rbtnodechain_prev unexpectedly returned %s\n",
		       dns_result_totext(dns_result));
	}

	nfails += t_namechk(dns_result, &dns_prevname, prevname, &dns_origin,
			    prevorigin, DNS_R_NEWORIGIN);

	if (nfails)
		result = T_FAIL;
	else
		result = T_PASS;

	dns_rbtnodechain_invalidate(&chain);
	dns_rbt_destroy(&rbt);

	isc_mem_destroy(&mctx);

	return(result);
}

static int
test_dns_rbtnodechain_prev(const char *filename) {
	FILE		*fp;
	char		*p;
	int		line;
	int		cnt;
	int		result;
	int		nfails;
	int		nprobs;

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

			cnt = t_bustline(p, Tokens);
			if (cnt == 4) {
				result = t_dns_rbtnodechain_prev(
						Tokens[0],     /* dbfile */
						Tokens[1],     /* findname */
						Tokens[2],     /* prevname */
						Tokens[3]);    /* prevorigin */
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

	if ((nfails == 0) && (nprobs == 0))
		result = T_PASS;
	else if (nfails)
		result = T_FAIL;

	return(result);
}

static const char *a13 = "a call to "
			"dns_rbtnodechain_prev(chain, name, origin) "
			"sets name to point to the previous node of the tree "
			"and returns ISC_R_SUCCESS or "
			"DNS_R_NEWORIGIN on success";

static void
t13() {
	int	result;

	t_assert("dns_rbtnodechain_prev", 13, T_REQUIRED, a13);
	result = test_dns_rbtnodechain_prev("dns_rbtnodechain_prev_data");
	t_result(result);
}



testspec_t	T_testlist[] = {
	{	t1,	"dns_rbt_create"		},
	{	t2,	"dns_rbt_addname 1"		},
	{	t3,	"dns_rbt_addname 2"		},
	{	t4,	"dns_rbt_deletename 1"		},
	{	t5,	"dns_rbt_deletename 2"		},
	{	t6,	"dns_rbt_findname 1"		},
	{	t7,	"dns_rbt_findname 2"		},
	{	t8,	"dns_rbt_findname 3"		},
	{	t9,	"dns_rbtnodechain_init"		},
	{	t10,	"dns_rbtnodechain_first"	},
	{	t11,	"dns_rbtnodechain_last"		},
	{	t12,	"dns_rbtnodechain_next"		},
	{	t13,	"dns_rbtnodechain_prev"		},
	{	NULL,	NULL				}
};

