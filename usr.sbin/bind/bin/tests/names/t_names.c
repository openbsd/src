/*
 * Copyright (C) 1998-2002  Internet Software Consortium.
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

/* $ISC: t_names.c,v 1.32.2.2 2002/08/05 06:57:06 marka Exp $ */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/mem.h>
#include <isc/string.h>

#include <dns/compress.h>
#include <dns/name.h>
#include <dns/result.h>

#include <tests/t_api.h>

#define	MAXTOKS		16
#define	BUFLEN		256
#define	BIGBUFLEN	4096

static const char *a1 =
	"dns_label_countbits returns the number of "
	"bits in a bitstring label";

static char	*Tokens[MAXTOKS + 1];

/*
 * get a hex formatted dns message from a data
 * file into an isc_buffer_t
 * caller supplies data storage and the isc_buffer
 * we read the file, convert, setup the buffer
 * and return the data length
 */

#ifdef	NEED_PBUF

static char *
ctoh(unsigned char c) {
	int		val;
	static char	buf[3];

	val = (c >> 4) & 0x0f;
	if ((0 <= val) && (val <= 9))
		buf[0] = '0' + val;
	else if ((10 <= val) && (val <= 16))
		buf[0] = 'a' + val - 10;
	val = c & 0x0f;
	if ((0 <= val) && (val <= 9))
		buf[1] = '0' + val;
	else if ((10 <= val) && (val <= 16))
		buf[1] = 'a' + val - 10;
	buf[2] = '\0';
	return (buf);
}

static void
pbuf(isc_buffer_t *pbuf) {
	size_t		len;
	unsigned char	*p;

	len = 0;
	p = pbuf->base;
	while (len < pbuf->length) {
		printf("%s", ctoh(*p));
		++p;
		++len;
		if ((len % 40) == 0)
			printf("\n");
	}
}

#endif	/* NEED_PBUF */

/*
 * Compare data at buf with data in hex representation at exp_data,
 * of length exp_data_len, for equality.
 * Return 0 if equal, else non-zero.
 */

static int
chkdata(unsigned char *buf, size_t buflen, char *exp_data,
	size_t exp_data_len)
{
	int		result;
	unsigned char	*p;
	unsigned char	*v;
	char		*q;
	unsigned char	*data;
	size_t		cnt;

	if (buflen == exp_data_len) {
		data = (unsigned char *)malloc(exp_data_len *
					       sizeof(unsigned char));
		if (data == NULL) {
			t_info("malloc failed unexpectedly\n");
			return (-1);
		}

		/*
		 * First convert exp_data from hex format.
		 */
		p = data;
		q = exp_data;
		cnt = 0;
		while (cnt < exp_data_len) {

			if (('0' <= *q) && (*q <= '9'))
				*p = *q - '0';
			else if (('a' <= *q) && (*q <= 'z'))
				*p = *q - 'a' + 10;
			else if (('A' <= *q) && (*q <= 'Z'))
				*p = *q - 'A' + 10;
			++q;

			*p <<= 4;

			if (('0' <= *q) && (*q <= '9'))
				*p |= ((*q - '0') & 0x0f);
			else if (('a' <= *q) && (*q <= 'z'))
				*p |= ((*q - 'a' + 10) & 0x0f);
			else if (('A' <= *q) && (*q <= 'Z'))
				*p |= ((*q - 'A' + 10) & 0x0f);
			++p;
			++q;
			++cnt;
		}

		/*
		 * Now compare data.
		 */
		p = buf;
		v = data;
		for (cnt = 0; cnt < exp_data_len; ++cnt) {
			if (*p != *v)
				break;
			++p;
			++v;
		}
		if (cnt == exp_data_len)
			result = 0;
		else {
			t_info("bad data at position %lu, "
			       "got 0x%.2x, expected 0x%.2x\n",
			       (unsigned long)cnt, *p, *q);
			result = cnt + 1;
		}
		(void)free(data);
	} else {
		t_info("data length error, expected %lu, got %lu\n",
			(unsigned long)exp_data_len, (unsigned long)buflen);
		result = exp_data_len - buflen;
	}
	return (result);
}

/*
 * Get a hex formatted dns message from a data file into an isc_buffer_t.
 * Caller supplies data storage and the isc_buffer.  We read the file, convert,
 * setup the buffer and return the data length.
 */
static int
getmsg(char *datafile_name, unsigned char *buf, int buflen, isc_buffer_t *pbuf)
{
	int			c;
	int			len;
	int			cnt;
	unsigned int		val;
	unsigned char		*p;
	FILE			*fp;

	fp = fopen(datafile_name, "r");
	if (fp == NULL) {
		t_info("No such file %s\n", datafile_name);
		return (0);
	}

	p = buf;
	cnt = 0;
	len = 0;
	val = 0;
	while ((c = getc(fp)) != EOF) {
		if (	(c == ' ') || (c == '\t') ||
			(c == '\r') || (c == '\n'))
				continue;
		if (c == '#') {
			while ((c = getc(fp)) != '\n')
				;
			continue;
		}
		if (('0' <= c) && (c <= '9'))
			val = c - '0';
		else if (('a' <= c) && (c <= 'z'))
			val = c - 'a' + 10;
		else if (('A' <= c) && (c <= 'Z'))
			val = c - 'A'+ 10;
		else {
			t_info("Bad format in datafile\n");
			return (0);
		}
		if ((len % 2) == 0) {
			*p = (val << 4);
		} else {
			*p += val;
			++p;
			++cnt;
			if (cnt >= buflen) {
				/*
				 * Buffer too small.
				 */
				t_info("Buffer overflow error\n");
				return (0);
			}
		}
		++len;
	}
	(void)fclose(fp);

	if (len % 2) {
		t_info("Bad format in %s\n", datafile_name);
		return (0);
	}

	*p = '\0';
	isc_buffer_init(pbuf, buf, cnt);
	isc_buffer_add(pbuf, cnt);
	return (cnt);
}

static int
bustline(char *line, char **toks) {
	int	cnt;
	char	*p;

	cnt = 0;
	if (line && *line) {
		while ((p = strtok(line, "\t")) && (cnt < MAXTOKS)) {
			*toks++ = p;
			line = NULL;
			++cnt;
		}
	}
	return (cnt);
}

/*
 * convert a name from hex representation to text form
 * format of hex notation is:
 * %xXXXXXXXX
 */

#ifdef	NEED_HNAME_TO_TNAME

static int
hname_to_tname(char *src, char *target, size_t len) {
	int		i;
	int		c;
	unsigned int	val;
	size_t		srclen;
	char		*p;
	char		*q;

	p = src;
	srclen = strlen(p);
	if ((srclen >= 2) && ((*p != '%') || (*(p+1) != 'x'))) {
		/*
		 * No conversion needed.
		 */
		if (srclen >= len)
			return (1);
		memcpy(target, src, srclen + 1);
		return (0);
	}

	i = 0;
	p += 2;
	q = target;
	while (*p) {
		c = *p;
		if (('0' < c) && (c <= '9'))
			val = c - '0';
		else if (('a' <= c) && (c <= 'z'))
			val = c + 10 - 'a';
		else if (('A' <= c) && (c <= 'Z'))
			val = c + 10 - 'A';
		else {
			return (1);
		}
		if (i % 2) {
			*q |= val;
			++q;
		} else
			*q = (val << 4);
		++i;
		++p;
	}
	if (i % 2) {
		return (1);
	} else {
		*q = '\0';
		return (0);
	}
}

#endif	/* NEED_HNAME_TO_TNAME */

/*
 * initialize a dns_name_t from a text name, hiding all
 * buffer and other object initialization from the caller
 *
 */

