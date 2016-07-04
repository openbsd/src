#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>

#include <openssl/ssl.h>
#include <openssl/ocsp.h>

static int tcp_connect(char *host, char *port) {
	int err, sd = -1;
	struct addrinfo hints, *res, *r;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	err = getaddrinfo(host, port, &hints, &res);
	if (err != 0) {
		perror("getaddrinfo()");
		exit(-1);
	}

	for (r = res; r != NULL; r = r->ai_next) {
		sd = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (sd == -1)
			continue;

		if (connect(sd, r->ai_addr, r->ai_addrlen) == 0)
			break;

		close(sd);
	}

	freeaddrinfo(res);

	return sd;
}

int main(int argc, char *argv[]) {
	int sd, ocsp_status;
	const unsigned char *p;
	long len;
	OCSP_RESPONSE *rsp = NULL;
	OCSP_BASICRESP *br = NULL;
	X509_STORE     *st = NULL;
	STACK_OF(X509) *ch = NULL;

	SSL *ssl;
	SSL_CTX *ctx;

	SSL_library_init();
	SSL_load_error_strings();

	ctx = SSL_CTX_new(SSLv23_client_method());

	SSL_CTX_load_verify_locations(ctx, "/etc/ssl/cert.pem", NULL);

	sd = tcp_connect(argv[1], argv[2]);

	ssl = SSL_new(ctx);

	SSL_set_fd(ssl, (int) sd);
	SSL_set_tlsext_status_type(ssl, TLSEXT_STATUSTYPE_ocsp);

	if (SSL_connect(ssl) <= 0) {
		puts("SSL connect error");
		exit(-1);
	}

	if (SSL_get_verify_result(ssl) != X509_V_OK) {
		puts("Certificate doesn't verify");
		exit(-1);
	}

	/* ==== VERIFY OCSP RESPONSE ==== */


	len = SSL_get_tlsext_status_ocsp_resp(ssl, &p);

	if (!p) {
		puts("No OCSP response received");
		exit(-1);
	}

	rsp = d2i_OCSP_RESPONSE(NULL, &p, len);
	if (!rsp) {
		puts("Invalid OCSP response");
		exit(-1);
	}

	ocsp_status = OCSP_response_status(rsp);
	if (ocsp_status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
		printf("Invalid OCSP response status: %s (%d)",
		       OCSP_response_status_str(ocsp_status), ocsp_status);
		exit(-1);
	}

	br = OCSP_response_get1_basic(rsp);
	if (!br) {
		puts("Invalid OCSP response");
		exit(-1);
	}

	ch = SSL_get_peer_cert_chain(ssl);
	st = SSL_CTX_get_cert_store(ctx);

	if (OCSP_basic_verify(br, ch, st, 0) <= 0) {
		puts("OCSP response verification failed");
		exit(-1);
	}

	printf("OCSP validated from %s %s\n", argv[1], argv[2]);

	return 0;
}

