BEGIN {
    if ($ENV{'PERL_CORE'}){
        chdir 't';
        unshift @INC, '../lib';
    }
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bEncode\b/) {
      print "1..0 # Skip: Encode was not built\n";
      exit 0;
    }
    $| = 1;
}

use strict;
use Test::More tests => 21;
use Encode;

# The specification of GSM 03.38 is not awfully clear.
# (http://www.unicode.org/Public/MAPPINGS/ETSI/GSM0338.TXT)
# The various combinations of 0x00 and 0x1B as leading bytes
# are unclear, as is the semantics of those bytes as standalone
# or as final single bytes.

sub t { is(decode("gsm0338", my $t = $_[0]), $_[1]) }

# t("\x00",     "\x00"); # ???

# "Round-trip".
t("\x41",     "\x41");

t("\x01",     "\xA3");
t("\x02",     "\x24");
t("\x03",     "\xA5");
t("\x09",     "\xE7");

t("\x00\x00", "\x00\x00"); # Maybe?
t("\x00\x1B", "\x40\xA0"); # Maybe?
t("\x00\x41", "\x40\x41");

# t("\x1B",     "\x1B"); # ???

# Escape with no special second byte is just a NBSP.
t("\x1B\x41", "\xA0\x41");

t("\x1B\x00", "\xA0\x40"); # Maybe?

# Special escape characters.
t("\x1B\x0A", "\x0C");
t("\x1B\x14", "\x5E");
t("\x1B\x28", "\x7B");
t("\x1B\x29", "\x7D");
t("\x1B\x2F", "\x5C");
t("\x1B\x3C", "\x5B");
t("\x1B\x3D", "\x7E");
t("\x1B\x3E", "\x5D");
t("\x1B\x40", "\x7C");
t("\x1B\x40", "\x7C");
t("\x1B\x65", "\x{20AC}");