static isc_result_t
dname_from_tname(char *name, dns_name_t *dns_name) {
	int		len;
	isc_buffer_t	txtbuf;
	isc_buffer_t	*binbuf;
	unsigned char	*junk;
	isc_result_t	result;

	len = strlen(name);
	isc_buffer_init(&txtbuf, name, len);
	isc_buffer_add(&txtbuf, len);
	junk = (unsigned char *)malloc(sizeof(unsigned char) * BUFLEN);
	binbuf = (isc_buffer_t *)malloc(sizeof(isc_buffer_t));
	if ((junk != NULL) && (binbuf != NULL)) {
		isc_buffer_init(binbuf, junk, BUFLEN);
		dns_name_init(dns_name, NULL);
		dns_name_setbuffer(dns_name, binbuf);
		result = dns_name_fromtext(dns_name,  &txtbuf,
						NULL, ISC_FALSE, NULL);
	} else {
		result = ISC_R_NOSPACE;
		if (junk != NULL)
			(void)free(junk);
		if (binbuf != NULL)
			(void)free(binbuf);
	}
	return (result);
}

static int
test_dns_label_countbits(char *test_name, int pos, int expected_bits) {
	dns_label_t	label;
	dns_name_t	dns_name;
	int		bits;
	int		rval;
	isc_result_t	result;

	rval = T_UNRESOLVED;
	t_info("testing name %s, label %d\n", test_name, pos);

	result = dname_from_tname(test_name, &dns_name);
	if (result == ISC_R_SUCCESS) {
		dns_name_getlabel(&dns_name, pos, &label);
		bits = dns_label_countbits(&label);
		if (bits == expected_bits)
			rval = T_PASS;
		else {
			t_info("got %d, expected %d\n", bits, expected_bits);
			rval = T_FAIL;
		}
	} else {
		t_info("dname_from_tname %s failed, result = %s\n",
				test_name, dns_result_totext(result));
		rval = T_UNRESOLVED;
	}
	return (rval);
}

