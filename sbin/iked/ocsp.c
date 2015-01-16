/*
 * Copyright (c) 2014 Markus Friedl
 * Copyright (c) 2005 Marco Pfatschbacher
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>

#include <openssl/pem.h>
#include <openssl/ocsp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <event.h>

#include "iked.h"

struct iked_ocsp {
	struct iked		*ocsp_env;	/* back pointer to env */
	struct iked_sahdr	 ocsp_sh;	/* ike sa */
	u_int8_t		 ocsp_type;	/* auth type */
	struct iked_socket	*ocsp_sock;	/* socket to ocsp responder */
	BIO			*ocsp_cbio;	/* matching OpenSSL obj */
	OCSP_CERTID		*ocsp_id;	/* ocsp-id for cert */
	OCSP_REQUEST		*ocsp_req;	/* ocsp-request */
	OCSP_REQ_CTX		*ocsp_req_ctx;	/* async ocsp-request */
};

struct ocsp_connect {
	struct iked_socket	 oc_sock;
	char			*oc_path;
};

/* priv */
void		 ocsp_connect_cb(int, short, void *);
int		 ocsp_connect_finish(struct iked *, int, struct ocsp_connect *);

/* unpriv */
void		 ocsp_free(struct iked_ocsp *);
void		 ocsp_callback(int, short, void *);
void		 ocsp_parse_response(struct iked_ocsp *, OCSP_RESPONSE *);
STACK_OF(X509)	*ocsp_load_certs(const char *);
int		 ocsp_validate_finish(struct iked_ocsp *, int);


/* priv */

/* async connect to configure ocsp-responder */
int
ocsp_connect(struct iked *env)
{
	struct ocsp_connect	*oc = NULL;
	struct addrinfo		 hints, *res0 = NULL, *res;
	char			*host = NULL, *port = NULL, *path = NULL;
	int			use_ssl, fd = -1, ret = -1, error;

	if (env->sc_ocsp_url == 0) {
		log_warnx("%s: no ocsp url", __func__);
		goto done;
	}
	if (!OCSP_parse_url(env->sc_ocsp_url, &host, &port, &path, &use_ssl)) {
		log_warnx("%s: error parsing OCSP-request-URL: %s", __func__,
		    env->sc_ocsp_url);
		goto done;
	}

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		log_debug("%s: socket failed", __func__);
		goto done;
	}
	if ((oc = calloc(1, sizeof(*oc))) == NULL) {
		log_debug("%s: calloc failed", __func__);
		goto done;
	}

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, port, &hints, &res0);
	if (error) {
		log_debug("%s: getaddrinfo(%s, %s) failed",
		    __func__, host, port);
		goto done;
	}
	/* XXX just pick the first answer. we could loop instead */
	for (res = res0; res; res = res->ai_next)
		if (res->ai_family == AF_INET)
			break;
	if (res == NULL) {
		log_debug("%s: no addr to connect to for %s:%s",
		    __func__, host, port);
		goto done;
	}

	oc->oc_sock.sock_fd = fd;
	oc->oc_sock.sock_env = env;
	oc->oc_path = path;
	path = NULL;

	log_debug("%s: connect(%s, %s)", __func__, host, port);
	socket_set_blockmode(fd, BM_NONBLOCK);
	if (connect(fd, res->ai_addr, res->ai_addrlen) == -1) {
		/* register callback for ansync connect */
		if (errno == EINPROGRESS) {
			event_set(&oc->oc_sock.sock_ev, fd, EV_WRITE,
			    ocsp_connect_cb, oc);
			event_add(&oc->oc_sock.sock_ev, NULL);
			ret = 0;
		} else
			log_debug("%s: error while connecting: %s", __func__,
			    strerror(errno));
	} else {
		ocsp_connect_finish(env, fd, oc);
		ret = 0;
	}
 done:
	if (res0)
		freeaddrinfo(res0);
	free(host);
	free(port);
	free(path);
	if (ret == -1) {
		ocsp_connect_finish(env, -1, oc);
		if (fd >= 0)
			close(fd);
	}
	return (ret);
}

