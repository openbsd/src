#!/bin/sh
#
# Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
#
# Permission to use, copy, modify, and/or distribute this software for any
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

# Fetch a copy of a current root signing key; used for testing
# DNSSEC validation in 'sample'.
#
# After running this script, "sample `cat sample.key` <args>" will
# perform a lookup as specified in <args> and validate the result
# using the root key.
#
# (This is NOT a secure method of obtaining the root key; it is
# included here for testing purposes only.)
dig +noall +answer dnskey . | perl -n -e '
local ($dn, $ttl, $class, $type, $flags, $proto, $alg, @rest) = split;
next if ($flags != 257);
local $key = join("", @rest);
print "-a $alg -e -k $dn -K $key\n"
' > sample.key