static void
t_dns_label_countbits(void) {
	FILE		*fp;
	char		*p;
	int		line;
	int		cnt;
	int		result;

	result = T_UNRESOLVED;
	t_assert("dns_label_countbits", 1, T_REQUIRED, a1);

	fp = fopen("dns_label_countbits_data", "r");
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
			 * testname, labelpos, bitpos, expected val.
			 */
			cnt = bustline(p, Tokens);
			if (cnt == 3) {
				result = test_dns_label_countbits(Tokens[0],
							      atoi(Tokens[1]),
							      atoi(Tokens[2]));
			} else {
				t_info("bad datafile format at line %d\n",
				       line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_label_countbits_data\n");
		t_result(result);
	}
}

static const char *a2 =	"dns_label_getbit returns the n'th most significant "
			"bit of a bitstring label";

static int
test_dns_label_getbit(char *test_name, int label_pos, int bit_pos,
		      int expected_bitval)
{
	dns_label_t	label;
	dns_name_t	dns_name;
	int		bitval;
	int		rval;
	isc_result_t	result;

	rval = T_UNRESOLVED;

	t_info("testing name %s, label %d, bit %d\n",
		test_name, label_pos, bit_pos);

	result = dname_from_tname(test_name, &dns_name);
	if (result == ISC_R_SUCCESS) {
		dns_name_getlabel(&dns_name, label_pos, &label);
		bitval = dns_label_getbit(&label, bit_pos);
		if (bitval == expected_bitval)
			rval = T_PASS;
		else {
			t_info("got %d, expected %d\n", bitval,
					expected_bitval);
			rval = T_FAIL;
		}
	} else {
		t_info("dname_from_tname %s failed, result = %s\n",
				test_name, dns_result_totext(result));
		rval = T_UNRESOLVED;
	}
	return (rval);
}

static void
t_dns_label_getbit(void) {
	int	line;
	int	cnt;
	int	result;
	char	*p;
	FILE	*fp;

	t_assert("dns_label_getbit", 1, T_REQUIRED, a2);

	result = T_UNRESOLVED;
	fp = fopen("dns_label_getbit_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 4) {
				/*
				 * label, bitpos, expected value.
				 */
				result = test_dns_label_getbit(Tokens[0],
							      atoi(Tokens[1]),
							      atoi(Tokens[2]),
							      atoi(Tokens[3]));
			} else {
				t_info("bad datafile format at line %d\n",
						line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_label_getbit_data\n");
		t_result(result);
	}
}

static const char *a3 =	"dns_name_init initializes 'name' to the empty name";

static void
t_dns_name_init(void) {
	int		rval;
	int		result;
	dns_name_t	name;
	unsigned char	offsets[1];

	rval = 0;
	t_assert("dns_name_init", 1, T_REQUIRED, a3);

	dns_name_init(&name, offsets);
	/* magic is hidden in name.c ...
	if (name.magic != NAME_MAGIC) {
		t_info("name.magic is not set to NAME_MAGIC\n");
		++rval;
	}
	*/
	if (name.ndata != NULL) {
		t_info("name.ndata is not NULL\n");
		++rval;
	}
	if (name.length != 0) {
		t_info("name.length is not 0\n");
		++rval;
	}
	if (name.labels != 0) {
		t_info("name.labels is not 0\n");
		++rval;
	}
	if (name.attributes != 0) {
		t_info("name.attributes is not 0\n");
		++rval;
	}
	if (name.offsets != offsets) {
		t_info("name.offsets is incorrect\n");
		++rval;
	}
	if (name.buffer != NULL) {
		t_info("name.buffer is not NULL\n");
		++rval;
	}

	if (rval == 0)
		result = T_PASS;
	else
		result = T_FAIL;
	t_result(result);
}

static const char *a4 =	"dns_name_invalidate invalidates 'name'";

static void
t_dns_name_invalidate(void) {
	int		rval;
	int		result;
	dns_name_t	name;
	unsigned char	offsets[1];

	t_assert("dns_name_invalidate", 1, T_REQUIRED, a4);

	rval = 0;
	dns_name_init(&name, offsets);
	dns_name_invalidate(&name);

	/* magic is hidden in name.c ...
	if (name.magic != 0) {
		t_info("name.magic is not set to NAME_MAGIC\n");
		++rval;
	}
	*/
	if (name.ndata != NULL) {
		t_info("name.ndata is not NULL\n");
		++rval;
	}
	if (name.length != 0) {
		t_info("name.length is not 0\n");
		++rval;
	}
	if (name.labels != 0) {
		t_info("name.labels is not 0\n");
		++rval;
	}
	if (name.attributes != 0) {
		t_info("name.attributes is not 0\n");
		++rval;
	}
	if (name.offsets != NULL) {
		t_info("name.offsets is not NULL\n");
		++rval;
	}
	if (name.buffer != NULL) {
		t_info("name.buffer is not NULL\n");
		++rval;
	}

	if (rval == 0)
		result = T_PASS;
	else
		result = T_FAIL;
	t_result(result);
}

static const char *a5 =	"dns_name_setbuffer dedicates a buffer for use "
			"with 'name'";

static void
t_dns_name_setbuffer(void) {
	int		result;
	unsigned char	junk[BUFLEN];
	dns_name_t	name;
	isc_buffer_t	buffer;

	t_assert("dns_name_setbuffer", 1, T_REQUIRED, a5);

	isc_buffer_init(&buffer, junk, BUFLEN);
	dns_name_init(&name, NULL);
	dns_name_setbuffer(&name, &buffer);
	if (name.buffer == &buffer)
		result = T_PASS;
	else
		result = T_FAIL;

	t_result(result);
}

static const char *a6 =	"dns_name_hasbuffer returns ISC_TRUE if 'name' has a "
			"dedicated buffer, otherwise it returns ISC_FALSE";

static void
t_dns_name_hasbuffer(void) {
	int		result;
	int		rval;
	unsigned char	junk[BUFLEN];
	dns_name_t	name;
	isc_buffer_t	buffer;

	t_assert("dns_name_hasbuffer", 1, T_REQUIRED, a6);

	rval = 0;
	isc_buffer_init(&buffer, junk, BUFLEN);
	dns_name_init(&name, NULL);
	if (dns_name_hasbuffer(&name) != ISC_FALSE)
		++rval;
	dns_name_setbuffer(&name, &buffer);
	if (dns_name_hasbuffer(&name) != ISC_TRUE)
		++rval;
	if (rval == 0)
		result = T_PASS;
	else
		result = T_FAIL;

	t_result(result);
}

static const char *a7 =	"dns_name_isabsolute returns ISC_TRUE if 'name' ends "
			"in the root label";

static int
test_dns_name_isabsolute(char *test_name, isc_boolean_t expected) {
	dns_name_t	name;
	isc_buffer_t	buf;
	isc_buffer_t	binbuf;
	unsigned char	junk[BUFLEN];
	int		len;
	int		rval;
	isc_boolean_t	isabs_p;
	isc_result_t	result;

	rval = T_UNRESOLVED;

	t_info("testing name %s\n", test_name);
	len = strlen(test_name);
	isc_buffer_init(&buf, test_name, len);
	isc_buffer_add(&buf, len);
	isc_buffer_init(&binbuf, &junk[0], BUFLEN);
	dns_name_init(&name, NULL);
	dns_name_setbuffer(&name, &binbuf);
	result = dns_name_fromtext(&name,  &buf, NULL, ISC_FALSE, NULL);
	if (result == ISC_R_SUCCESS) {
		isabs_p = dns_name_isabsolute(&name);
		if (isabs_p == expected)
			rval = T_PASS;
		else
			rval = T_FAIL;
	} else {
		t_info("dns_name_fromtext %s failed, result = %s\n",
				test_name, dns_result_totext(result));
	}
	return (rval);
}

static void
t_dns_name_isabsolute(void) {
	int	line;
	int	cnt;
	int	result;
	char	*p;
	FILE	*fp;

	t_assert("dns_name_isabsolute", 1, T_REQUIRED, a7);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_isabsolute_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 2) {
				/*
				 * label, bitpos, expected value.
				 */
				result = test_dns_name_isabsolute(Tokens[0],
							        atoi(Tokens[1])
								  == 0 ?
								  ISC_FALSE :
								  ISC_TRUE);
			} else {
				t_info("bad datafile format at line %d\n",
				       line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_isabsolute_data\n");
		t_result(result);
	}
}

static const char *a8 =	"dns_name_hash(name, case_sensitive) returns "
		"a hash of 'name' which is case_sensitive if case_sensitive "
		"is true";

/*
 * a9 merged with a8.
 */

static int
test_dns_name_hash(char *test_name1, char *test_name2,
		isc_boolean_t csh_match, isc_boolean_t cish_match) {
	int		rval;
	int		failures;
	isc_boolean_t	match;
	unsigned int	hash1;
	unsigned int	hash2;
	dns_name_t	dns_name1;
	dns_name_t	dns_name2;
	isc_result_t	result;

	rval = T_UNRESOLVED;
	failures = 0;

	t_info("testing names %s and %s\n", test_name1, test_name2);

	result = dname_from_tname(test_name1, &dns_name1);
	if (result == ISC_R_SUCCESS) {
		result = dname_from_tname(test_name2, &dns_name2);
		if (result == ISC_R_SUCCESS) {
			hash1 = dns_name_hash(&dns_name1, ISC_TRUE);
			hash2 = dns_name_hash(&dns_name2, ISC_TRUE);
			match = ISC_FALSE;
			if (hash1 == hash2)
				match = ISC_TRUE;
			if (match != csh_match) {
				++failures;
				t_info("hash mismatch when ISC_TRUE\n");
			}
			hash1 = dns_name_hash(&dns_name1, ISC_FALSE);
			hash2 = dns_name_hash(&dns_name2, ISC_FALSE);
			match = ISC_FALSE;
			if (hash1 == hash2)
				match = ISC_TRUE;
			if (match != cish_match) {
				++failures;
				t_info("hash mismatch when ISC_FALSE\n");
			}
			if (failures == 0)
				rval = T_PASS;
			else
				rval = T_FAIL;
		} else {
			t_info("dns_fromtext %s failed, result = %s\n",
				test_name2, dns_result_totext(result));
		}
	} else {
		t_info("dns_fromtext %s failed, result = %s\n",
				test_name1, dns_result_totext(result));
	}
	return (rval);
}

static void
t_dns_name_hash(void) {
	int	line;
	int	cnt;
	int	result;
	char	*p;
	FILE	*fp;

	t_assert("dns_name_hash", 1, T_REQUIRED, a8);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_hash_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 4) {
				/*
				 * name1, name2, exp match value if
				 * case_sensitive true,
				 * exp match value of case_sensitive false
				 */
				result = test_dns_name_hash(
						Tokens[0],
						Tokens[1],
						atoi(Tokens[2]) == 0 ?
							ISC_FALSE : ISC_TRUE,
						atoi(Tokens[3]) == 0 ?
							ISC_FALSE : ISC_TRUE);
			} else {
				t_info("bad datafile format at line %d\n",
						line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_hash_data\n");
		t_result(result);
	}
}

static const char *a10 =
		"dns_name_fullcompare(name1, name2, orderp, nlabelsp, nbitsp) "
		"returns the DNSSEC ordering relationship between name1 and "
		"name2, sets orderp to -1 if name1 < name2, to 0 if "
		"name1 == name2, or to 1 if name1 > name2, sets nlabelsp "
		"to the number of labels name1 and name2 have in common, "
		"and sets nbitsp to the number of bits name1 and name2 "
		"have in common";

/*
 * a11 thru a22 merged into a10.
 */
static const char *
dns_namereln_to_text(dns_namereln_t reln) {
	const char *p;

	if (reln == dns_namereln_contains)
		p = "contains";
	else if (reln == dns_namereln_subdomain)
		p = "subdomain";
	else if (reln == dns_namereln_equal)
		p = "equal";
	else if (reln == dns_namereln_none)
		p = "none";
	else if (reln == dns_namereln_commonancestor)
		p = "commonancestor";
	else
		p = "unknown";

	return (p);
}

static int
test_dns_name_fullcompare(char *name1, char *name2,
			  dns_namereln_t exp_dns_reln,
			  int exp_order, int exp_nlabels, int exp_nbits)
{
	int		result;
	int		nfails;
	int		order;
	unsigned int	nlabels;
	unsigned int	nbits;
	dns_name_t	dns_name1;
	dns_name_t	dns_name2;
	isc_result_t	dns_result;
	dns_namereln_t	dns_reln;

	nfails = 0;
	result = T_UNRESOLVED;


	t_info("testing names %s and %s for relation %s\n", name1, name2,
	       dns_namereln_to_text(exp_dns_reln));

	dns_result = dname_from_tname(name1, &dns_name1);
	if (dns_result == ISC_R_SUCCESS) {
		dns_result = dname_from_tname(name2, &dns_name2);
		if (dns_result == ISC_R_SUCCESS) {
			dns_reln = dns_name_fullcompare(&dns_name1, &dns_name2,
					&order, &nlabels, &nbits);

			if (dns_reln != exp_dns_reln) {
				++nfails;
				t_info("expected relationship of %s, got %s\n",
					dns_namereln_to_text(exp_dns_reln),
					dns_namereln_to_text(dns_reln));
			}
			/*
			 * Normalize order.
			 */
			if (order < 0)
				order = -1;
			else if (order > 0)
				order = 1;
			if (order != exp_order) {
				++nfails;
				t_info("expected ordering %d, got %d\n",
						exp_order, order);
			}
			if ((exp_nlabels >= 0) &&
			    (nlabels != (unsigned int)exp_nlabels)) {
				++nfails;
				t_info("expecting %d labels, got %d\n",
				       exp_nlabels, nlabels);
			}
			if ((exp_nbits >= 0) &&
			    (nbits != (unsigned int)exp_nbits)) {
				++nfails;
				t_info("expecting %d bits, got %d\n",
				       exp_nbits, nbits);
			}
			if (nfails == 0)
				result = T_PASS;
			else
				result = T_FAIL;
		} else {
			t_info("dname_from_tname failed, result == %s\n",
			       dns_result_totext(result));
		}
	} else {
		t_info("dname_from_tname failed, result == %s\n",
		       dns_result_totext(result));
	}

	return (result);
}

static void
t_dns_name_fullcompare(void) {
	int		line;
	int		cnt;
	int		result;
	char		*p;
	FILE		*fp;
	dns_namereln_t	reln;

	t_assert("dns_name_fullcompare", 1, T_REQUIRED, a10);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_fullcompare_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 6) {
				/*
				 * name1, name2, exp_reln, exp_order,
				 * exp_nlabels, exp_nbits
				 */
				if (!strcmp(Tokens[2], "none"))
					reln = dns_namereln_none;
				else if (!strcmp(Tokens[2], "contains"))
					reln = dns_namereln_contains;
				else if (!strcmp(Tokens[2], "subdomain"))
					reln = dns_namereln_subdomain;
				else if (!strcmp(Tokens[2], "equal"))
					reln = dns_namereln_equal;
				else if (!strcmp(Tokens[2], "commonancestor"))
					reln = dns_namereln_commonancestor;
				else {
					t_info("bad format at line %d\n",
					       line);
					continue;
				}
				result = test_dns_name_fullcompare(
						Tokens[0],
						Tokens[1],
						reln,
						atoi(Tokens[3]),
						atoi(Tokens[4]),
						atoi(Tokens[5]));
			} else {
				t_info("bad format at line %d\n", line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_fullcompare_data\n");
		t_result(result);
	}
}

static const char *a23 =
		"dns_name_compare(name1, name2) returns information about "
		"the relative ordering under the DNSSEC ordering relationship "
		"of name1 and name2";

/*
 * a24 thru a29 merged into a23.
 */

static int
test_dns_name_compare(char *name1, char *name2, int exp_order) {
	int		result;
	int		order;
	isc_result_t	dns_result;
	dns_name_t	dns_name1;
	dns_name_t	dns_name2;

	result = T_UNRESOLVED;

	t_info("testing %s %s %s\n", name1,
	       exp_order == 0 ? "==": (exp_order == -1 ? "<" : ">"),
	       name2);

	dns_result = dname_from_tname(name1, &dns_name1);
	if (dns_result == ISC_R_SUCCESS) {
		dns_result = dname_from_tname(name2, &dns_name2);
		if (dns_result == ISC_R_SUCCESS) {
			order = dns_name_compare(&dns_name1, &dns_name2);
			/*
			 * Normalize order.
			 */
			if (order < 0)
				order = -1;
			else if (order > 0)
				order = 1;
			if (order != exp_order) {
				t_info("expected order of %d, got %d\n",
				       exp_order, order);
				result = T_FAIL;
			} else
				result = T_PASS;
		} else {
			t_info("dname_from_tname failed, result == %s\n",
					dns_result_totext(result));
		}
	} else {
		t_info("dname_from_tname failed, result == %s\n",
				dns_result_totext(result));
	}

	return (result);
}

static void
t_dns_name_compare(void) {
	int		line;
	int		cnt;
	int		result;
	char		*p;
	FILE		*fp;

	t_assert("dns_name_compare", 1, T_REQUIRED, a23);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_compare_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 3) {
				/*
				 * name1, name2, order.
				 */
				result = test_dns_name_compare(
						Tokens[0],
						Tokens[1],
						atoi(Tokens[2]));
			} else {
				t_info("bad datafile format at line %d\n",
				       line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_compare_data\n");
		t_result(result);
	}
}

static const char *a30 =
		"dns_name_rdatacompare(name1, name2) returns information "
		"about the relative ordering of name1 and name2 as if they "
		"are part of rdata in DNSSEC canonical form";

/*
 * a31, a32 merged into a30.
 */

static int
test_dns_name_rdatacompare(char *name1, char *name2, int exp_order) {
	int		result;
	int		order;
	isc_result_t	dns_result;
	dns_name_t	dns_name1;
	dns_name_t	dns_name2;

	result = T_UNRESOLVED;

	t_info("testing %s %s %s\n", name1,
	       exp_order == 0 ? "==": (exp_order == -1 ? "<" : ">"), name2);

	dns_result = dname_from_tname(name1, &dns_name1);
	if (dns_result == ISC_R_SUCCESS) {
		dns_result = dname_from_tname(name2, &dns_name2);
		if (dns_result == ISC_R_SUCCESS) {
			order = dns_name_rdatacompare(&dns_name1, &dns_name2);
			/*
			 * Normalize order.
			 */
			if (order < 0)
				order = -1;
			else if (order > 0)
				order = 1;
			if (order != exp_order) {
				t_info("expected order of %d, got %d\n",
				       exp_order, order);
				result = T_FAIL;
			} else
				result = T_PASS;
		} else {
			t_info("dname_from_tname failed, result == %s\n",
			       dns_result_totext(result));
		}
	} else {
		t_info("dname_from_tname failed, result == %s\n",
		       dns_result_totext(result));
	}

	return (result);
}

static void
t_dns_name_rdatacompare(void) {
	int		line;
	int		cnt;
	int		result;
	char		*p;
	FILE		*fp;

	t_assert("dns_name_rdatacompare", 1, T_REQUIRED, a30);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_rdatacompare_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 3) {
				/*
				 * name1, name2, order.
				 */
				result = test_dns_name_rdatacompare(
						Tokens[0],
						Tokens[1],
						atoi(Tokens[2]));
			} else {
				t_info("bad datafile format at line %d\n",
				       line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_rdatacompare_data\n");
		t_result(result);
	}
}


static const char *a33 =
		"when name1 is a subdomain of name2, "
		"dns_name_issubdomain(name1, name2) returns true, "
		"otherwise it returns false.";

/*
 * a34 merged into a33.
 */

static int
test_dns_name_issubdomain(char *name1, char *name2, isc_boolean_t exp_rval) {
	int		result;
	isc_boolean_t	rval;
	isc_result_t	dns_result;
	dns_name_t	dns_name1;
	dns_name_t	dns_name2;

	result = T_UNRESOLVED;

	t_info("testing %s %s a subdomain of %s\n", name1,
	       exp_rval == 0 ? "is not" : "is", name2);

	dns_result = dname_from_tname(name1, &dns_name1);
	if (dns_result == ISC_R_SUCCESS) {
		dns_result = dname_from_tname(name2, &dns_name2);
		if (dns_result == ISC_R_SUCCESS) {
			rval = dns_name_issubdomain(&dns_name1, &dns_name2);

			if (rval != exp_rval) {
				t_info("expected return value of %s, got %s\n",
				       exp_rval == ISC_TRUE ? "true" : "false",
				       rval == ISC_TRUE ? "true" : "false");
				result = T_FAIL;
			} else
				result = T_PASS;
		} else {
			t_info("dname_from_tname failed, result == %s\n",
			       dns_result_totext(result));
		}
	} else {
		t_info("dname_from_tname failed, result == %s\n",
		       dns_result_totext(result));
	}

	return (result);
}

static void
t_dns_name_issubdomain(void) {
	int		line;
	int		cnt;
	int		result;
	char		*p;
	FILE		*fp;

	t_assert("dns_name_issubdomain", 1, T_REQUIRED, a33);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_issubdomain_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 3) {
				/*
				 * name1, name2, issubdomain_p.
				 */
				result = test_dns_name_issubdomain(
						Tokens[0],
						Tokens[1],
						atoi(Tokens[2]) == 0 ?
						ISC_FALSE : ISC_TRUE);
			} else {
				t_info("bad datafile format at line %d\n",
				       line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_issubdomain_data\n");
		t_result(result);
	}
}

static const char *a35 =
		"dns_name_countlabels(name) returns the number "
		"of labels in name";

static int
test_dns_name_countlabels(char *test_name, unsigned int exp_nlabels) {
	int		result;
	unsigned int	nlabels;
	isc_result_t	dns_result;
	dns_name_t	dns_name;

	result = T_UNRESOLVED;

	t_info("testing %s\n", test_name);

	dns_result = dname_from_tname(test_name, &dns_name);
	if (dns_result == ISC_R_SUCCESS) {
		nlabels = dns_name_countlabels(&dns_name);

		if (nlabels != exp_nlabels) {
			t_info("expected %d, got %d\n", exp_nlabels, nlabels);
			result = T_FAIL;
		} else
			result = T_PASS;
	} else {
		t_info("dname_from_tname failed, result == %s\n",
		       dns_result_totext(dns_result));
	}

	return (result);
}

static void
t_dns_name_countlabels(void) {
	int		line;
	int		cnt;
	int		result;
	char		*p;
	FILE		*fp;

	t_assert("dns_name_countlabels", 1, T_REQUIRED, a35);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_countlabels_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 2) {
				/*
				 * name, nlabels.
				 */
				result = test_dns_name_countlabels(Tokens[0],
							      atoi(Tokens[1]));
			} else {
				t_info("bad format at line %d\n", line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_countlabels_data\n");
		t_result(result);
	}
}

static const char *a36 =
		"when n is less than the number of labels in name, "
		"dns_name_getlabel(name, n, labelp) initializes labelp "
		"to point to the nth label in name";

/*
 * The strategy here is two take two dns names with a shared label in
 * different positions, get the two labels and compare them for equality.
 * If they don't match, dns_name_getlabel failed.
 */

static int
test_dns_name_getlabel(char *test_name1, int label1_pos, char *test_name2,
		       int label2_pos)
{
	int		result;
	int		nfails;
	unsigned int	cnt;
	unsigned char	*p;
	unsigned char	*q;
	dns_name_t	dns_name1;
	dns_name_t	dns_name2;
	dns_label_t	dns_label1;
	dns_label_t	dns_label2;
	isc_result_t	dns_result;

	nfails = 0;
	result = T_UNRESOLVED;

	t_info("testing with %s and %s\n", test_name1, test_name2);

	dns_result = dname_from_tname(test_name1, &dns_name1);
	if (dns_result == ISC_R_SUCCESS) {
		dns_result = dname_from_tname(test_name2, &dns_name2);
		if (dns_result == ISC_R_SUCCESS) {
			dns_name_getlabel(&dns_name1, label1_pos, &dns_label1);
			dns_name_getlabel(&dns_name2, label2_pos, &dns_label2);
			if (dns_label1.length != dns_label2.length) {
				t_info("label lengths differ\n");
				++nfails;
			}
			p = dns_label1.base;
			q = dns_label2.base;
			for (cnt = 0; cnt < dns_label1.length; ++cnt) {
				if (*p++ != *q++) {
					t_info("labels differ at position %d",
					       cnt);
					++nfails;
				}
			}
			if (nfails == 0)
				result = T_PASS;
			else
				result = T_FAIL;
		} else {
			t_info("dname_from_tname failed, result == %s",
			       dns_result_totext(result));
		}
	} else {
		t_info("dname_from_tname failed, result == %s",
		       dns_result_totext(result));
	}
	return (result);
}

static void
t_dns_name_getlabel(void) {
	int		line;
	int		cnt;
	int		result;
	char		*p;
	FILE		*fp;

	t_assert("dns_name_getlabel", 1, T_REQUIRED, a36);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_getlabel_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 4) {
				/*
				 * name1, name2, nlabels.
				 */
				result = test_dns_name_getlabel(Tokens[0],
							      atoi(Tokens[1]),
							           Tokens[2],
							      atoi(Tokens[3]));
			} else {
				t_info("bad format at line %d\n", line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_getlabel_data\n");
		t_result(result);
	}
}

static const char *a37 =
		"when source contains at least first + n labels, "
		"dns_name_getlabelsequence(source, first, n, target) "
		"initializes target to point to the n label sequence of "
		"labels in source starting with first";

/*
 * We adopt a similiar strategy to that used by the dns_name_getlabel test.
 */

static int
test_dns_name_getlabelsequence(char *test_name1, int label1_start,
			       char *test_name2, int label2_start, int range)
{
	int		result;
	int		nfails;
	unsigned int	cnt;
	unsigned char	*p;
	unsigned char	*q;
	dns_name_t	dns_name1;
	dns_name_t	dns_name2;
	dns_name_t	dns_targetname1;
	dns_name_t	dns_targetname2;
	isc_result_t	dns_result;
	isc_buffer_t	buffer1;
	isc_buffer_t	buffer2;
	unsigned char	junk1[BUFLEN];
	unsigned char	junk2[BUFLEN];

	nfails = 0;
	result = T_UNRESOLVED;
	dns_result = dname_from_tname(test_name1, &dns_name1);
	if (dns_result == ISC_R_SUCCESS) {
		dns_result = dname_from_tname(test_name2, &dns_name2);
		if (dns_result == ISC_R_SUCCESS) {
			t_info("testing %s %s\n", test_name1, test_name2);
			dns_name_init(&dns_targetname1, NULL);
			dns_name_init(&dns_targetname2, NULL);
			dns_name_getlabelsequence(&dns_name1, label1_start,
						  range, &dns_targetname1);
			dns_name_getlabelsequence(&dns_name2, label2_start,
						  range, &dns_targetname2);

			/*
			 * Now convert both targets to text for comparison.
			 */
			isc_buffer_init(&buffer1, junk1, BUFLEN);
			isc_buffer_init(&buffer2, junk2, BUFLEN);
			dns_name_totext(&dns_targetname1, ISC_TRUE, &buffer1);
			dns_name_totext(&dns_targetname2, ISC_TRUE, &buffer2);
			if (buffer1.used == buffer2.used) {
				p = buffer1.base;
				q = buffer2.base;
				for (cnt = 0; cnt < buffer1.used; ++cnt) {
					if (*p != *q) {
						++nfails;
						t_info("names differ at %d\n",
						       cnt);
						break;
					}
					++p; ++q;
				}
			} else {
				++nfails;
				t_info("lengths differ\n");
			}
			if (nfails == 0)
				result = T_PASS;
			else
				result = T_FAIL;
		} else {
			t_info("dname_from_tname failed, result == %s",
			       dns_result_totext(dns_result));
		}
	} else {
		t_info("dname_from_tname failed, result == %s",
		       dns_result_totext(dns_result));
	}
	return (result);
}

static void
t_dns_name_getlabelsequence(void) {
	int		line;
	int		cnt;
	int		result;
	char		*p;
	FILE		*fp;

	t_assert("dns_name_getlabelsequence", 1, T_REQUIRED, a37);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_getlabelsequence_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 5) {
				/*
				 * name1, name2, nlabels.
				 */
				result = test_dns_name_getlabelsequence(
								   Tokens[0],
							      atoi(Tokens[1]),
								   Tokens[2],
							      atoi(Tokens[3]),
							      atoi(Tokens[4]));
			} else {
				t_info("bad format at line %d\n", line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_getlabelsequence_data\n");
		t_result(result);
	}
}

static const char *a38 =
		"dns_name_fromregion(name, region) converts a DNS name "
		"from a region representation to a name representation";

static int
test_dns_name_fromregion(char *test_name) {
	int		result;
	int		order;
	unsigned int	nlabels;
	unsigned int	nbits;
	isc_result_t	dns_result;
	dns_name_t	dns_name1;
	dns_name_t	dns_name2;
	dns_namereln_t	dns_namereln;
	isc_region_t	region;

	result = T_UNRESOLVED;

	t_info("testing %s\n", test_name);

	dns_result = dname_from_tname(test_name, &dns_name1);
	if (dns_result == ISC_R_SUCCESS) {

		dns_name_toregion(&dns_name1, &region);

		dns_name_init(&dns_name2, NULL);
		dns_name_fromregion(&dns_name2, &region);
		dns_namereln = dns_name_fullcompare(&dns_name1, &dns_name2,
						    &order, &nlabels, &nbits);
		if (dns_namereln == dns_namereln_equal)
			result = T_PASS;
		else
			result = T_FAIL;
	} else {
		t_info("dname_from_tname failed, result == %s\n",
		       dns_result_totext(result));
	}
	return (result);
}

static void
t_dns_name_fromregion(void) {
	int		line;
	int		cnt;
	int		result;
	char		*p;
	FILE		*fp;

	t_assert("dns_name_fromregion", 1, T_REQUIRED, a38);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_fromregion_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 1) {
				/*
				 * test_name.
				 */
				result = test_dns_name_fromregion(Tokens[0]);
			} else {
				t_info("bad format at line %d\n", line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_fromregion_data\n");
		t_result(result);
	}
}

static const char *a39 =
		"dns_name_toregion(name, region) converts a DNS name "
		"from a region representation to a name representation";

static void
t_dns_name_toregion(void) {
	int		line;
	int		cnt;
	int		result;
	char		*p;
	FILE		*fp;

	t_assert("dns_name_toregion", 1, T_REQUIRED, a39);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_toregion_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 1) {
				/*
				 * test_name.
				 */
				result = test_dns_name_fromregion(Tokens[0]);
			} else {
				t_info("bad format at line %d\n", line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_toregion_data\n");
		t_result(result);
	}
}

static const char *a40 =
		"dns_name_fromtext(name, source, origin, downcase, target) "
		"converts the textual representation of a DNS name at source "
		"into uncompressed wire form at target, appending origin to "
		"the converted name if origin is non-NULL and converting "
		"upper case to lower case during conversion "
		"if downcase is true.";

static int
test_dns_name_fromtext(char *test_name1, char *test_name2, char *test_origin,
		       isc_boolean_t downcase)
{
	int		result;
	int		order;
	unsigned int	nlabels;
	unsigned int	nbits;
	unsigned char	junk1[BUFLEN];
	unsigned char	junk2[BUFLEN];
	unsigned char	junk3[BUFLEN];
	isc_buffer_t	binbuf1;
	isc_buffer_t	binbuf2;
	isc_buffer_t	binbuf3;
	isc_buffer_t	txtbuf1;
	isc_buffer_t	txtbuf2;
	isc_buffer_t	txtbuf3;
	dns_name_t	dns_name1;
	dns_name_t	dns_name2;
	dns_name_t	dns_name3;
	isc_result_t	dns_result;
	dns_namereln_t	dns_namereln;

	result = T_UNRESOLVED;

	t_info("testing %s %s %s\n", test_name1, test_name2, test_origin);

	isc_buffer_init(&binbuf1, junk1, BUFLEN);
	isc_buffer_init(&binbuf2, junk2, BUFLEN);
	isc_buffer_init(&binbuf3, junk3, BUFLEN);

	isc_buffer_init(&txtbuf1, test_name1, strlen(test_name1));
	isc_buffer_add(&txtbuf1, strlen(test_name1));

	isc_buffer_init(&txtbuf2, test_name2, strlen(test_name2));
	isc_buffer_add(&txtbuf2, strlen(test_name2));

	isc_buffer_init(&txtbuf3, test_origin, strlen(test_origin));
	isc_buffer_add(&txtbuf3, strlen(test_origin));
	dns_name_init(&dns_name1, NULL);
	dns_name_init(&dns_name2, NULL);
	dns_name_init(&dns_name3, NULL);
	dns_name_setbuffer(&dns_name1, &binbuf1);
	dns_name_setbuffer(&dns_name2, &binbuf2);
	dns_name_setbuffer(&dns_name3, &binbuf3);

	dns_result = dns_name_fromtext(&dns_name3,  &txtbuf3, NULL,
						ISC_FALSE, &binbuf3);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext(dns_name3) failed, result == %s\n",
			dns_result_totext(dns_result));
		return (T_FAIL);
	}

	dns_result = dns_name_fromtext(&dns_name1, &txtbuf1, &dns_name3,
				       downcase, &binbuf1);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext(dns_name1) failed, result == %s\n",
		       dns_result_totext(dns_result));
		return (T_FAIL);
	}

	dns_result = dns_name_fromtext(&dns_name2,  &txtbuf2, NULL,
						ISC_FALSE, &binbuf2);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext(dns_name2) failed, result == %s\n",
		       dns_result_totext(dns_result));
		return (T_FAIL);
	}

	dns_namereln = dns_name_fullcompare(&dns_name1, &dns_name2, &order,
					    &nlabels, &nbits);

	if (dns_namereln == dns_namereln_equal)
		result = T_PASS;
	else {
		t_info("dns_name_fullcompare returned %s\n",
		       dns_namereln_to_text(dns_namereln));
		result = T_FAIL;
	}

	return (result);
}