/* callback triggered if connection to ocsp-responder completes/fails */
void
ocsp_connect_cb(int fd, short event, void *arg)
{
	struct ocsp_connect	*oc = arg;
	int			 error, send_fd = -1;
	socklen_t		 len;

	len = sizeof(error);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		log_warn("%s: getsockopt SOL_SOCKET SO_ERROR", __func__);
	} else if (error) {
		log_debug("%s: error while connecting: %s", __func__,
		    strerror(error));
	} else {
		send_fd = fd;
	}
	ocsp_connect_finish(oc->oc_sock.sock_env, send_fd, oc);

	/* if we did not send the fd, we need to close it ourself */
	if (send_fd == -1)
		close(fd);
}

/* send FD+path or error back to CA process */
int
ocsp_connect_finish(struct iked *env, int fd, struct ocsp_connect *oc)
{
	struct iovec		 iov[1];
	int			 iovcnt = 1, ret;

	if (oc && fd >= 0) {
		/* the imsg framework will close the FD after send */
		iov[0].iov_base = oc->oc_path;
		iov[0].iov_len = strlen(oc->oc_path);
		ret = proc_composev_imsg(&env->sc_ps, PROC_CERT, -1,
		    IMSG_OCSP_FD, fd, iov, iovcnt);
	} else {
		ret = proc_compose_imsg(&env->sc_ps, PROC_CERT, -1,
		    IMSG_OCSP_FD, -1, NULL, 0);
		if (fd >= 0)
			close(fd);
	}
	if (oc) {
		free(oc->oc_path);
		free(oc);
	}
	return (ret);
}


/* unpriv */

/* validate the certifcate stored in 'data' by querying the ocsp-responder */
int
ocsp_validate_cert(struct iked *env, struct iked_static_id *id,
    void *data, size_t len, struct iked_sahdr sh, u_int8_t type)
{
	struct iked_ocsp_entry	*ioe;
	struct iked_ocsp	*ocsp;
	BIO			*rawcert = NULL, *bissuer = NULL;
	X509			*cert = NULL, *issuer = NULL;

	if ((ioe = calloc(1, sizeof(*ioe))) == NULL)
		return (-1);
	if ((ocsp = calloc(1, sizeof(*ocsp))) == NULL) {
		free(ioe);
		return (-1);
	}

	ocsp->ocsp_env = env;
	ocsp->ocsp_sh = sh;
	ocsp->ocsp_type = type;

	if ((rawcert = BIO_new_mem_buf(data, len)) == NULL ||
	    (cert = d2i_X509_bio(rawcert, NULL)) == NULL ||
	    (bissuer = BIO_new_file(IKED_OCSP_ISSUER, "r")) == NULL ||
	    (issuer = PEM_read_bio_X509(bissuer, NULL, NULL, NULL)) == NULL ||
	    (ocsp->ocsp_cbio = BIO_new(BIO_s_socket())) == NULL ||
	    (ocsp->ocsp_req = OCSP_REQUEST_new()) == NULL ||
	    !(ocsp->ocsp_id = OCSP_cert_to_id(NULL, cert, issuer)) ||
	    !OCSP_request_add0_id(ocsp->ocsp_req, ocsp->ocsp_id))
		goto err;

	BIO_free(rawcert);
	BIO_free(bissuer);
	X509_free(cert);
	X509_free(issuer);

	ioe->ioe_ocsp = ocsp;
	TAILQ_INSERT_TAIL(&env->sc_ocsp, ioe, ioe_entry);

	/* request connection to ocsp-responder */
	proc_compose_imsg(&env->sc_ps, PROC_PARENT, -1,
	    IMSG_OCSP_FD, -1, NULL, 0);
	return (0);

 err:
	ca_sslerror(__func__);
	free(ioe);
	if (rawcert != NULL)
		BIO_free(rawcert);
	if (cert != NULL)
		X509_free(cert);
	if (bissuer != NULL)
		BIO_free(bissuer);
	if (issuer != NULL)
		X509_free(issuer);
	ocsp_validate_finish(ocsp, 0);	/* failed */
	return (-1);
}

