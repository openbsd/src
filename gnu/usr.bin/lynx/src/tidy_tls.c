/*
 * $LynxId: tidy_tls.c,v 1.3 2008/12/14 18:25:25 tom Exp $
 * Copyright 2008, Thomas E. Dickey
 * with fix Copyright 2008 by Thomas Viehmann
 *
 * Required libraries:
 *	libgnutls
 *	libcrypt
 */
#include <tidy_tls.h>

#include <gnutls/x509.h>
#include <gcrypt.h>
#include <libtasn1.h>		/* ASN1_SUCCESS,etc */
#include <string.h>

#define typeCalloc(type) (type *) calloc(1, sizeof(type))

static int last_error = 0;

/* ugly, but hey, we could just use a more sane api, too */
#define GetDnByOID(target, oid, thewhat) \
		len = sizeof(target); \
                if (! thewhat) \
		  gnutls_x509_crt_get_dn_by_oid(xcert, oid, 0, 0, target, &len); \
                else \
                  gnutls_x509_crt_get_issuer_dn_by_oid(xcert, oid, 0, 0, target, &len)

/* thewhat: which DN to get 0 = subject, 1 = issuer */
static int ExtractCertificate(const gnutls_datum_t * cert, X509_NAME * result, int thewhat)
{
    gnutls_x509_crt_t xcert;
    int rc;
    size_t len;

    if ((rc = gnutls_x509_crt_init(&xcert)) >= 0) {
	if ((rc = gnutls_x509_crt_import(xcert, cert, GNUTLS_X509_FMT_DER)) >= 0) {
	    GetDnByOID(result->country,
		       GNUTLS_OID_X520_COUNTRY_NAME, thewhat);
	    GetDnByOID(result->organization,
		       GNUTLS_OID_X520_ORGANIZATION_NAME, thewhat);
	    GetDnByOID(result->organizational_unit_name,
		       GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME, thewhat);
	    GetDnByOID(result->common_name,
		       GNUTLS_OID_X520_COMMON_NAME, thewhat);
	    GetDnByOID(result->locality_name,
		       GNUTLS_OID_X520_LOCALITY_NAME, thewhat);
	    GetDnByOID(result->state_or_province_name,
		       GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME, thewhat);
	    GetDnByOID(result->email,
		       GNUTLS_OID_PKCS9_EMAIL, thewhat);
	    rc = 0;
	}
	gnutls_x509_crt_deinit(xcert);
    }
    return rc;
}

/*
 * (stub)
 * ERR_error_string() generates a human-readable string representing the
 * error code 'e', and places it at 'buffer', which must be at least 120 bytes
 * long. If 'buffer' is NULL, the error string is placed in a static buffer.
 */
const char *ERR_error_string(unsigned long e, char *buffer)
{
    (void) buffer;
    return gnutls_strerror(-e);
}

/*
 * (stub)
 * Return the earliest error code from the thread's error queue and remove the
 * entry.
 */
unsigned long ERR_get_error(void)
{
    unsigned long rc;

    rc = -last_error;
    last_error = 0;

    return rc;
}

/*
 * Put 'num' cryptographically strong pseudo-random bytes into 'buffer'.
 */
int RAND_bytes(unsigned char *buffer, int num)
{
    gcry_randomize(buffer, num, GCRY_VERY_STRONG_RANDOM);
    return 1;
}

/*
 * (stub)
 * Generate a default path for the random seed file.  'buffer' points to a
 * buffer of size 'len' in which to store the filename.
 */
const char *RAND_file_name(char *buffer, size_t len)
{
    (void) buffer;
    (void) len;
    return "";
}

/*
 * (stub)
 * Read a number of bytes from file 'name' and adds them to the PRNG.  If
 * 'maxbytes' is non-negative, up to to 'maxbytes' are read; if 'maxbytes' is
 * -1, the complete file is read.
 */
int RAND_load_file(const char *name, long maxbytes)
{
    (void) name;
    return maxbytes;
}

/*
 * (stub)
 * Mix the 'num' bytes at 'buffer' into the PRNG state.
 */
void RAND_seed(const void *buffer, int num)
{
    (void) buffer;
    (void) num;
}

/*
 * (stub)
 * Return 1 if the PRNG has been seeded with enough data, 0 otherwise.
 */
int RAND_status(void)
{
    return 1;
}

/*
 * (stub)
 * Write a number of random bytes (currently 1024) to file 'name' which can be
 * used to initialize the PRNG by calling RAND_load_file() in a later session.
 */
