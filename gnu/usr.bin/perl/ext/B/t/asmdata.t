#!./perl -Tw

BEGIN {
    chdir 't';
    @INC = '../lib';
    require Config;
    if (($Config::Config{'extensions'} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
}

use Test::More tests => 13;

use_ok('B::Asmdata', qw(%insn_data @insn_name @optype @specialsv_name));

# check we got something.
isnt( keys %insn_data,  0,  '%insn_data exported and populated' );
isnt( @insn_name,       0,  '   @insn_name' );
isnt( @optype,          0,  '   @optype' );
isnt( @specialsv_name,  0,  '   @specialsv_name' );

# pick an op that's not likely to go away in the future
my @data = values %insn_data;
is( (grep { ref eq 'ARRAY' } @data),  @data,   '%insn_data contains arrays' );

# pick one at random to test with.
my $opname = (keys %insn_data)[rand @data];
my $data = $insn_data{$opname};
like( $data->[0], qr/^\d+$/,    '   op number' );
is( ref $data->[1],  'CODE',    '   PUT code ref' );
ok( !ref $data->[2],            '   GET method' );

is( $insn_name[$data->[0]], $opname,    '@insn_name maps correctly' );


# I'm going to assume that op types will all be named /OP$/.
# If this changes in the future, change this test.
is( grep(/OP$/, @optype), @optype,  '@optype is all /OP$/' );


# comment in bytecode.pl says "Nullsv *must come first so that the 
# condition ($$sv == 0) can continue to be used to test (sv == Nullsv)."
is( $specialsv_name[0],  'Nullsv',  'Nullsv come first in @special_sv_name' );

# other than that, we can't really say much more about @specialsv_name
# than it has to contain strings (on the off chance &PL_sv_undef gets 
# flubbed)
is( grep(!ref, @specialsv_name), @specialsv_name,   '  contains all strings' );
