#
# This test aims to detect (using CPAN Testers) platforms where the locale
# encoding detection doesn't work.
#

use strict;
use warnings;

use Test::More tests => 3;

use encoding ();
use Encode qw<find_encoding>;

my $locale_encoding = encoding::_get_locale_encoding;

SKIP: {
    is(ref $locale_encoding, '', '_get_locale_encoding returns a scalar value')
	or skip 'no locale encoding found', 1;

    my $enc = find_encoding($locale_encoding);
    ok(defined $enc, 'encoding returned is supported')
	or diag("Encoding: ", explain($locale_encoding));
    isa_ok($enc, 'Encode::Encoding');
    note($locale_encoding, ' => ', $enc->name);
}