/* free ocsp query context */
void
ocsp_free(struct iked_ocsp *ocsp)
{
	if (ocsp != NULL) {
		if (ocsp->ocsp_sock != NULL) {
			close(ocsp->ocsp_sock->sock_fd);
			free(ocsp->ocsp_sock);
		}
		if (ocsp->ocsp_cbio != NULL)
			BIO_free_all(ocsp->ocsp_cbio);
		if (ocsp->ocsp_id != NULL)
			OCSP_CERTID_free(ocsp->ocsp_id);

		/* XXX not sure about ownership XXX */
		if (ocsp->ocsp_req_ctx != NULL)
			OCSP_REQ_CTX_free(ocsp->ocsp_req_ctx);
		else if (ocsp->ocsp_req != NULL)
			OCSP_REQUEST_free(ocsp->ocsp_req);

		free(ocsp);
	}
}

/* we got a connection to the ocsp responder */
int
ocsp_receive_fd(struct iked *env, struct imsg *imsg)
{
	struct iked_ocsp_entry	*ioe = NULL;
	struct iked_ocsp	*ocsp = NULL;
	struct iked_socket	*sock;
	char			*path = NULL;
	int			 ret = -1;

	log_debug("%s: received socket fd %d", __func__, imsg->fd);
	if ((ioe = TAILQ_FIRST(&env->sc_ocsp)) == NULL) {
		log_debug("%s: oops, no request for", __func__);
		close(imsg->fd);
		return (-1);
	}
	TAILQ_REMOVE(&env->sc_ocsp, ioe, ioe_entry);
	ocsp = ioe->ioe_ocsp;
	free(ioe);

	if ((sock = calloc(1, sizeof(*sock))) == NULL)
		fatal("ocsp_receive_fd: calloc sock");

	/* note that sock_addr is not set */
	sock->sock_fd = imsg->fd;
	sock->sock_env = env;
	ocsp->ocsp_sock = sock;

	/* fetch 'path' and 'fd' from imsg */
	if ((path = get_string(imsg->data, IMSG_DATA_SIZE(imsg))) == NULL)
		goto done;

	BIO_set_fd(ocsp->ocsp_cbio, imsg->fd, BIO_NOCLOSE);

	if ((ocsp->ocsp_req_ctx = OCSP_sendreq_new(ocsp->ocsp_cbio,
	    path, NULL, -1)) == NULL)
		goto done;
	if (!OCSP_REQ_CTX_set1_req(ocsp->ocsp_req_ctx, ocsp->ocsp_req))
		goto done;

	event_set(&sock->sock_ev, sock->sock_fd, EV_WRITE, ocsp_callback, ocsp);
	event_add(&sock->sock_ev, NULL);
	ret = 0;
 done:
	if (ret == -1)
		ocsp_validate_finish(ocsp, 0);	/* failed */
	free(path);
	return (ret);
}

/* load a stack of x509 certificates */
STACK_OF(X509)*
ocsp_load_certs(const char *file)
{
	BIO			*bio = NULL;
	STACK_OF(X509)		*certs = NULL;
	STACK_OF(X509_INFO)	*xis = NULL;
	X509_INFO		*xi;
	int			 i;

	if ((bio = BIO_new_file(file, "r")) == NULL) {
		log_warn("%s: BIO_new_file failed for %s",
		    __func__, file);
		return (NULL);
	}
	if ((xis = PEM_X509_INFO_read_bio(bio, NULL, NULL, NULL)) == NULL) {
		ca_sslerror(__func__);
		goto done;
	}
	if ((certs = sk_X509_new_null()) == NULL) {
		log_debug("%s: sk_X509_new_null failed for %s", __func__, file);
		goto done;
	}
	for (i = 0; i < sk_X509_INFO_num(xis); i++) {
		xi = sk_X509_INFO_value(xis, i);
		if (xi->x509) {
			if (!sk_X509_push(certs, xi->x509))
				goto done;
			xi->x509 = NULL;
		}
	}

 done:
	if (bio)
		BIO_free(bio);
	if (xis)
		sk_X509_INFO_pop_free(xis, X509_INFO_free);
	if (certs && sk_X509_num(certs) <= 0) {
		sk_X509_pop_free(certs, X509_free);
		certs = NULL;
	}
	return (certs);
}

