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

/* $ISC: rdata_test.c,v 1.35 2001/01/09 21:41:34 bwelling Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/lex.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/rdata.h>
#include <dns/compress.h>
#include <dns/rdataclass.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/result.h>

isc_mem_t *mctx;
isc_lex_t *lex;

isc_lexspecials_t specials;

static void
viastruct(dns_rdata_t *rdata, isc_mem_t *mctx,
	  dns_rdata_t *rdata2, isc_buffer_t *b)
{
	isc_result_t result;
	void *sp = NULL;
	isc_boolean_t need_free = ISC_FALSE;
	dns_rdatatype_t rdt;
	dns_rdataclass_t rdc;

	UNUSED(rdata2);	/* XXXMPA remove when fromstruct is ready. */
	UNUSED(b);

	switch (rdata->type) {
	case dns_rdatatype_a6: {
		dns_rdata_in_a6_t in_a6;
		result = dns_rdata_tostruct(rdata, sp = &in_a6, NULL);
		break;
	}
	case dns_rdatatype_a: {
		switch (rdata->rdclass) {
		case dns_rdataclass_hs: {
			dns_rdata_hs_a_t hs_a;
			result = dns_rdata_tostruct(rdata, sp = &hs_a, NULL);
			break;
		}
		case dns_rdataclass_in: {
			dns_rdata_in_a_t in_a;
			result = dns_rdata_tostruct(rdata, sp = &in_a, NULL);
			break;
		}
		default:
			result = ISC_R_NOTIMPLEMENTED;
			break;
		}
		break;
	}
	case dns_rdatatype_aaaa: {
		dns_rdata_in_aaaa_t in_aaaa;
		result = dns_rdata_tostruct(rdata, sp = &in_aaaa, NULL);
		break;
	}
	case dns_rdatatype_afsdb: {
		dns_rdata_afsdb_t afsdb;
		result = dns_rdata_tostruct(rdata, sp = &afsdb, NULL);
		break;
	}
	case dns_rdatatype_any: {
		result = ISC_R_NOTIMPLEMENTED;
		break;
	}
	case dns_rdatatype_cert: {
		dns_rdata_cert_t cert;
		result = dns_rdata_tostruct(rdata, sp = &cert, NULL);
		break;
	}
	case dns_rdatatype_cname: {
		dns_rdata_cname_t cname;
		result = dns_rdata_tostruct(rdata, sp = &cname, NULL);
		break;
	}
	case dns_rdatatype_dname: {
		dns_rdata_dname_t dname;
		result = dns_rdata_tostruct(rdata, sp = &dname, NULL);
		break;
	}
	case dns_rdatatype_gpos: {
		dns_rdata_gpos_t gpos;
		result = dns_rdata_tostruct(rdata, sp = &gpos, NULL);
		break;
	}
	case dns_rdatatype_hinfo: {
		dns_rdata_hinfo_t hinfo;
		result = dns_rdata_tostruct(rdata, sp = &hinfo, NULL);
		break;
	}
	case dns_rdatatype_isdn: {
		dns_rdata_isdn_t isdn;
		result = dns_rdata_tostruct(rdata, sp = &isdn, NULL);
		break;
	}
	case dns_rdatatype_key: {
		dns_rdata_key_t key;
		result = dns_rdata_tostruct(rdata, sp = &key, NULL);
		break;
	}
	case dns_rdatatype_kx: {
		dns_rdata_in_kx_t in_kx;
		result = dns_rdata_tostruct(rdata, sp = &in_kx, NULL);
		break;
	}
	case dns_rdatatype_loc: {
		dns_rdata_loc_t loc;
		result = dns_rdata_tostruct(rdata, sp = &loc, NULL);
		break;
	}
	case dns_rdatatype_mb: {
		dns_rdata_mb_t mb;
		result = dns_rdata_tostruct(rdata, sp = &mb, NULL);
		break;
	}
	case dns_rdatatype_md: {
		dns_rdata_md_t md;
		result = dns_rdata_tostruct(rdata, sp = &md, NULL);
		break;
	}
	case dns_rdatatype_mf: {
		dns_rdata_mf_t mf;
		result = dns_rdata_tostruct(rdata, sp = &mf, NULL);
		break;
	}
	case dns_rdatatype_mg: {
		dns_rdata_mg_t mg;
		result = dns_rdata_tostruct(rdata, sp = &mg, NULL);
		break;
	}
	case dns_rdatatype_minfo: {
		dns_rdata_minfo_t minfo;
		result = dns_rdata_tostruct(rdata, sp = &minfo, NULL);
		break;
	}
	case dns_rdatatype_mr: {
		dns_rdata_mr_t mr;
		result = dns_rdata_tostruct(rdata, sp = &mr, NULL);
		break;
	}
	case dns_rdatatype_mx: {
		dns_rdata_mx_t mx;
		result = dns_rdata_tostruct(rdata, sp = &mx, NULL);
		break;
	}
	case dns_rdatatype_naptr: {
		dns_rdata_in_naptr_t in_naptr;
		result = dns_rdata_tostruct(rdata, sp = &in_naptr, NULL);
		break;
	}
	case dns_rdatatype_ns: {
		dns_rdata_ns_t ns;
		result = dns_rdata_tostruct(rdata, sp = &ns, NULL);
		break;
	}
	case dns_rdatatype_nsap: {
		dns_rdata_in_nsap_t in_nsap;
		result = dns_rdata_tostruct(rdata, sp = &in_nsap, NULL);
		break;
	}
	case dns_rdatatype_nsap_ptr: {
		dns_rdata_in_nsap_ptr_t in_nsap_ptr;
		result = dns_rdata_tostruct(rdata, sp = &in_nsap_ptr, NULL);
		break;
	}
	case dns_rdatatype_null: {
		dns_rdata_null_t null;
		result = dns_rdata_tostruct(rdata, sp = &null, NULL);
		break;
	}
	case dns_rdatatype_nxt: {
		dns_rdata_nxt_t nxt;
		result = dns_rdata_tostruct(rdata, sp = &nxt, NULL);
		break;
	}
	case dns_rdatatype_opt: {
		dns_rdata_opt_t opt;
		result = dns_rdata_tostruct(rdata, sp = &opt, NULL);
		break;
	}
	case dns_rdatatype_ptr: {
		dns_rdata_ptr_t ptr;
		result = dns_rdata_tostruct(rdata, sp = &ptr, NULL);
		break;
	}
	case dns_rdatatype_px: {
		dns_rdata_in_px_t in_px;
		result = dns_rdata_tostruct(rdata, sp = &in_px, NULL);
		break;
	}
	case dns_rdatatype_rp: {
		dns_rdata_rp_t rp;
		result = dns_rdata_tostruct(rdata, sp = &rp, NULL);
		break;
	}
	case dns_rdatatype_rt: {
		dns_rdata_rt_t rt;
		result = dns_rdata_tostruct(rdata, sp = &rt, NULL);
		break;
	}
	case dns_rdatatype_sig: {
		dns_rdata_sig_t sig;
		result = dns_rdata_tostruct(rdata, sp = &sig, NULL);
		break;
	}
	case dns_rdatatype_soa: {
		dns_rdata_soa_t soa;
		result = dns_rdata_tostruct(rdata, sp = &soa, NULL);
		break;
	}
	case dns_rdatatype_srv: {
		dns_rdata_in_srv_t in_srv;
		result = dns_rdata_tostruct(rdata, sp = &in_srv, NULL);
		break;
	}
	case dns_rdatatype_tkey: {
		dns_rdata_tkey_t tkey;
		result = dns_rdata_tostruct(rdata, sp = &tkey, NULL);
		break;
	}
	case dns_rdatatype_tsig: {
		dns_rdata_any_tsig_t tsig;
		result = dns_rdata_tostruct(rdata, sp = &tsig, NULL);
		break;
	}
	case dns_rdatatype_txt: {
		dns_rdata_txt_t txt;
		result = dns_rdata_tostruct(rdata, sp = &txt, NULL);
		break;
	}
	case dns_rdatatype_unspec: {
		dns_rdata_unspec_t unspec;
		result = dns_rdata_tostruct(rdata, sp = &unspec, NULL);
		break;
	}
	case dns_rdatatype_wks: {
		dns_rdata_in_wks_t in_wks;
		result = dns_rdata_tostruct(rdata, sp = &in_wks, NULL);
		break;
	}
	case dns_rdatatype_x25: {
		dns_rdata_x25_t x25;
		result = dns_rdata_tostruct(rdata, sp = &x25, NULL);
		break;
	}
	default:
		result = ISC_R_NOTIMPLEMENTED;
		break;
	}
	if (result != ISC_R_SUCCESS)
		fprintf(stdout, "viastruct: tostuct %d %d return %s\n",
			rdata->type, rdata->rdclass,
			dns_result_totext(result));
	else
		dns_rdata_freestruct(sp);

	switch (rdata->type) {
	case dns_rdatatype_a6: {
		dns_rdata_in_a6_t in_a6;
		result = dns_rdata_tostruct(rdata, sp = &in_a6, mctx);
		break;
	}
	case dns_rdatatype_a: {
		switch (rdata->rdclass) {
		case dns_rdataclass_hs: {
			dns_rdata_hs_a_t hs_a;
			result = dns_rdata_tostruct(rdata, sp = &hs_a, mctx);
			break;
		}
		case dns_rdataclass_in: {
			dns_rdata_in_a_t in_a;
			result = dns_rdata_tostruct(rdata, sp = &in_a, mctx);
			break;
		}
		default:
			result = ISC_R_NOTIMPLEMENTED;
			break;
		}
		break;
	}
	case dns_rdatatype_aaaa: {
		dns_rdata_in_aaaa_t in_aaaa;
		result = dns_rdata_tostruct(rdata, sp = &in_aaaa, mctx);
		break;
	}
	case dns_rdatatype_afsdb: {
		dns_rdata_afsdb_t afsdb;
		result = dns_rdata_tostruct(rdata, sp = &afsdb, mctx);
		break;
	}
	case dns_rdatatype_any: {
		result = ISC_R_NOTIMPLEMENTED;
		break;
	}
	case dns_rdatatype_cert: {
		dns_rdata_cert_t cert;
		result = dns_rdata_tostruct(rdata, sp = &cert, mctx);
		break;
	}
	case dns_rdatatype_cname: {
		dns_rdata_cname_t cname;
		result = dns_rdata_tostruct(rdata, sp = &cname, mctx);
		break;
	}
	case dns_rdatatype_dname: {
		dns_rdata_dname_t dname;
		result = dns_rdata_tostruct(rdata, sp = &dname, mctx);
		break;
	}
	case dns_rdatatype_gpos: {
		dns_rdata_gpos_t gpos;
		result = dns_rdata_tostruct(rdata, sp = &gpos, mctx);
		break;
	}
	case dns_rdatatype_hinfo: {
		dns_rdata_hinfo_t hinfo;
		result = dns_rdata_tostruct(rdata, sp = &hinfo, mctx);
		break;
	}
	case dns_rdatatype_isdn: {
		dns_rdata_isdn_t isdn;
		result = dns_rdata_tostruct(rdata, sp = &isdn, mctx);
		break;
	}
	case dns_rdatatype_key: {
		dns_rdata_key_t key;
		result = dns_rdata_tostruct(rdata, sp = &key, mctx);
		break;
	}
	case dns_rdatatype_kx: {
		dns_rdata_in_kx_t in_kx;
		result = dns_rdata_tostruct(rdata, sp = &in_kx, mctx);
		break;
	}
	case dns_rdatatype_loc: {
		dns_rdata_loc_t loc;
		result = dns_rdata_tostruct(rdata, sp = &loc, mctx);
		break;
	}
	case dns_rdatatype_mb: {
		dns_rdata_mb_t mb;
		result = dns_rdata_tostruct(rdata, sp = &mb, mctx);
		break;
	}
	case dns_rdatatype_md: {
		dns_rdata_md_t md;
		result = dns_rdata_tostruct(rdata, sp = &md, mctx);
		break;
	}
	case dns_rdatatype_mf: {
		dns_rdata_mf_t mf;
		result = dns_rdata_tostruct(rdata, sp = &mf, mctx);
		break;
	}
	case dns_rdatatype_mg: {
		dns_rdata_mg_t mg;
		result = dns_rdata_tostruct(rdata, sp = &mg, mctx);
		break;
	}
	case dns_rdatatype_minfo: {
		dns_rdata_minfo_t minfo;
		result = dns_rdata_tostruct(rdata, sp = &minfo, mctx);
		break;
	}
	case dns_rdatatype_mr: {
		dns_rdata_mr_t mr;
		result = dns_rdata_tostruct(rdata, sp = &mr, mctx);
		break;
	}
	case dns_rdatatype_mx: {
		dns_rdata_mx_t mx;
		result = dns_rdata_tostruct(rdata, sp = &mx, mctx);
		break;
	}
	case dns_rdatatype_naptr: {
		dns_rdata_in_naptr_t in_naptr;
		result = dns_rdata_tostruct(rdata, sp = &in_naptr, mctx);
		break;
	}
	case dns_rdatatype_ns: {
		dns_rdata_ns_t ns;
		result = dns_rdata_tostruct(rdata, sp = &ns, mctx);
		break;
	}
	case dns_rdatatype_nsap: {
		dns_rdata_in_nsap_t in_nsap;
		result = dns_rdata_tostruct(rdata, sp = &in_nsap, mctx);
		break;
	}
	case dns_rdatatype_nsap_ptr: {
		dns_rdata_in_nsap_ptr_t in_nsap_ptr;
		result = dns_rdata_tostruct(rdata, sp = &in_nsap_ptr, mctx);
		break;
	}
	case dns_rdatatype_null: {
		dns_rdata_null_t null;
		result = dns_rdata_tostruct(rdata, sp = &null, mctx);
		break;
	}
	case dns_rdatatype_nxt: {
		dns_rdata_nxt_t nxt;
		result = dns_rdata_tostruct(rdata, sp = &nxt, mctx);
		break;
	}
	case dns_rdatatype_opt: {
		dns_rdata_opt_t opt;
		result = dns_rdata_tostruct(rdata, sp = &opt, mctx);
		break;
	}
	case dns_rdatatype_ptr: {
		dns_rdata_ptr_t ptr;
		result = dns_rdata_tostruct(rdata, sp = &ptr, mctx);
		break;
	}
	case dns_rdatatype_px: {
		dns_rdata_in_px_t in_px;
		result = dns_rdata_tostruct(rdata, sp = &in_px, mctx);
		break;
	}
	case dns_rdatatype_rp: {
		dns_rdata_rp_t rp;
		result = dns_rdata_tostruct(rdata, sp = &rp, mctx);
		break;
	}
	case dns_rdatatype_rt: {
		dns_rdata_rt_t rt;
		result = dns_rdata_tostruct(rdata, sp = &rt, mctx);
		break;
	}
	case dns_rdatatype_sig: {
		dns_rdata_sig_t sig;
		result = dns_rdata_tostruct(rdata, sp = &sig, mctx);
		break;
	}
	case dns_rdatatype_soa: {
		dns_rdata_soa_t soa;
		result = dns_rdata_tostruct(rdata, sp = &soa, mctx);
		break;
	}
	case dns_rdatatype_srv: {
		dns_rdata_in_srv_t in_srv;
		result = dns_rdata_tostruct(rdata, sp = &in_srv, mctx);
		break;
	}
	case dns_rdatatype_tkey: {
		dns_rdata_tkey_t tkey;
		result = dns_rdata_tostruct(rdata, sp = &tkey, mctx);
		break;
	}
	case dns_rdatatype_tsig: {
		dns_rdata_any_tsig_t tsig;
		result = dns_rdata_tostruct(rdata, sp = &tsig, mctx);
		break;
	}
	case dns_rdatatype_txt: {
		dns_rdata_txt_t txt;
		result = dns_rdata_tostruct(rdata, sp = &txt, mctx);
		break;
	}
	case dns_rdatatype_unspec: {
		dns_rdata_unspec_t unspec;
		result = dns_rdata_tostruct(rdata, sp = &unspec, mctx);
		break;
	}
	case dns_rdatatype_wks: {
		dns_rdata_in_wks_t in_wks;
		result = dns_rdata_tostruct(rdata, sp = &in_wks, mctx);
		break;
	}
	case dns_rdatatype_x25: {
		dns_rdata_x25_t x25;
		result = dns_rdata_tostruct(rdata, sp = &x25, mctx);
		break;
	}
	default:
		result = ISC_R_NOTIMPLEMENTED;
		break;
	}
	if (result != ISC_R_SUCCESS)
		fprintf(stdout, "viastruct: tostuct %d %d return %s\n",
			rdata->type, rdata->rdclass,
			dns_result_totext(result));
	else {
		need_free = ISC_TRUE;

		rdc = rdata->rdclass;
		rdt = rdata->type;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, sp, b);
		if (result != ISC_R_SUCCESS)
			fprintf(stdout,
				"viastruct: fromstuct %d %d return %s\n",
				rdata->type, rdata->rdclass,
				dns_result_totext(result));
		else if (rdata->length != rdata2->length ||
			 memcmp(rdata->data, rdata2->data, rdata->length) != 0)
		{
			isc_uint32_t i;
			isc_uint32_t l;

			fprintf(stdout, "viastruct: memcmp failed\n");

			fprintf(stdout, "%d %d\n",
				rdata->length, rdata2->length);
			l = rdata->length;
			if (rdata2->length < l)
				l = rdata2->length;
			for (i = 0; i < l; i++)
				fprintf(stdout, "%02x %02x\n",
					rdata->data[i], rdata2->data[i]);
		}
	}
