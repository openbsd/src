/* $OpenBSD: tasn_dec.c,v 1.59 2022/05/07 10:03:49 jsing Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 2000-2005 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/objects.h>

#include "asn1_locl.h"
#include "bytestring.h"

/* Constructed types with a recursive definition (such as can be found in PKCS7)
 * could eventually exceed the stack given malicious input with excessive
 * recursion. Therefore we limit the stack depth.
 */
#define ASN1_MAX_CONSTRUCTED_NEST 30

static int asn1_check_eoc(const unsigned char **in, long len);
static int asn1_check_eoc_cbs(CBS *cbs);
static int asn1_find_end(CBS *cbs, size_t length, char indefinite);

static int asn1_collect(CBB *cbb, CBS *cbs, char indefinite, int expected_tag,
    int expected_class, int depth);

static int asn1_item_ex_d2i(ASN1_VALUE **pval, const unsigned char **in,
    long len, const ASN1_ITEM *it, int tag, int aclass, char opt, int depth);
static int asn1_template_ex_d2i(ASN1_VALUE **pval, const unsigned char **in,
    long len, const ASN1_TEMPLATE *tt, char opt, int depth);
static int asn1_template_noexp_d2i(ASN1_VALUE **val, const unsigned char **in,
    long len, const ASN1_TEMPLATE *tt, char opt, int depth);
static int asn1_d2i_ex_primitive(ASN1_VALUE **pval, const unsigned char **in,
    long len, const ASN1_ITEM *it, int tag, int aclass, char opt);
static int asn1_ex_c2i(ASN1_VALUE **pval, CBS *content, int utype,
    const ASN1_ITEM *it);

static int asn1_check_tag_cbs(CBS *cbs, size_t *out_len, int *out_tag,
    uint8_t *out_class, char *out_indefinite, char *out_constructed,
    int expected_tag, int expected_class, char optional);
static int asn1_check_tag(long *out_len, int *out_tag, uint8_t *out_class,
    char *out_indefinite, char *out_constructed, const unsigned char **in,
    long len, int expected_tag, int expected_class, char optional);

ASN1_VALUE *
ASN1_item_d2i(ASN1_VALUE **pval, const unsigned char **in, long len,
    const ASN1_ITEM *it)
{
	ASN1_VALUE *ptmpval = NULL;

	if (pval == NULL)
		pval = &ptmpval;
	if (asn1_item_ex_d2i(pval, in, len, it, -1, 0, 0, 0) <= 0)
		return NULL;

	return *pval;
}

int
ASN1_template_d2i(ASN1_VALUE **pval, const unsigned char **in, long len,
    const ASN1_TEMPLATE *tt)
{
	return asn1_template_ex_d2i(pval, in, len, tt, 0, 0);
}

/* Decode an item, taking care of IMPLICIT tagging, if any.
 * If 'opt' set and tag mismatch return -1 to handle OPTIONAL
 */

