/*	$OpenBSD: smtpfilter.c,v 1.1 2011/08/27 22:32:41 gilles Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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

#include <sys/types.h>

#include <stdlib.h>

#include "filter.h"

int helo_cb(u_int64_t, struct filter_helo *, void *);
int ehlo_cb(u_int64_t, struct filter_helo *, void *);
int mail_cb(u_int64_t, struct filter_mail *, void *);
int rcpt_cb(u_int64_t, struct filter_rcpt *, void *);
int data_cb(u_int64_t, struct filter_data *, void *);


/*
 * *ALL* of the callbacks follow the same principle:
 *
 * First parameter is a unique identifier that has been assigned to the
 * client session when it connected. The same client session entering a
 * second callback will enter it with the same id.
 *
 * Second parameter is a callback-specific structure filled with all of
 * the information necessary to handle filtering at the callback stage.
 * Some of the callbacks allow envelope or data rewriting.
 *
 * Third parameter is a filter-specific argument. It can be NULL, or it
 * can be anything the developer wants, typically a structure allocated
 * in main() to keep various states.
 * A different paramter can be registered for each callback.
 *
 * RETURN VALUES:
 *
 * callbacks return 1 if the test succeesd, 0 if it fails.
 * once a test fails, the entire envelope is rejected.
 *
 *
 * -- gilles@
 *
 */

int
helo_cb(u_int64_t id, struct filter_helo *helo, void *mystuff)
{
	return 1;
}

int
mail_cb(u_int64_t id, struct filter_mail *mail, void *mystuff)
{
	return 1;
}

int
rcpt_cb(u_int64_t id, struct filter_rcpt *rcpt, void *mystuff)
{
	if (rcpt->user[0] == 'a')
		return 0;
	return 1;
}


/*
 * Not all callbacks need to be implemented !
 */

int
main(int argc, char *argv[])
{
	void	*mystuff;

	/*
	 * this MUST be the first think your filter does.
	 * do not do anything before that call: NOTHING.
	 */
	filter_init();


	/*
	 * NOW you can allocate your structures, read a configuration
	 * file or do anything required to prepare your filter before
	 * it starts handling requests.
	 */
	mystuff = malloc(42);


	/*
	 * we only need to register for callbacks we're interested in
	 */
	filter_register_helo_callback(helo_cb, mystuff);
	filter_register_mail_callback(mail_cb, mystuff);
	filter_register_rcpt_callback(rcpt_cb, mystuff);

	/*
	 * filter_register_ehlo_callback(helo_cb, mystuff);
	 * filter_register_rcpt_callback(rcpt_cb, mystuff);
	 * filter_register_data_callback(data_cb, mystuff);
	 * filter_register_quit_callback(data_cb, mystuff);
	 */

	/*
	 * finally, enter the filter_loop().
	 * it will not return unless a critical failure happens.
	 * do not call exit() from a callback.
	 */
	filter_loop();

	return 0;
}