#if 0
	switch (rdata->type) {
	case dns_rdatatype_a6: {
		dns_rdata_in_a6_t in_a6;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &in_a6, b);
		break;
	}
	case dns_rdatatype_a: {
		switch (rdata->rdclass) {
		case dns_rdataclass_hs: {
			dns_rdata_hs_a_t hs_a;
			result = dns_rdata_fromstruct(rdata2, rdc, rdt,
						      &hs_a, b);
			break;
		}
		case dns_rdataclass_in: {
			dns_rdata_in_a_t in_a;
			result = dns_rdata_fromstruct(rdata2, rdc, rdt,
						      &in_a, b);
			break;
		}
		default:
			result = ISC_R_NOTIMPLEMENTED;
			break;
		}
		break;
	}
	case dns_rdatatype_aaaa: {
		dns_rdata_in_aaaa_t in_aaaa;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &in_aaaa, b);
		break;
	}
	case dns_rdatatype_afsdb: {
		dns_rdata_afsdb_t afsdb;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &afsdb, b);
		break;
	}
	case dns_rdatatype_any: {
		result = ISC_R_NOTIMPLEMENTED;
		break;
	}
	case dns_rdatatype_cert: {
		dns_rdata_cert_t cert;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &cert, b);
		break;
	}
	case dns_rdatatype_cname: {
		dns_rdata_cname_t cname;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &cname, b);
		break;
	}
	case dns_rdatatype_dname: {
		dns_rdata_dname_t dname;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &dname, b);
		break;
	}
	case dns_rdatatype_gpos: {
		dns_rdata_gpos_t gpos;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &gpos, b);
		break;
	}
	case dns_rdatatype_hinfo: {
		dns_rdata_hinfo_t hinfo;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &hinfo, b);
		break;
	}
	case dns_rdatatype_isdn: {
		dns_rdata_isdn_t isdn;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &isdn, b);
		break;
	}
	case dns_rdatatype_key: {
		dns_rdata_key_t key;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &key, b);
		break;
	}
	case dns_rdatatype_kx: {
		dns_rdata_in_kx_t in_kx;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &in_kx, b);
		break;
	}
	case dns_rdatatype_loc: {
		dns_rdata_loc_t loc;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &loc, b);
		break;
	}
	case dns_rdatatype_mb: {
		dns_rdata_mb_t mb;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &mb, b);
		break;
	}
	case dns_rdatatype_md: {
		dns_rdata_md_t md;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &md, b);
		break;
	}
	case dns_rdatatype_mf: {
		dns_rdata_mf_t mf;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &mf, b);
		break;
	}
	case dns_rdatatype_mg: {
		dns_rdata_mg_t mg;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &mg, b);
		break;
	}
	case dns_rdatatype_minfo: {
		dns_rdata_minfo_t minfo;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &minfo, b);
		break;
	}
	case dns_rdatatype_mr: {
		dns_rdata_mr_t mr;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &mr, b);
		break;
	}
	case dns_rdatatype_mx: {
		dns_rdata_mx_t mx;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &mx, b);
		break;
	}
	case dns_rdatatype_naptr: {
		dns_rdata_in_naptr_t in_naptr;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &in_naptr, b);
		break;
	}
	case dns_rdatatype_ns: {
		dns_rdata_ns_t ns;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &ns, b);
		break;
	}
	case dns_rdatatype_nsap: {
		dns_rdata_in_nsap_t in_nsap;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &in_nsap, b);
		break;
	}
	case dns_rdatatype_nsap_ptr: {
		dns_rdata_in_nsap_ptr_t in_nsap_ptr;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &in_nsap_ptr,
					      b);
		break;
	}
	case dns_rdatatype_null: {
		dns_rdata_null_t null;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &null, b);
		break;
	}
	case dns_rdatatype_nxt: {
		dns_rdata_nxt_t nxt;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &nxt, b);
		break;
	}
	case dns_rdatatype_opt: {
		dns_rdata_opt_t opt;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &opt, b);
		break;
	}
	case dns_rdatatype_ptr: {
		dns_rdata_ptr_t ptr;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &ptr, b);
		break;
	}
	case dns_rdatatype_px: {
		dns_rdata_in_px_t in_px;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &in_px, b);
		break;
	}
	case dns_rdatatype_rp: {
		dns_rdata_rp_t rp;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &rp, b);
		break;
	}
	case dns_rdatatype_rt: {
		dns_rdata_rt_t rt;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &rt, b);
		break;
	}
	case dns_rdatatype_sig: {
		dns_rdata_sig_t sig;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &sig, b);
		break;
	}
	case dns_rdatatype_soa: {
		dns_rdata_soa_t soa;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &soa, b);
		break;
	}
	case dns_rdatatype_srv: {
		dns_rdata_in_srv_t in_srv;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &in_srv, b);
		break;
	}
	case dns_rdatatype_tkey: {
		dns_rdata_tkey_t tkey;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &tkey, b);
		break;
	}
	case dns_rdatatype_tsig: {
		dns_rdata_any_tsig_t tsig;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &tsig, b);
		break;
	}
	case dns_rdatatype_txt: {
		dns_rdata_txt_t txt;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &txt, b);
		break;
	}
	case dns_rdatatype_unspec: {
		dns_rdata_unspec_t unspec;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &unspec, b);
		break;
	}
	case dns_rdatatype_wks: {
		dns_rdata_in_wks_t in_wks;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &in_wks, b);
		break;
	}
	case dns_rdatatype_x25: {
		dns_rdata_x25_t x25;
		result = dns_rdata_fromstruct(rdata2, rdc, rdt, &x25, b);
		break;
	}
	default:
		result = ISC_R_NOTIMPLEMENTED;
		break;
	}