/* read/write callback that sends the requests and reads the ocsp response */
void
ocsp_callback(int fd, short event, void *arg)
{
	struct iked_ocsp	*ocsp = arg;
	struct iked_socket	*sock = ocsp->ocsp_sock;
	OCSP_RESPONSE		*resp = NULL;

	/*
	 * Only call OCSP_sendreq_nbio() if should_read/write is
	 * either not requested or read/write can be called.
	 */
	if ((!BIO_should_read(ocsp->ocsp_cbio) || (event & EV_READ)) &&
	    (!BIO_should_write(ocsp->ocsp_cbio) || (event & EV_WRITE)) &&
	    OCSP_sendreq_nbio(&resp, ocsp->ocsp_req_ctx) != -1 ) {
		ocsp_parse_response(ocsp, resp);
		return;
	}
	if (BIO_should_read(ocsp->ocsp_cbio))
		event_set(&sock->sock_ev, sock->sock_fd, EV_READ,
		    ocsp_callback, ocsp);
	else if (BIO_should_write(ocsp->ocsp_cbio))
		event_set(&sock->sock_ev, sock->sock_fd, EV_WRITE,
		    ocsp_callback, ocsp);
	event_add(&sock->sock_ev, NULL);
}

/* parse the actual OCSP response */
void
ocsp_parse_response(struct iked_ocsp *ocsp, OCSP_RESPONSE *resp)
{
	int status;
	X509_STORE *store = NULL;
	STACK_OF(X509) *verify_other = NULL;
	OCSP_BASICRESP *bs = NULL;
	int verify_flags = 0;
	ASN1_GENERALIZEDTIME *rev, *thisupd, *nextupd;
	int reason = 0;
	int error = 1;

	if (!resp) {
		log_warnx("%s: error querying OCSP responder", __func__);
		goto done;
	}

	status = OCSP_response_status(resp);
	if (status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
		log_warnx("%s: responder error: %s (%i)\n", __func__,
		    OCSP_response_status_str(status), status);
		goto done;
	}

	verify_other = ocsp_load_certs(IKED_OCSP_RESPCERT);
	verify_flags |= OCSP_TRUSTOTHER;
	if (!verify_other)
		goto done;

	bs = OCSP_response_get1_basic(resp);
	if (!bs) {
		log_warnx("%s: error parsing response", __func__);
		goto done;
	}

	status = OCSP_check_nonce(ocsp->ocsp_req, bs);
	if (status <= 0) {
		if (status == -1)
			log_warnx("%s: no nonce in response", __func__);
		else {
			log_warnx("%s: nonce verify error", __func__);
			goto done;
		}
	}

	store = X509_STORE_new();
	status = OCSP_basic_verify(bs, verify_other, store, verify_flags);
	if (status < 0)
		status = OCSP_basic_verify(bs, NULL, store, 0);

	if (status <= 0) {
		ca_sslerror(__func__);
		log_warnx("%s: response verify failure", __func__);
		goto done;
	} else
		log_debug("%s: response verify ok", __func__);

	if (!OCSP_resp_find_status(bs, ocsp->ocsp_id, &status, &reason,
	    &rev, &thisupd, &nextupd)) {
		log_warnx("%s: no status found", __func__);
		goto done;
	}
	log_debug("%s: status: %s", __func__, OCSP_cert_status_str(status));

	if (status == V_OCSP_CERTSTATUS_GOOD)
		error = 0;

 done:
	if (store)
		X509_STORE_free(store);
	if (verify_other)
		sk_X509_pop_free(verify_other, X509_free);
	if (resp)
		OCSP_RESPONSE_free(resp);
	if (bs)
		OCSP_BASICRESP_free(bs);

	ocsp_validate_finish(ocsp, error == 0);
}

/*
 * finish the ocsp_validate_cert() RPC by sending the appropriate
 * message back to the IKEv2 process
 */
int
ocsp_validate_finish(struct iked_ocsp *ocsp, int valid)
{
	struct iked		*env = ocsp->ocsp_env;
	struct iovec		 iov[2];
	int			 iovcnt = 2, ret, cmd;

	iov[0].iov_base = &ocsp->ocsp_sh;
	iov[0].iov_len = sizeof(ocsp->ocsp_sh);
	iov[1].iov_base = &ocsp->ocsp_type;
	iov[1].iov_len = sizeof(ocsp->ocsp_type);

	cmd = valid ? IMSG_CERTVALID : IMSG_CERTINVALID;
	ret = proc_composev_imsg(&env->sc_ps, PROC_IKEV2, -1,
	    cmd, -1, iov, iovcnt);

	ocsp_free(ocsp);
	return (ret);
}