static void
t_dns_name_fromtext(void) {
	int		line;
	int		cnt;
	int		result;
	char		*p;
	FILE		*fp;

	t_assert("dns_name_fromtext", 1, T_REQUIRED, a40);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_fromtext_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 4) {
				/*
				 * test_name1, test_name2, test_origin,
				 * downcase.
				 */
				result = test_dns_name_fromtext(Tokens[0],
								Tokens[1],
								Tokens[2],
							   atoi(Tokens[3])
								== 0 ?
								ISC_FALSE :
								ISC_TRUE);
			} else {
				t_info("bad format at line %d\n", line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_fromtext\n");
		t_result(result);
	}
}

static const char *a41 =
		"dns_name_totext(name, omit_final_dot, target) converts "
		"the DNS name 'name' in wire format to textual format "
		"at target, and adds a final '.' to the name if "
		"omit_final_dot is false";

static int
test_dns_name_totext(char *test_name, isc_boolean_t omit_final) {
	int		result;
	int		len;
	int		order;
	unsigned int	nlabels;
	unsigned int	nbits;
	unsigned char	junk1[BUFLEN];
	unsigned char	junk2[BUFLEN];
	unsigned char	junk3[BUFLEN];
	isc_buffer_t	buf1;
	isc_buffer_t	buf2;
	isc_buffer_t	buf3;
	dns_name_t	dns_name1;
	dns_name_t	dns_name2;
	isc_result_t	dns_result;
	dns_namereln_t	dns_namereln;

	result = T_UNRESOLVED;

	t_info("testing %s\n", test_name);

	len = strlen(test_name);
	isc_buffer_init(&buf1, test_name, len);
	isc_buffer_add(&buf1, len);

	dns_name_init(&dns_name1, NULL);
	isc_buffer_init(&buf2, junk2, BUFLEN);

	/*
	 * Out of the data file to dns_name1.
	 */
	dns_result = dns_name_fromtext(&dns_name1, &buf1, NULL, ISC_FALSE,
				       &buf2);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed, result == %s\n",
		       dns_result_totext(dns_result));
		return (T_UNRESOLVED);
	}

	/*
	 * From dns_name1 into a text buffer.
	 */
	isc_buffer_invalidate(&buf1);
	isc_buffer_init(&buf1, junk1, BUFLEN);
	dns_result = dns_name_totext(&dns_name1, omit_final, &buf1);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_totext failed, result == %s\n",
		       dns_result_totext(dns_result));
		return (T_FAIL);
	}

	/*
	 * From the text buffer into dns_name2.
	 */
	dns_name_init(&dns_name2, NULL);
	isc_buffer_init(&buf3, junk3, BUFLEN);
	dns_result = dns_name_fromtext(&dns_name2, &buf1, NULL, ISC_FALSE,
				       &buf3);
	if (dns_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed, result == %s\n",
		       dns_result_totext(dns_result));
		return (T_UNRESOLVED);
	}

	dns_namereln = dns_name_fullcompare(&dns_name1, &dns_name2,
					    &order, &nlabels, &nbits);
	if (dns_namereln == dns_namereln_equal)
		result = T_PASS;
	else {
		t_info("dns_name_fullcompare returned %s\n",
		       dns_namereln_to_text(dns_namereln));
		result = T_FAIL;
	}

	return (result);
}

