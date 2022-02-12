/*	$OpenBSD: ca.c,v 1.41 2022/02/12 18:22:04 eric Exp $	*/

/*
 * Copyright (c) 2021 Eric Faurot <eric@openbsd.org>
 * Copyright (c) 2014 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2012 Gilles Chehade <gilles@poolp.org>
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

#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <tls.h>
#include <unistd.h>

#include "smtpd.h"
#include "ssl.h"
#include "log.h"

static void ca_imsg(struct mproc *, struct imsg *);
static void ca_init(void);

static struct tls_signer *signer;
static uint64_t reqid = 0;

static void
ca_shutdown(void)
{
	log_debug("debug: ca agent exiting");
	_exit(0);
}

int
ca(void)
{
	struct passwd	*pw;

	ca_init();

	purge_config(PURGE_EVERYTHING);

	if ((pw = getpwnam(SMTPD_USER)) == NULL)
		fatalx("unknown user " SMTPD_USER);

	if (chroot(PATH_CHROOT) == -1)
		fatal("ca: chroot");
	if (chdir("/") == -1)
		fatal("ca: chdir(\"/\")");

	config_process(PROC_CA);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("ca: cannot drop privileges");

	imsg_callback = ca_imsg;
	event_init();

	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peer(PROC_CONTROL);
	config_peer(PROC_PARENT);
	config_peer(PROC_DISPATCHER);

	if (pledge("stdio", NULL) == -1)
		fatal("pledge");

	event_dispatch();
	fatalx("exited event loop");

	return (0);
}

static void
ca_init(void)
{
	struct pki *pki;
	void *iter_dict;

	if ((signer = tls_signer_new()) == NULL)
		fatal("tls_signer_new");

	iter_dict = NULL;
	while (dict_iter(env->sc_pki_dict, &iter_dict, NULL, (void **)&pki)) {
		if (pki->pki_key == NULL)
			continue;
		if (tls_signer_add_keypair_mem(signer, pki->pki_cert,
		    pki->pki_cert_len, pki->pki_key, pki->pki_key_len) == -1)
			fatalx("ca_init: tls_signer_add_keypair_mem");
	}
}

static void
ca_imsg(struct mproc *p, struct imsg *imsg)
{
	const void *input = NULL;
	uint8_t *sig = NULL;
	struct msg m;
	const char *hash;
	size_t input_len, siglen;
	int padding_type, ret, v;
	uint64_t id;

	if (imsg == NULL)
		ca_shutdown();

	switch (imsg->hdr.type) {
	case IMSG_CONF_START:
		return;
	case IMSG_CONF_END:
		return;

	case IMSG_CTL_VERBOSE:
		m_msg(&m, imsg);
		m_get_int(&m, &v);
		m_end(&m);
		log_trace_verbose(v);
		return;

	case IMSG_CTL_PROFILE:
		m_msg(&m, imsg);
		m_get_int(&m, &v);
		m_end(&m);
		profiling = v;
		return;

	case IMSG_CA_SIGN:
		m_msg(&m, imsg);
		m_get_id(&m, &id);
		m_get_string(&m, &hash);
		m_get_data(&m, &input, &input_len);
		m_get_int(&m, &padding_type);
		m_end(&m);

		ret = tls_signer_sign(signer, hash, input, input_len,
		    padding_type, &sig, &siglen);

		m_create(p, imsg->hdr.type, 0, 0, -1);
		m_add_id(p, id);
		m_add_int(p, ret);
		if (ret != -1)
			m_add_data(p, sig, siglen);
		m_close(p);
		free(sig);
		return;
	}

	fatalx("ca_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static int
ca_imsg_get_sync(uint32_t type, uint8_t **output, size_t *output_len)
{
	struct imsgbuf *ibuf;
	struct imsg imsg;
	struct msg m;
	const void *data;
	uint64_t id;
	int ret, n, done = 0;

	ibuf = &p_ca->imsgbuf;

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatalx("imsg_read");
		if (n == 0)
			fatalx("pipe closed");

		while (!done) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				fatalx("imsg_get error");
			if (n == 0)
				break;

			log_imsg(PROC_DISPATCHER, PROC_CA, &imsg);

			if (imsg.hdr.type != type) {
				/* Another imsg is queued up in the buffer */
				dispatcher_imsg(p_ca, &imsg);
				imsg_free(&imsg);
				continue;
			}

			m_msg(&m, &imsg);
			m_get_id(&m, &id);
			if (id != reqid)
				fatalx("invalid response id");
			m_get_int(&m, &ret);
			if (ret != -1) {
				m_get_data(&m, &data, output_len);
				if ((*output = malloc(*output_len)) == NULL) {
					*output_len = 0;
					ret = -1;
				}
				else
					memcpy(*output, data, *output_len);
			}
			m_end(&m);
			imsg_free(&imsg);
			done = 1;
		}
	}

	mproc_event_add(p_ca);

	return (ret);
}

int
ca_sign(void *arg, const char *hash, const uint8_t *input, size_t input_len,
    int padding_type, uint8_t **output, size_t *output_len)
{
	m_create(p_ca, IMSG_CA_SIGN, 0, 0, -1);
	m_add_id(p_ca, ++reqid);
	m_add_string(p_ca, hash);
	m_add_data(p_ca, input, input_len);
	m_add_int(p_ca, padding_type);
	m_flush(p_ca);

	return (ca_imsg_get_sync(IMSG_CA_SIGN, output, output_len));
}
