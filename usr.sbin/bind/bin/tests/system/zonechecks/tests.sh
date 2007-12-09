#!/bin/sh
#
# Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

# $ISC: tests.sh,v 1.2.2.1 2004/11/23 05:24:49 marka Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

#
echo "I: checking that we detect a NS which refers to a CNAME"
if $CHECKZONE . cname.db > cname.out 2>&1
then
	echo "I:failed (status)"; status=1
else
	if grep "is a CNAME" cname.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=1
	fi
fi

#
echo "I: checking that we detect a NS which is below a DNAME"
if $CHECKZONE . dname.db > dname.out 2>&1
then
	echo "I:failed (status)"; status=1
else
	if grep "is below a DNAME" dname.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=1
	fi
fi

#
echo "I: checking that we detect a NS which has no address records (A/AAAA)"
if $CHECKZONE . noaddress.db > noaddress.out
then
	echo "I:failed (status)"; status=1
else
	if grep "has no address records" noaddress.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=1
	fi
fi

#
echo "I: checking that we detect a NS which has no records"
if $CHECKZONE . nxdomain.db > nxdomain.out
then
	echo "I:failed (status)"; status=1
else
	if grep "has no address records" noaddress.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=1
	fi
fi

#
echo "I: checking that we detect a NS which looks like a A record (fail)"
if $CHECKZONE -n fail . a.db > a.out 2>&1
then
	echo "I:failed (status)"; status=1
else
	if grep "appears to be an address" a.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=1
	fi
fi

#
echo "I: checking that we detect a NS which looks like a A record (warn=default)"
if $CHECKZONE . a.db > a.out 2>&1
then
	if grep "appears to be an address" a.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=1
	fi
else
	echo "I:failed (status)"; status=1
fi

#
echo "I: checking that we detect a NS which looks like a A record (ignore)"
if $CHECKZONE -n ignore . a.db > a.out 2>&1
then
	if grep "appears to be an address" a.out > /dev/null
	then
		echo "I:failed (message)"; status=1
	else
		:
	fi
else
	echo "I:failed (status)"; status=1
fi

#
echo "I: checking that we detect a NS which looks like a AAAA record (fail)"
if $CHECKZONE -n fail . aaaa.db > aaaa.out 2>&1
then
	echo "I:failed (status)"; status=1
else
	if grep "appears to be an address" aaaa.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=1
	fi
fi

#
echo "I: checking that we detect a NS which looks like a AAAA record (warn=default)"
if $CHECKZONE . aaaa.db > aaaa.out 2>&1
then
	if grep "appears to be an address" aaaa.out > /dev/null
	then
		:
	else
		echo "I:failed (message)"; status=1
	fi
else
	echo "I:failed (status)"; status=1
fi

#
echo "I: checking that we detect a NS which looks like a AAAA record (ignore)"
if $CHECKZONE -n ignore . aaaa.db > aaaa.out 2>&1
then
	if grep "appears to be an address" aaaa.out > /dev/null
	then
		echo "I:failed (message)"; status=1
	else
		:
	fi
else
	echo "I:failed (status)"; status=1
fi
echo "I:exit status: $status"
exit $?