static void
t_dns_name_totext(void) {
	int		line;
	int		cnt;
	int		result;
	char		*p;
	FILE		*fp;

	t_assert("dns_name_totext", 1, T_REQUIRED, a41);

	result = T_UNRESOLVED;
	fp = fopen("dns_name_totext_data", "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 2) {
				/*
				 * test_name, omit_final.
				 */
				result = test_dns_name_totext(Tokens[0],
							 atoi(Tokens[1]) == 0 ?
							      ISC_FALSE :
							      ISC_TRUE);
			} else {
				t_info("bad format at line %d\n", line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile dns_name_totext\n");
		t_result(result);
	}
}

static const char *a42 =
		"dns_name_fromwire(name, source, dctx, downcase, target) "
		"converts the possibly compressed DNS name 'name' in wire "
		"format to canonicalized form at target, performing upper to "
		"lower case conversion if downcase is true, and returns "
		"ISC_R_SUCCESS";

#if 0
	/*
	 * XXXRTH these tests appear to be broken, so I have
	 * disabled them.
	 */
static const char *a43 =
		"when a label length is invalid, dns_name_fromwire() "
		"returns DNS_R_FORMERR";

static const char *a44 =
		"when a label type is invalid, dns_name_fromwire() "
		"returns DNS_R_BADLABELTYPE";
#endif

static const char *a45 =
		"when a name length is invalid, dns_name_fromwire() "
		"returns DNS_R_FORMERR";

static const char *a46 =
		"when a compression type is invalid, dns_name_fromwire() "
		"returns DNS_R_DISALLOWED";

static const char *a47 =
		"when a bad compression pointer is encountered, "
		"dns_name_fromwire() returns DNS_R_BADPOINTER";

static const char *a48 =
		"when input ends unexpected, dns_name_fromwire() "
		"returns ISC_R_UNEXPECTEDEND";

static const char *a49 =
		"when there are too many compression pointers, "
		"dns_name_fromwire() returns DNS_R_TOOMANYHOPS";

static const char *a50 =
		"when there is not enough space in target, "
		"dns_name_fromwire(name, source, dcts, downcase, target) "
		"returns ISC_R_NOSPACE";

static int
test_dns_name_fromwire(char *datafile_name, int testname_offset, int downcase,
		       unsigned int dc_method, char *exp_name,
		       isc_result_t exp_result, size_t buflen)
{
	int			result;
	int			order;
	unsigned int		nlabels;
	unsigned int		nbits;
	int			len;
	unsigned char		buf1[BIGBUFLEN];
	char			buf2[BUFLEN];
	isc_buffer_t		iscbuf1;
	isc_buffer_t		iscbuf2;
	dns_name_t		dns_name1;
	dns_name_t		dns_name2;
	isc_result_t		dns_result;
	dns_namereln_t		dns_namereln;
	dns_decompress_t	dctx;

	result = T_UNRESOLVED;

	t_info("testing using %s\n", datafile_name);
	len = getmsg(datafile_name, buf1, BIGBUFLEN, &iscbuf1);

	isc_buffer_setactive(&iscbuf1, len);
	iscbuf1.current = testname_offset;

	isc_buffer_init(&iscbuf2, buf2, buflen);
	dns_name_init(&dns_name1, NULL);
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_STRICT);
	dns_decompress_setmethods(&dctx, dc_method);
	dns_result = dns_name_fromwire(&dns_name1, &iscbuf1,
				       &dctx, downcase ? ISC_TRUE : ISC_FALSE,
				       &iscbuf2);

	if ((dns_result == exp_result) && (exp_result == ISC_R_SUCCESS)) {

		dns_result = dname_from_tname(exp_name, &dns_name2);
		if (dns_result == ISC_R_SUCCESS) {
			dns_namereln = dns_name_fullcompare(&dns_name1,
							    &dns_name2,
							    &order, &nlabels,
							    &nbits);
			if (dns_namereln != dns_namereln_equal) {
				t_info("dns_name_fullcompare  returned %s\n",
				       dns_namereln_to_text(dns_namereln));
				result = T_FAIL;
			} else {
				result = T_PASS;
			}
		} else {
			t_info("dns_name_fromtext %s failed, result = %s\n",
			       exp_name, dns_result_totext(dns_result));
			result = T_UNRESOLVED;
		}
	} else if (dns_result == exp_result) {
		result = T_PASS;
	} else {
		t_info("dns_name_fromwire returned %s\n",
		       dns_result_totext(dns_result));
		result = T_FAIL;
	}

	return (result);
}

