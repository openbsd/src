#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use strict;
use warnings;
use Config;

require './test.pl';

plan(3);

# first test using -Margs ...
$ENV{PERL5OPT} = "-Mlib=optm1 -Iopti1 -Mlib=optm2 -Iopti2";
$ENV{PERL5LIB} = join($Config{path_sep}, qw(e1 e2));

# -I causes S_incpush to be called with options that will add this if it exists,
# so we may need to expect it.
my $archdir = '../lib/' . $Config{archname};
my $archdir_expected = -d $archdir ? " $archdir" : '';

# this isn't *quite* identical to what's in perlrun.pod, because
# test.pl:_create_runperl adds -I../lib and -I.
like(
    runperl(
        switches => [qw(-Ii1 -Mlib=m1 -Ii2 -Mlib=m2)],
        prog     => 'print join(q( ), @INC)'
    ),
    qr{^\Qoptm2 optm1 m2 m1 opti2 opti1$archdir_expected ../lib . i1 i2 e1 e2\E\b},
    "Order of application of -I and -M matches documentation"
);

# and now using -M and -I args with a space. NB that '-M foo' and '-I foo'
# aren't supported in PERL5OPT.
like(
    runperl(
        switches => [qw(-I i1 -M lib=m1 -I i2 -M lib=m2)],
        prog     => 'print join(q( ), @INC)'
    ),
    qr{^\Qoptm2 optm1 m2 m1 opti2 opti1$archdir_expected ../lib . i1 i2 e1 e2\E\b},
    "... still matches when the switch is followed by a space then its parameter"
);

# and now with a mixture of args with and without spaces
like(
    runperl(
        switches => [qw(-Ii1 -Mlib=m1 -I i2 -M lib=m2)],
        prog     => 'print join(q( ), @INC)'
    ),
    qr{^\Qoptm2 optm1 m2 m1 opti2 opti1$archdir_expected ../lib . i1 i2 e1 e2\E\b},
    "... still matches when we've got a mixture of args with and without spaces"
);
