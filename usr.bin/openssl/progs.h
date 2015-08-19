/* $OpenBSD: progs.h,v 1.5 2015/08/19 18:25:31 deraadt Exp $ */
/* Public domain */

extern int asn1parse_main(int argc, char *argv[]);
extern int ca_main(int argc, char *argv[]);
extern int certhash_main(int argc, char *argv[]);
extern int ciphers_main(int argc, char *argv[]);
extern int cms_main(int argc, char *argv[]);
extern int crl2pkcs7_main(int argc, char *argv[]);
extern int crl_main(int argc, char *argv[]);
extern int dgst_main(int argc, char *argv[]);
extern int dh_main(int argc, char *argv[]);
extern int dhparam_main(int argc, char *argv[]);
extern int dsa_main(int argc, char *argv[]);
extern int dsaparam_main(int argc, char *argv[]);
extern int ec_main(int argc, char *argv[]);
extern int ecparam_main(int argc, char *argv[]);
extern int enc_main(int argc, char *argv[]);
extern int engine_main(int argc, char *argv[]);
extern int errstr_main(int argc, char *argv[]);
extern int gendh_main(int argc, char *argv[]);
extern int gendsa_main(int argc, char *argv[]);
extern int genpkey_main(int argc, char *argv[]);
extern int genrsa_main(int argc, char *argv[]);
extern int nseq_main(int argc, char *argv[]);
extern int ocsp_main(int argc, char *argv[]);
extern int passwd_main(int argc, char *argv[]);
extern int pkcs7_main(int argc, char *argv[]);
extern int pkcs8_main(int argc, char *argv[]);
extern int pkcs12_main(int argc, char *argv[]);
extern int pkey_main(int argc, char *argv[]);
extern int pkeyparam_main(int argc, char *argv[]);
extern int pkeyutl_main(int argc, char *argv[]);
extern int prime_main(int argc, char *argv[]);
extern int rand_main(int argc, char *argv[]);
extern int req_main(int argc, char *argv[]);
extern int rsa_main(int argc, char *argv[]);
extern int rsautl_main(int argc, char *argv[]);
extern int s_client_main(int argc, char *argv[]);
extern int s_server_main(int argc, char *argv[]);
extern int s_time_main(int argc, char *argv[]);
extern int sess_id_main(int argc, char *argv[]);
extern int smime_main(int argc, char *argv[]);
extern int speed_main(int argc, char *argv[]);
extern int spkac_main(int argc, char *argv[]);
extern int ts_main(int argc, char *argv[]);
extern int verify_main(int argc, char *argv[]);
extern int version_main(int argc, char *argv[]);
extern int x509_main(int argc, char *argv[]);

#define FUNC_TYPE_GENERAL	1
#define FUNC_TYPE_MD		2
#define FUNC_TYPE_CIPHER	3
#define FUNC_TYPE_PKEY		4
#define FUNC_TYPE_MD_ALG	5
#define FUNC_TYPE_CIPHER_ALG	6

typedef struct {
	int type;
	const char *name;
	int (*func)(int argc, char *argv[]);
} FUNCTION;
DECLARE_LHASH_OF(FUNCTION);