static int
asn1_item_ex_d2i(ASN1_VALUE **pval, const unsigned char **in, long len,
    const ASN1_ITEM *it, int tag, int aclass, char opt, int depth)
{
	const ASN1_TEMPLATE *tt, *errtt = NULL;
	const ASN1_EXTERN_FUNCS *ef;
	const ASN1_AUX *aux = it->funcs;
	ASN1_aux_cb *asn1_cb = NULL;
	ASN1_TLC ctx = { 0 };
	const unsigned char *p = NULL, *q;
	unsigned char oclass;
	char seq_eoc, seq_nolen, cst, isopt;
	long tmplen;
	int i;
	int otag;
	int ret = 0;
	ASN1_VALUE **pchptr;
	int combine;

	combine = aclass & ASN1_TFLG_COMBINE;
	aclass &= ~ASN1_TFLG_COMBINE;

	if (!pval)
		return 0;

	if (aux && aux->asn1_cb)
		asn1_cb = aux->asn1_cb;

	if (++depth > ASN1_MAX_CONSTRUCTED_NEST) {
		ASN1error(ASN1_R_NESTED_TOO_DEEP);
		goto err;
	}

	switch (it->itype) {
	case ASN1_ITYPE_PRIMITIVE:
		if (it->templates) {
			/* tagging or OPTIONAL is currently illegal on an item
			 * template because the flags can't get passed down.
			 * In practice this isn't a problem: we include the
			 * relevant flags from the item template in the
			 * template itself.
			 */
			if ((tag != -1) || opt) {
				ASN1error(ASN1_R_ILLEGAL_OPTIONS_ON_ITEM_TEMPLATE);
				goto err;
			}
			return asn1_template_ex_d2i(pval, in, len,
			    it->templates, opt, depth);
		}
		return asn1_d2i_ex_primitive(pval, in, len, it,
		    tag, aclass, opt);
		break;

	case ASN1_ITYPE_MSTRING:
		/*
		 * It never makes sense for multi-strings to have implicit
		 * tagging, so if tag != -1, then this looks like an error in
		 * the template.
		 */
		if (tag != -1) {
			ASN1error(ASN1_R_BAD_TEMPLATE);
			goto err;
		}

		p = *in;
		/* Just read in tag and class */
		ret = asn1_check_tag(NULL, &otag, &oclass, NULL, NULL, &p, len,
		    -1, 0, 1);
		if (!ret) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			goto err;
		}

		/* Must be UNIVERSAL class */
		if (oclass != V_ASN1_UNIVERSAL) {
			/* If OPTIONAL, assume this is OK */
			if (opt)
				return -1;
			ASN1error(ASN1_R_MSTRING_NOT_UNIVERSAL);
			goto err;
		}
		/* Check tag matches bit map */
		if (!(ASN1_tag2bit(otag) & it->utype)) {
			/* If OPTIONAL, assume this is OK */
			if (opt)
				return -1;
			ASN1error(ASN1_R_MSTRING_WRONG_TAG);
			goto err;
		}
		return asn1_d2i_ex_primitive(pval, in, len,
		    it, otag, 0, 0);

	case ASN1_ITYPE_EXTERN:
		/* Use new style d2i */
		ef = it->funcs;
		return ef->asn1_ex_d2i(pval, in, len, it, tag, aclass, opt, &ctx);

	case ASN1_ITYPE_CHOICE:
		/*
		 * It never makes sense for CHOICE types to have implicit
		 * tagging, so if tag != -1, then this looks like an error in
		 * the template.
		 */
		if (tag != -1) {
			ASN1error(ASN1_R_BAD_TEMPLATE);
			goto err;
		}

		if (asn1_cb && !asn1_cb(ASN1_OP_D2I_PRE, pval, it, NULL))
			goto auxerr;

		if (*pval) {
			/* Free up and zero CHOICE value if initialised */
			i = asn1_get_choice_selector(pval, it);
			if ((i >= 0) && (i < it->tcount)) {
				tt = it->templates + i;
				pchptr = asn1_get_field_ptr(pval, tt);
				ASN1_template_free(pchptr, tt);
				asn1_set_choice_selector(pval, -1, it);
			}
		} else if (!ASN1_item_ex_new(pval, it)) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			goto err;
		}
		/* CHOICE type, try each possibility in turn */
		p = *in;
		for (i = 0, tt = it->templates; i < it->tcount; i++, tt++) {
			pchptr = asn1_get_field_ptr(pval, tt);
			/* We mark field as OPTIONAL so its absence
			 * can be recognised.
			 */
			ret = asn1_template_ex_d2i(pchptr, &p, len, tt, 1,
			    depth);
			/* If field not present, try the next one */
			if (ret == -1)
				continue;
			/* If positive return, read OK, break loop */
			if (ret > 0)
				break;
			/* Otherwise must be an ASN1 parsing error */
			errtt = tt;
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			goto err;
		}

		/* Did we fall off the end without reading anything? */
		if (i == it->tcount) {
			/* If OPTIONAL, this is OK */
			if (opt) {
				/* Free and zero it */
				ASN1_item_ex_free(pval, it);
				return -1;
			}
			ASN1error(ASN1_R_NO_MATCHING_CHOICE_TYPE);
			goto err;
		}

		asn1_set_choice_selector(pval, i, it);
		*in = p;
		if (asn1_cb && !asn1_cb(ASN1_OP_D2I_POST, pval, it, NULL))
			goto auxerr;
		return 1;

	case ASN1_ITYPE_NDEF_SEQUENCE:
	case ASN1_ITYPE_SEQUENCE:
		p = *in;
		tmplen = len;

		/* If no IMPLICIT tagging set to SEQUENCE, UNIVERSAL */
		if (tag == -1) {
			tag = V_ASN1_SEQUENCE;
			aclass = V_ASN1_UNIVERSAL;
		}
		/* Get SEQUENCE length and update len, p */
		ret = asn1_check_tag(&len, NULL, NULL, &seq_eoc, &cst, &p, len,
		    tag, aclass, opt);
		if (!ret) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			goto err;
		} else if (ret == -1)
			return -1;
		if (aux && (aux->flags & ASN1_AFLG_BROKEN)) {
			len = tmplen - (p - *in);
			seq_nolen = 1;
		}
		/* If indefinite we don't do a length check */
		else
			seq_nolen = seq_eoc;
		if (!cst) {
			ASN1error(ASN1_R_SEQUENCE_NOT_CONSTRUCTED);
			goto err;
		}

		if (!*pval && !ASN1_item_ex_new(pval, it)) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			goto err;
		}

		if (asn1_cb && !asn1_cb(ASN1_OP_D2I_PRE, pval, it, NULL))
			goto auxerr;

		/* Free up and zero any ADB found */
		for (i = 0, tt = it->templates; i < it->tcount; i++, tt++) {
			if (tt->flags & ASN1_TFLG_ADB_MASK) {
				const ASN1_TEMPLATE *seqtt;
				ASN1_VALUE **pseqval;
				seqtt = asn1_do_adb(pval, tt, 1);
				if (!seqtt)
					goto err;
				pseqval = asn1_get_field_ptr(pval, seqtt);
				ASN1_template_free(pseqval, seqtt);
			}
		}

		/* Get each field entry */
		for (i = 0, tt = it->templates; i < it->tcount; i++, tt++) {
			const ASN1_TEMPLATE *seqtt;
			ASN1_VALUE **pseqval;
			seqtt = asn1_do_adb(pval, tt, 1);
			if (!seqtt)
				goto err;
			pseqval = asn1_get_field_ptr(pval, seqtt);
			/* Have we ran out of data? */
			if (!len)
				break;
			q = p;
			if (asn1_check_eoc(&p, len)) {
				if (!seq_eoc) {
					ASN1error(ASN1_R_UNEXPECTED_EOC);
					goto err;
				}
				len -= p - q;
				seq_eoc = 0;
				q = p;
				break;
			}
			/* This determines the OPTIONAL flag value. The field
			 * cannot be omitted if it is the last of a SEQUENCE
			 * and there is still data to be read. This isn't
			 * strictly necessary but it increases efficiency in
			 * some cases.
			 */
			if (i == (it->tcount - 1))
				isopt = 0;
			else
				isopt = (char)(seqtt->flags & ASN1_TFLG_OPTIONAL);
			/* attempt to read in field, allowing each to be
			 * OPTIONAL */

			ret = asn1_template_ex_d2i(pseqval, &p, len,
			    seqtt, isopt, depth);
			if (!ret) {
				errtt = seqtt;
				goto err;
			} else if (ret == -1) {
				/* OPTIONAL component absent.
				 * Free and zero the field.
				 */
				ASN1_template_free(pseqval, seqtt);
				continue;
			}
			/* Update length */
			len -= p - q;
		}

		/* Check for EOC if expecting one */
		if (seq_eoc && !asn1_check_eoc(&p, len)) {
			ASN1error(ASN1_R_MISSING_EOC);
			goto err;
		}
		/* Check all data read */
		if (!seq_nolen && len) {
			ASN1error(ASN1_R_SEQUENCE_LENGTH_MISMATCH);
			goto err;
		}

		/* If we get here we've got no more data in the SEQUENCE,
		 * however we may not have read all fields so check all
		 * remaining are OPTIONAL and clear any that are.
		 */
		for (; i < it->tcount; tt++, i++) {
			const ASN1_TEMPLATE *seqtt;
			seqtt = asn1_do_adb(pval, tt, 1);
			if (!seqtt)
				goto err;
			if (seqtt->flags & ASN1_TFLG_OPTIONAL) {
				ASN1_VALUE **pseqval;
				pseqval = asn1_get_field_ptr(pval, seqtt);
				ASN1_template_free(pseqval, seqtt);
			} else {
				errtt = seqtt;
				ASN1error(ASN1_R_FIELD_MISSING);
				goto err;
			}
		}
		/* Save encoding */
		if (!asn1_enc_save(pval, *in, p - *in, it)) {
			ASN1error(ERR_R_MALLOC_FAILURE);
			goto auxerr;
		}
		*in = p;
		if (asn1_cb && !asn1_cb(ASN1_OP_D2I_POST, pval, it, NULL))
			goto auxerr;
		return 1;

	default:
		return 0;
	}

 auxerr:
	ASN1error(ASN1_R_AUX_ERROR);
 err:
	if (combine == 0)
		ASN1_item_ex_free(pval, it);
	if (errtt)
		ERR_asprintf_error_data("Field=%s, Type=%s", errtt->field_name,
		    it->sname);
	else
		ERR_asprintf_error_data("Type=%s", it->sname);
	return 0;
}