int RAND_write_file(const char *name)
{
    (void) name;
    return 0;
}

/*
 * Return the number of secret bits used for cipher.  If 'bits' is not NULL, it
 * contains the number of bits processed by the chosen algorithm.  If cipher is
 * NULL, 0 is returned.
 */
int SSL_CIPHER_get_bits(SSL_CIPHER * cipher, int *bits)
{
    int result = 0;

    if (cipher) {
	result = (8 * gnutls_cipher_get_key_size(cipher->encrypts));
    }

    if (bits)
	*bits = result;

    return result;
}

/*
 * Return a pointer to the name of 'cipher'.  If 'cipher' is NULL the constant
 * value "NONE" is returned.
 */
const char *SSL_CIPHER_get_name(SSL_CIPHER * cipher)
{
    const char *result = "NONE";

    if (cipher) {
	result = gnutls_cipher_suite_get_name(cipher->key_xchg,
					      cipher->encrypts,
					      cipher->msg_code);
    }
    return result;
}

/*
 * Return the protocol version for cipher, currently "SSLv2", "SSLv3", or
 * "TLSv1".  If cipher is NULL, "(NONE)" is returned.
 */
const char *SSL_CIPHER_get_version(SSL_CIPHER * cipher)
{
    const char *result = "NONE";

    if (cipher) {
	if ((result = gnutls_protocol_get_name(cipher->protocol)) == 0)
	    result = "unknown";
    }
    return result;
}

/*
 * Free an allocated SSL_CTX object.
 */
void SSL_CTX_free(SSL_CTX * ctx)
{
    free(ctx->method);
    free(ctx);
}

/*
 * Create a new SSL_CTX object as framework for TLS/SSL enabled functions.
 */
SSL_CTX *SSL_CTX_new(SSL_METHOD * method)
{
    SSL_CTX *ctx;

    ctx = typeCalloc(SSL_CTX);
    ctx->method = method;

    return ctx;
}

/*
 * See SSL_CTX_load_verify_locations() - this sets the paths for that and
 * SSL_CTX_set_verify() to their default values.  GNU TLS does not have a
 * comparable feature (stub).
 */
int SSL_CTX_set_default_verify_paths(SSL_CTX * ctx)
{
    (void) ctx;
    return 0;
}

/*
 * SSL_CTX_set_options() adds the options set via bitmask in options to
 * ctx.  Options already set before are not cleared.
 */
unsigned long SSL_CTX_set_options(SSL_CTX * ctx, unsigned long options)
{
    ctx->options |= options;
    return ctx->options;
}

/*
 * Set peer certificate verification parameters.
 */
void SSL_CTX_set_verify(SSL_CTX * ctx, int verify_mode,
			int (*verify_callback) (int, X509_STORE_CTX *))
{
    ctx->verify_mode = verify_mode;
    ctx->verify_callback = verify_callback;
}

static void RemoveProtocol(SSL * ssl, int protocol)
{
    int j, k;
    int changed = 0;
    int *protocols = ssl->ctx->method->priority.protocol;

    for (j = k = 0; j < GNUTLS_MAX_ALGORITHM_NUM;) {
	if (protocols[k] == protocol) {
	    if (++k >= GNUTLS_MAX_ALGORITHM_NUM)
		break;
	    changed = 1;
	} else {
	    protocols[j++] = protocols[k++];
	}
    }

    if (changed) {
	gnutls_protocol_set_priority(ssl->gnutls_state, protocols);
    }
}

/*
 * Initiate the TLS/SSL handshake with an TLS/SSL server.
 */
int SSL_connect(SSL * ssl)
{
    X509_STORE_CTX *store;
    int rc;

    if (ssl->options & SSL_OP_NO_TLSv1)
	RemoveProtocol(ssl, GNUTLS_TLS1);
    if (ssl->options & SSL_OP_NO_SSLv3)
	RemoveProtocol(ssl, GNUTLS_SSL3);

    rc = gnutls_handshake(ssl->gnutls_state);
    ssl->last_error = rc;

    if (rc < 0) {
	last_error = rc;
	return 0;
    }

    store = typeCalloc(X509_STORE_CTX);
    store->ssl = ssl;
    store->cert_list = SSL_get_peer_certificate(ssl);

    if (ssl->verify_callback) {
	ssl->verify_callback(1, store);
    }
    ssl->state = SSL_ST_OK;

    rc = store->error;
    free(store);

    /* FIXME: deal with error from callback */

    return 1;
}