static void
t_dns_name_fromwire_x(const char *testfile, size_t buflen) {
	int		line;
	int		cnt;
	int		result;
	unsigned int	dc_method;
	isc_result_t	exp_result;
	char		*p;
	char		*tok;
	FILE		*fp;

	result = T_UNRESOLVED;
	fp = fopen(testfile, "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 6) {
				/*
				 *	datafile_name, testname_offset,
				 *	downcase, dc_method,
				 *	exp_name, exp_result.
				 */

				tok = Tokens[5];
				exp_result = ISC_R_SUCCESS;
				if (! strcmp(tok, "ISC_R_SUCCESS"))
					exp_result = ISC_R_SUCCESS;
				else if (! strcmp(tok, "ISC_R_NOSPACE"))
					exp_result = ISC_R_NOSPACE;
				else if (! strcmp(tok, "DNS_R_BADLABELTYPE"))
					exp_result = DNS_R_BADLABELTYPE;
				else if (! strcmp(tok, "DNS_R_FORMERR"))
					exp_result = DNS_R_FORMERR;
				else if (! strcmp(tok, "DNS_R_BADPOINTER"))
					exp_result = DNS_R_BADPOINTER;
				else if (! strcmp(tok, "ISC_R_UNEXPECTEDEND"))
					exp_result = ISC_R_UNEXPECTEDEND;
				else if (! strcmp(tok, "DNS_R_TOOMANYHOPS"))
					exp_result = DNS_R_TOOMANYHOPS;
				else if (! strcmp(tok, "DNS_R_DISALLOWED"))
					exp_result = DNS_R_DISALLOWED;
				else if (! strcmp(tok, "DNS_R_NAMETOOLONG"))
					exp_result = DNS_R_NAMETOOLONG;

				tok = Tokens[3];
				dc_method = DNS_COMPRESS_NONE;
				if (! strcmp(tok, "DNS_COMPRESS_GLOBAL14"))
					dc_method = DNS_COMPRESS_GLOBAL14;
				else if (! strcmp(tok, "DNS_COMPRESS_ALL"))
					dc_method = DNS_COMPRESS_ALL;

				result = test_dns_name_fromwire(Tokens[0],
							   atoi(Tokens[1]),
							   atoi(Tokens[2]),
								dc_method,
								Tokens[4],
								exp_result,
								buflen);
			} else {
				t_info("bad format at line %d\n", line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile %s\n", testfile);
		t_result(result);
	}
}

static void
t_dns_name_fromwire(void) {
	t_assert("dns_name_fromwire", 1, T_REQUIRED, a42);
	t_dns_name_fromwire_x("dns_name_fromwire_1_data", BUFLEN);

#if 0
	/*
	 * XXXRTH these tests appear to be broken, so I have
	 * disabled them.
	 */
	t_assert("dns_name_fromwire", 2, T_REQUIRED, a43);
	t_dns_name_fromwire_x("dns_name_fromwire_2_data", BUFLEN);

	t_assert("dns_name_fromwire", 3, T_REQUIRED, a44);
	t_dns_name_fromwire_x("dns_name_fromwire_3_data", BUFLEN);
#endif

	t_assert("dns_name_fromwire", 4, T_REQUIRED, a45);
	t_dns_name_fromwire_x("dns_name_fromwire_4_data", BUFLEN);

	t_assert("dns_name_fromwire", 5, T_REQUIRED, a46);
	t_dns_name_fromwire_x("dns_name_fromwire_5_data", BUFLEN);

	t_assert("dns_name_fromwire", 6, T_REQUIRED, a47);
	t_dns_name_fromwire_x("dns_name_fromwire_6_data", BUFLEN);

	t_assert("dns_name_fromwire", 7, T_REQUIRED, a48);
	t_dns_name_fromwire_x("dns_name_fromwire_7_data", BUFLEN);

	t_assert("dns_name_fromwire", 8, T_REQUIRED, a49);
	t_dns_name_fromwire_x("dns_name_fromwire_8_data", BUFLEN);

	t_assert("dns_name_fromwire", 9, T_REQUIRED, a50);
	t_dns_name_fromwire_x("dns_name_fromwire_9_data", 2);
}


static const char *a51 =
		"dns_name_towire(name, cctx, target) converts the DNS name "
		"'name' into wire format, compresses it as specified "
		"by the compression context cctx, stores the result in "
		"target and returns DNS_SUCCESS";

static const char *a52 =
		"when not enough space exists in target, "
		"dns_name_towire(name, cctx, target) returns ISC_R_NOSPACE";

static int
test_dns_name_towire(char *testname, unsigned int dc_method, char *exp_data,
		     int exp_data_len, isc_result_t exp_result, size_t buflen)
{
	int			result;
	int			val;
	int			len;
	unsigned char		buf2[BUFLEN];
	unsigned char		buf3[BUFLEN];
	isc_buffer_t		iscbuf1;
	isc_buffer_t		iscbuf2;
	isc_buffer_t		iscbuf3;
	dns_name_t		dns_name;
	isc_result_t		dns_result;
	isc_result_t		isc_result;
	dns_compress_t		cctx;
	isc_mem_t		*mctx;

	t_info("testing using %s\n", testname);

	result = T_UNRESOLVED;
	mctx = NULL;

	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed\n");
		return (result);
	}
	dns_compress_init(&cctx, -1, mctx);
	dns_compress_setmethods(&cctx, dc_method);
	dns_name_init(&dns_name, NULL);
	len = strlen(testname);
	isc_buffer_init(&iscbuf1, testname, len);
	isc_buffer_add(&iscbuf1, len);
	isc_buffer_init(&iscbuf2, buf2, BUFLEN);
	dns_result = dns_name_fromtext(&dns_name, &iscbuf1, NULL, ISC_FALSE,
				       &iscbuf2);
	if (dns_result == ISC_R_SUCCESS) {
		isc_buffer_init(&iscbuf3, buf3, buflen);
		dns_result = dns_name_towire(&dns_name, &cctx, &iscbuf3);
		if (dns_result == exp_result) {
			if (exp_result == ISC_R_SUCCESS) {
				/*
				 * Compare results with expected data.
				 */
				val = chkdata(buf3, iscbuf3.used, exp_data,
					      exp_data_len);
				if (val == 0)
					result = T_PASS;
				else
					result = T_FAIL;
			} else
				result = T_PASS;
		} else {
			t_info("dns_name_towire unexpectedly returned %s\n",
			       dns_result_totext(dns_result));
			result = T_FAIL;
		}
	} else {
		t_info("dns_name_fromtext %s failed, result = %s\n",
				testname, dns_result_totext(dns_result));
	}
	return (result);
}

