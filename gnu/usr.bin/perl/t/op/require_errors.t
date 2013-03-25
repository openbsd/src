#!perl
use strict;
use warnings;

BEGIN {
    require './test.pl';
}

plan(tests => 3);

my $nonfile = tempfile();

@INC = qw(Perl Rules);

eval {
    require $nonfile;
};

like $@, qr/^Can't locate $nonfile in \@INC \(\@INC contains: @INC\) at/;

eval {
    require "$nonfile.ph";
};

like $@, qr/^Can't locate $nonfile\.ph in \@INC \(did you run h2ph\?\) \(\@INC contains: @INC\) at/;

eval {
    require "$nonfile.h";
};

like $@, qr/^Can't locate $nonfile\.h in \@INC \(change \.h to \.ph maybe\?\) \(did you run h2ph\?\) \(\@INC contains: @INC\) at/;

# I can't see how to test the EMFILE case
# I can't see how to test the case of not displaying @INC in the message.
# (and does that only happen on VMS?)
