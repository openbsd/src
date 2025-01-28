use strict; use warnings;

BEGIN { require './t/lib/ok.pl' }
use Text::Wrap;

print "1..4\n";
my ( $in, $out );

$Text::Tabs::tabstop = 8;
$in  = "\x{300}\t.\n\x{300}\t.";
$out = "\x{300}       .\n\x{300}       .";
ok( Text::Tabs::expand($in) eq $out );
ok( Text::Tabs::unexpand($out) eq $in );

$in  = "\x{300} Taglia Unica\n\x{300} Taglia Unica";
$out = "\x{300} Taglia Unica\n\x{300} Taglia Unica";
my $got = eval { Text::Wrap::wrap( '', '', $in ) };
ok( !$@ ) or diag( $@ );
ok( $got eq $out );