static void
t_dns_name_towire_x(const char *testfile, size_t buflen) {
	int		line;
	int		cnt;
	int		result;
	unsigned int	dc_method;
	isc_result_t	exp_result;
	char		*p;
	FILE		*fp;

	result = T_UNRESOLVED;
	fp = fopen(testfile, "r");
	if (fp != NULL) {
		line = 0;
		while ((p = t_fgetbs(fp)) != NULL) {

			++line;

			/*
			 * Skip comment lines.
			 */
			if ((isspace((unsigned char)*p)) || (*p == '#'))
				continue;

			cnt = bustline(p, Tokens);
			if (cnt == 5) {
				/*
				 *	testname, dc_method,
				 *	exp_data, exp_data_len,
				 *	exp_result.
				 */

				dc_method = t_dc_method_fromtext(Tokens[3]);
				exp_result = t_dns_result_fromtext(Tokens[4]);

				result = test_dns_name_towire(Tokens[0],
							      dc_method,
							      Tokens[2],
							      atoi(Tokens[3]),
							      exp_result,
							      buflen);
			} else {
				t_info("bad format at line %d\n", line);
			}

			(void)free(p);
			t_result(result);
		}
		(void)fclose(fp);
	} else {
		t_info("Missing datafile %s\n", testfile);
		t_result(result);
	}
}