int
ASN1_item_ex_d2i(ASN1_VALUE **pval, const unsigned char **in, long len,
    const ASN1_ITEM *it, int tag, int aclass, char opt, ASN1_TLC *ctx)
{
	return asn1_item_ex_d2i(pval, in, len, it, tag, aclass, opt, 0);
}

/* Templates are handled with two separate functions.
 * One handles any EXPLICIT tag and the other handles the rest.
 */

static int
asn1_template_ex_d2i(ASN1_VALUE **val, const unsigned char **in, long inlen,
    const ASN1_TEMPLATE *tt, char opt, int depth)
{
	int flags, aclass;
	int ret;
	long len;
	const unsigned char *p, *q;
	char exp_eoc;

	if (!val)
		return 0;
	flags = tt->flags;
	aclass = flags & ASN1_TFLG_TAG_CLASS;

	p = *in;

	/* Check if EXPLICIT tag expected */
	if (flags & ASN1_TFLG_EXPTAG) {
		char cst;
		/* Need to work out amount of data available to the inner
		 * content and where it starts: so read in EXPLICIT header to
		 * get the info.
		 */
		ret = asn1_check_tag(&len, NULL, NULL, &exp_eoc, &cst, &p,
		    inlen, tt->tag, aclass, opt);
		q = p;
		if (!ret) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			return 0;
		} else if (ret == -1)
			return -1;
		if (!cst) {
			ASN1error(ASN1_R_EXPLICIT_TAG_NOT_CONSTRUCTED);
			return 0;
		}
		/* We've found the field so it can't be OPTIONAL now */
		ret = asn1_template_noexp_d2i(val, &p, len, tt, 0, depth);
		if (!ret) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			return 0;
		}
		/* We read the field in OK so update length */
		len -= p - q;
		if (exp_eoc) {
			/* If NDEF we must have an EOC here */
			if (!asn1_check_eoc(&p, len)) {
				ASN1error(ASN1_R_MISSING_EOC);
				goto err;
			}
		} else {
			/* Otherwise we must hit the EXPLICIT tag end or its
			 * an error */
			if (len) {
				ASN1error(ASN1_R_EXPLICIT_LENGTH_MISMATCH);
				goto err;
			}
		}
	} else
		return asn1_template_noexp_d2i(val, in, inlen, tt, opt, depth);

	*in = p;
	return 1;

 err:
	ASN1_template_free(val, tt);
	return 0;
}