/*
 * Free an allocated SSL structure.
 */
void SSL_free(SSL * ssl)
{
    gnutls_certificate_free_credentials(ssl->gnutls_cred);
    gnutls_deinit(ssl->gnutls_state);
    free(ssl);
}

/*
 * Get SSL_CIPHER data of a connection.
 */
SSL_CIPHER *SSL_get_current_cipher(SSL * ssl)
{
    SSL_CIPHER *result = 0;

    if (ssl) {
	result = &(ssl->ciphersuite);

	result->protocol = gnutls_protocol_get_version(ssl->gnutls_state);
	result->encrypts = gnutls_cipher_get(ssl->gnutls_state);
	result->key_xchg = gnutls_kx_get(ssl->gnutls_state);
	result->msg_code = gnutls_mac_get(ssl->gnutls_state);
	result->compress = gnutls_compression_get(ssl->gnutls_state);
	result->cert = gnutls_certificate_type_get(ssl->gnutls_state);
    }

    return result;
}

/*
 * Get the X509 certificate of the peer.
 */
X509 *SSL_get_peer_certificate(SSL * ssl)
{
    gnutls_datum_t *result;
    unsigned list_size = 0;

    result = gnutls_certificate_get_peers(ssl->gnutls_state, &list_size);

    return (X509 *) result;
}

/*
 * Initialize SSL library by registering algorithms.
 */
int SSL_library_init(void)
{
    gnutls_global_init();
    return 1;
}

/*
 * (stub)
 * OpenSSL uses this to prepare for ERR_get_error() calls.
 */
void SSL_load_error_strings(void)
{
}

/*
 * Create a new SSL structure for a connection.
 */
SSL *SSL_new(SSL_CTX * ctx)
{
    SSL *ssl;
    int rc;

    if ((ssl = typeCalloc(SSL)) != 0) {

	rc = gnutls_certificate_allocate_credentials(&ssl->gnutls_cred);
	if (rc < 0) {
	    last_error = rc;
	    free(ssl);
	    ssl = 0;
	} else {

	    gnutls_init(&ssl->gnutls_state, ctx->method->connend);

	    gnutls_protocol_set_priority(ssl->gnutls_state,
					 ctx->method->priority.protocol);
	    gnutls_cipher_set_priority(ssl->gnutls_state,
				       ctx->method->priority.encrypts);
	    gnutls_compression_set_priority(ssl->gnutls_state,
					    ctx->method->priority.compress);
	    gnutls_kx_set_priority(ssl->gnutls_state,
				   ctx->method->priority.key_xchg);
	    gnutls_mac_set_priority(ssl->gnutls_state,
				    ctx->method->priority.msg_code);

	    gnutls_credentials_set(ssl->gnutls_state, GNUTLS_CRD_CERTIFICATE,
				   ssl->gnutls_cred);
	    if (ctx->certfile)
		gnutls_certificate_set_x509_trust_file(ssl->gnutls_cred,
						       ctx->certfile,
						       ctx->certfile_type);
	    if (ctx->keyfile)
		gnutls_certificate_set_x509_key_file(ssl->gnutls_cred,
						     ctx->certfile,
						     ctx->keyfile,
						     ctx->keyfile_type);
	    ssl->ctx = ctx;
	    ssl->verify_mode = ctx->verify_mode;
	    ssl->verify_callback = ctx->verify_callback;

	    ssl->options = ctx->options;

	    ssl->rfd = (gnutls_transport_ptr_t) (-1);
	    ssl->wfd = (gnutls_transport_ptr_t) (-1);
	}
    }
    return ssl;
}

/*
 * Read 'length' bytes into 'buffer' from the given SSL connection.
 * Returns the number of bytes read, or zero on error.
 */
int SSL_read(SSL * ssl, void *buffer, int length)
{
    int rc;

    rc = gnutls_record_recv(ssl->gnutls_state, buffer, length);
    ssl->last_error = rc;

    if (rc < 0) {
	last_error = rc;
	rc = 0;
    }

    return rc;
}

/*
 * Connect the SSL object with a file descriptor.
 * This always returns 1 (success) since GNU TLS does not check for errors.
 */
int SSL_set_fd(SSL * ssl, int fd)
{
    gnutls_transport_set_ptr(ssl->gnutls_state,
			     (gnutls_transport_ptr_t) (fd));
    return 1;
}

