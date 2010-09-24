#!perl -w

BEGIN {
    if ($ENV{'PERL_CORE'}){
        chdir 't' if -d 't';
        @INC = '../lib';
    }
}

BEGIN {
    eval {
	require warnings;
    };
    if ($@) {
	print "1..0\n";
	print $@;
	exit;
    }
}

use strict;
use MIME::Base64 qw(decode_base64);

print "1..1\n";

use warnings;

my @warn;
$SIG{__WARN__} = sub { push(@warn, @_) };

warn;
my $a;
$a = decode_base64("aa");
$a = decode_base64("a===");
warn;
$a = do {
    no warnings;
    decode_base64("aa");
};
$a = do {
    no warnings;
    decode_base64("a===");
};
warn;
$a = do {
    local $^W;
    decode_base64("aa");
};
$a = do {
    local $^W;
    decode_base64("a===");
};
warn;

for (@warn) {
    print "# $_";
}

print "not " unless join("", @warn) eq <<"EOT"; print "ok 1\n";
Warning: something's wrong at $0 line 31.
Premature end of base64 data at $0 line 33.
Premature padding of base64 data at $0 line 34.
Warning: something's wrong at $0 line 35.
Premature end of base64 data at $0 line 38.
Premature padding of base64 data at $0 line 42.
Warning: something's wrong at $0 line 44.
Warning: something's wrong at $0 line 53.
EOT
