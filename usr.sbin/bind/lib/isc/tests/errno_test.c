/*
 * Copyright (C) 2016  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>

#include <stdio.h>
#include <sys/errno.h>

#include <atf-c.h>

#include <isc/errno.h>
#include <isc/result.h>

typedef struct {
	int err;
	isc_result_t result;
} testpair_t;

testpair_t testpair[] = {
	{ EPERM, ISC_R_NOPERM },
	{ ENOENT, ISC_R_FILENOTFOUND },
	{ EIO, ISC_R_IOERROR },
	{ EBADF, ISC_R_INVALIDFILE },
	{ ENOMEM, ISC_R_NOMEMORY },
	{ EACCES, ISC_R_NOPERM },
	{ EEXIST, ISC_R_FILEEXISTS },
	{ ENOTDIR, ISC_R_INVALIDFILE },
	{ EINVAL, ISC_R_INVALIDFILE },
	{ ENFILE, ISC_R_TOOMANYOPENFILES },
	{ EMFILE, ISC_R_TOOMANYOPENFILES },
	{ EPIPE, ISC_R_CONNECTIONRESET },
	{ ENAMETOOLONG, ISC_R_INVALIDFILE },
	{ ELOOP, ISC_R_INVALIDFILE },
#ifdef EOVERFLOW
	{ EOVERFLOW, ISC_R_RANGE },
#endif
#ifdef EAFNOSUPPORT
	{ EAFNOSUPPORT, ISC_R_FAMILYNOSUPPORT },
#endif
#ifdef EADDRINUSE
	{ EADDRINUSE, ISC_R_ADDRINUSE },
#endif
	{ EADDRNOTAVAIL, ISC_R_ADDRNOTAVAIL },
#ifdef ENETDOWN
	{ ENETDOWN, ISC_R_NETDOWN },
#endif
#ifdef ENETUNREACH
	{ ENETUNREACH, ISC_R_NETUNREACH },
#endif
#ifdef ECONNABORTED
	{ ECONNABORTED, ISC_R_CONNECTIONRESET },
#endif
#ifdef ECONNRESET
	{ ECONNRESET, ISC_R_CONNECTIONRESET },
#endif
#ifdef ENOBUFS
	{ ENOBUFS, ISC_R_NORESOURCES },
#endif
#ifdef ENOTCONN
	{ ENOTCONN, ISC_R_NOTCONNECTED },
#endif
#ifdef ETIMEDOUT
	{ ETIMEDOUT, ISC_R_TIMEDOUT },
#endif
	{ ECONNREFUSED, ISC_R_CONNREFUSED },
#ifdef EHOSTDOWN
	{ EHOSTDOWN, ISC_R_HOSTDOWN },
#endif
#ifdef EHOSTUNREACH
	{ EHOSTUNREACH, ISC_R_HOSTUNREACH },
#endif
	{ 0, ISC_R_UNEXPECTED }
};

ATF_TC(isc_errno_toresult);
ATF_TC_HEAD(isc_errno_toresult, tc) {
	atf_tc_set_md_var(tc, "descr", "convert errno to ISC result");
}
ATF_TC_BODY(isc_errno_toresult, tc) {
	isc_result_t result, expect;
	size_t i;

	for (i = 0; i < sizeof(testpair)/sizeof(testpair[0]); i++) {
		result = isc_errno_toresult(testpair[i].err);
		expect = testpair[i].result;
		ATF_CHECK(result == expect);
	}
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, isc_errno_toresult);
	return (atf_no_error());
}