static int
asn1_template_noexp_d2i(ASN1_VALUE **val, const unsigned char **in, long len,
    const ASN1_TEMPLATE *tt, char opt, int depth)
{
	int flags, aclass;
	int ret;
	const unsigned char *p, *q;

	if (!val)
		return 0;
	flags = tt->flags;
	aclass = flags & ASN1_TFLG_TAG_CLASS;

	p = *in;
	q = p;

	if (flags & ASN1_TFLG_SK_MASK) {
		/* SET OF, SEQUENCE OF */
		int sktag, skaclass;
		char sk_eoc;
		/* First work out expected inner tag value */
		if (flags & ASN1_TFLG_IMPTAG) {
			sktag = tt->tag;
			skaclass = aclass;
		} else {
			skaclass = V_ASN1_UNIVERSAL;
			if (flags & ASN1_TFLG_SET_OF)
				sktag = V_ASN1_SET;
			else
				sktag = V_ASN1_SEQUENCE;
		}
		/* Get the tag */
		ret = asn1_check_tag(&len, NULL, NULL, &sk_eoc, NULL, &p, len,
		    sktag, skaclass, opt);
		if (!ret) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			return 0;
		} else if (ret == -1)
			return -1;
		if (!*val)
			*val = (ASN1_VALUE *)sk_new_null();
		else {
			/* We've got a valid STACK: free up any items present */
			STACK_OF(ASN1_VALUE) *sktmp =
			    (STACK_OF(ASN1_VALUE) *)*val;
			ASN1_VALUE *vtmp;
			while (sk_ASN1_VALUE_num(sktmp) > 0) {
				vtmp = sk_ASN1_VALUE_pop(sktmp);
				ASN1_item_ex_free(&vtmp,
				    tt->item);
			}
		}

		if (!*val) {
			ASN1error(ERR_R_MALLOC_FAILURE);
			goto err;
		}

		/* Read as many items as we can */
		while (len > 0) {
			ASN1_VALUE *skfield;
			q = p;
			/* See if EOC found */
			if (asn1_check_eoc(&p, len)) {
				if (!sk_eoc) {
					ASN1error(ASN1_R_UNEXPECTED_EOC);
					goto err;
				}
				len -= p - q;
				sk_eoc = 0;
				break;
			}
			skfield = NULL;
			if (!asn1_item_ex_d2i(&skfield, &p, len,
			    tt->item, -1, 0, 0, depth)) {
				ASN1error(ERR_R_NESTED_ASN1_ERROR);
				goto err;
			}
			len -= p - q;
			if (!sk_ASN1_VALUE_push((STACK_OF(ASN1_VALUE) *)*val,
			    skfield)) {
				ASN1error(ERR_R_MALLOC_FAILURE);
				goto err;
			}
		}
		if (sk_eoc) {
			ASN1error(ASN1_R_MISSING_EOC);
			goto err;
		}
	} else if (flags & ASN1_TFLG_IMPTAG) {
		/* IMPLICIT tagging */
		ret = asn1_item_ex_d2i(val, &p, len,
		    tt->item, tt->tag, aclass, opt, depth);
		if (!ret) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			goto err;
		} else if (ret == -1)
			return -1;
	} else {
		/* Nothing special */
		ret = asn1_item_ex_d2i(val, &p, len, tt->item,
		    -1, tt->flags & ASN1_TFLG_COMBINE, opt, depth);
		if (!ret) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			goto err;
		} else if (ret == -1)
			return -1;
	}

	*in = p;
	return 1;

 err:
	ASN1_template_free(val, tt);
	return 0;
}

