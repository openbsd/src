/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: unistd.h,v 1.3 2001/07/09 21:06:23 gson Exp $ */

/* None of these are defined in NT, so define them for our use */
#define O_NONBLOCK 1
#define PORT_NONBLOCK O_NONBLOCK

/*
 * fcntl() commands
 */
#define F_SETFL 0
#define F_GETFL 1
#define F_SETFD 2
#define F_GETFD 3
/*
 * Enough problems not having full fcntl() without worrying about this!
 */
#undef F_DUPFD 

int fcntl(int, int, ...);

#include <process.h>