#endif
	if (need_free)
		dns_rdata_freestruct(sp);
}

int
main(int argc, char *argv[]) {
	isc_token_t token;
	isc_result_t result;
	int quiet = 0;
	int c;
	int stats = 0;
	unsigned int options = 0;
	dns_rdatatype_t type;
	dns_rdataclass_t class;
	dns_rdatatype_t lasttype = 0;
	char outbuf[16*1024];
	char inbuf[16*1024];
	char wirebuf[16*1024];
	char viabuf[16*1024];
	isc_buffer_t dbuf;
	isc_buffer_t tbuf;
	isc_buffer_t wbuf;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_t last = DNS_RDATA_INIT;
	int need_eol = 0;
	int wire = 0;
	dns_compress_t cctx;
	dns_decompress_t dctx;
	int trunc = 0;
	int add = 0;
	int len;
	int zero = 0;
	int debug = 0;
	isc_region_t region;
	int first = 1;
	int raw = 0;
	int tostruct = 0;

	while ((c = isc_commandline_parse(argc, argv, "dqswtarzS")) != -1) {
		switch (c) {
		case 'd':
			debug = 1;
			quiet = 0;
			break;
		case 'q':
			quiet = 1;
			debug = 0;
			break;
		case 's':
			stats = 1;
			break;
		case 'w':
			wire = 1;
			break;
		case 't':
			trunc = 1;
			break;
		case 'a':
			add = 1;
			break;
		case 'z':
			zero = 1;
			break;
		case 'r':
			raw++;
			break;
		case 'S':
			tostruct++;
			break;
		}
	}

	memset(&dctx, '0', sizeof dctx);
	dctx.allowed = DNS_COMPRESS_ALL;

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_lex_create(mctx, 256, &lex) == ISC_R_SUCCESS);

	/*
	 * Set up to lex DNS master file.
	 */

	specials['('] = 1;
	specials[')'] = 1;
	specials['"'] = 1;
	isc_lex_setspecials(lex, specials);
	options = ISC_LEXOPT_EOL;
	isc_lex_setcomments(lex, ISC_LEXCOMMENT_DNSMASTERFILE);

	RUNTIME_CHECK(isc_lex_openstream(lex, stdin) == ISC_R_SUCCESS);

	dns_rdata_init(&last);
	while ((result = isc_lex_gettoken(lex, options | ISC_LEXOPT_NUMBER,
					  &token)) == ISC_R_SUCCESS) {
		if (debug) fprintf(stdout, "token.type = %d\n", token.type);
		if (need_eol) {
			if (token.type == isc_tokentype_eol)
				need_eol = 0;
			continue;
		}
		if (token.type == isc_tokentype_eof)
			break;

		/*
		 * Get type.
		 */
		if (token.type == isc_tokentype_number) {
			type = token.value.as_ulong;
			isc_buffer_init(&tbuf, outbuf, sizeof(outbuf));
			result = dns_rdatatype_totext(type, &tbuf);
			fprintf(stdout, "type = %.*s(%d)\n",
				(int)tbuf.used, (char*)tbuf.base, type);
		} else if (token.type == isc_tokentype_string) {
			result = dns_rdatatype_fromtext(&type,
					&token.value.as_textregion);
			if (result != ISC_R_SUCCESS) {
				fprintf(stdout,
					"dns_rdatatype_fromtext "
					"returned %s(%d)\n",
					dns_result_totext(result), result);
				fflush(stdout);
				need_eol = 1;
				continue;
			}
			fprintf(stdout, "type = %.*s(%d)\n",
				(int)token.value.as_textregion.length,
				token.value.as_textregion.base, type);
		} else
			continue;

		result = isc_lex_gettoken(lex, options | ISC_LEXOPT_NUMBER,
					  &token);
		if (result != ISC_R_SUCCESS)
			break;
		if (token.type == isc_tokentype_eol)
			continue;
		if (token.type == isc_tokentype_eof)
			break;
		if (token.type == isc_tokentype_number) {
			class = token.value.as_ulong;
			isc_buffer_init(&tbuf, outbuf, sizeof(outbuf));
			result = dns_rdatatype_totext(class, &tbuf);
			fprintf(stdout, "class = %.*s(%d)\n",
				(int)tbuf.used, (char*)tbuf.base, class);
		} else if (token.type == isc_tokentype_string) {
			result = dns_rdataclass_fromtext(&class,
					&token.value.as_textregion);
			if (result != ISC_R_SUCCESS) {
				fprintf(stdout, "dns_rdataclass_fromtext "
					"returned %s(%d)\n",
					dns_result_totext(result), result);
				fflush(stdout);
				need_eol = 1;
				continue;
			}
			fprintf(stdout, "class = %.*s(%d)\n",
				(int)token.value.as_textregion.length,
				token.value.as_textregion.base, class);
		} else
			continue;

		fflush(stdout);
		dns_rdata_init(&rdata);
		isc_buffer_init(&dbuf, inbuf, sizeof(inbuf));
		result = dns_rdata_fromtext(&rdata, class, type, lex,
					    NULL, ISC_FALSE, mctx, &dbuf,
					    NULL);
		if (result != ISC_R_SUCCESS) {
			fprintf(stdout,
				"dns_rdata_fromtext returned %s(%d)\n",
				dns_result_totext(result), result);
			fflush(stdout);
			continue;
		}
		if (raw) {
			unsigned int i;
			for (i = 0 ; i < rdata.length ; /* */ ) {
				fprintf(stdout, "%02x", rdata.data[i]);
				if ((++i % 20) == 0)
					fputs("\n", stdout);
				else
					if (i == rdata.length)
						fputs("\n", stdout);
					else
						fputs(" ", stdout);
			}
		}

		/*
		 * Convert to wire and back?
		 */
		if (wire) {
			result = dns_compress_init(&cctx, -1, mctx);
			if (result != ISC_R_SUCCESS) {
				fprintf(stdout,
					"dns_compress_init returned %s(%d)\n",
					dns_result_totext(result), result);
				continue;
			}
			isc_buffer_init(&wbuf, wirebuf, sizeof(wirebuf));
			result = dns_rdata_towire(&rdata, &cctx, &wbuf);
			dns_compress_invalidate(&cctx);
			if (result != ISC_R_SUCCESS) {
				fprintf(stdout,
					"dns_rdata_towire returned %s(%d)\n",
					dns_result_totext(result), result);
				continue;
			}
			len = wbuf.used - wbuf.current;
			if (raw > 2) {
				unsigned int i;
				fputs("\n", stdout);
				for (i = 0 ; i < (unsigned int)len ; /* */ ) {
					fprintf(stdout, "%02x",
				((unsigned char*)wbuf.base)[i + wbuf.current]);
					if ((++i % 20) == 0)
						fputs("\n", stdout);
					else
						if (i == wbuf.used)
							fputs("\n", stdout);
						else
							fputs(" ", stdout);
				}
			}
			if (zero)
				len = 0;
			if (trunc)
				len = (len * 3) / 4;
			if (add) {
				isc_buffer_add(&wbuf, len / 4 + 1);
				len += len / 4 + 1;
			}

			isc_buffer_setactive(&wbuf, len);
			dns_rdata_init(&rdata);
			isc_buffer_init(&dbuf, inbuf, sizeof(inbuf));
			dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_ANY);
			result = dns_rdata_fromwire(&rdata, class, type, &wbuf,
						    &dctx, ISC_FALSE, &dbuf);
			dns_decompress_invalidate(&dctx);
			if (result != ISC_R_SUCCESS) {
			fprintf(stdout,
					"dns_rdata_fromwire returned %s(%d)\n",
					dns_result_totext(result), result);
				fflush(stdout);
				continue;
			}
		}
		if (raw > 1) {
			unsigned int i;
			fputs("\n", stdout);
			for (i = 0 ; i < rdata.length ; /* */ ) {
				fprintf(stdout, "%02x", rdata.data[i]);
				if ((++i % 20) == 0)
					fputs("\n", stdout);
				else
					if (i == rdata.length)
						fputs("\n", stdout);
					else
						fputs(" ", stdout);
			}
		}
		if (tostruct) {
			isc_mem_t *mctx2 = NULL;
			dns_rdata_t rdata2 = DNS_RDATA_INIT;
			isc_buffer_t vbuf;

			RUNTIME_CHECK(isc_mem_create(0, 0, &mctx2)
				      == ISC_R_SUCCESS);

			isc_buffer_init(&vbuf, viabuf, sizeof(viabuf));
			dns_rdata_init(&rdata2);
			viastruct(&rdata, mctx2, &rdata2, &vbuf);
			if (!quiet && stats)
				isc_mem_stats(mctx2, stdout);
			isc_mem_destroy(&mctx2);
		}

		isc_buffer_init(&tbuf, outbuf, sizeof(outbuf));
		result = dns_rdata_totext(&rdata, NULL, &tbuf);
		if (result != ISC_R_SUCCESS)
			fprintf(stdout, "dns_rdata_totext returned %s(%d)\n",
				dns_result_totext(result), result);
		else
			fprintf(stdout, "\"%.*s\"\n",
				(int)tbuf.used, (char*)tbuf.base);
		fflush(stdout);
		if (lasttype == type) {
			fprintf(stdout, "dns_rdata_compare = %d\n",
				dns_rdata_compare(&rdata, &last));

		}
		if (!first) {
			free(last.data);
		}
		dns_rdata_init(&last);
		region.base = malloc(region.length = rdata.length);
		if (region.base) {
			memcpy(region.base, rdata.data, rdata.length);
			dns_rdata_fromregion(&last, class, type, &region);
			lasttype = type;
			first = 0;
		} else
			first = 1;

	}
	if (result != ISC_R_EOF)
		printf("Result: %s\n", isc_result_totext(result));

	isc_lex_close(lex);
	isc_lex_destroy(&lex);
	if (!quiet && stats)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	return (0);
}