static int
asn1_d2i_ex_primitive(ASN1_VALUE **pval, const unsigned char **in, long inlen,
    const ASN1_ITEM *it, int tag, int aclass, char opt)
{
	int ret = 0, utype;
	long plen;
	char cst, inf;
	const unsigned char *p;
	const unsigned char *content = NULL;
	uint8_t *data = NULL;
	size_t data_len = 0;
	CBS cbs;
	CBB cbb;
	long len;

	memset(&cbb, 0, sizeof(cbb));

	if (!pval) {
		ASN1error(ASN1_R_ILLEGAL_NULL);
		return 0; /* Should never happen */
	}

	if (it->itype == ASN1_ITYPE_MSTRING) {
		utype = tag;
		tag = -1;
	} else
		utype = it->utype;

	if (utype == V_ASN1_ANY) {
		/* If type is ANY need to figure out type from tag */
		unsigned char oclass;
		if (tag >= 0) {
			ASN1error(ASN1_R_ILLEGAL_TAGGED_ANY);
			return 0;
		}
		if (opt) {
			ASN1error(ASN1_R_ILLEGAL_OPTIONAL_ANY);
			return 0;
		}
		p = *in;
		ret = asn1_check_tag(NULL, &utype, &oclass, NULL, NULL, &p,
		    inlen, -1, 0, 0);
		if (!ret) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			return 0;
		}
		if (oclass != V_ASN1_UNIVERSAL)
			utype = V_ASN1_OTHER;
	}
	if (tag == -1) {
		tag = utype;
		aclass = V_ASN1_UNIVERSAL;
	}
	p = *in;
	/* Check header */
	ret = asn1_check_tag(&plen, NULL, NULL, &inf, &cst, &p, inlen, tag,
	    aclass, opt);
	if (!ret) {
		ASN1error(ERR_R_NESTED_ASN1_ERROR);
		return 0;
	} else if (ret == -1)
		return -1;
	ret = 0;
	/* SEQUENCE, SET and "OTHER" are left in encoded form */
	if ((utype == V_ASN1_SEQUENCE) || (utype == V_ASN1_SET) ||
	    (utype == V_ASN1_OTHER)) {
		if (utype != V_ASN1_OTHER && !cst) {
			/* SEQUENCE and SET must be constructed */
			ASN1error(ASN1_R_TYPE_NOT_CONSTRUCTED);
			return 0;
		}

		content = *in;
		if (plen < 0)
			goto err;
		CBS_init(&cbs, p, plen);
		if (!asn1_find_end(&cbs, plen, inf))
			goto err;
		p = CBS_data(&cbs);
		len = p - content;
	} else if (cst) {
		/*
		 * Should really check the internal tags are correct but
		 * some things may get this wrong. The relevant specs
		 * say that constructed string types should be OCTET STRINGs
		 * internally irrespective of the type. So instead just check
		 * for UNIVERSAL class and ignore the tag.
		 */
		if (!CBB_init(&cbb, 0))
			goto err;
		if (plen < 0)
			goto err;
		CBS_init(&cbs, p, plen);
		if (!asn1_collect(&cbb, &cbs, inf, -1, V_ASN1_UNIVERSAL, 0))
			goto err;
		p = CBS_data(&cbs);
		if (!CBB_finish(&cbb, &data, &data_len))
			goto err;

		if (data_len > LONG_MAX)
			goto err;

		content = data;
		len = data_len;
	} else {
		content = p;
		len = plen;
		p += plen;
	}

	/* We now have content length and type: translate into a structure */
	if (len < 0)
		goto err;
	CBS_init(&cbs, content, len);
	if (!asn1_ex_c2i(pval, &cbs, utype, it))
		goto err;

	*in = p;
	ret = 1;

 err:
	CBB_cleanup(&cbb);
	freezero(data, data_len);

	return ret;
}

