/* 
**  ca-fix, X.509 Certificate Patch Utility / Version 0.41
**
**  Copyright (c) 1997-1998
**  Dr Stephen N. Henson <shenson@drh-consultancy.demon.co.uk>
**  http://www.drh-consultancy.demon.co.uk/
**
**  Commercial and non-commercial use is permitted.
**
**  Any software using this code must include the following message in its
**  startup code or documentation and in any advertising material:
**  "This Product includes cryptographic software written by Dr S N Henson
**  (shenson@bigfoot.com)"
**
**  This software is allowed to be used in the mod_ssl package
**  without the above advertisment clause with permission by Dr S N Henson as
**  long as it's used under build-time only and never gets installed as part
**  of neither the Apache nor the mod_ssl package.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <bio.h>
#include <objects.h>
#include <asn1.h>
#include <asn1_mac.h>
#include <x509.h>
#include <err.h>
#include <crypto.h>
#include <stack.h>
#include <evp.h>
#include <pem.h>

#if SSLEAY_VERSION_NUMBER < 0x0900
#define OBJ_create(a,b,c) OBJ_create_and_add_object(a,b,c) 
#endif

typedef struct {
    int ca;
    ASN1_INTEGER *pathlen;
} BASIC_CONSTRAINTS;

int i2d_BASIC_CONSTRAINTS(BASIC_CONSTRAINTS *, unsigned char **);
ASN1_OBJECT *__OBJ_txt2obj(char *);

int i2d_BASIC_CONSTRAINTS(BASIC_CONSTRAINTS *a, unsigned char **pp)
{
    M_ASN1_I2D_vars(a);
    if (a->ca)
        M_ASN1_I2D_len(a->ca, i2d_ASN1_BOOLEAN);
    M_ASN1_I2D_len(a->pathlen, i2d_ASN1_INTEGER);

    M_ASN1_I2D_seq_total();
    if (a->ca)
        M_ASN1_I2D_put(a->ca, i2d_ASN1_BOOLEAN);
    M_ASN1_I2D_put(a->pathlen, i2d_ASN1_INTEGER);
    M_ASN1_I2D_finish();
}

typedef struct {
    char *name;
    char *value;
    char flag;
#define CERT_CRIT       0x1
#define CERT_RAW        0x2
#define CERT_RAW_FILE   0x4
} EXT_ADD;

STACK *exts;
STACK *extusage;

void add_ext(char *, char *, char);

unsigned char extbuf[10240];

int main(int argc, char **argv)
{
    char *infile = NULL, *outfile = NULL, *keyname = NULL;
    BIO *in = NULL, *out = NULL, *inkey = NULL, *bio_err = NULL;
    char **args;
    int i;
    int badarg = 0;
    int bconsadd = 0, bconsdel = 0;
    int nset = 0, nsclr = 0;
    unsigned char ntype = 0, noout = 0, exthex = 0, extparse = 0;
    unsigned char bscrit = 0, nscrit = 0, keycrit = 0, print = 0, sign = 1;
    unsigned char setkey = 0;
    X509 *cert;
    EVP_PKEY *pkey = NULL;
    BASIC_CONSTRAINTS bcons = {0, NULL};
    EVP_MD *dgst;

    if (bio_err == NULL)
        bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

    ERR_load_crypto_strings();
    args = argv + 1;

    while (*args) {
        if (*args[0] == '-') {
            if (!strcmp(*args, "-caset")) {
                bconsadd = 1;
                bcons.ca = 0xff;
            }
            else if (!strcmp(*args, "-caunset")) {
                if (bcons.ca)
                    badarg = 1;
                else
                    bconsadd = 1;
            }
            else if (!strcmp(*args, "-caclr"))
                bconsdel = 1;
            else if (!strcmp(*args, "-setkey"))
                setkey = 1;
            else if (!strcmp(*args, "-print"))
                print = 1;
            else if (!strcmp(*args, "-noout"))
                noout = 1;
            else if (!strcmp(*args, "-nosign"))
                sign = 0;
            else if (!strcmp(*args, "-exthex"))
                exthex = 1;
            else if (!strcmp(*args, "-extparse"))
                extparse = 1;
            else if (!strcmp(*args, "-nsclr"))
                nsclr = 1;
            else if (!strcmp(*args, "-nobscrit"))
                bscrit = 0;
            else if (!strcmp(*args, "-bscrit"))
                bscrit = 1;
            else if (!strcmp(*args, "-nscrit"))
                nscrit = 1;
            else if (!strcmp(*args, "-extcrit"))
                keycrit = 1;
            else if (!strcmp(*args, "-pathlen")) {
                if (args[1]) {
                    args++;
                    bconsadd = 1;
                    bcons.ca = 0xff;
                    bcons.pathlen = ASN1_INTEGER_new();
                    ASN1_INTEGER_set(bcons.pathlen,
                                     strtol(*args, NULL, 0));
                }
                else
                    badarg = 1;
            }
            else if (!strcmp(*args, "-nscertype")) {
                if (args[1]) {
                    args++;
                    nset = 1;
                    ntype = (unsigned char) strtol(*args, NULL, 0);
                }
                else
                    badarg = 1;
            }
            else if (!strcmp(*args, "-rawfile")) {
                if (args[1] && args[2]) {
                    add_ext(args[1], args[2], CERT_RAW_FILE);
                    args += 2;
                }
                else
                    badarg = 1;
            }
            else if (!strcmp(*args, "-Crawfile")) {
                if (args[1] && args[2]) {
                    add_ext(args[1], args[2], CERT_RAW_FILE | CERT_CRIT);
                    args += 2;
                }
                else
                    badarg = 1;
            }
            else if (!strcmp(*args, "-rawext")) {
                if (args[1] && args[2]) {
                    add_ext(args[1], args[2], CERT_RAW);
                    args += 2;
                }
                else
                    badarg = 1;
            }
            else if (!strcmp(*args, "-Crawext")) {
                if (args[1] && args[2]) {
                    add_ext(args[1], args[2], CERT_RAW | CERT_CRIT);
                    args += 2;
                }
                else
                    badarg = 1;
            }
            else if (!strcmp(*args, "-ext")) {
                if (args[1] && args[2]) {
                    add_ext(args[1], args[2], 0);
                    args += 2;
                }
                else
                    badarg = 1;
            }
            else if (!strcmp(*args, "-Cext")) {
                if (args[1] && args[2]) {
                    add_ext(args[1], args[2], CERT_CRIT);
                    args += 2;
                }
                else
                    badarg = 1;
            }
            else if (!strcmp(*args, "-extusage")) {
                if (args[1]) {
                    ASN1_OBJECT *obj;
                    if (!extusage)
                        extusage = sk_new(NULL);
                    obj = __OBJ_txt2obj(args[1]);
                    if (!obj) {
                        BIO_printf(bio_err, "Error parsing extended usage object\n");
                        ERR_print_errors(bio_err);
                        exit(1);
                    }
                    else
                        sk_push(extusage, (char *) obj);
                    args++;
                }
                else
                    badarg = 1;
            }
            else if (!strcmp(*args, "-delext")) {
                if (args[1]) {
                    add_ext(args[1], NULL, 0);
                    args++;
                }
                else
                    badarg = 1;
            }
            else if (!strcmp(*args, "-inkey")) {
                if (args[1]) {
                    args++;
                    keyname = *args;
                }
                else
                    badarg = 1;
            }
            else if (!strcmp(*args, "-in")) {
                if (args[1]) {
                    args++;
                    infile = *args;
                }
                else
                    badarg = 1;
            }
            else if (!strcmp(*args, "-out")) {
                if (args[1]) {
                    args++;
                    outfile = *args;
                }
                else
                    badarg = 1;
            }
            else
                badarg = 1;
        }
        else
            badarg = 1;
        args++;
    }

    if (badarg || (bconsadd && bconsdel)) {
        BIO_printf(bio_err, "ca-fix version 0.41, certificate patcher\n");
        BIO_printf(bio_err, "Written by Dr. S N Henson (shenson@bigfoot.com)\n");
        BIO_printf(bio_err, "ca-fix [args]\n");
        BIO_printf(bio_err, "-in cert.pem      input certificate.\n");
        BIO_printf(bio_err, "-out cert.pem     output certificate.\n");
        BIO_printf(bio_err, "-noout            don't output certificate\n");
        BIO_printf(bio_err, "-nosign           don't sign certificate\n");
        BIO_printf(bio_err, "-print            print certificate\n");
        BIO_printf(bio_err, "-extparse         ASN1 parse extensions\n");
        BIO_printf(bio_err, "-exthex           hex dump extensions\n");
        BIO_printf(bio_err, "-caset            set cA flag, add basic constraints\n");
        BIO_printf(bio_err, "-caunset          don't set cA flag, add basic constraints\n");
        BIO_printf(bio_err, "-caclr            delete basic constraints\n");
        BIO_printf(bio_err, "-pathlen n        set path length to \'n\'\n");
        BIO_printf(bio_err, "-bscrit           make basic constraints critical\n");
        BIO_printf(bio_err, "-nscrit           make nscertype critical (not recommended)\n");
        BIO_printf(bio_err, "-nscertype num    set nscertype to num\n");
        BIO_printf(bio_err, "-nsclr            delete nscertype\n");
        BIO_printf(bio_err, "-inkey pkey.pem   private key of signer\n");
        BIO_printf(bio_err, "Expert options:\n");
        BIO_printf(bio_err, "-setkey           changed certificate public key to match signer\n");
        BIO_printf(bio_err, "-delext ext       delete extension (can use OID)\n");
        BIO_printf(bio_err, "-ext genopt  val  add several extensions\n");
        BIO_printf(bio_err, "-Cext genopt val  add several critical extensions\n");
        BIO_printf(bio_err, "genopt can be:    keyUsage, nsCertType, nsBaseUrl, nsRevocationUrl,\n");
        BIO_printf(bio_err, "                  nsCaRevocationUrl, nsRenewalUrl, nsCaPolicyUrl,\n");
        BIO_printf(bio_err, "                  nsSslServerName, nsComment\n");
        BIO_printf(bio_err, "-rawext opt HEX   add raw extension (can use OID)\n");
        BIO_printf(bio_err, "-Crawext opt HEX  add critical raw extension (can use OID)\n");
        BIO_printf(bio_err, "-Crawfile opt fn  add raw extension from file (can use OID)\n");
        BIO_printf(bio_err, "-rawfile opt fn   add critical raw extension from file (can use OID)\n");
        BIO_printf(bio_err, "-extusage OID     add extended key usage extension\n");
        BIO_printf(bio_err, "-extcrit          make extended key usage extension critical\n");
        exit(1);
    }

    SSLeay_add_all_algorithms();
    X509v3_add_netscape_extensions();
    X509v3_add_standard_extensions();

    in = BIO_new(BIO_s_file());
    out = BIO_new(BIO_s_file());

    if (!infile)
        BIO_set_fp(in, stdin, BIO_NOCLOSE);
    else {
        if (!keyname)
            keyname = infile;
        if (BIO_read_filename(in, infile) <= 0) {
            perror(infile);
            exit(1);
        }
    }

    if (keyname) {
        inkey = BIO_new(BIO_s_file());
        if (BIO_read_filename(inkey, keyname) <= 0) {
            perror(keyname);
            exit(1);
        }
    }

    if (!outfile)
        BIO_set_fp(out, stdout, BIO_NOCLOSE);
    else {
        if (BIO_write_filename(out, outfile) <= 0) {
            perror(outfile);
            exit(1);
        }
    }

    cert = PEM_read_bio_X509(in, NULL, NULL);
    if (!cert) {
        ERR_print_errors(bio_err);
        exit(1);
    }

    if (sign || setkey) {
        pkey = PEM_read_bio_PrivateKey(inkey ? inkey : in, NULL, NULL);
        if (!pkey) {
            BIO_printf(bio_err, "Error loading private key\n");
            ERR_print_errors(bio_err);
            exit(1);
        }
    }

    /* OK we've got the certificate: now fix it up */

    /* Make it a V3 certificate */
    X509_set_version(cert, 2);

    if (setkey)
        X509_set_pubkey(cert, pkey);

    if (bconsadd || bconsdel) {
        int index;
        index = X509_get_ext_by_NID(cert, NID_basic_constraints, -1);
        if (index >= 0)
            X509_delete_ext(cert, index);
    }
    if (nset || nsclr) {
        int index;
        index = X509_get_ext_by_NID(cert, NID_netscape_cert_type, -1);
        if (index >= 0)
            X509_delete_ext(cert, index);
    }

    if (bconsadd) {
        ASN1_OCTET_STRING *bcons_ext;
        X509_EXTENSION *x;
        unsigned char *bcons_der, *p;
        int bcons_len;

        /* generate encoding of extension */
        bcons_len = i2d_BASIC_CONSTRAINTS(&bcons, NULL);
        bcons_der = malloc(bcons_len);
        p = bcons_der;
        i2d_BASIC_CONSTRAINTS(&bcons, &p);

        bcons_ext = ASN1_OCTET_STRING_new();
        ASN1_OCTET_STRING_set(bcons_ext, bcons_der, bcons_len);
        free(bcons_der);
        x = X509_EXTENSION_create_by_NID(NULL, NID_basic_constraints, bscrit,
                                         bcons_ext);
        ASN1_OCTET_STRING_free(bcons_ext);
        if (!x) {
            BIO_printf(bio_err, "Error creating extension\n");
            ERR_print_errors(bio_err);
            exit(1);
        }
        X509_add_ext(cert, x, -1);
        X509_EXTENSION_free(x);
    }

    if (nset) {
        X509_EXTENSION *x;
        ASN1_OCTET_STRING *str;
        int data_type;
        str = NULL;
        data_type = X509v3_data_type_by_NID(NID_netscape_cert_type);
        X509v3_pack_string(&str, data_type, &ntype, 1);
        x = X509_EXTENSION_create_by_NID(NULL, NID_netscape_cert_type,
                                         nscrit, str);
        X509_add_ext(cert, x, -1);
        X509_EXTENSION_free(x);
        /*ASN1_OCTET_STRING_free(str); */
    }
    /* Handle the generic extensions */
    if (exts) {
        while (sk_num(exts)) {
            EXT_ADD *tmpext;
            int ext_nid, index;
            char *ext_str;
            unsigned char ext_bit;
            ASN1_OCTET_STRING *str;
            X509_EXTENSION *x;
            ASN1_OBJECT *extobj;
            tmpext = (EXT_ADD *) sk_pop(exts);
            extobj = __OBJ_txt2obj(tmpext->name);
            if (!extobj) {
                fprintf(stderr, "Invalid object %s\n", tmpext->name);
                ERR_print_errors(bio_err);
                exit(1);
            }
            /* Delete extension if already present */
            index = X509_get_ext_by_OBJ(cert, extobj, -1);
            if (index >= 0)
                X509_delete_ext(cert, index);
            if (!tmpext->value)
                continue;
            ext_nid = OBJ_obj2nid(extobj);
            if (tmpext->flag & CERT_RAW) {
                /* Covert hex extension into an OCTET STRING */
                unsigned char *rawext, *p, *q, tmphex[3];
                long rawlen;
                rawlen = strlen(tmpext->value);
                if (rawlen & 1) {
                    fprintf(stderr, "Invalid raw extension length\n");
                    exit(1);
                }
                for (p = (unsigned char *)tmpext->value; *p; p++)
                    if (!isxdigit(*p)) {
                        fprintf(stderr, "Extension %s invalid hex digit %c\n",
                                tmpext->value, *p);
                        exit(1);
                    }
                rawlen >>= 1;
                rawext = Malloc(rawlen);
                tmphex[2] = 0;
                for (p = (unsigned char *)tmpext->value, q = rawext; *p; p += 2, q++) {
                    tmphex[0] = p[0];
                    tmphex[1] = p[1];
                    *q = (unsigned char)strtol((const char *)tmphex, NULL, 16);
                }
                str = ASN1_OCTET_STRING_new();
                ASN1_OCTET_STRING_set(str, rawext, rawlen);
                Free(rawext);
            }
            else if (tmpext->flag & CERT_RAW_FILE) {
                BIO *tmpin;
                int extlen;
                if (!(tmpin = BIO_new_file(tmpext->value, "rb"))) {
                    BIO_printf(bio_err, "Error opening file %s\n", tmpext->value);
                    ERR_print_errors(bio_err);
                }
                extlen = BIO_read(tmpin, (char *)extbuf, sizeof(extbuf));
                str = ASN1_STRING_new();
                ASN1_OCTET_STRING_set(str, extbuf, extlen);
            }
            else {
                /* Get extension type */
                switch (ext_nid) {

                case NID_key_usage:
                case NID_netscape_cert_type:

                    ext_bit = (unsigned char) strtol(tmpext->value, NULL, 0);
                    ext_str = NULL;

                    break;

                case NID_netscape_base_url:
                case NID_netscape_revocation_url:
                case NID_netscape_ca_revocation_url:
                case NID_netscape_renewal_url:
                case NID_netscape_ca_policy_url:
                case NID_netscape_ssl_server_name:
                case NID_netscape_comment:

                    ext_str = tmpext->value;

                    break;

                default:

                    fprintf(stderr, "Unsuported extension %s\n", tmpext->name);
                    exit(1);
                    break;
                }

                if (ext_str)
                    str = X509v3_pack_string(NULL, V_ASN1_IA5STRING,
                                      (unsigned char *)tmpext->value, 
                                      strlen(tmpext->value));
                else
                    str = X509v3_pack_string(NULL, V_ASN1_BIT_STRING,
                                             &ext_bit, 1);
            }
            x = X509_EXTENSION_create_by_NID(NULL, ext_nid,
                                             tmpext->flag & CERT_CRIT, str);
            X509_add_ext(cert, x, -1);
            X509_EXTENSION_free(x);
        }
    }

    /* Handle extended key usage */
    if (extusage) {
        int extlen;
        unsigned char *extder, *p;
        ASN1_OCTET_STRING *extkey;
        X509_EXTENSION *x;
        ASN1_OBJECT *extobj;


        extobj = __OBJ_txt2obj("2.5.29.37");
        /* generate encoding of extension */
        extlen = i2d_ASN1_SET(extusage, NULL, i2d_ASN1_OBJECT, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
        extder = malloc(extlen);
        p = extder;
        i2d_ASN1_SET(extusage, &p, i2d_ASN1_OBJECT, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
        extkey = ASN1_OCTET_STRING_new();
        ASN1_OCTET_STRING_set(extkey, extder, extlen);
        free(extder);
        x = X509_EXTENSION_create_by_OBJ(NULL, extobj, keycrit,
                                         extkey);
        if (!x) {
            BIO_printf(bio_err, "Error creating extension\n");
            ERR_print_errors(bio_err);
            exit(1);
        }
        X509_add_ext(cert, x, -1);
        X509_EXTENSION_free(x);
        sk_pop_free(extusage, ASN1_OBJECT_free);
        ASN1_OBJECT_free(extobj);
    }


    /* OK we've modified the certificate so it will have to be re-signed */
    dgst = EVP_get_digestbyobj(cert->sig_alg->algorithm);
    if (sign)
        X509_sign(cert, pkey, dgst);
    if (print)
        X509_print(out, cert);
    if (extparse || exthex) {
        BIO_printf(out, "X509 V3 Extensions.\n");
        for (i = 0; i < X509_get_ext_count(cert); i++) {
            X509_EXTENSION *tmpext;
            ASN1_OCTET_STRING *octval;
            tmpext = X509_get_ext(cert, i);
            octval = X509_EXTENSION_get_data(tmpext);
            i2a_ASN1_OBJECT(out, X509_EXTENSION_get_object(tmpext));
            BIO_printf(out, ":\n");
            if (exthex) {
                int j;
                for (j = 0; j < octval->length; j++)
                    BIO_printf(out, "%02X", octval->data[j]);
                BIO_printf(out, "\n");
            }
            if (extparse)
                ASN1_parse(out, octval->data, octval->length, 0);
        }
    }
    if (!noout)
        PEM_write_bio_X509(out, cert);

    return (0);

}

void add_ext(char *name, char *val, char flag)
{
    EXT_ADD *tmpext;
    if (!exts)
        exts = sk_new(NULL);
    tmpext = (EXT_ADD *) Malloc(sizeof(EXT_ADD));
    tmpext->name = name;
    tmpext->value = val;
    tmpext->flag = flag;
    sk_push(exts, (char *) tmpext);
}

ASN1_OBJECT *__OBJ_txt2obj(char *name)
{
    int obj_nid;
    ASN1_OBJECT *obj;
    obj_nid = OBJ_sn2nid(name);
    if (obj_nid != NID_undef)
        return OBJ_nid2obj(obj_nid);
    obj_nid = OBJ_create(name, name, name);
    if (obj_nid <= 0)
        return NULL;
    obj = OBJ_nid2obj(obj_nid);
    obj->flags &= ~ASN1_OBJECT_FLAG_DYNAMIC_DATA;
    return obj;
}

