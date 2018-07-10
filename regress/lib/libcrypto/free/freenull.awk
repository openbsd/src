# $OpenBSD: freenull.awk,v 1.1 2018/07/10 20:53:30 tb Exp $
# Copyright (c) 2018 Theo Buehler <tb@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# usage: awk -f freenull.awk < Symbols.list > freenull.c.body

# Skip this function because it calls abort(3).
/^CRYPTO_dbg_free/ {
	next
}

# Skip *_free functions that take more than one or no argument.
/^ASN1_item_ex_free$/				||
/^ASN1_item_free$/				||
/^ASN1_primitive_free$/				||
/^ASN1_template_free$/				||
/^CONF_modules_free$/				||
/^EVP_PKEY_asn1_set_free$/			||
/^OBJ_sigid_free$/				||
/^X509V3_section_free$/				||
/^X509V3_string_free$/				||
/^asn1_enc_free$/				||
/^sk_pop_free$/ {
	next
}

# Skip functions that are prototyped in a .c file.
/^BIO_CONNECT_free$/				||
/^CRYPTO_free$/					||
/^EC_PRIVATEKEY_free$/				||
/^ECPARAMETERS_free$/				||
/^ECPKPARAMETERS_free$/				||
/^NETSCAPE_ENCRYPTED_PKEY_free$/		||
/^NETSCAPE_PKEY_free$/				||
/^X9_62_CHARACTERISTIC_TWO_free$/		||
/^X9_62_PENTANOMIAL_free$/ {
	next
}

/^ENGINE_free$/ {
	printf("#ifndef OPENSSL_NO_ENGINE\n")
	printf("\tENGINE_free(NULL);\n")
	printf("#endif\n")
	next
}

/_free$/ {
	printf("\t%s(NULL);\n", $0)
}