static int
asn1_ex_c2i_primitive(ASN1_VALUE **pval, CBS *content, int utype, const ASN1_ITEM *it)
{
	ASN1_STRING *stmp;
	ASN1_INTEGER **tint;
	ASN1_BOOLEAN *tbool;
	uint8_t u8val;
	int ret = 0;

	if (it->funcs != NULL)
		return 0;

	if (CBS_len(content) > INT_MAX)
		return 0;

	switch (utype) {
	case V_ASN1_OBJECT:
		if (!c2i_ASN1_OBJECT_cbs((ASN1_OBJECT **)pval, content))
			goto err;
		break;

	case V_ASN1_NULL:
		if (CBS_len(content) != 0) {
			ASN1error(ASN1_R_NULL_IS_WRONG_LENGTH);
			goto err;
		}
		*pval = (ASN1_VALUE *)1;
		break;

	case V_ASN1_BOOLEAN:
		tbool = (ASN1_BOOLEAN *)pval;
		if (CBS_len(content) != 1) {
			ASN1error(ASN1_R_BOOLEAN_IS_WRONG_LENGTH);
			goto err;
		}
		if (!CBS_get_u8(content, &u8val))
			goto err;
		*tbool = u8val;
		break;

	case V_ASN1_BIT_STRING:
		if (!c2i_ASN1_BIT_STRING_cbs((ASN1_BIT_STRING **)pval, content))
			goto err;
		break;

	case V_ASN1_INTEGER:
	case V_ASN1_ENUMERATED:
		tint = (ASN1_INTEGER **)pval;
		if (!c2i_ASN1_INTEGER_cbs(tint, content))
			goto err;
		/* Fixup type to match the expected form */
		(*tint)->type = utype | ((*tint)->type & V_ASN1_NEG);
		break;

	case V_ASN1_OCTET_STRING:
	case V_ASN1_NUMERICSTRING:
	case V_ASN1_PRINTABLESTRING:
	case V_ASN1_T61STRING:
	case V_ASN1_VIDEOTEXSTRING:
	case V_ASN1_IA5STRING:
	case V_ASN1_UTCTIME:
	case V_ASN1_GENERALIZEDTIME:
	case V_ASN1_GRAPHICSTRING:
	case V_ASN1_VISIBLESTRING:
	case V_ASN1_GENERALSTRING:
	case V_ASN1_UNIVERSALSTRING:
	case V_ASN1_BMPSTRING:
	case V_ASN1_UTF8STRING:
	case V_ASN1_OTHER:
	case V_ASN1_SET:
	case V_ASN1_SEQUENCE:
	default:
		if (utype == V_ASN1_BMPSTRING && (CBS_len(content) & 1)) {
			ASN1error(ASN1_R_BMPSTRING_IS_WRONG_LENGTH);
			goto err;
		}
		if (utype == V_ASN1_UNIVERSALSTRING && (CBS_len(content) & 3)) {
			ASN1error(ASN1_R_UNIVERSALSTRING_IS_WRONG_LENGTH);
			goto err;
		}
		/* All based on ASN1_STRING and handled the same way. */
		if (*pval == NULL) {
			if ((stmp = ASN1_STRING_type_new(utype)) == NULL) {
				ASN1error(ERR_R_MALLOC_FAILURE);
				goto err;
			}
			*pval = (ASN1_VALUE *)stmp;
		} else {
			stmp = (ASN1_STRING *)*pval;
			stmp->type = utype;
		}
		if (!ASN1_STRING_set(stmp, CBS_data(content), CBS_len(content))) {
			ASN1_STRING_free(stmp);
			*pval = NULL;
			goto err;
		}
		break;
	}

	ret = 1;

 err:
	return ret;
}

