use strict; use warnings;

BEGIN { require './t/lib/ok.pl' }

use Text::Wrap;

print "1..2\n";

my $sentence = join "\x{a0}", qw( This sentence should never wrap. );
$Text::Wrap::columns = 2 * ( length $sentence ) - 4;

my $in  = join ' ',  ( $sentence ) x 10;
my $out = join "\n", ( $sentence ) x 10;
my $got = eval { wrap('', '', $in) };

ok( !$@ ) or diag( $@ );
ok( $got eq $out ) or diag( $got );
