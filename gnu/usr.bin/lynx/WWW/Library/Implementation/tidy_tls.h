/*
 * $LynxId: tidy_tls.h,v 1.4 2014/01/11 17:09:33 tom Exp $
 * Copyright 2008-2011,2014 Thomas E. Dickey
 */
#ifndef TIDY_TLS_H
#define TIDY_TLS_H

#include <gnutls/gnutls.h>

#define OPENSSL_VERSION_NUMBER (0x0090604F)
#define SSLEAY_VERSION_NUMBER OPENSSL_VERSION_NUMBER

#define SSLeay_add_ssl_algorithms()   SSL_library_init()

#define SSL_ST_OK       (1)

#define SSL_OP_ALL      (0x000FFFFF)
#define SSL_OP_NO_SSLv2 (0x00100000)
#define SSL_OP_NO_SSLv3 (0x00200000)
#define SSL_OP_NO_TLSv1 (0x00400000)

#define SSL_get_cipher_name(ssl)    SSL_CIPHER_get_name(SSL_get_current_cipher(ssl))
#define SSL_get_cipher(ssl)         SSL_get_cipher_name(ssl)
#define SSL_get_cipher_bits(ssl,bp) SSL_CIPHER_get_bits(SSL_get_current_cipher(ssl),(bp))
#define SSL_get_cipher_version(ssl) SSL_CIPHER_get_version(SSL_get_current_cipher(ssl))

#define TIDY_TLS_BUFSIZE 256

typedef struct {
    char common_name[TIDY_TLS_BUFSIZE];
    char country[TIDY_TLS_BUFSIZE];
    char email[TIDY_TLS_BUFSIZE];
    char locality_name[TIDY_TLS_BUFSIZE];
    char organization[TIDY_TLS_BUFSIZE];
    char organizational_unit_name[TIDY_TLS_BUFSIZE];
    char state_or_province_name[TIDY_TLS_BUFSIZE];
} X509_NAME;

typedef struct _SSL SSL;

typedef gnutls_datum_t X509;

typedef struct {
    unsigned connend;
    struct {
	int protocol[GNUTLS_MAX_ALGORITHM_NUM];
	int encrypts[GNUTLS_MAX_ALGORITHM_NUM];
	int compress[GNUTLS_MAX_ALGORITHM_NUM];
	int key_xchg[GNUTLS_MAX_ALGORITHM_NUM];
	int msg_code[GNUTLS_MAX_ALGORITHM_NUM];
    } priority;
} SSL_METHOD;

typedef struct {
    SSL *ssl;
    int error;
    const gnutls_datum_t *cert_list;
#define current_cert cert_list
} X509_STORE_CTX;

typedef struct {
    gnutls_certificate_type_t cert;
    gnutls_cipher_algorithm_t encrypts;
    gnutls_compression_method_t compress;
    gnutls_kx_algorithm_t key_xchg;
    gnutls_mac_algorithm_t msg_code;
    gnutls_protocol_t protocol;
} SSL_CIPHER;

typedef struct _SSL_CTX {
    SSL_METHOD *method;
    char *certfile;
    int certfile_type;
    char *keyfile;
    int keyfile_type;
    unsigned long options;

    int (*verify_callback) (int, X509_STORE_CTX *);
    int verify_mode;

} SSL_CTX;

struct _SSL {
    gnutls_session_t gnutls_state;

    gnutls_certificate_client_credentials gnutls_cred;

    SSL_CTX *ctx;
    SSL_CIPHER ciphersuite;

    int last_error;
    int shutdown;
    int state;
    unsigned long options;

    int (*verify_callback) (int, X509_STORE_CTX *);
    int verify_mode;

    gnutls_transport_ptr_t rfd;
    gnutls_transport_ptr_t wfd;
};

/* use either SSL_VERIFY_NONE or SSL_VERIFY_PEER, the last 2 options
 * are 'ored' with SSL_VERIFY_PEER if they are desired */
#define SSL_VERIFY_PEER			0x01
/* *INDENT-OFF* */
extern SSL *SSL_new(SSL_CTX * ctx);
extern SSL_CIPHER *SSL_get_current_cipher(SSL * ssl);
extern SSL_CTX *SSL_CTX_new(SSL_METHOD * method);
extern SSL_METHOD *SSLv23_client_method(void);
extern const X509 *SSL_get_peer_certificate(SSL * ssl);
extern X509_NAME *X509_get_issuer_name(const X509 * cert);
extern X509_NAME *X509_get_subject_name(const X509 * cert);
extern char *X509_NAME_oneline(X509_NAME * name, char *buf, int len);
extern const char *ERR_error_string(unsigned long e, char *buf);
extern const char *RAND_file_name(char *buf, size_t len);
extern const char *SSL_CIPHER_get_name(SSL_CIPHER * cipher);
extern const char *SSL_CIPHER_get_version(SSL_CIPHER * cipher);
extern int RAND_bytes(unsigned char *buf, int num);
extern int RAND_load_file(const char *name, long maxbytes);
extern int RAND_status(void);
extern int RAND_write_file(const char *name);
extern int SSL_CIPHER_get_bits(SSL_CIPHER * cipher, int *bits);
extern int SSL_CTX_set_default_verify_paths(SSL_CTX * ctx);
extern int SSL_connect(SSL * ssl);
extern int SSL_library_init(void);
extern int SSL_read(SSL * ssl, void *buf, int len);
extern int SSL_set_fd(SSL * ssl, int fd);
extern int SSL_write(SSL * ssl, const void *buf, int len);
extern unsigned long ERR_get_error(void);
extern unsigned long SSL_CTX_set_options(SSL_CTX * ctx, unsigned long options);
extern void RAND_seed(const void *buf, int num);
extern void SSL_CTX_free(SSL_CTX * ctx);
extern void SSL_CTX_set_verify(SSL_CTX * ctx, int verify_mode, int (*verify_callback) (int, X509_STORE_CTX *));
extern void SSL_free(SSL * ssl);
extern void SSL_load_error_strings(void);
/* *INDENT-ON* */

#endif /* TIDY_TLS_H */