static int
asn1_ex_c2i_any(ASN1_VALUE **pval, CBS *content, int utype, const ASN1_ITEM *it)
{
	ASN1_TYPE *atype;

	if (it->utype != V_ASN1_ANY || it->funcs != NULL)
		return 0;

	if (*pval != NULL) {
		ASN1_TYPE_free((ASN1_TYPE *)*pval);
		*pval = NULL;
	}

	if ((atype = ASN1_TYPE_new()) == NULL)
		return 0;

	if (!asn1_ex_c2i_primitive(&atype->value.asn1_value, content, utype, it)) {
		ASN1_TYPE_free(atype);
		return 0;
	}
	atype->type = utype;

	/* Fix up value for ASN.1 NULL. */
	if (atype->type == V_ASN1_NULL)
		atype->value.ptr = NULL;

	*pval = (ASN1_VALUE *)atype;

	return 1;
}

static int
asn1_ex_c2i(ASN1_VALUE **pval, CBS *content, int utype, const ASN1_ITEM *it)
{
	if (CBS_len(content) > INT_MAX)
		return 0;

	if (it->funcs != NULL) {
		const ASN1_PRIMITIVE_FUNCS *pf = it->funcs;
		char free_content = 0;

		if (pf->prim_c2i == NULL)
			return 0;

		return pf->prim_c2i(pval, CBS_data(content), CBS_len(content),
		    utype, &free_content, it);
	}

	if (it->utype == V_ASN1_ANY)
		return asn1_ex_c2i_any(pval, content, utype, it);

	return asn1_ex_c2i_primitive(pval, content, utype, it);
}

/* Find the end of an ASN.1 object. */
static int
asn1_find_end(CBS *cbs, size_t length, char indefinite)
{
	size_t eoc_count;

	if (!indefinite) {
		if (!CBS_skip(cbs, length)) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			return 0;
		}
		return 1;
	}

	eoc_count = 1;

	while (CBS_len(cbs) > 0) {
		if (asn1_check_eoc_cbs(cbs)) {
			if (--eoc_count == 0)
				break;
			continue;
		}
		if (!asn1_check_tag_cbs(cbs, &length, NULL, NULL,
		    &indefinite, NULL, -1, 0, 0)) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			return 0;
		}
		if (indefinite) {
			eoc_count++;
			continue;
		}
		if (!CBS_skip(cbs, length))
			return 0;
	}

	if (eoc_count > 0) {
		ASN1error(ASN1_R_MISSING_EOC);
		return 0;
	}

	return 1;
}

#ifndef ASN1_MAX_STRING_NEST
/* This determines how many levels of recursion are permitted in ASN1
 * string types. If it is not limited stack overflows can occur. If set
 * to zero no recursion is allowed at all. Although zero should be adequate
 * examples exist that require a value of 1. So 5 should be more than enough.
 */
#define ASN1_MAX_STRING_NEST 5
#endif