static void
t_dns_name_towire_1(void) {
	t_assert("dns_name_towire", 1, T_REQUIRED, a51);
	t_dns_name_towire_x("dns_name_towire_1_data", BUFLEN);
}

static void
t_dns_name_towire_2(void) {
	t_assert("dns_name_towire", 2, T_REQUIRED, a52);
	t_dns_name_towire_x("dns_name_towire_2_data", 2);
}

static void
t_dns_name_towire(void) {
	t_dns_name_towire_1();
	t_dns_name_towire_2();
}

#if 0 /* This is silly.  A test should either exist, or not, but not
       * one that just returns "UNTESTED."
       */
static const char *a53 =
		"dns_name_concatenate(prefix, suffix, name, target) "
		"concatenates prefix and suffix, stores the result "
		"in target, canonicalizes any bitstring labels "
		"and returns ISC_R_SUCCESS";

static void
t_dns_name_concatenate(void) {
	t_assert("dns_name_concatenate", 1, T_REQUIRED, a53);
	t_result(T_UNTESTED);
}
#endif

testspec_t T_testlist[] = {
	{	t_dns_label_countbits,		"dns_label_countbits"	},
	{	t_dns_label_getbit,		"dns_label_getbit"	},
	{	t_dns_name_init,		"dns_name_init"		},
	{	t_dns_name_invalidate,		"dns_name_invalidate"	},
	{	t_dns_name_setbuffer,		"dns_name_setbuffer"	},
	{	t_dns_name_hasbuffer,		"dns_name_hasbuffer"	},
	{	t_dns_name_isabsolute,		"dns_name_isabsolute"	},
	{	t_dns_name_hash,		"dns_name_hash"		},
	{	t_dns_name_fullcompare,		"dns_name_fullcompare"	},
	{	t_dns_name_compare,		"dns_name_compare"	},
	{	t_dns_name_rdatacompare,	"dns_name_rdatacompare"	},
	{	t_dns_name_issubdomain,		"dns_name_issubdomain"	},
	{	t_dns_name_countlabels,		"dns_name_countlabels"	},
	{	t_dns_name_getlabel,		"dns_name_getlabel"	},
	{	t_dns_name_getlabelsequence,	"dns_name_getlabelsequence" },
	{	t_dns_name_fromregion,		"dns_name_fromregion"	},
	{	t_dns_name_toregion,		"dns_name_toregion"	},
	{	t_dns_name_fromwire,		"dns_name_fromwire"	},
	{	t_dns_name_towire,		"dns_name_towire"	},
	{	t_dns_name_fromtext,		"dns_name_fromtext"	},
	{	t_dns_name_totext,		"dns_name_totext"	},
#if 0
	{	t_dns_name_concatenate,		"dns_name_concatenate"	},
#endif
	{	NULL,				NULL			}

};

