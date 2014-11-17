use warnings;
use strict;

use Test::More tests => 32;

use Carp ();

sub lmm { Carp::longmess("x") }
sub lm { lmm() }

like lm(3), qr/main::lm\(3\)/;
like lm(substr("3\x{2603}", 0, 1)), qr/main::lm\(3\)/;
like lm(-3), qr/main::lm\(-3\)/;
like lm(-3.5), qr/main::lm\(-3\.5\)/;
like lm(-3.5e100), qr/main::lm\(-3\.5[eE]\+?100\)/;
like lm(""), qr/main::lm\(""\)/;
like lm("foo"), qr/main::lm\("foo"\)/;
like lm("a\$b\@c\\d\"e"), qr/main::lm\("a\\\$b\\\@c\\\\d\\\"e"\)/;
like lm("a\nb"), qr/main::lm\("a\\x\{a\}b"\)/;
like lm("a\x{666}b"), qr/main::lm\("a\\x\{666\}b"\)/;
like lm("\x{666}b"), qr/main::lm\("\\x\{666\}b"\)/;
like lm("a\x{666}"), qr/main::lm\("a\\x\{666\}"\)/;
like lm("L\xe9on"), qr/main::lm\("L\\x\{e9\}on"\)/;
like lm("L\xe9on \x{2603} !"), qr/main::lm\("L\\x\{e9\}on \\x\{2603\} !"\)/;

$Carp::MaxArgLen = 5;
foreach my $arg ("foo bar baz", "foo bar ba", "foo bar b", "foo bar ", "foo bar", "foo ba") {
    like lm($arg), qr/main::lm\("fo"\.\.\.\)/;
}
foreach my $arg ("foo b", "foo ", "foo", "fo", "f", "") {
    like lm($arg), qr/main::lm\("\Q$arg\E"\)/;
}
like lm("L\xe9on \x{2603} !"), qr/main::lm\("L\\x\{e9\}"\.\.\.\)/;
like lm("L\xe9on\x{2603}"), qr/main::lm\("L\\x\{e9\}on\\x\{2603\}"\)/;
like lm("foo\x{2603}"), qr/main::lm\("foo\\x\{2603\}"\)/;

$Carp::MaxArgLen = 0;
foreach my $arg ("wibble." x 20, "foo bar baz") {
    like lm($arg), qr/main::lm\("\Q$arg\E"\)/;
}
like lm("L\xe9on\x{2603}"), qr/main::lm\("L\\x\{e9\}on\\x\{2603\}"\)/;

1;