/* Collect the contents from a constructed ASN.1 object. */
static int
asn1_collect(CBB *cbb, CBS *cbs, char indefinite, int expected_tag,
    int expected_class, int depth)
{
	char constructed;
	size_t length;
	CBS content;
	int need_eoc;

	if (depth > ASN1_MAX_STRING_NEST) {
		ASN1error(ASN1_R_NESTED_ASN1_STRING);
		return 0;
	}

	need_eoc = indefinite;

	while (CBS_len(cbs) > 0) {
		if (asn1_check_eoc_cbs(cbs)) {
			if (!need_eoc) {
				ASN1error(ASN1_R_UNEXPECTED_EOC);
				return 0;
			}
			return 1;
		}
		if (!asn1_check_tag_cbs(cbs, &length, NULL, NULL, &indefinite,
		    &constructed, expected_tag, expected_class, 0)) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			return 0;
		}

		if (constructed) {
			if (!asn1_collect(cbb, cbs, indefinite, expected_tag,
			    expected_class, depth + 1))
				return 0;
			continue;
		}

		if (!CBS_get_bytes(cbs, &content, length)) {
			ASN1error(ERR_R_NESTED_ASN1_ERROR);
			return 0;
		}
		if (!CBB_add_bytes(cbb, CBS_data(&content), CBS_len(&content)))
			return 0;
	}

	if (need_eoc) {
		ASN1error(ASN1_R_MISSING_EOC);
		return 0;
	}

	return 1;
}

static int
asn1_check_eoc_cbs(CBS *cbs)
{
	uint16_t eoc;

	if (!CBS_peek_u16(cbs, &eoc))
		return 0;
	if (eoc != 0)
		return 0;

	return CBS_skip(cbs, 2);
}

static int
asn1_check_eoc(const unsigned char **in, long len)
{
	const unsigned char *p;

	if (len < 2)
		return 0;
	p = *in;
	if (!p[0] && !p[1]) {
		*in += 2;
		return 1;
	}
	return 0;
}

static int
asn1_check_tag_cbs(CBS *cbs, size_t *out_len, int *out_tag, uint8_t *out_class,
    char *out_indefinite, char *out_constructed, int expected_tag,
    int expected_class, char optional)
{
	int constructed, indefinite;
	uint32_t tag_number;
	uint8_t tag_class;
	size_t length;

	if (out_len != NULL)
		*out_len = 0;
	if (out_tag != NULL)
		*out_tag = 0;
	if (out_class != NULL)
		*out_class = 0;
	if (out_indefinite != NULL)
		*out_indefinite = 0;
	if (out_constructed != NULL)
		*out_constructed = 0;

	if (!asn1_get_identifier_cbs(cbs, 0, &tag_class, &constructed,
	    &tag_number)) {
		ASN1error(ASN1_R_BAD_OBJECT_HEADER);
		return 0;
	}
	if (expected_tag >= 0) {
		if (expected_tag != tag_number ||
		    expected_class != tag_class << 6) {
			/* Indicate missing type if this is OPTIONAL. */
			if (optional)
				return -1;

			ASN1error(ASN1_R_WRONG_TAG);
			return 0;
		}
	}
	if (!asn1_get_length_cbs(cbs, 0, &indefinite, &length)) {
		ASN1error(ASN1_R_BAD_OBJECT_HEADER);
		return 0;
	}

	/* Indefinite length can only be used with constructed encoding. */
	if (indefinite && !constructed) {
		ASN1error(ASN1_R_BAD_OBJECT_HEADER);
		return 0;
	}

	if (!indefinite && CBS_len(cbs) < length) {
		ASN1error(ASN1_R_TOO_LONG);
		return 0;
	}

	if (tag_number > INT_MAX) {
		ASN1error(ASN1_R_TOO_LONG);
		return 0;
	}

	if (indefinite)
		length = CBS_len(cbs);

	if (out_len != NULL)
		*out_len = length;
	if (out_tag != NULL)
		*out_tag = tag_number;
	if (out_class != NULL)
		*out_class = tag_class << 6;
	if (out_indefinite != NULL && indefinite)
		*out_indefinite = 1 << 0;
	if (out_constructed != NULL && constructed)
		*out_constructed = 1 << 5;

	return 1;
}

static int
asn1_check_tag(long *out_len, int *out_tag, unsigned char *out_class,
    char *out_indefinite, char *out_constructed, const unsigned char **in,
    long len, int expected_tag, int expected_class, char optional)
{
	size_t length;
	CBS cbs;
	int ret;

	if (len < 0)
		return 0;

	CBS_init(&cbs, *in, len);

	ret = asn1_check_tag_cbs(&cbs, &length, out_tag, out_class,
	    out_indefinite, out_constructed, expected_tag, expected_class,
	    optional);

	if (length > LONG_MAX)
		return 0;
	if (out_len != NULL)
		*out_len = (long)length;

	if (ret == 1)
		*in = CBS_data(&cbs);

	return ret;
}
