use strict; use warnings;

BEGIN { require './t/lib/ok.pl' }
use Text::Wrap;

print "1..3\n";

$Text::Wrap::huge = 'overflow';
my $in = "\x{300}" x 1_000_000;
my $out = eval { wrap('', '', $in) };
ok( !$@ ) or diag( $@ );
ok( $in eq $out );
ok( length($in) == length($out) );
