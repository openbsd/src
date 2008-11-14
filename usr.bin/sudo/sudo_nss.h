/*
 * Copyright (c) 2007-2008 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * $Sudo: sudo_nss.h,v 1.5 2008/11/09 14:13:12 millert Exp $
 */

struct lbuf;
struct passwd;

struct sudo_nss {
    struct sudo_nss *prev;
    struct sudo_nss *next;
    int (*open) __P((struct sudo_nss *nss));
    int (*close) __P((struct sudo_nss *nss));
    int (*parse) __P((struct sudo_nss *nss));
    int (*setdefs) __P((struct sudo_nss *nss));
    int (*lookup) __P((struct sudo_nss *nss, int, int));
    int (*display_cmnd) __P((struct sudo_nss *nss, struct passwd *));
    int (*display_defaults) __P((struct sudo_nss *nss, struct passwd *, struct lbuf *));
    int (*display_bound_defaults) __P((struct sudo_nss *nss, struct passwd *, struct lbuf *));
    int (*display_privs) __P((struct sudo_nss *nss, struct passwd *, struct lbuf *));
    void *handle;
    int ret_notfound;
};

TQ_DECLARE(sudo_nss)

struct sudo_nss_list *sudo_read_nss	__P((void));
