/*
 * Copyright (c) 2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "krb5_locl.h"
#include <err.h>

RCSID("$KTH: test_keytab.c,v 1.2 2005/05/20 09:01:29 lha Exp $");

/*
 * Test that removal entry from of empty keytab doesn't corrupts
 * memory.
 */

static void
test_empty_keytab(krb5_context context, const char *keytab)
{
    krb5_error_code ret;
    krb5_keytab id;
    krb5_keytab_entry entry;

    ret = krb5_kt_resolve(context, keytab, &id);
    if (ret)
	krb5_err(context, 1, ret, "krb5_kt_resolve");

    memset(&entry, 0, sizeof(entry));

    krb5_kt_remove_entry(context, id, &entry);

    ret = krb5_kt_close(context, id);
    if (ret)
	krb5_err(context, 1, ret, "krb5_kt_close");
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;

    setprogname(argv[0]);

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    test_empty_keytab(context, "MEMORY:foo");
    test_empty_keytab(context, "FILE:foo");
    test_empty_keytab(context, "KRB4:foo");

    krb5_free_context(context);

    return 0;
}
