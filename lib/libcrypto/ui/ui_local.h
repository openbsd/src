/* $OpenBSD: ui_local.h,v 1.4 2026/05/19 10:44:42 jsg Exp $ */

/* Written by Richard Levitte (richard@levitte.org) for the OpenSSL
 * project 2001.
 */
/* ====================================================================
 * Copyright (c) 2001 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#ifndef HEADER_UI_LOCAL_H
#define HEADER_UI_LOCAL_H

#include <openssl/ui.h>
#include <openssl/crypto.h>

__BEGIN_HIDDEN_DECLS

struct ui_method_st {
	char *name;

	/* All the functions return 1 or non-NULL for success and 0 or NULL
	   for failure */

	/* Open whatever channel for this, be it the console, an X window
	   or whatever.
	   This function should use the ex_data structure to save
	   intermediate data. */
	int (*ui_open_session)(UI *ui);

	int (*ui_write_string)(UI *ui, UI_STRING *uis);

	/* Flush the output.  If a GUI dialog box is used, this function can
	   be used to actually display it. */
	int (*ui_flush)(UI *ui);

	int (*ui_read_string)(UI *ui, UI_STRING *uis);

	int (*ui_close_session)(UI *ui);

	/* Construct a prompt in a user-defined manner.  object_desc is a
	   textual short description of the object, for example "pass phrase",
	   and object_name is the name of the object (might be a card name or
	   a file name.
	   The returned string shall always be allocated on the heap with
	   malloc(), and need to be free'd with free(). */
	char *(*ui_construct_prompt)(UI *ui, const char *object_desc,
	    const char *object_name);
};

struct ui_st {
	const UI_METHOD *meth;
	STACK_OF(UI_STRING) *strings; /* We might want to prompt for more
					 than one thing at a time, and
					 with different echoing status.  */
	void *user_data;
	CRYPTO_EX_DATA ex_data;

#define UI_FLAG_REDOABLE	0x0001
#define UI_FLAG_PRINT_ERRORS	0x0100
	int flags;
};

__END_HIDDEN_DECLS

#endif	/* !HEADER_UI_LOCAL_H */
