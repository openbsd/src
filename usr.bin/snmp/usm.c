/*	$OpenBSD: usm.c,v 1.1 2019/09/18 09:48:14 martijn Exp $	*/

/*
 * Copyright (c) 2019 Martijn van Duren <martijn@openbsd.org>
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

#include <sys/time.h>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <ber.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "smi.h"
#include "snmp.h"
#include "usm.h"

#define USM_MAX_DIGESTLEN 48
#define USM_MAX_TIMEWINDOW 150
#define USM_SALTOFFSET 8

struct usm_sec {
	struct snmp_sec snmp;
	char *user;
	size_t userlen;
	int engineidset;
	char *engineid;
	size_t engineidlen;
	int bootsset;
	uint32_t boots;
	int timeset;
	uint32_t time;
	struct timespec timecheck;
};

static int usm_doinit(struct snmp_agent *);
static char *usm_genparams(struct snmp_agent *, size_t *);
static int usm_parseparams(struct snmp_agent *, char *, size_t, off_t, char *,
    size_t, uint8_t);
static void usm_free(void *);

struct snmp_sec *
usm_init(const char *user, size_t userlen)
{
	struct snmp_sec *sec;
	struct usm_sec *usm;

	if (user == NULL || user[0] == '\0') {
		errno = EINVAL;
		return NULL;
	}

	if ((sec = malloc(sizeof(*sec))) == NULL)
		return NULL;

	if ((usm = calloc(1, sizeof(struct usm_sec))) == NULL) {
		free(sec);
		return NULL;
	}
	if ((usm->user = malloc(userlen)) == NULL) {
		free(sec);
		free(usm);
		return NULL;
	}
	memcpy(usm->user, user, userlen);
	usm->userlen = userlen;

	sec->model = SNMP_SEC_USM;
	sec->init = usm_doinit;
	sec->genparams = usm_genparams;
	sec->parseparams = usm_parseparams;
	sec->free = usm_free;
	sec->data = usm;
	return sec;
}

static int
usm_doinit(struct snmp_agent *agent)
{
	struct ber_element *ber;
	struct usm_sec *usm = agent->v3->sec->data;
	int level;
	size_t userlen;

	if (usm->engineidset && usm->bootsset && usm->timeset)
		return 0;

	level = agent->v3->level;
	agent->v3->level = SNMP_MSGFLAG_REPORT;
	userlen = usm->userlen;
	usm->userlen = 0;

	if ((ber = snmp_get(agent, NULL, 0)) == NULL) {
		agent->v3->level = level;
		usm->userlen = userlen;
		return -1;
	}
	ber_free_element(ber);

	agent->v3->level = level;
	usm->userlen = userlen;

	return 0;
}

static char *
usm_genparams(struct snmp_agent *agent, size_t *len)
{
	struct ber ber;
	struct ber_element *params;
	struct usm_sec *usm = agent->v3->sec->data;
	char *secparams = NULL;
	ssize_t berlen = 0;
	struct timespec now, timediff;
	uint32_t boots, time;

	if (usm->timeset) {
		if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
			return NULL;
		timespecsub(&now, &(usm->timecheck), &timediff);
		time = usm->time + timediff.tv_sec;
	} else
		time = 0;
	boots = usm->boots;

	if ((params = ber_printf_elements(NULL, "{xddxxx}", usm->engineid,
	    usm->engineidlen, boots, time, usm->user, usm->userlen, NULL,
	    (size_t) 0, NULL, (size_t) 0)) == NULL)
		return NULL;

	bzero(&ber, sizeof(ber));
	ber_set_application(&ber, smi_application);
	if (ber_write_elements(&ber, params) != -1)
	    berlen = ber_copy_writebuf(&ber, (void **)&secparams);

	*len = berlen;
	ber_free_element(params);
	ber_free(&ber);
	return secparams;
}

static int
usm_parseparams(struct snmp_agent *agent, char *packet, size_t packetlen,
    off_t secparamsoffset, char *buf, size_t buflen, uint8_t level)
{
	struct usm_sec *usm = agent->v3->sec->data;
	struct ber ber;
	struct ber_element *secparams;
	char *engineid, *user;
	size_t engineidlen, userlen;
	struct timespec now, timediff;
	uint32_t boots, time;

	bzero(&ber, sizeof(ber));

	ber_set_application(&ber, smi_application);
	ber_set_readbuf(&ber, buf, buflen);
	if ((secparams = ber_read_elements(&ber, NULL)) == NULL)
		return -1;
	ber_free(&ber);

	if (ber_scanf_elements(secparams, "{xddxSS}", &engineid, &engineidlen,
	    &boots, &time, &user, &userlen) == -1)
		goto fail;

	if (!usm->engineidset) {
		if (usm_setengineid(agent->v3->sec, engineid,
		    engineidlen) == -1)
			goto fail;
	} else {
		if (usm->engineidlen != engineidlen)
			goto fail;
		if (memcmp(usm->engineid, engineid, engineidlen) != 0)
			goto fail;
	}

	if (!usm->bootsset) {
		usm->boots = boots;
		usm->bootsset = 1;
	} else {
		if (boots < usm->boots)
			goto fail;
		if (boots > usm->boots) {
			usm->bootsset = 0;
			usm->timeset = 0;
			usm_doinit(agent);
			goto fail;
		}
	}

	if (!usm->timeset) {
		usm->time = time;
		if (clock_gettime(CLOCK_MONOTONIC, &usm->timecheck) == -1)
			goto fail;
		usm->timeset = 1;
	} else {
		if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
			goto fail;
		timespecsub(&now, &(usm->timecheck), &timediff);
		if (time < usm->time + timediff.tv_sec - USM_MAX_TIMEWINDOW ||
		    time > usm->time + timediff.tv_sec + USM_MAX_TIMEWINDOW) {
			usm->bootsset = 0;
			usm->timeset = 0;
			usm_doinit(agent);
			goto fail;
		}
	}

	if (userlen != usm->userlen ||
	    memcmp(user, usm->user, userlen) != 0)
		goto fail;

	ber_free_element(secparams);
	return 0;

fail:
	ber_free_element(secparams);
	return -1;
}

static void
usm_free(void *data)
{
	struct usm_sec *usm = data;

	free(usm->user);
	free(usm->engineid);
	free(usm);
}

int
usm_setengineid(struct snmp_sec *sec, char *engineid, size_t engineidlen)
{
	struct usm_sec *usm = sec->data;

	if (usm->engineid != NULL)
		free(usm->engineid);
	if ((usm->engineid = malloc(engineidlen)) == NULL)
		return -1;
	memcpy(usm->engineid, engineid, engineidlen);
	usm->engineidlen = engineidlen;
	usm->engineidset = 1;

	return 0;
}

int
usm_setbootstime(struct snmp_sec *sec, uint32_t boots, uint32_t time)
{
	struct usm_sec *usm = sec->data;

	if (clock_gettime(CLOCK_MONOTONIC, &(usm->timecheck)) == -1)
		return -1;

	usm->boots = boots;
	usm->bootsset = 1;
	usm->time = time;
	usm->timeset = 1;
	return 0;
}
