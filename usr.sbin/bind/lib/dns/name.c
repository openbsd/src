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

/* $ISC: name.c,v 1.127.2.5 2002/08/02 00:33:05 marka Exp $ */

#include <config.h>

#include <ctype.h>

#include <isc/buffer.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/compress.h>
#include <dns/name.h>
#include <dns/result.h>

#define VALID_NAME(n)	ISC_MAGIC_VALID(n, DNS_NAME_MAGIC)

typedef enum {
	ft_init = 0,
	ft_start,
	ft_ordinary,
	ft_initialescape,
	ft_escape,
	ft_escdecimal,
	ft_bitstring,
	ft_binary,
	ft_octal,
	ft_hex,
	ft_dottedquad,
	ft_dqdecimal,
	ft_maybeslash,
	ft_finishbitstring,
	ft_bitlength,
	ft_eatdot,
	ft_at
} ft_state;

typedef enum {
	fw_start = 0,
	fw_ordinary,
	fw_copy,
	fw_bitstring,
	fw_newcurrent
} fw_state;

static char digitvalue[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	/*16*/
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*32*/
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*48*/
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1, /*64*/
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*80*/
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*96*/
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*112*/
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*128*/
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*256*/
};

static char hexdigits[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

static unsigned char maptolower[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

#define CONVERTTOASCII(c)
#define CONVERTFROMASCII(c)

#define INIT_OFFSETS(name, var, default) \
	if (name->offsets != NULL) \
		var = name->offsets; \
	else \
		var = default;

#define SETUP_OFFSETS(name, var, default) \
	if (name->offsets != NULL) \
		var = name->offsets; \
	else { \
		var = default; \
		set_offsets(name, var, NULL); \
	}

/*
 * Note:  If additional attributes are added that should not be set for
 *	  empty names, MAKE_EMPTY() must be changed so it clears them.
 */
#define MAKE_EMPTY(name) \
do { \
	name->ndata = NULL; \
	name->length = 0; \
	name->labels = 0; \
	name->attributes &= ~DNS_NAMEATTR_ABSOLUTE; \
} while (0);

/*
 * A name is "bindable" if it can be set to point to a new value, i.e.
 * name->ndata and name->length may be changed.
 */
#define BINDABLE(name) \
	((name->attributes & (DNS_NAMEATTR_READONLY|DNS_NAMEATTR_DYNAMIC)) \
	 == 0)

/*
 * Note that the name data must be a char array, not a string
 * literal, to avoid compiler warnings about discarding
 * the const attribute of a string.
 */
static unsigned char root_ndata[] = { '\0' };
static unsigned char root_offsets[] = { 0 };

static dns_name_t root = 
{
	DNS_NAME_MAGIC,
	root_ndata, 1, 1,
	DNS_NAMEATTR_READONLY | DNS_NAMEATTR_ABSOLUTE,
	root_offsets, NULL,
	{(void *)-1, (void *)-1},
	{NULL, NULL}
};

/* XXXDCL make const? */
dns_name_t *dns_rootname = &root;

static unsigned char wild_ndata[] = { '\001', '*' };
static unsigned char wild_offsets[] = { 0 };

static dns_name_t wild =
{
	DNS_NAME_MAGIC,
	wild_ndata, 2, 1,
	DNS_NAMEATTR_READONLY,
	wild_offsets, NULL,
	{(void *)-1, (void *)-1},
	{NULL, NULL}
};

/* XXXDCL make const? */
dns_name_t *dns_wildcardname = &wild;

static void
set_offsets(const dns_name_t *name, unsigned char *offsets,
	    dns_name_t *set_name);

static void
compact(dns_name_t *name, unsigned char *offsets);

/*
 * Yes, get_bit and set_bit are lame.  We define them here so they can
 * be inlined by smart compilers.
 */

static inline unsigned int
get_bit(unsigned char *array, unsigned int idx) {
	unsigned int byte, shift;

	byte = array[idx / 8];
	shift = 7 - (idx % 8);

	return ((byte >> shift) & 0x01);
}

static inline void
set_bit(unsigned char *array, unsigned int idx, unsigned int bit) {
	unsigned int shift, mask;

	shift = 7 - (idx % 8);
	mask = 1 << shift;

	if (bit != 0)
		array[idx / 8] |= mask;
	else
		array[idx / 8] &= (~mask & 0xFF);
}

dns_labeltype_t
dns_label_type(dns_label_t *label) {
	/*
	 * Get the type of 'label'.
	 */

	REQUIRE(label != NULL);
	REQUIRE(label->length > 0);
	REQUIRE(label->base[0] <= 63 ||
		label->base[0] == DNS_LABELTYPE_BITSTRING);

	if (label->base[0] <= 63)
		return (dns_labeltype_ordinary);
	else
		return (dns_labeltype_bitstring);
}

unsigned int
dns_label_countbits(dns_label_t *label) {
	unsigned int count;

	/*
	 * The number of bits in a bitstring label.
	 */

	REQUIRE(label != NULL);
	REQUIRE(label->length > 2);
	REQUIRE(label->base[0] == DNS_LABELTYPE_BITSTRING);

	count = label->base[1];
	if (count == 0)
		count = 256;

	return (count);
}

dns_bitlabel_t
dns_label_getbit(dns_label_t *label, unsigned int n) {
	unsigned int count, bit;

	/*
	 * The 'n'th most significant bit of 'label'.
	 *
	 * Notes:
	 *	Numbering starts at 0.
	 */

	REQUIRE(label != NULL);
	REQUIRE(label->length > 2);
	REQUIRE(label->base[0] == DNS_LABELTYPE_BITSTRING);

	count = label->base[1];
	if (count == 0)
		count = 256;

	REQUIRE(n < count);

	bit = get_bit(&label->base[2], n);
	if (bit == 0)
		return (dns_bitlabel_0);
	return (dns_bitlabel_1);
}

void
dns_name_init(dns_name_t *name, unsigned char *offsets) {
	/*
	 * Initialize 'name'.
	 */
	DNS_NAME_INIT(name, offsets);
}

void
dns_name_reset(dns_name_t *name) {
	REQUIRE(VALID_NAME(name));
	REQUIRE(BINDABLE(name));

	DNS_NAME_RESET(name);
}

void
dns_name_invalidate(dns_name_t *name) {
	/*
	 * Make 'name' invalid.
	 */

	REQUIRE(VALID_NAME(name));

	name->magic = 0;
	name->ndata = NULL;
	name->length = 0;
	name->labels = 0;
	name->attributes = 0;
	name->offsets = NULL;
	name->buffer = NULL;
	ISC_LINK_INIT(name, link);
}

void
dns_name_setbuffer(dns_name_t *name, isc_buffer_t *buffer) {
	/*
	 * Dedicate a buffer for use with 'name'.
	 */

	REQUIRE(VALID_NAME(name));
	REQUIRE((buffer != NULL && name->buffer == NULL) ||
		(buffer == NULL));

	name->buffer = buffer;
}

isc_boolean_t
dns_name_hasbuffer(const dns_name_t *name) {
	/*
	 * Does 'name' have a dedicated buffer?
	 */

	REQUIRE(VALID_NAME(name));

	if (name->buffer != NULL)
		return (ISC_TRUE);

	return (ISC_FALSE);
}

isc_boolean_t
dns_name_isabsolute(const dns_name_t *name) {

	/*
	 * Does 'name' end in the root label?
	 */

	REQUIRE(VALID_NAME(name));

	if ((name->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

isc_boolean_t
dns_name_iswildcard(const dns_name_t *name) {
	unsigned char *ndata;

	/*
	 * Is 'name' a wildcard name?
	 */

	REQUIRE(VALID_NAME(name));
	REQUIRE(name->labels > 0);

	if (name->length >= 2) {
		ndata = name->ndata;
		if (ndata[0] == 1 && ndata[1] == '*')
			return (ISC_TRUE);
	}

	return (ISC_FALSE);
}

isc_boolean_t
dns_name_requiresedns(const dns_name_t *name) {
	unsigned int count, nrem;
	unsigned char *ndata;
	isc_boolean_t requiresedns = ISC_FALSE;

	/*
	 * Does 'name' require EDNS for transmission?
	 */

	REQUIRE(VALID_NAME(name));
	REQUIRE(name->labels > 0);

	ndata = name->ndata;
	nrem = name->length;
	while (nrem > 0) {
		count = *ndata++;
		nrem--;
		if (count == 0)
			break;
		if (count > 63) {
			INSIST(count == DNS_LABELTYPE_BITSTRING);
			requiresedns = ISC_TRUE;
			break;
		}
		INSIST(nrem >= count);
		nrem -= count;
		ndata += count;
	}

	return (requiresedns);
}

unsigned int
dns_name_hash(dns_name_t *name, isc_boolean_t case_sensitive) {
	unsigned int length;
	const unsigned char *s;
	unsigned int h = 0;
	unsigned char c;

	/*
	 * Provide a hash value for 'name'.
	 */
	REQUIRE(VALID_NAME(name));

	if (name->labels == 0)
		return (0);
	length = name->length;
	if (length > 16)
		length = 16;

	/*
	 * This hash function is similar to the one Ousterhout
	 * uses in Tcl.
	 */
	s = name->ndata;
	if (case_sensitive) {
		while (length > 0) {
			h += ( h << 3 ) + *s;
			s++;
			length--;
		}
	} else {
		while (length > 0) {
			c = maptolower[*s];
			h += ( h << 3 ) + c;
			s++;
			length--;
		}
	}

	return (h);
}

dns_namereln_t
dns_name_fullcompare(const dns_name_t *name1, const dns_name_t *name2,
		     int *orderp,
		     unsigned int *nlabelsp, unsigned int *nbitsp)
{
	unsigned int l1, l2, l, count1, count2, count;
	unsigned int b1, b2, n, nlabels, nbits;
	int cdiff, ldiff, chdiff;
	unsigned char *label1, *label2;
	unsigned char *offsets1, *offsets2;
	dns_offsets_t odata1, odata2;
	dns_namereln_t namereln = dns_namereln_none;

	/*
	 * Determine the relative ordering under the DNSSEC order relation of
	 * 'name1' and 'name2', and also determine the hierarchical
	 * relationship of the names.
	 *
	 * Note: It makes no sense for one of the names to be relative and the
	 * other absolute.  If both names are relative, then to be meaningfully
	 * compared the caller must ensure that they are both relative to the
	 * same domain.
	 */

	REQUIRE(VALID_NAME(name1));
	REQUIRE(VALID_NAME(name2));
	REQUIRE(orderp != NULL);
	REQUIRE(nlabelsp != NULL);
	REQUIRE(nbitsp != NULL);
	/*
	 * Either name1 is absolute and name2 is absolute, or neither is.
	 */
	REQUIRE((name1->attributes & DNS_NAMEATTR_ABSOLUTE) ==
		(name2->attributes & DNS_NAMEATTR_ABSOLUTE));

	SETUP_OFFSETS(name1, offsets1, odata1);
	SETUP_OFFSETS(name2, offsets2, odata2);

	nlabels = 0;
	nbits = 0;
	l1 = name1->labels;
	l2 = name2->labels;
	ldiff = (int)l1 - (int)l2;
	if (ldiff < 0)
		l = l1;
	else
		l = l2;

	while (l > 0) {
		l--;
		l1--;
		l2--;
		label1 = &name1->ndata[offsets1[l1]];
		label2 = &name2->ndata[offsets2[l2]];
		count1 = *label1++;
		count2 = *label2++;
		if (count1 <= 63 && count2 <= 63) {
			cdiff = (int)count1 - (int)count2;
			if (cdiff < 0)
				count = count1;
			else
				count = count2;

			while (count > 0) {
				chdiff = (int)maptolower[*label1] -
					(int)maptolower[*label2];
				if (chdiff != 0) {
					*orderp = chdiff;
					goto done;
				}
				count--;
				label1++;
				label2++;
			}
			if (cdiff != 0) {
				*orderp = cdiff;
				goto done;
			}
			nlabels++;
		} else if (count1 == DNS_LABELTYPE_BITSTRING && count2 <= 63) {
			if (count2 == 0)
				*orderp = 1;
			else
				*orderp = -1;
			goto done;
		} else if (count2 == DNS_LABELTYPE_BITSTRING && count1 <= 63) {
			if (count1 == 0)
				*orderp = -1;
			else
				*orderp = 1;
			goto done;
		} else {
			INSIST(count1 == DNS_LABELTYPE_BITSTRING &&
			       count2 == DNS_LABELTYPE_BITSTRING);
			count1 = *label1++;
			if (count1 == 0)
				count1 = 256;
			count2 = *label2++;
			if (count2 == 0)
				count2 = 256;
			if (count1 < count2) {
				cdiff = -1;
				count = count1;
			} else {
				count = count2;
				if (count1 > count2)
					cdiff = 1;
				else
					cdiff = 0;
			}
			/* Yes, this loop is really slow! */
			for (n = 0; n < count; n++) {
				b1 = get_bit(label1, n);
				b2 = get_bit(label2, n);
				if (b1 < b2) {
					*orderp = -1;
					goto done;
				} else if (b1 > b2) {
					*orderp = 1;
					goto done;
				}
				if (nbits == 0)
					nlabels++;
				nbits++;
			}
			if (cdiff != 0) {
				/*
				 * If we're here, then we have two bitstrings
				 * of differing length.
				 *
				 * If the name with the shorter bitstring
				 * has any labels, then it must be greater
				 * than the longer bitstring.  This is a bit
				 * counterintuitive.  If the name with the
				 * shorter bitstring has any more labels, then
				 * the next label must be an ordinary label.
				 * It can't be a bitstring label because if it
				 * were, then there would be room for it in
				 * the current bitstring label (since all
				 * bitstrings are canonicalized).  Since
				 * there's at least one more bit in the
				 * name with the longer bitstring, and since
				 * a bitlabel sorts before any ordinary label,
				 * the name with the longer bitstring must
				 * be lexically before the one with the shorter
				 * bitstring.
				 *
				 * On the other hand, if there are no more
				 * labels in the name with the shorter
				 * bitstring, then that name contains the
				 * other name.
				 */
				namereln = dns_namereln_commonancestor;
				if (cdiff < 0) {
					if (l1 > 0)
						*orderp = 1;
					else {
						*orderp = -1;
						namereln =
							dns_namereln_contains;
					}
				} else {
					if (l2 > 0)
						*orderp = -1;
					else {
						*orderp = 1;
						namereln =
							dns_namereln_subdomain;
					}
				}
				goto done;
			}
			nbits = 0;
		}
	}

	*orderp = ldiff;
	if (ldiff < 0)
		namereln = dns_namereln_contains;
	else if (ldiff > 0)
		namereln = dns_namereln_subdomain;
	else
		namereln = dns_namereln_equal;

 done:
	*nlabelsp = nlabels;
	*nbitsp = nbits;

	if (nlabels > 0 && namereln == dns_namereln_none)
		namereln = dns_namereln_commonancestor;

	return (namereln);
}

int
dns_name_compare(const dns_name_t *name1, const dns_name_t *name2) {
	int order;
	unsigned int nlabels, nbits;

	/*
	 * Determine the relative ordering under the DNSSEC order relation of
	 * 'name1' and 'name2'.
	 *
	 * Note: It makes no sense for one of the names to be relative and the
	 * other absolute.  If both names are relative, then to be meaningfully
	 * compared the caller must ensure that they are both relative to the
	 * same domain.
	 */

	(void)dns_name_fullcompare(name1, name2, &order, &nlabels, &nbits);

	return (order);
}

isc_boolean_t
dns_name_equal(const dns_name_t *name1, const dns_name_t *name2) {
	unsigned int l, count;
	unsigned char c;
	unsigned char *label1, *label2;

	/*
	 * Are 'name1' and 'name2' equal?
	 *
	 * Note: It makes no sense for one of the names to be relative and the
	 * other absolute.  If both names are relative, then to be meaningfully
	 * compared the caller must ensure that they are both relative to the
	 * same domain.
	 */

	REQUIRE(VALID_NAME(name1));
	REQUIRE(VALID_NAME(name2));
	/*
	 * Either name1 is absolute and name2 is absolute, or neither is.
	 */
	REQUIRE((name1->attributes & DNS_NAMEATTR_ABSOLUTE) ==
		(name2->attributes & DNS_NAMEATTR_ABSOLUTE));

	if (name1->length != name2->length)
		return (ISC_FALSE);

	l = name1->labels;

	if (l != name2->labels)
		return (ISC_FALSE);

	label1 = name1->ndata;
	label2 = name2->ndata;
	while (l > 0) {
		l--;
		count = *label1++;
		if (count != *label2++)
			return (ISC_FALSE);
		if (count <= 63) {
			while (count > 0) {
				count--;
				c = maptolower[*label1++];
				if (c != maptolower[*label2++])
					return (ISC_FALSE);
			}
		} else {
			INSIST(count == DNS_LABELTYPE_BITSTRING);
			count = *label1++;
			if (count != *label2++)
				return (ISC_FALSE);
			if (count == 0)
				count = 256;
			/*
			 * Number of bytes.
			 */
			count = (count + 7) / 8;
			while (count > 0) {
				count--;
				c = *label1++;
				if (c != *label2++)
					return (ISC_FALSE);
			}
		}
	}

	return (ISC_TRUE);
}

int
dns_name_rdatacompare(const dns_name_t *name1, const dns_name_t *name2) {
	unsigned int l1, l2, l, count1, count2, count;
	unsigned char c1, c2;
	unsigned char *label1, *label2;

	/*
	 * Compare two absolute names as rdata.
	 */

	REQUIRE(VALID_NAME(name1));
	REQUIRE(name1->labels > 0);
	REQUIRE((name1->attributes & DNS_NAMEATTR_ABSOLUTE) != 0);
	REQUIRE(VALID_NAME(name2));
	REQUIRE(name2->labels > 0);
	REQUIRE((name2->attributes & DNS_NAMEATTR_ABSOLUTE) != 0);

	l1 = name1->labels;
	l2 = name2->labels;

	l = (l1 < l2) ? l1 : l2;

	label1 = name1->ndata;
	label2 = name2->ndata;
	while (l > 0) {
		l--;
		count1 = *label1++;
		count2 = *label2++;
		if (count1 <= 63 && count2 <= 63) {
			if (count1 != count2)
				return ((count1 < count2) ? -1 : 1);
			count = count1;
			while (count > 0) {
				count--;
				c1 = maptolower[*label1++];
				c2 = maptolower[*label2++];
				if (c1 < c2)
					return (-1);
				else if (c1 > c2)
					return (1);
			}
		} else if (count1 == DNS_LABELTYPE_BITSTRING && count2 <= 63) {
			return (1);
		} else if (count2 == DNS_LABELTYPE_BITSTRING && count1 <= 63) {
			return (-1);
		} else {
			INSIST(count1 == DNS_LABELTYPE_BITSTRING &&
			       count2 == DNS_LABELTYPE_BITSTRING);
			count2 = *label2++;
			count1 = *label1++;
			if (count1 != count2)
				return ((count1 < count2) ? -1 : 1);
			if (count1 == 0)
				count1 = 256;
			if (count2 == 0)
				count2 = 256;
			/* number of bytes */
			count = (count1 + 7) / 8;
			while (count > 0) {
				count--;
				c1 = *label1++;
				c2 = *label2++;
				if (c1 != c2)
					return ((c1 < c2) ? -1 : 1);
			}
		}
	}

	/*
	 * If one name had more labels than the other, their common
	 * prefix must have been different because the shorter name
	 * ended with the root label and the longer one can't have
	 * a root label in the middle of it.  Therefore, if we get
	 * to this point, the lengths must be equal.
	 */
	INSIST(l1 == l2);

	return (0);
}

isc_boolean_t
dns_name_issubdomain(const dns_name_t *name1, const dns_name_t *name2) {
	int order;
	unsigned int nlabels, nbits;
	dns_namereln_t namereln;

	/*
	 * Is 'name1' a subdomain of 'name2'?
	 *
	 * Note: It makes no sense for one of the names to be relative and the
	 * other absolute.  If both names are relative, then to be meaningfully
	 * compared the caller must ensure that they are both relative to the
	 * same domain.
	 */

	namereln = dns_name_fullcompare(name1, name2, &order, &nlabels,
					&nbits);
	if (namereln == dns_namereln_subdomain ||
	    namereln == dns_namereln_equal)
		return (ISC_TRUE);

	return (ISC_FALSE);
}

isc_boolean_t
dns_name_matcheswildcard(const dns_name_t *name, const dns_name_t *wname) {
	int order;
	unsigned int nlabels, nbits, labels;
	dns_name_t tname;

	REQUIRE(VALID_NAME(name));
	REQUIRE(name->labels > 0);
	REQUIRE(VALID_NAME(wname));
	labels = wname->labels;
	REQUIRE(labels > 0);
	REQUIRE(dns_name_iswildcard(wname));

	DNS_NAME_INIT(&tname, NULL);
	dns_name_getlabelsequence(wname, 1, labels - 1, &tname);
	if (dns_name_fullcompare(name, &tname, &order, &nlabels, &nbits) ==
	    dns_namereln_subdomain)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

unsigned int
dns_name_depth(const dns_name_t *name) {
	unsigned int depth, count, nrem, n;
	unsigned char *ndata;

	/*
	 * The depth of 'name'.
	 */

	REQUIRE(VALID_NAME(name));

	if (name->labels == 0)
		return (0);

	depth = 0;
	ndata = name->ndata;
	nrem = name->length;
	while (nrem > 0) {
		count = *ndata++;
		nrem--;
		if (count > 63) {
			INSIST(count == DNS_LABELTYPE_BITSTRING);
			INSIST(nrem != 0);
			n = *ndata++;
			nrem--;
			if (n == 0)
				n = 256;
			depth += n;
			count = n / 8;
			if (n % 8 != 0)
				count++;
		} else {
			depth++;
			if (count == 0)
				break;
		}
		INSIST(nrem >= count);
		nrem -= count;
		ndata += count;
	}

	return (depth);
}

unsigned int
dns_name_countlabels(const dns_name_t *name) {
	/*
	 * How many labels does 'name' have?
	 */

	REQUIRE(VALID_NAME(name));

	ENSURE(name->labels <= 128);

	return (name->labels);
}

void
dns_name_getlabel(const dns_name_t *name, unsigned int n, dns_label_t *label) {
	unsigned char *offsets;
	dns_offsets_t odata;

	/*
	 * Make 'label' refer to the 'n'th least significant label of 'name'.
	 */

	REQUIRE(VALID_NAME(name));
	REQUIRE(name->labels > 0);
	REQUIRE(n < name->labels);
	REQUIRE(label != NULL);

	SETUP_OFFSETS(name, offsets, odata);

	label->base = &name->ndata[offsets[n]];
	if (n == name->labels - 1)
		label->length = name->length - offsets[n];
	else
		label->length = offsets[n + 1] - offsets[n];
}

void
dns_name_getlabelsequence(const dns_name_t *source,
			  unsigned int first, unsigned int n,
			  dns_name_t *target)
{
	unsigned char *offsets;
	dns_offsets_t odata;
	unsigned int firstoffset, endoffset;

	/*
	 * Make 'target' refer to the 'n' labels including and following
	 * 'first' in 'source'.
	 */

	REQUIRE(VALID_NAME(source));
	REQUIRE(VALID_NAME(target));
	REQUIRE(first <= source->labels);
	REQUIRE(first + n <= source->labels);
	REQUIRE(BINDABLE(target));

	SETUP_OFFSETS(source, offsets, odata);

	if (first == source->labels)
		firstoffset = source->length;
	else
		firstoffset = offsets[first];

	if (first + n == source->labels)
		endoffset = source->length;
	else
		endoffset = offsets[first + n];

	target->ndata = &source->ndata[firstoffset];
	target->length = endoffset - firstoffset;
	
	if (first + n == source->labels && n > 0 &&
	    (source->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
		target->attributes |= DNS_NAMEATTR_ABSOLUTE;
	else
		target->attributes &= ~DNS_NAMEATTR_ABSOLUTE;

	target->labels = n;

	/*
	 * If source and target are the same, and we're making target
	 * a prefix of source, the offsets table is correct already
	 * so we don't need to call set_offsets().
	 */
	if (target->offsets != NULL &&
	    (target != source || first != 0))
		set_offsets(target, target->offsets, NULL);
}

void
dns_name_clone(dns_name_t *source, dns_name_t *target) {

	/*
	 * Make 'target' refer to the same name as 'source'.
	 */

	REQUIRE(VALID_NAME(source));
	REQUIRE(VALID_NAME(target));
	REQUIRE(BINDABLE(target));

	target->ndata = source->ndata;
	target->length = source->length;
	target->labels = source->labels;
	target->attributes = source->attributes &
		(unsigned int)~(DNS_NAMEATTR_READONLY | DNS_NAMEATTR_DYNAMIC |
				DNS_NAMEATTR_DYNOFFSETS);
	if (target->offsets != NULL && source->labels > 0) {
		if (source->offsets != NULL)
			memcpy(target->offsets, source->offsets,
			       source->labels);
		else
			set_offsets(target, target->offsets, NULL);
	}
}

void
dns_name_fromregion(dns_name_t *name, isc_region_t *r) {
	unsigned char *offsets;
	dns_offsets_t odata;
	unsigned int len;
	isc_region_t r2;

	/*
	 * Make 'name' refer to region 'r'.
	 */

	REQUIRE(VALID_NAME(name));
	REQUIRE(r != NULL);
	REQUIRE(BINDABLE(name));

	INIT_OFFSETS(name, offsets, odata);

	if (name->buffer != NULL) {
		isc_buffer_clear(name->buffer);
		isc_buffer_availableregion(name->buffer, &r2);
		len = (r->length < r2.length) ? r->length : r2.length;
		if (len > DNS_NAME_MAXWIRE)
			len = DNS_NAME_MAXWIRE;
		memcpy(r2.base, r->base, len);
		name->ndata = r2.base;
		name->length = len;
	} else {
		name->ndata = r->base;
		name->length = (r->length <= DNS_NAME_MAXWIRE) ? 
			r->length : DNS_NAME_MAXWIRE;
	}

	if (r->length > 0)
		set_offsets(name, offsets, name);
	else {
		name->labels = 0;
		name->attributes &= ~DNS_NAMEATTR_ABSOLUTE;
	}

	if (name->buffer != NULL)
		isc_buffer_add(name->buffer, name->length);
}

void
dns_name_toregion(dns_name_t *name, isc_region_t *r) {
	/*
	 * Make 'r' refer to 'name'.
	 */

	REQUIRE(VALID_NAME(name));
	REQUIRE(r != NULL);

	DNS_NAME_TOREGION(name, r);
}


isc_result_t
dns_name_fromtext(dns_name_t *name, isc_buffer_t *source,
		  dns_name_t *origin, isc_boolean_t downcase,
		  isc_buffer_t *target)
{
	unsigned char *ndata, *label;
	char *tdata;
	char c;
	ft_state state, kind;
	unsigned int value, count, tbcount, bitlength, maxlength;
	unsigned int n1, n2, vlen, tlen, nrem, nused, digits, labels, tused;
	isc_boolean_t done, saw_bitstring;
	unsigned char dqchars[4];
	unsigned char *offsets;
	dns_offsets_t odata;

	/*
	 * Convert the textual representation of a DNS name at source
	 * into uncompressed wire form stored in target.
	 *
	 * Notes:
	 *	Relative domain names will have 'origin' appended to them
	 *	unless 'origin' is NULL, in which case relative domain names
	 *	will remain relative.
	 */

	REQUIRE(VALID_NAME(name));
	REQUIRE(ISC_BUFFER_VALID(source));
	REQUIRE((target != NULL && ISC_BUFFER_VALID(target)) ||
		(target == NULL && ISC_BUFFER_VALID(name->buffer)));

	if (target == NULL && name->buffer != NULL) {
		target = name->buffer;
		isc_buffer_clear(target);
	}

	REQUIRE(BINDABLE(name));

	INIT_OFFSETS(name, offsets, odata);
	offsets[0] = 0;

	/*
	 * Initialize things to make the compiler happy; they're not required.
	 */
	n1 = 0;
	n2 = 0;
	vlen = 0;
	label = NULL;
	digits = 0;
	value = 0;
	count = 0;
	tbcount = 0;
	bitlength = 0;
	maxlength = 0;
	kind = ft_init;

	/*
	 * Make 'name' empty in case of failure.
	 */
	MAKE_EMPTY(name);

	/*
	 * Set up the state machine.
	 */
	tdata = (char *)source->base + source->current;
	tlen = isc_buffer_remaininglength(source);
	tused = 0;
	ndata = isc_buffer_used(target);
	nrem = isc_buffer_availablelength(target);
	if (nrem > 255)
		nrem = 255;
	nused = 0;
	labels = 0;
	done = ISC_FALSE;
	saw_bitstring = ISC_FALSE;
	state = ft_init;

	while (nrem > 0 && tlen > 0 && !done) {
		c = *tdata++;
		tlen--;
		tused++;

	no_read:
		switch (state) {
		case ft_init:
			/*
			 * Is this the root name?
			 */
			if (c == '.') {
				if (tlen != 0)
					return (DNS_R_EMPTYLABEL);
				labels++;
				*ndata++ = 0;
				nrem--;
				nused++;
				done = ISC_TRUE;
				break;
			}
			if (c == '@' && tlen == 0) {
				state = ft_at;
				break;
			}

			/* FALLTHROUGH */
		case ft_start:
			label = ndata;
			ndata++;
			nrem--;
			nused++;
			count = 0;
			if (c == '\\') {
				state = ft_initialescape;
				break;
			}
			kind = ft_ordinary;
			state = ft_ordinary;
			if (nrem == 0)
				return (ISC_R_NOSPACE);
			/* FALLTHROUGH */
		case ft_ordinary:
			if (c == '.') {
				if (count == 0)
					return (DNS_R_EMPTYLABEL);
				*label = count;
				labels++;
				INSIST(labels <= 127);
				offsets[labels] = nused;
				if (tlen == 0) {
					labels++;
					*ndata++ = 0;
					nrem--;
					nused++;
					done = ISC_TRUE;
				}
				state = ft_start;
			} else if (c == '\\') {
				state = ft_escape;
			} else {
				if (count >= 63)
					return (DNS_R_LABELTOOLONG);
				count++;
				CONVERTTOASCII(c);
				if (downcase)
					c = maptolower[(int)c];
				*ndata++ = c;
				nrem--;
				nused++;
			}
			break;
		case ft_initialescape:
			if (c == '[') {
				saw_bitstring = ISC_TRUE;
				kind = ft_bitstring;
				state = ft_bitstring;
				*label = DNS_LABELTYPE_BITSTRING;
				label = ndata;
				ndata++;
				nrem--;
				nused++;
				break;
			}
			kind = ft_ordinary;
			state = ft_escape;
			/* FALLTHROUGH */
		case ft_escape:
			if (!isdigit(c & 0xff)) {
				if (count >= 63)
					return (DNS_R_LABELTOOLONG);
				count++;
				CONVERTTOASCII(c);
				if (downcase)
					c = maptolower[(int)c];
				*ndata++ = c;
				nrem--;
				nused++;
				state = ft_ordinary;
				break;
			}
			digits = 0;
			value = 0;
			state = ft_escdecimal;
			/* FALLTHROUGH */
		case ft_escdecimal:
			if (!isdigit(c & 0xff))
				return (DNS_R_BADESCAPE);
			value *= 10;
			value += digitvalue[(int)c];
			digits++;
			if (digits == 3) {
				if (value > 255)
					return (DNS_R_BADESCAPE);
				if (count >= 63)
					return (DNS_R_LABELTOOLONG);
				count++;
				if (downcase)
					value = maptolower[value];
				*ndata++ = value;
				nrem--;
				nused++;
				state = ft_ordinary;
			}
			break;
		case ft_bitstring:
			/* count is zero */
			tbcount = 0;
			value = 0;
			if (c == 'b') {
				vlen = 8;
				maxlength = 256;
				kind = ft_binary;
				state = ft_binary;
			} else if (c == 'o') {
				vlen = 8;
				maxlength = 256;
				kind = ft_octal;
				state = ft_octal;
			} else if (c == 'x') {
				vlen = 8;
				maxlength = 256;
				kind = ft_hex;
				state = ft_hex;
			} else if (isdigit(c & 0xff)) {
				vlen = 32;
				maxlength = 32;
				n1 = 0;
				n2 = 0;
				digits = 0;
				kind = ft_dottedquad;
				state = ft_dqdecimal;
				goto no_read;
			} else
				return (DNS_R_BADBITSTRING);
			break;
		case ft_binary:
			if (c != '0' && c != '1') {
				state = ft_maybeslash;
				goto no_read;
			}
			value <<= 1;
			if (c == '1')
				value |= 1;
			count++;
			tbcount++;
			if (tbcount > 256)
				return (DNS_R_BITSTRINGTOOLONG);
			if (count == 8) {
				*ndata++ = value;
				nrem--;
				nused++;
				count = 0;
			}
			break;
		case ft_octal:
			if (!isdigit(c & 0xff) || c == '9' || c == '8') {
				state = ft_maybeslash;
				goto no_read;
			}
			value <<= 3;
			value += digitvalue[(int)c];
			count += 3;
			tbcount += 3;
			/*
			 * The total bit count is tested against 258 instead
			 * of 256 because of the possibility that the bitstring
			 * label is exactly 256 bits long; on the last octal
			 * digit (which must be 4) tbcount is incremented
			 * from 255 to 258.  This case is adequately handled
			 * later.
			 */
			if (tbcount > 258)
				return (DNS_R_BITSTRINGTOOLONG);
			if (count == 8) {
				*ndata++ = value;
				nrem--;
				nused++;
				count = 0;
			} else if (count == 9) {
				*ndata++ = (value >> 1);
				nrem--;
				nused++;
				value &= 1;
				count = 1;
			} else if (count == 10) {
				*ndata++ = (value >> 2);
				nrem--;
				nused++;
				value &= 3;
				count = 2;
			}
			break;
		case ft_hex:
			if (!isxdigit(c & 0xff)) {
				state = ft_maybeslash;
				goto no_read;
			}
			value <<= 4;
			value += digitvalue[(int)c];
			count += 4;
			tbcount += 4;
			if (tbcount > 256)
				return (DNS_R_BITSTRINGTOOLONG);
			if (count == 8) {
				*ndata++ = value;
				nrem--;
				nused++;
				count = 0;
			}
			break;
		case ft_dottedquad:
			if (c != '.' && n1 < 3)
				return (DNS_R_BADDOTTEDQUAD);
			dqchars[n1] = value;
			n2 *= 256;
			n2 += value;
			n1++;
			if (n1 == 4) {
				tbcount = 32;
				value = n2;
				state = ft_maybeslash;
				goto no_read;
			}
			value = 0;
			digits = 0;
			state = ft_dqdecimal;
			break;
		case ft_dqdecimal:
			if (!isdigit(c & 0xff)) {
				if (digits == 0 || value > 255)
					return (DNS_R_BADDOTTEDQUAD);
				state = ft_dottedquad;
				goto no_read;
			}
			digits++;
			if (digits > 3)
				return (DNS_R_BADDOTTEDQUAD);
			value *= 10;
			value += digitvalue[(int)c];
			break;
		case ft_maybeslash:
			bitlength = 0;
			if (c == '/') {
				state = ft_bitlength;
				break;
			}
			/* FALLTHROUGH */
		case ft_finishbitstring:
			if (c == ']') {
				if (tbcount == 0)
					return (DNS_R_BADBITSTRING);

				if (count > 0) {
					n1 = count % 8;
					if (n1 != 0)
						value <<= (8 - n1);
				}

				if (bitlength != 0) {
					if (bitlength > tbcount)
						return (DNS_R_BADBITSTRING);
					if (kind == ft_binary &&
					    bitlength != tbcount) {
						return (DNS_R_BADBITSTRING);
					} else if (kind == ft_octal) {
						/*
						 * Figure out correct number
						 * of octal digits for the
						 * bitlength, and compare to
						 * what was given.
						 */
						n1 = bitlength / 3;
						if (bitlength % 3 != 0)
							n1++;
						n2 = tbcount / 3;
						/* tbcount % 3 == 0 */
						if (n1 != n2)
						  return (DNS_R_BADBITSTRING);

						/*
						 * Check that no bits extend
						 * past the end of the last
						 * byte that is included in
						 * the bitlength.  Example:
						 * \[o036/8] == \[b00001111],
						 * which fits into just one
						 * byte, but the three octal
						 * digits actually specified
						 * two bytes worth of data,
						 * 9 bits, before the bitlength
						 * limited it back to one byte.
						 *
						 * n1 is the number of bytes
						 * necessary for the bitlength.
						 * n2 is the number of bytes
						 * encompassed by the octal
						 * digits.  If they are not
						 * equal, then "value" holds
						 * the excess bits, which
						 * must be zero.  If the bits
						 * are zero, then "count" is
						 * zero'ed to prevent the
						 * addition of another byte
						 * below.
						 */
						n1 = (bitlength - 1) / 8;
						n2 = (tbcount - 1) / 8;
						if (n1 != n2) {
						    if (value != 0)
						       return
							  (DNS_R_BADBITSTRING);
						    else
						       count = 0;
						}
					} else if (kind == ft_hex) {
						/*
						 * Figure out correct number
						 * of hex digits for the
						 * bitlength, and compare to
						 * what was given.
						 */
						n1 = bitlength / 4;
						if (bitlength % 4 != 0)
							n1++;
						n2 = tbcount / 4;
						/* tbcount % 4 == 0 */
						if (n1 != n2)
						  return (DNS_R_BADBITSTRING);
					}
					n1 = bitlength % vlen;
					if (n1 != 0) {
						/*
						 * Are the pad bits in the
						 * last 'vlen' bits zero?
						 */
						if ((value &
						    ~((~0) << (vlen-n1))) != 0)
						  return (DNS_R_BADBITSTRING);
					}
				} else if (kind == ft_dottedquad)
					bitlength = 32;
				else if (tbcount > 256)
					/*
					 * This can happen when an octal
					 * bitstring label of 86 octal digits
					 * is specified; tbcount will be 258.
					 * This is not trapped above because
					 * the bitstring label might be limited
					 * by a "/256" modifier.
					 */
					return (DNS_R_BADBITSTRING);
				else
					bitlength = tbcount;

				if (count > 0) {
					*ndata++ = value;
					nrem--;
					nused++;
				}

				if (kind == ft_dottedquad) {
					n1 = bitlength / 8;
					if (bitlength % 8 != 0)
						n1++;
					if (nrem < n1)
						return (ISC_R_NOSPACE);
					for (n2 = 0; n2 < n1; n2++) {
						*ndata++ = dqchars[n2];
						nrem--;
						nused++;
					}
				}
				if (bitlength == 256)
					*label = 0;
				else
					*label = bitlength;
				labels++;
				INSIST(labels <= 127);
				offsets[labels] = nused;
			} else
				return (DNS_R_BADBITSTRING);
			state = ft_eatdot;
			break;
		case ft_bitlength:
			if (!isdigit(c & 0xff)) {
				if (bitlength == 0)
					return (DNS_R_BADBITSTRING);
				state = ft_finishbitstring;
				goto no_read;
			}
			bitlength *= 10;
			bitlength += digitvalue[(int)c];
			if (bitlength > maxlength)
				return (DNS_R_BADBITSTRING);
			break;
		case ft_eatdot:
			if (c != '.')
				return (DNS_R_BADBITSTRING);
			if (tlen == 0) {
				labels++;
				*ndata++ = 0;
				nrem--;
				nused++;
				done = ISC_TRUE;
			}
			state = ft_start;
			break;
		default:
			FATAL_ERROR(__FILE__, __LINE__,
				    "Unexpected state %d", state);
			/* Does not return. */
		}
	}

	if (!done) {
		if (nrem == 0)
			return (ISC_R_NOSPACE);
		INSIST(tlen == 0);
		if (state != ft_ordinary && state != ft_eatdot &&
		    state != ft_at)
			return (ISC_R_UNEXPECTEDEND);
		if (state == ft_ordinary) {
			INSIST(count != 0);
			*label = count;
			labels++;
			INSIST(labels <= 127);
			offsets[labels] = nused;
		}
		if (origin != NULL) {
			if (nrem < origin->length)
				return (ISC_R_NOSPACE);
			label = origin->ndata;
			n1 = origin->length;
			nrem -= n1;
			while (n1 > 0) {
				n2 = *label++;
				if (n2 <= 63) {
					*ndata++ = n2;
					n1 -= n2 + 1;
					nused += n2 + 1;
					while (n2 > 0) {
						c = *label++;
						if (downcase)
							c = maptolower[(int)c];
						*ndata++ = c;
						n2--;
					}
				} else {
					INSIST(n2 == DNS_LABELTYPE_BITSTRING);
					*ndata++ = n2;
					bitlength = *label++;
					*ndata++ = bitlength;
					if (bitlength == 0)
						bitlength = 256;
					n2 = bitlength / 8;
					if (bitlength % 8 != 0)
						n2++;
					n1 -= n2 + 2;
					nused += n2 + 2;
					while (n2 > 0) {
						*ndata++ = *label++;
						n2--;
					}
				}
				labels++;
				if (n1 > 0) {
					INSIST(labels <= 127);
					offsets[labels] = nused;
				}
			}
			if ((origin->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
				name->attributes |= DNS_NAMEATTR_ABSOLUTE;
		}
	} else
		name->attributes |= DNS_NAMEATTR_ABSOLUTE;

	name->ndata = (unsigned char *)target->base + target->used;
	name->labels = labels;
	name->length = nused;

	if (saw_bitstring)
		compact(name, offsets);

	isc_buffer_forward(source, tused);
	isc_buffer_add(target, name->length);

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_name_totext(dns_name_t *name, isc_boolean_t omit_final_dot,
		isc_buffer_t *target)
{
	unsigned char *ndata;
	char *tdata;
	unsigned int nlen, tlen;
	unsigned char c;
	unsigned int trem, count;
	unsigned int bytes, nibbles;
	size_t i, len;
	unsigned int labels;
	isc_boolean_t saw_root = ISC_FALSE;
	char num[4];

	/*
	 * This function assumes the name is in proper uncompressed
	 * wire format.
	 */
	REQUIRE(VALID_NAME(name));
	REQUIRE(ISC_BUFFER_VALID(target));

	ndata = name->ndata;
	nlen = name->length;
	labels = name->labels;
	tdata = isc_buffer_used(target);
	tlen = isc_buffer_availablelength(target);

	trem = tlen;

	if (labels == 0 && nlen == 0) {
		/*
		 * Special handling for an empty name.
		 */
		if (trem == 0)
			return (ISC_R_NOSPACE);

		/*
		 * The names of these booleans are misleading in this case.
		 * This empty name is not necessarily from the root node of
		 * the DNS root zone, nor is a final dot going to be included.
		 * They need to be set this way, though, to keep the "@"
		 * from being trounced.
		 */
		saw_root = ISC_TRUE;
		omit_final_dot = ISC_FALSE;
		*tdata++ = '@';
		trem--;

		/*
		 * Skip the while() loop.
		 */
		nlen = 0;
	} else if (nlen == 1 && labels == 1 && *ndata == '\0') {
		/*
		 * Special handling for the root label.
		 */
		if (trem == 0)
			return (ISC_R_NOSPACE);

		saw_root = ISC_TRUE;
		omit_final_dot = ISC_FALSE;
		*tdata++ = '.';
		trem--;

		/*
		 * Skip the while() loop.
		 */
		nlen = 0;
	}

	while (labels > 0 && nlen > 0 && trem > 0) {
		labels--;
		count = *ndata++;
		nlen--;
		if (count == 0) {
			saw_root = ISC_TRUE;
			break;
		}
		if (count < 64) {
			INSIST(nlen >= count);
			while (count > 0) {
				c = *ndata;
				switch (c) {
				case 0x22: /* '"' */
				case 0x28: /* '(' */
				case 0x29: /* ')' */
				case 0x2E: /* '.' */
				case 0x3B: /* ';' */
				case 0x5C: /* '\\' */
				/* Special modifiers in zone files. */
				case 0x40: /* '@' */
				case 0x24: /* '$' */
					if (trem < 2)
						return (ISC_R_NOSPACE);
					*tdata++ = '\\';
					CONVERTFROMASCII(c);
					*tdata++ = c;
					ndata++;
					trem -= 2;
					nlen--;
					break;
				default:
					if (c > 0x20 && c < 0x7f) {
						if (trem == 0)
							return (ISC_R_NOSPACE);
						CONVERTFROMASCII(c);
						*tdata++ = c;
						ndata++;
						trem--;
						nlen--;
					} else {
						char buf[5];
						if (trem < 4)
							return (ISC_R_NOSPACE);
						snprintf(tdata, trem,
							 "\\%03u", c);
						tdata += 4;
						trem -= 4;
						ndata++;
						nlen--;
					}
				}
				count--;
			}
		} else if (count == DNS_LABELTYPE_BITSTRING) {
			if (trem < 3)
				return (ISC_R_NOSPACE);
			*tdata++ = '\\';
			*tdata++ = '[';
			*tdata++ = 'x';
			trem -= 3;
			INSIST(nlen > 0);
			count = *ndata++;
			if (count == 0)
				count = 256;
			nlen--;
			len = snprintf(num, sizeof(num), "%u", count);
			INSIST(len <= 4);
			bytes = count / 8;
			if (count % 8 != 0)
				bytes++;
			INSIST(nlen >= bytes);
			nibbles = count / 4;
			if (count % 4 != 0)
				nibbles++;
			if (trem < nibbles)
				return (ISC_R_NOSPACE);
			trem -= nibbles;
			nlen -= bytes;
			while (nibbles > 0) {
				c = *ndata++;
				*tdata++ = hexdigits[(c >> 4)];
				nibbles--;
				if (nibbles != 0) {
					*tdata++ = hexdigits[c & 0xf];
					nibbles--;
				}
			}
			if (trem < 2 + len)
				return (ISC_R_NOSPACE);
			*tdata++ = '/';
			for (i = 0; i < len; i++)
				*tdata++ = num[i];
			*tdata++ = ']';
			trem -= 2 + len;
		} else {
			FATAL_ERROR(__FILE__, __LINE__,
				    "Unexpected label type %02x", count);
			/* NOTREACHED */
		}

		/*
		 * The following assumes names are absolute.  If not, we
		 * fix things up later.  Note that this means that in some
		 * cases one more byte of text buffer is required than is
		 * needed in the final output.
		 */
		if (trem == 0)
			return (ISC_R_NOSPACE);
		*tdata++ = '.';
		trem--;
	}

	if (nlen != 0 && trem == 0)
		return (ISC_R_NOSPACE);

	if (!saw_root || omit_final_dot)
		trem++;

	isc_buffer_add(target, tlen - trem);

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_name_tofilenametext(dns_name_t *name, isc_boolean_t omit_final_dot,
			isc_buffer_t *target)
{
	unsigned char *ndata;
	char *tdata;
	unsigned int nlen, tlen;
	unsigned char c;
	unsigned int trem, count;
	unsigned int bytes, nibbles;
	size_t i, len;
	unsigned int labels;
	char num[4];

	/*
	 * This function assumes the name is in proper uncompressed
	 * wire format.
	 */
	REQUIRE(VALID_NAME(name));
	REQUIRE((name->attributes & DNS_NAMEATTR_ABSOLUTE) != 0);
	REQUIRE(ISC_BUFFER_VALID(target));

	ndata = name->ndata;
	nlen = name->length;
	labels = name->labels;
	tdata = isc_buffer_used(target);
	tlen = isc_buffer_availablelength(target);

	trem = tlen;

	if (nlen == 1 && labels == 1 && *ndata == '\0') {
		/*
		 * Special handling for the root label.
		 */
		if (trem == 0)
			return (ISC_R_NOSPACE);

		omit_final_dot = ISC_FALSE;
		*tdata++ = '.';
		trem--;

		/*
		 * Skip the while() loop.
		 */
		nlen = 0;
	}

	while (labels > 0 && nlen > 0 && trem > 0) {
		labels--;
		count = *ndata++;
		nlen--;
		if (count == 0)
			break;
		if (count < 64) {
			INSIST(nlen >= count);
			while (count > 0) {
				c = *ndata;
				if ((c >= 0x30 && c <= 0x39) || /* digit */
				    (c >= 0x41 && c <= 0x5A) ||	/* uppercase */
				    (c >= 0x61 && c <= 0x7A) || /* lowercase */
				    c == 0x2D ||		/* hyphen */
				    c == 0x5F)			/* underscore */
				{
					if (trem == 0)
						return (ISC_R_NOSPACE);
					/* downcase */
					if (c >= 0x41 && c <= 0x5A)
						c += 0x20;
					CONVERTFROMASCII(c);
					*tdata++ = c;
					ndata++;
					trem--;
					nlen--;
				} else {
					if (trem < 3)
						return (ISC_R_NOSPACE);
					snprintf(tdata, trem, "%%%02X", c);
					tdata += 3;
					trem -= 3;
					ndata++;
					nlen--;
				}
				count--;
			}
		} else if (count == DNS_LABELTYPE_BITSTRING) {
			if (trem < 3)
				return (ISC_R_NOSPACE);
			*tdata++ = '%';
			*tdata++ = 'x';
			trem -= 2;
			INSIST(nlen > 0);
			count = *ndata++;
			if (count == 0)
				count = 256;
			nlen--;
			len = snprintf(num, sizeof(num), "%u", count);
			INSIST(len <= 4);
			bytes = count / 8;
			if (count % 8 != 0)
				bytes++;
			INSIST(nlen >= bytes);
			nibbles = count / 4;
			if (count % 4 != 0)
				nibbles++;
			if (trem < nibbles)
				return (ISC_R_NOSPACE);
			trem -= nibbles;
			nlen -= bytes;
			while (nibbles > 0) {
				c = *ndata++;
				*tdata++ = hexdigits[(c >> 4)];
				nibbles--;
				if (nibbles != 0) {
					*tdata++ = hexdigits[c & 0xf];
					i++;
					nibbles--;
				}
			}
			if (trem < 2 + len)
				return (ISC_R_NOSPACE);
			*tdata++ = '%';
			for (i = 0; i < len; i++)
				*tdata++ = num[i];
			*tdata++ = '%';
			trem -= 2 + len;
		} else {
			FATAL_ERROR(__FILE__, __LINE__,
				    "Unexpected label type %02x", count);
			/* NOTREACHED */
		}

		/*
		 * The following assumes names are absolute.  If not, we
		 * fix things up later.  Note that this means that in some
		 * cases one more byte of text buffer is required than is
		 * needed in the final output.
		 */
		if (trem == 0)
			return (ISC_R_NOSPACE);
		*tdata++ = '.';
		trem--;
	}

	if (nlen != 0 && trem == 0)
		return (ISC_R_NOSPACE);

	if (omit_final_dot)
		trem++;

	isc_buffer_add(target, tlen - trem);

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_name_downcase(dns_name_t *source, dns_name_t *name, isc_buffer_t *target) {
	unsigned char *sndata, *ndata;
	unsigned int nlen, count, bytes, labels;
	isc_buffer_t buffer;

	/*
	 * Downcase 'source'.
	 */

	REQUIRE(VALID_NAME(source));
	REQUIRE(VALID_NAME(name));
	if (source == name) {
		REQUIRE((name->attributes & DNS_NAMEATTR_READONLY) == 0);
		isc_buffer_init(&buffer, source->ndata, source->length);
		target = &buffer;
		ndata = source->ndata;
	} else {
		REQUIRE(BINDABLE(name));
		REQUIRE((target != NULL && ISC_BUFFER_VALID(target)) ||
			(target == NULL && ISC_BUFFER_VALID(name->buffer)));
		if (target == NULL) {
			target = name->buffer;
			isc_buffer_clear(name->buffer);
		}
		ndata = (unsigned char *)target->base + target->used;
		name->ndata = ndata;
	}

	sndata = source->ndata;
	nlen = source->length;
	labels = source->labels;

	if (nlen > (target->length - target->used)) {
		MAKE_EMPTY(name);
		return (ISC_R_NOSPACE);
	}

	while (labels > 0 && nlen > 0) {
		labels--;
		count = *sndata++;
		*ndata++ = count;
		nlen--;
		if (count < 64) {
			INSIST(nlen >= count);
			while (count > 0) {
				*ndata++ = maptolower[(*sndata++)];
				nlen--;
				count--;
			}
		} else if (count == DNS_LABELTYPE_BITSTRING) {
			INSIST(nlen > 0);
			count = *sndata++;
			*ndata++ = count;
			if (count == 0)
				count = 256;
			nlen--;

			bytes = count / 8;
			if (count % 8 != 0)
				bytes++;

			INSIST(nlen >= bytes);
			nlen -= bytes;
			while (bytes > 0) {
				*ndata++ = *sndata++;
				bytes--;
			}
		} else {
			FATAL_ERROR(__FILE__, __LINE__,
				    "Unexpected label type %02x", count);
			/* Does not return. */
		}
	}

	if (source != name) {
		name->labels = source->labels;
		name->length = source->length;
		if ((source->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
			name->attributes = DNS_NAMEATTR_ABSOLUTE;
		else
			name->attributes = 0;
		if (name->labels > 0 && name->offsets != NULL)
			set_offsets(name, name->offsets, NULL);
	}

	isc_buffer_add(target, name->length);

	return (ISC_R_SUCCESS);
}

static void
set_offsets(const dns_name_t *name, unsigned char *offsets,
	    dns_name_t *set_name)
{
	unsigned int offset, count, length, nlabels, n;
	unsigned char *ndata;
	isc_boolean_t absolute;

	ndata = name->ndata;
	length = name->length;
	offset = 0;
	nlabels = 0;
	absolute = ISC_FALSE;
	while (offset != length) {
		INSIST(nlabels < 128);
		offsets[nlabels++] = offset;
		count = *ndata++;
		offset++;
		if (count <= 63) {
			offset += count;
			ndata += count;
			INSIST(offset <= length);
			if (count == 0) {
				absolute = ISC_TRUE;
				break;
			}
		} else {
			INSIST(count == DNS_LABELTYPE_BITSTRING);
			n = *ndata++;
			offset++;
			if (n == 0)
				n = 256;
			count = n / 8;
			if (n % 8 != 0)
				count++;
			offset += count;
			ndata += count;
			INSIST(offset <= length);
		}
	}
	if (set_name != NULL) {
		INSIST(set_name == name);

		set_name->labels = nlabels;
		set_name->length = offset;
		if (absolute)
			set_name->attributes |= DNS_NAMEATTR_ABSOLUTE;
		else
			set_name->attributes &= ~DNS_NAMEATTR_ABSOLUTE;
	}
	INSIST(nlabels == name->labels);
	INSIST(offset == name->length);
}

static void
compact(dns_name_t *name, unsigned char *offsets) {
	unsigned char *head, *curr, *last;
	unsigned int count, n, bit;
	unsigned int headbits, currbits, tailbits, newbits;
	unsigned int headrem, newrem;
	unsigned int headindex, currindex, tailindex, newindex;
	unsigned char tail[32];

	/*
	 * The caller MUST ensure that all bitstrings are correctly formatted
	 * and that the offsets table is valid.
	 */

 again:
	memset(tail, 0, sizeof tail);
	INSIST(name->labels != 0);
	n = name->labels - 1;

	while (n > 0) {
		head = &name->ndata[offsets[n]];
		if (head[0] == DNS_LABELTYPE_BITSTRING && head[1] != 0) {
			if (n != 0) {
				n--;
				curr = &name->ndata[offsets[n]];
				if (curr[0] != DNS_LABELTYPE_BITSTRING)
					continue;
				/*
				 * We have consecutive bitstrings labels, and
				 * the more significant label ('head') has
				 * space.
				 */
				currbits = curr[1];
				if (currbits == 0)
					currbits = 256;
				currindex = 0;
				headbits = head[1];
				if (headbits == 0)
					headbits = 256;
				headindex = headbits;
				count = 256 - headbits;
				if (count > currbits)
					count = currbits;
				headrem = headbits % 8;
				if (headrem != 0)
					headrem = 8 - headrem;
				if (headrem != 0) {
					if (headrem > count)
						headrem = count;
					do {
						bit = get_bit(&curr[2],
							      currindex);
						set_bit(&head[2], headindex,
							bit);
						currindex++;
						headindex++;
						headbits++;
						count--;
						headrem--;
					} while (headrem != 0);
				}
				tailindex = 0;
				tailbits = 0;
				while (count > 0) {
					bit = get_bit(&curr[2], currindex);
					set_bit(tail, tailindex, bit);
					currindex++;
					tailindex++;
					tailbits++;
					count--;
				}
				newbits = 0;
				newindex = 0;
				if (currindex < currbits) {
					while (currindex < currbits) {
						bit = get_bit(&curr[2],
							      currindex);
						set_bit(&curr[2], newindex,
							bit);
						currindex++;
						newindex++;
						newbits++;
					}
					INSIST(newbits < 256);
					curr[1] = newbits;
					count = newbits / 8;
					newrem = newbits % 8;
					/* Zero remaining pad bits, if any. */
					if (newrem != 0) {
						count++;
						newrem = 8 - newrem;
						while (newrem > 0) {
							set_bit(&curr[2],
								newindex,
								0);
							newrem--;
							newindex++;
						}
					}
					curr += count + 2;
				} else {
					/* We got rid of curr. */
					name->labels--;
				}
				/* copy head, then tail, then rest to curr. */
				count = headbits + tailbits;
				INSIST(count <= 256);
				curr[0] = DNS_LABELTYPE_BITSTRING;
				if (count == 256)
					curr[1] = 0;
				else
					curr[1] = count;
				curr += 2;
				head += 2;
				count = headbits / 8;
				if (headbits % 8 != 0)
					count++;
				while (count > 0) {
					*curr++ = *head++;
					count--;
				}
				count = tailbits / 8;
				if (tailbits % 8 != 0)
					count++;
				last = tail;
				while (count > 0) {
					*curr++ = *last++;
					count--;
				}
				last = name->ndata + name->length;
				while (head != last)
					*curr++ = *head++;
				name->length = (curr - name->ndata);
				/*
				 * The offsets table may now be invalid.
				 */
				set_offsets(name, offsets, NULL);
				goto again;
			}
		}
		n--;
	}
}

isc_result_t
dns_name_fromwire(dns_name_t *name, isc_buffer_t *source,
		  dns_decompress_t *dctx, isc_boolean_t downcase,
		  isc_buffer_t *target)
{
	unsigned char *cdata, *ndata;
	unsigned int cused; /* Bytes of compressed name data used */
	unsigned int hops,  nused, labels, n, nmax;
	unsigned int current, new_current, biggest_pointer;
	isc_boolean_t saw_bitstring, done;
	fw_state state = fw_start;
	unsigned int c;
	unsigned char *offsets;
	dns_offsets_t odata;

	/*
	 * Copy the possibly-compressed name at source into target,
	 * decompressing it.
	 */

	REQUIRE(VALID_NAME(name));
	REQUIRE((target != NULL && ISC_BUFFER_VALID(target)) ||
		(target == NULL && ISC_BUFFER_VALID(name->buffer)));

	if (target == NULL && name->buffer != NULL) {
		target = name->buffer;
		isc_buffer_clear(target);
	}

	REQUIRE(dctx != NULL);
	REQUIRE(BINDABLE(name));

	INIT_OFFSETS(name, offsets, odata);

	/*
	 * Make 'name' empty in case of failure.
	 */
	MAKE_EMPTY(name);

	/*
	 * Initialize things to make the compiler happy; they're not required.
	 */
	n = 0;
	new_current = 0;

	/*
	 * Set up.
	 */
	labels = 0;
	hops = 0;
	saw_bitstring = ISC_FALSE;
	done = ISC_FALSE;

	ndata = isc_buffer_used(target);
	nused = 0;

	/*
	 * Find the maximum number of uncompressed target name
	 * bytes we are willing to generate.  This is the smaller
	 * of the available target buffer length and the
	 * maximum legal domain name length (255).
	 */
	nmax = isc_buffer_availablelength(target);
	if (nmax > DNS_NAME_MAXWIRE)
		nmax = DNS_NAME_MAXWIRE;

	cdata = isc_buffer_current(source);
	cused = 0;

	current = source->current;
	biggest_pointer = current;

	/*
	 * Note:  The following code is not optimized for speed, but
	 * rather for correctness.  Speed will be addressed in the future.
	 */

	while (current < source->active && !done) {
		c = *cdata++;
		current++;
		if (hops == 0)
			cused++;

		switch (state) {
		case fw_start:
			if (c < 64) {
				offsets[labels] = nused;
				labels++;
				if (nused + c + 1 > nmax)
					goto full;
				nused += c + 1;
				*ndata++ = c;
				if (c == 0)
					done = ISC_TRUE;
				n = c;
				state = fw_ordinary;
			} else if (c >= 128 && c < 192) {
				/*
				 * 14 bit local compression pointer.
				 * Local compression is no longer an
				 * IETF draft.
				 */
				return (DNS_R_BADLABELTYPE);
			} else if (c >= 192) {
				/*
				 * Ordinary 14-bit pointer.
				 */
				if ((dctx->allowed & DNS_COMPRESS_GLOBAL14) ==
				    0)
					return (DNS_R_DISALLOWED);
				new_current = c & 0x3F;
				n = 1;
				state = fw_newcurrent;
			} else if (c == DNS_LABELTYPE_BITSTRING) {
				offsets[labels] = nused;
				labels++;
				if (nused == nmax)
					goto full;
				nused++;
				*ndata++ = c;
				saw_bitstring = ISC_TRUE;
				state = fw_bitstring;
			} else
				return (DNS_R_BADLABELTYPE);
			break;
		case fw_ordinary:
			if (downcase)
				c = maptolower[c];
			/* FALLTHROUGH */
		case fw_copy:
			*ndata++ = c;
			n--;
			if (n == 0)
				state = fw_start;
			break;
		case fw_bitstring:
			if (c == 0)
				n = 256 / 8;
			else
				n = c / 8;
			if ((c % 8) != 0)
				n++;
			if (nused + n + 1 > nmax)
				goto full;
			nused += n + 1;
			*ndata++ = c;
			state = fw_copy;
			break;
		case fw_newcurrent:
			new_current *= 256;
			new_current += c;
			n--;
			if (n != 0)
				break;
			if (new_current >= biggest_pointer)
				return (DNS_R_BADPOINTER);
			biggest_pointer = new_current;
			current = new_current;
			cdata = (unsigned char *)source->base +
				current;
			hops++;
			if (hops > DNS_POINTER_MAXHOPS)
				return (DNS_R_TOOMANYHOPS);
			state = fw_start;
			break;
		default:
			FATAL_ERROR(__FILE__, __LINE__,
				    "Unknown state %d", state);
			/* Does not return. */
		}
	}

	if (!done)
		return (ISC_R_UNEXPECTEDEND);

	name->ndata = (unsigned char *)target->base + target->used;
	name->labels = labels;
	name->length = nused;
	name->attributes |= DNS_NAMEATTR_ABSOLUTE;

	if (saw_bitstring)
		compact(name, offsets);

	isc_buffer_forward(source, cused);
	isc_buffer_add(target, name->length);

	return (ISC_R_SUCCESS);

 full:
	if (nmax == DNS_NAME_MAXWIRE)
		/*
		 * The name did not fit even though we had a buffer
		 * big enough to fit a maximum-length name.
		 */
		return (DNS_R_NAMETOOLONG);
	else
		/*
		 * The name might fit if only the caller could give us a
		 * big enough buffer.
		 */
		return (ISC_R_NOSPACE);

}

isc_result_t
dns_name_towire(dns_name_t *name, dns_compress_t *cctx, isc_buffer_t *target) {
	unsigned int methods;
	isc_uint16_t offset;
	dns_name_t gp;	/* Global compression prefix */
	isc_boolean_t gf;	/* Global compression target found */
	isc_uint16_t go;	/* Global compression offset */
	dns_offsets_t clo;
	dns_name_t clname;

	/*
	 * Convert 'name' into wire format, compressing it as specified by the
	 * compression context 'cctx', and storing the result in 'target'.
	 */

	REQUIRE(VALID_NAME(name));
	REQUIRE(cctx != NULL);
	REQUIRE(ISC_BUFFER_VALID(target));

	/*
	 * If 'name' doesn't have an offsets table, make a clone which
	 * has one.
	 */
	if (name->offsets == NULL) {
		DNS_NAME_INIT(&clname, clo);
		dns_name_clone(name, &clname);
		name = &clname;
	}
	DNS_NAME_INIT(&gp, NULL);

	offset = target->used;	/*XXX*/

	methods = dns_compress_getmethods(cctx);

	if ((methods & DNS_COMPRESS_GLOBAL14) != 0)
		gf = dns_compress_findglobal(cctx, name, &gp, &go);
	else
		gf = ISC_FALSE;

	/*
	 * If the offset is too high for 14 bit global compression, we're
	 * out of luck.
	 */
	if (gf && go >= 0x4000)
		gf = ISC_FALSE;

	/*
	 * Will the compression pointer reduce the message size?
	 */
	if (gf && (gp.length + 2) >= name->length)
		gf = ISC_FALSE;

	if (gf) {
		if (target->length - target->used < gp.length)
			return (ISC_R_NOSPACE);
		(void)memcpy((unsigned char *)target->base + target->used,
			     gp.ndata, (size_t)gp.length);
		isc_buffer_add(target, gp.length);
		go |= 0xc000;
		if (target->length - target->used < 2)
			return (ISC_R_NOSPACE);
		isc_buffer_putuint16(target, go);
		if (gp.length != 0)
			dns_compress_add(cctx, name, &gp, offset);
	} else {
		if (target->length - target->used < name->length)
			return (ISC_R_NOSPACE);
		(void)memcpy((unsigned char *)target->base + target->used,
			     name->ndata, (size_t)name->length);
		isc_buffer_add(target, name->length);
		dns_compress_add(cctx, name, name, offset);
	}
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_name_concatenate(dns_name_t *prefix, dns_name_t *suffix, dns_name_t *name,
		     isc_buffer_t *target)
{
	unsigned char *ndata, *offsets;
	unsigned int nrem, labels, prefix_length, length, offset;
	isc_boolean_t copy_prefix = ISC_TRUE;
	isc_boolean_t copy_suffix = ISC_TRUE;
	isc_boolean_t saw_bitstring = ISC_FALSE;
	isc_boolean_t absolute = ISC_FALSE;
	dns_name_t tmp_name;
	dns_offsets_t odata;

	/*
	 * Concatenate 'prefix' and 'suffix'.
	 */

	REQUIRE(prefix == NULL || VALID_NAME(prefix));
	REQUIRE(suffix == NULL || VALID_NAME(suffix));
	REQUIRE(name == NULL || VALID_NAME(name));
	REQUIRE((target != NULL && ISC_BUFFER_VALID(target)) ||
		(target == NULL && name != NULL && ISC_BUFFER_VALID(name->buffer)));
	if (prefix == NULL || prefix->labels == 0)
		copy_prefix = ISC_FALSE;
	if (suffix == NULL || suffix->labels == 0)
		copy_suffix = ISC_FALSE;
	if (copy_prefix &&
	    (prefix->attributes & DNS_NAMEATTR_ABSOLUTE) != 0) {
		absolute = ISC_TRUE;
		REQUIRE(!copy_suffix);
	}
	if (name == NULL) {
		DNS_NAME_INIT(&tmp_name, odata);
		name = &tmp_name;
	}
	if (target == NULL) {
		INSIST(name->buffer != NULL);
		target = name->buffer;
		isc_buffer_clear(name->buffer);
	}

	REQUIRE(BINDABLE(name));

	/*
	 * XXX IMPORTANT NOTE
	 *
	 * If the most-signficant label in prefix is a bitstring,
	 * and the least-signficant label in suffix is a bitstring,
	 * it's possible that compaction could convert them into
	 * one label.  If this happens, then the final size will
	 * be three bytes less than nrem.
	 *
	 * We do not check for this special case, and handling it is
	 * a little messy; we can't just concatenate and compact,
	 * because we may only have 255 bytes but might need 258 bytes
	 * temporarily.  There are ways to do this with only 255 bytes,
	 * which will be implemented later.
	 *
	 * For now, we simply reject these few cases as being too
	 * long.
	 */

	/*
	 * Set up.
	 */
	nrem = target->length - target->used;
	ndata = (unsigned char *)target->base + target->used;
	if (nrem > DNS_NAME_MAXWIRE)
		nrem = DNS_NAME_MAXWIRE;
	length = 0;
	prefix_length = 0;
	labels = 0;
	if (copy_prefix) {
		prefix_length = prefix->length;
		length += prefix_length;
		labels += prefix->labels;
	}
	if (copy_suffix) {
		length += suffix->length;
		labels += suffix->labels;
	}
	if (length > DNS_NAME_MAXWIRE) {
		MAKE_EMPTY(name);
		return (DNS_R_NAMETOOLONG);
	}
	if (length > nrem) {
		MAKE_EMPTY(name);
		return (ISC_R_NOSPACE);
	}

	if (copy_suffix) {
		if ((suffix->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
			absolute = ISC_TRUE;
		if (copy_prefix &&
		    suffix->ndata[0] == DNS_LABELTYPE_BITSTRING) {
			/*
			 * We only need to call compact() if both the
			 * least-significant label of the suffix and the
			 * most-significant label of the prefix are both
			 * bitstrings.
			 *
			 * A further possible optimization, which we don't do,
			 * is to not compact() if the suffix bitstring is
			 * full.  It will usually not be full, so I don't
			 * think this is worth it.
			 */
			if (prefix->offsets != NULL) {
				offset = prefix->offsets[prefix->labels - 1];
				if (prefix->ndata[offset] ==
				    DNS_LABELTYPE_BITSTRING)
					saw_bitstring = ISC_TRUE;
			} else {
				/*
				 * We don't have an offsets table for prefix,
				 * and rather than spend the effort to make it
				 * we'll just compact(), which doesn't cost
				 * more than computing the offsets table if
				 * there is no bitstring in prefix.
				 */
				saw_bitstring = ISC_TRUE;
			}
		}
		if (suffix == name && suffix->buffer == target)
			memmove(ndata + prefix_length, suffix->ndata,
				suffix->length);
		else
			memcpy(ndata + prefix_length, suffix->ndata,
			       suffix->length);
	}

	/*
	 * If 'prefix' and 'name' are the same object, and the object has
	 * a dedicated buffer, and we're using it, then we don't have to
	 * copy anything.
	 */
	if (copy_prefix && (prefix != name || prefix->buffer != target))
		memcpy(ndata, prefix->ndata, prefix_length);

	name->ndata = ndata;
	name->labels = labels;
	name->length = length;
	if (absolute)
		name->attributes = DNS_NAMEATTR_ABSOLUTE;
	else
		name->attributes = 0;

	if (name->labels > 0 && (name->offsets != NULL || saw_bitstring)) {
		INIT_OFFSETS(name, offsets, odata);
		set_offsets(name, offsets, NULL);
		if (saw_bitstring)
			compact(name, offsets);
	}

	isc_buffer_add(target, name->length);

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_name_split(dns_name_t *name,
	       unsigned int suffixlabels, unsigned int nbits,
	       dns_name_t *prefix, dns_name_t *suffix)

{
	dns_offsets_t name_odata, prefix_odata, suffix_odata;
	unsigned char *offsets, *prefix_offsets = NULL, *suffix_offsets;
	isc_result_t result = ISC_R_SUCCESS;
	unsigned int splitlabel, bitbytes, mod, len;
	unsigned char *p, *src, *dst;
	isc_boolean_t maybe_compact_prefix = ISC_FALSE;

	REQUIRE(VALID_NAME(name));
	REQUIRE(suffixlabels > 0);
	REQUIRE((nbits == 0 && suffixlabels < name->labels) ||
		(nbits != 0 && suffixlabels <= name->labels));
	REQUIRE(prefix != NULL || suffix != NULL);
	REQUIRE(prefix == NULL ||
		(VALID_NAME(prefix) &&
		 prefix->buffer != NULL &&
		 BINDABLE(prefix)));
	REQUIRE(suffix == NULL ||
		(VALID_NAME(suffix) &&
		 suffix->buffer != NULL &&
		 BINDABLE(suffix)));

	/*
	 * When splitting bitstring labels, if prefix and suffix have the same
	 * buffer, suffix will overwrite the ndata of prefix, corrupting it.
	 * If prefix has the ndata of name, then it modifies the bitstring
	 * label and suffix doesn't have the original available.  This latter
	 * problem could be worked around if it is ever deemed desirable.
	 */
	REQUIRE(nbits == 0 || prefix == NULL || suffix == NULL ||
		(prefix->buffer->base != suffix->buffer->base &&
		 prefix->buffer->base != name->ndata));

	SETUP_OFFSETS(name, offsets, name_odata);

	splitlabel = name->labels - suffixlabels;

	/*
	 * Make p point at the count byte of the bitstring label,
	 * if there is one (p will not be used if we are not
	 * splitting bits).
	 */
	p = &name->ndata[offsets[splitlabel] + 1];

	/*
	 * When a bit count is specified, ensure that the label is a bitstring
	 * label and it has more bits than the requested slice.
	 */
	REQUIRE(nbits == 0 ||
		(*(p - 1) == DNS_LABELTYPE_BITSTRING && nbits < 256 &&
		 (*p == 0 || *p > nbits)));

	mod = nbits % 8;

	if (prefix != NULL) {
		if (nbits > 0) {
			isc_buffer_clear(prefix->buffer);

			/*
			 * '2' is for the DNS_LABELTYPE_BITSTRING id
			 * plus the existing number of bits byte.
			 */
			len = offsets[splitlabel] + 2;
			src = name->ndata;
			dst = prefix->buffer->base;

			if (src != dst) {
				/*
				 * If these are overlapping names ...
				 * wow.  How bizarre could that be?
				 */
				INSIST(! (src <= dst && src + len > dst) ||
					 (dst <= src && dst + len > src));

				memcpy(dst, src, len);

				p = dst + len - 1;
			}

			/*
			 * Set the new bit count.  Also, when a bitstring
			 * label being split is maximal length, compaction
			 * might be necessary on the prefix.
			 */
			if (*p == 0) {
				maybe_compact_prefix = ISC_TRUE;
				*p = 256 - nbits;
			} else
				*p = *p - nbits;

			/*
			 * Calculate the number of bytes necessary to hold
			 * all of the bits left in the prefix.
			 */
			bitbytes = (*p - 1) / 8 + 1;

			prefix->length = len + bitbytes;

			if (prefix->length > prefix->buffer->length ) {
				dns_name_invalidate(prefix);
				return (ISC_R_NOSPACE);
			}

			/*
			 * All of the bits now need to be shifted to the left
			 * to fill in the space taken by the removed bits.
			 * This is wonderfully easy when the number of removed
			 * bits is an integral multiple of 8, but of course
			 * life isn't always that easy.
			 */
			src += len + nbits / 8;
			dst = p + 1;
			len = bitbytes;

			if (mod == 0) {
				memmove(dst, src, len);
			} else {
				/*
				 * p is adjusted to point to the last byte of
				 * the starting bitstring label to make it
				 * cheap to determine when bits from the next
				 * byte should be shifted into the low order
				 * bits of the current byte.
				 */
				p = src + (mod + *p - 1) / 8;

				while (len--) {
					*dst = *src++ << mod;
					/*
					 * The 0xff subexpression guards
					 * against arithmetic sign extension
					 * by the right shift.
					 */
					if (src <= p)
						*dst++ |=
							(*src >> (8 - mod)) &
							~(0xFF << mod);
				}

				/*
				 * Et voila, the very last byte has
				 * automatically already had its padding
				 * fixed by the left shift.
				 */
			}

			prefix->buffer->used = prefix->length;
			prefix->ndata = prefix->buffer->base;

			/*
			 * Yes, = is meant here, not ==.  The intent is
			 * to have it set only when INSISTs are turned on,
			 * to doublecheck the result of set_offsets.
			 */
			INSIST(len = prefix->length);

			INIT_OFFSETS(prefix, prefix_offsets, prefix_odata);
			set_offsets(prefix, prefix_offsets, prefix);

			INSIST(prefix->labels == splitlabel + 1 &&
			       prefix->length == len);

		} else
			dns_name_getlabelsequence(name, 0, splitlabel,
						  prefix);

	}

	if (suffix != NULL && result == ISC_R_SUCCESS) {
		if (nbits > 0) {
			bitbytes = (nbits - 1) / 8 + 1;

			isc_buffer_clear(suffix->buffer);

			/*
			 * The existing bitcount is in src.
			 * Set len to the number of bytes to be removed,
			 * and the suffix length to the number of bytes in
			 * the new name.
			 */
			src = &name->ndata[offsets[splitlabel] + 1];
			len = ((*src == 0 ? 256 : *src) - 1) / 8;
			len -= (bitbytes - 1);
			src++;

			suffix->length = name->length -
				offsets[splitlabel] - len;

			INSIST(suffix->length > 0);
			if (suffix->length > suffix->buffer->length) {
				dns_name_invalidate(suffix);
				return (ISC_R_NOSPACE);
			}

			/*
			 * First set up the bitstring label.
			 */
			dst = suffix->buffer->base;
			*dst++ = DNS_LABELTYPE_BITSTRING;
			*dst++ = nbits;

			if (len > 0) {
				/*
				 * Remember where the next label starts.
				 */
				p = src + bitbytes + len;

				/*
				 * Some bytes are being removed from the
				 * middle of the name because of the truncation
				 * of bits in the bitstring label.  Copy
				 * the bytes (whether full with 8 bits or not)
				 * that are being kept.
				 */
				for (len = bitbytes; len > 0; len--)
					*dst++ = *src++;

				/*
				 * Now just copy the rest of the labels of
				 * the name by adjusting src to point to
				 * the next label.
				 *
				 * 2 == label type byte + bitcount byte.
				 */
				len = suffix->length - bitbytes - 2;
				src = p;
			} else
				len = suffix->length - 2;

			if (len > 0)
				memmove(dst, src, len);

			suffix->buffer->used = suffix->length;
			suffix->ndata = suffix->buffer->base;

			/*
			 * The byte that contains the end of the
			 * bitstring has its pad bits (if any) masked
			 * to zero.
			 */
			if (mod != 0)
				suffix->ndata[bitbytes + 1] &=
					0xFF << (8 - mod);

			/*
			 * Yes, = is meant here, not ==.  The intent is
			 * to have it set only when INSISTs are turned on,
			 * to doublecheck the result of set_offsets.
			 */
			INSIST(len = suffix->length);

			INIT_OFFSETS(suffix, suffix_offsets, suffix_odata);
			set_offsets(suffix, suffix_offsets, suffix);

			INSIST(suffix->labels == suffixlabels &&
			       suffix->length == len);

		} else
			dns_name_getlabelsequence(name, splitlabel,
						  suffixlabels, suffix);

	}

	/*
	 * Compacting the prefix can't be done until after the suffix is
	 * set, because it would screw up the offsets table of 'name'
	 * when 'name' == 'prefix'.
	 */
	if (maybe_compact_prefix && splitlabel > 0 &&
	    prefix->ndata[prefix_offsets[splitlabel - 1]] ==
	    DNS_LABELTYPE_BITSTRING)
		compact(prefix, prefix_offsets);

	return (result);
}

isc_result_t
dns_name_splitatdepth(dns_name_t *name, unsigned int depth,
		      dns_name_t *prefix, dns_name_t *suffix)
{
	unsigned int suffixlabels, nbits, label, count, n;
	unsigned char *offsets, *ndata;
	dns_offsets_t odata;

	/*
	 * Split 'name' into two pieces at a certain depth.
	 */

	REQUIRE(VALID_NAME(name));
	REQUIRE(name->labels > 0);
	REQUIRE(depth > 0);

	SETUP_OFFSETS(name, offsets, odata);

	suffixlabels = 0;
	nbits = 0;
	label = name->labels;
	do {
		label--;
		ndata = &name->ndata[offsets[label]];
		count = *ndata++;
		if (count > 63) {
			INSIST(count == DNS_LABELTYPE_BITSTRING);
			/*
			 * Get the number of bits in the bitstring label.
			 */
			n = *ndata++;
			if (n == 0)
				n = 256;
			suffixlabels++;
			if (n <= depth) {
				/*
				 * This entire bitstring is in the suffix.
				 */
				depth -= n;
			} else {
				/*
				 * Only the first 'depth' bits of this
				 * bitstring are in the suffix.
				 */
				nbits = depth;
				depth = 0;
			}
		} else {
			suffixlabels++;
			depth--;
		}
	} while (depth != 0 && label != 0);

	/*
	 * If depth is not zero, then the caller violated the requirement
	 * that depth <= dns_name_depth(name).
	 */
	if (depth != 0) {
		REQUIRE(depth <= dns_name_depth(name));
		/*
		 * We should never get here!
		 */
		INSIST(0);
	}

	return (dns_name_split(name, suffixlabels, nbits, prefix, suffix));
}

isc_result_t
dns_name_dup(dns_name_t *source, isc_mem_t *mctx, dns_name_t *target) {
	/*
	 * Make 'target' a dynamically allocated copy of 'source'.
	 */

	REQUIRE(VALID_NAME(source));
	REQUIRE(source->length > 0);
	REQUIRE(VALID_NAME(target));
	REQUIRE(BINDABLE(target));

	/*
	 * Make 'target' empty in case of failure.
	 */
	MAKE_EMPTY(target);

	target->ndata = isc_mem_get(mctx, source->length);
	if (target->ndata == NULL)
		return (ISC_R_NOMEMORY);

	memcpy(target->ndata, source->ndata, source->length);

	target->length = source->length;
	target->labels = source->labels;
	target->attributes = DNS_NAMEATTR_DYNAMIC;
	if ((source->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
		target->attributes |= DNS_NAMEATTR_ABSOLUTE;
	if (target->offsets != NULL) {
		if (source->offsets != NULL)
			memcpy(target->offsets, source->offsets,
			       source->labels);
		else
			set_offsets(target, target->offsets, NULL);
	}

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_name_dupwithoffsets(dns_name_t *source, isc_mem_t *mctx,
			dns_name_t *target)
{
	/*
	 * Make 'target' a read-only dynamically allocated copy of 'source'.
	 * 'target' will also have a dynamically allocated offsets table.
	 */

	REQUIRE(VALID_NAME(source));
	REQUIRE(source->length > 0);
	REQUIRE(VALID_NAME(target));
	REQUIRE(BINDABLE(target));
	REQUIRE(target->offsets == NULL);

	/*
	 * Make 'target' empty in case of failure.
	 */
	MAKE_EMPTY(target);

	target->ndata = isc_mem_get(mctx, source->length + source->labels);
	if (target->ndata == NULL)
		return (ISC_R_NOMEMORY);

	memcpy(target->ndata, source->ndata, source->length);

	target->length = source->length;
	target->labels = source->labels;
	target->attributes = DNS_NAMEATTR_DYNAMIC | DNS_NAMEATTR_DYNOFFSETS |
		DNS_NAMEATTR_READONLY;
	if ((source->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
		target->attributes |= DNS_NAMEATTR_ABSOLUTE;
	target->offsets = target->ndata + source->length;
	if (source->offsets != NULL)
		memcpy(target->offsets, source->offsets, source->labels);
	else
		set_offsets(target, target->offsets, NULL);

	return (ISC_R_SUCCESS);
}

void
dns_name_free(dns_name_t *name, isc_mem_t *mctx) {
	size_t size;

	/*
	 * Free 'name'.
	 */

	REQUIRE(VALID_NAME(name));
	REQUIRE((name->attributes & DNS_NAMEATTR_DYNAMIC) != 0);

	size = name->length;
	if ((name->attributes & DNS_NAMEATTR_DYNOFFSETS) != 0)
		size += name->labels;
	isc_mem_put(mctx, name->ndata, size);
	dns_name_invalidate(name);
}

isc_result_t
dns_name_digest(dns_name_t *name, dns_digestfunc_t digest, void *arg) {
	dns_name_t downname;
	unsigned char data[256];
	isc_buffer_t buffer;
	isc_result_t result;
	isc_region_t r;

	/*
	 * Send 'name' in DNSSEC canonical form to 'digest'.
	 */

	REQUIRE(VALID_NAME(name));
	REQUIRE(digest != NULL);

	DNS_NAME_INIT(&downname, NULL);
	isc_buffer_init(&buffer, data, sizeof(data));

	result = dns_name_downcase(name, &downname, &buffer);
	if (result != ISC_R_SUCCESS)
		return (result);

	isc_buffer_usedregion(&buffer, &r);

	return ((digest)(arg, &r));
}

isc_boolean_t
dns_name_dynamic(dns_name_t *name) {
	REQUIRE(VALID_NAME(name));

	/*
	 * Returns whether there is dynamic memory associated with this name.
	 */

	return ((name->attributes & DNS_NAMEATTR_DYNAMIC) != 0 ?
		ISC_TRUE : ISC_FALSE);
}

isc_result_t
dns_name_print(dns_name_t *name, FILE *stream) {
	isc_result_t result;
	isc_buffer_t b;
	isc_region_t r;
	char t[1024];

	/*
	 * Print 'name' on 'stream'.
	 */

	REQUIRE(VALID_NAME(name));

	isc_buffer_init(&b, t, sizeof(t));
	result = dns_name_totext(name, ISC_FALSE, &b);
	if (result != ISC_R_SUCCESS)
		return (result);
	isc_buffer_usedregion(&b, &r);
	fprintf(stream, "%.*s", (int)r.length, (char *)r.base);

	return (ISC_R_SUCCESS);
}

void
dns_name_format(dns_name_t *name, char *cp, unsigned int size) {
	isc_result_t result;
	isc_buffer_t buf;

	REQUIRE(size > 0);

	/*
	 * Leave room for null termination after buffer.
	 */
	isc_buffer_init(&buf, cp, size - 1);
	result = dns_name_totext(name, ISC_TRUE, &buf);
	if (result == ISC_R_SUCCESS) {
		/*
		 * Null terminate.
		 */
		isc_region_t r;
		isc_buffer_usedregion(&buf, &r);
		((char *) r.base)[r.length] = '\0';

	} else
		snprintf(cp, size, "<unknown>");
}

isc_result_t
dns_name_copy(dns_name_t *source, dns_name_t *dest, isc_buffer_t *target) {
	unsigned char *ndata, *offsets;
	dns_offsets_t odata;

	/*
	 * Make dest a copy of source.
	 */

	REQUIRE(VALID_NAME(source));
	REQUIRE(VALID_NAME(dest));
	REQUIRE(target != NULL || dest->buffer != NULL);

	if (target == NULL) {
		target = dest->buffer;
		isc_buffer_clear(dest->buffer);
	}

	REQUIRE(BINDABLE(dest));

	/*
	 * Set up.
	 */
	if (target->length - target->used < source->length)
		return (ISC_R_NOSPACE);

	ndata = (unsigned char *)target->base + target->used;
	dest->ndata = target->base;

	memcpy(ndata, source->ndata, source->length);

	dest->ndata = ndata;
	dest->labels = source->labels;
	dest->length = source->length;
	if ((source->attributes & DNS_NAMEATTR_ABSOLUTE) != 0)
		dest->attributes = DNS_NAMEATTR_ABSOLUTE;
	else
		dest->attributes = 0;

	if (dest->labels > 0 && dest->offsets != NULL) {
		INIT_OFFSETS(dest, offsets, odata);
		set_offsets(dest, offsets, NULL);
	}

	isc_buffer_add(target, dest->length);

	return (ISC_R_SUCCESS);
}

