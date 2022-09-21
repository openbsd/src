# $OpenBSD: symbols.awk,v 1.7 2022/09/21 15:24:45 tb Exp $

# Copyright (c) 2018,2020 Theo Buehler <tb@openbsd.org>
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

# usage: awk -f symbols.awk < Symbols.list > symbols.c

BEGIN {
	printf("#include <openssl/pem.h> /* CMS special */\n\n")
	printf("#include \"include_headers.c\"\n\n")
}

/^DHparams_it$/							||
/^DSA_SIG_it$/							||
/^ECDSA_SIG_it$/						||
/^ECPARAMETERS_it$/						||
/^ECPKPARAMETERS_it$/						||
/^EC_PRIVATEKEY_it$/						||
/^ESS_CERT_ID_it$/						||
/^ESS_ISSUER_SERIAL_it$/					||
/^ESS_SIGNING_CERT_it$/						||
/^NETSCAPE_ENCRYPTED_PKEY_it$/					||
/^NETSCAPE_PKEY_it$/						||
/^TS_ACCURACY_it$/						||
/^TS_MSG_IMPRINT_it$/						||
/^TS_REQ_it$/							||
/^TS_RESP_it$/							||
/^TS_STATUS_INFO_it$/						||
/^TS_TST_INFO_it$/						||
/^X509_ATTRIBUTE_SET_it$/					||
/^X509_NAME_ENTRIES_it$/					||
/^X509_NAME_INTERNAL_it$/					||
/^X9_62_CHARACTERISTIC_TWO_it$/					||
/^X9_62_CURVE_it$/						||
/^X9_62_FIELDID_it$/						||
/^X9_62_PENTANOMIAL_it$/ {
	printf("extern ASN1_ITEM %s;\n", $0)
}

# internal function used in libtls
/^ASN1_time_tm_clamp_notafter$/ {
	printf("extern int ASN1_time_tm_clamp_notafter(struct tm *);\n")
}

/^OBJ_bsearch_$/ {
	printf("const void *OBJ_bsearch_(const void *key, const void *base, int num,\n")
	printf("    int size, int (*cmp)(const void *, const void *));\n")
}

# These are machdep (at least cpuid_setup and ia32cap_P are internal on amd64).
/^OPENSSL_cpuid_setup$/						||
/^OPENSSL_cpu_caps$/						||
/^OPENSSL_ia32cap_P$/ {
	printf("/* skipped %s */\n", $0)
	next
}

/^OPENSSL_strcasecmp$/ {
	printf("extern int %s(const char *, const char *);\n", $0)
}

/^OPENSSL_strncasecmp$/ {
	printf("extern int %s(const char *, const char *, size_t);\n", $0)
}

/^BIO_CONNECT_free$/						||
/^ECPARAMETERS_free$/						||
/^ECPKPARAMETERS_free$/						||
/^EC_PRIVATEKEY_free$/						||
/^NETSCAPE_ENCRYPTED_PKEY_free$/				||
/^NETSCAPE_PKEY_free$/						||
/^X9_62_CHARACTERISTIC_TWO_free$/				||
/^X9_62_PENTANOMIAL_free$/ {
	printf("extern void %s(void *);\n", $0)
}

/^BIO_CONNECT_new$/						||
/^ECPARAMETERS_new$/						||
/^ECPKPARAMETERS_new$/						||
/^EC_PRIVATEKEY_new$/						||
/^NETSCAPE_ENCRYPTED_PKEY_new$/					||
/^NETSCAPE_PKEY_new$/						||
/^X9_62_CHARACTERISTIC_TWO_new$/				||
/^X9_62_PENTANOMIAL_new$/ {
	printf("extern void *%s(void);\n", $0)
}

/^d2i_ECPKPARAMETERS$/						||
/^d2i_EC_PRIVATEKEY$/						||
/^d2i_NETSCAPE_ENCRYPTED_PKEY$/					||
/^d2i_NETSCAPE_PKEY$/ {
	printf("extern void *%s", $0)
	printf("(void *, const unsigned char *, const unsigned char *);\n")
}

/^i2d_ECPKPARAMETERS$/						||
/^i2d_EC_PRIVATEKEY$/						||
/^i2d_NETSCAPE_ENCRYPTED_PKEY$/					||
/^i2d_NETSCAPE_PKEY$/ {
	printf("extern int %s", $0)
	printf("(const void *, unsigned char **);\n")
}

{
	symbols[$0] = $0

	# Undefine aliases, so we don't accidentally leave them in Symbols.list.
	# The _cfb ciphers are aliased to _cfb64, so skip them.
	if ($0 !~ "^EVP_.*cfb$")
		printf("#ifdef %s\n#undef %s\n#endif\n", $0, $0)
}

END {
	printf("\nint\nmain(void)\n{\n")
	printf("\tsize_t i;\n");

	printf("\tstruct {\n")
	printf("\t\tconst char *const name;\n")
	printf("\t\tconst void *addr;\n")
	printf("\t} symbols[] = {\n")

	for (symbol in symbols) {
		printf("\t\t{\n")
		printf("\t\t\t.name = \"%s\",\n", symbol)
		printf("\t\t\t.addr = &%s,\n", symbol)
		printf("\t\t},\n")
	}

	printf("\t\};\n\n")

	printf("\tfor (i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++)\n")
	printf("\t\tfprintf(stderr, \"%%s: %%p\\n\", symbols[i].name, symbols[i].addr);\n")
	printf("\n\tprintf(\"OK\\n\");\n")
	printf("\n\treturn 0;\n}\n")
}