/*
 * Write 'length' bytes from 'buffer' to the given SSL connection.
 */
int SSL_write(SSL * ssl, const void *buffer, int length)
{
    int rc;

    rc = gnutls_record_send(ssl->gnutls_state, buffer, length);
    ssl->last_error = rc;

    if (rc < 0) {
	last_error = rc;
	rc = 0;
    }

    return rc;
}

/*
 * Return method-data for SSL verion 3, with the option of rollback to SSL
 * version 2.
 */
SSL_METHOD *SSLv23_client_method(void)
{
    SSL_METHOD *m;

    if ((m = typeCalloc(SSL_METHOD)) != 0) {

	/*
	 * List the protocols in decreasing order of priority.
	 */
	m->priority.protocol[0] = GNUTLS_TLS1;
	m->priority.protocol[1] = GNUTLS_SSL3;
	m->priority.protocol[2] = 0;

	/*
	 * List the cipher algorithms in decreasing order of priority.
	 */
	m->priority.encrypts[0] = GNUTLS_CIPHER_AES_128_CBC;
	m->priority.encrypts[1] = GNUTLS_CIPHER_3DES_CBC;
	m->priority.encrypts[2] = GNUTLS_CIPHER_AES_256_CBC;
	m->priority.encrypts[3] = GNUTLS_CIPHER_ARCFOUR_128;
	m->priority.encrypts[4] = 0;

	/*
	 * List the compression algorithms in decreasing order of priority.
	 */
	m->priority.compress[0] = GNUTLS_COMP_ZLIB;
	m->priority.compress[1] = GNUTLS_COMP_NULL;
	m->priority.compress[2] = 0;

	/*
	 * List the key exchange algorithms in decreasing order of priority.
	 */
	m->priority.key_xchg[0] = GNUTLS_KX_DHE_RSA;
	m->priority.key_xchg[1] = GNUTLS_KX_RSA;
	m->priority.key_xchg[2] = GNUTLS_KX_DHE_DSS;
	m->priority.key_xchg[3] = 0;

	/*
	 * List message authentication code (MAC) algorithms in decreasing
	 * order of priority to specify via gnutls_mac_set_priority().
	 */
	m->priority.msg_code[0] = GNUTLS_MAC_SHA1;
	m->priority.msg_code[1] = GNUTLS_MAC_MD5;
	m->priority.msg_code[2] = 0;

	/*
	 * For gnutls_init, says we're a client.
	 */
	m->connend = GNUTLS_CLIENT;
    }

    return m;
}

static int add_name(char *target, int len, const char *tag, const char *data)
{
    int need = strlen(tag);

    target += strlen(target);
    if (need < len) {
	strcat(target, tag);
	len -= need;
	target += need;
    }
    need = strlen(data);
    if (need >= len - 1)
	need = len - 1;
    strncat(target, data, need)[need] = '\0';
    return len;
}
#define ADD_NAME(tag, data) len = add_name(target, len, tag, data);

/*
 * Convert the X509 name 'source' to a string in the given buffer 'target',
 * whose length is 'len'.  Return a pointer to the buffer.
 */
char *X509_NAME_oneline(X509_NAME * source, char *target, int len)
{
    if (target && (len > 0)) {
	*target = '\0';
	if (source) {
	    ADD_NAME("C=", source->country);
	    ADD_NAME(", ST=", source->state_or_province_name);
	    ADD_NAME(", L=", source->locality_name);
	    ADD_NAME(", O=", source->organization);
	    ADD_NAME(", OU=", source->organizational_unit_name);
	    ADD_NAME(", CN=", source->common_name);
	    ADD_NAME("/Email=", source->email);
	}
    }
    return target;
}

/*
 * Extract the certificate's issuer-name data.
 */
X509_NAME *X509_get_issuer_name(const X509 * cert)
{
    X509_NAME *result;

    if ((result = typeCalloc(X509_NAME)) != 0) {
	if (ExtractCertificate(cert, result, 1) < 0) {
	    free(result);
	    result = 0;
	}
    }
    return result;
}

/*
 * Extract the certificate's subject-name data.
 */
X509_NAME *X509_get_subject_name(const X509 * cert)
{
    X509_NAME *result;

    if ((result = typeCalloc(X509_NAME)) != 0) {
	if (ExtractCertificate(cert, result, 0) < 0) {
	    free(result);
	    result = 0;
	}
    }
    return result;
}
