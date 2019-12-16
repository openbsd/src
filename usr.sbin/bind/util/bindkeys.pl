#!/usr/bin/env perl
#
# Copyright (C) 2009-2012, 2014, 2017  Internet Systems Consortium, Inc. ("ISC")
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

use strict;
use warnings;

my $lines;
while (<>) {
    chomp;
    if (/\/\* .Id:.* \*\//) {
	next;
    }
    s/\"/\\\"/g;
    s/$/\\n\\/;
    $lines .= $_ . "\n";
}

my $mkey = '#define MANAGED_KEYS "\\' . "\n" . $lines . "\"\n";

$lines =~ s/managed-keys/trusted-keys/;
$lines =~ s/\s+initial-key//g;
my $tkey = '#define TRUSTED_KEYS "\\' . "\n" . $lines . "\"\n";

print $tkey;
print "\n";
print $mkey;
