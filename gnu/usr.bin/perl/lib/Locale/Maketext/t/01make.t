
require 5;
use Test;
BEGIN { plan tests => 6; }
use Locale::Maketext 1.01;
print "# Hi there...\n";
ok 1;

# declare some classes...
{
  package Woozle;
  @ISA = ('Locale::Maketext');
  sub dubbil   { return $_[1] * 2 }
  sub numerate { return $_[2] . 'en' }
}
{
  package Woozle::elx;
  @ISA = ('Woozle');
  %Lexicon = (
   'd2' => 'hum [dubbil,_1]',
   'd3' => 'hoo [quant,_1,zaz]',
   'd4' => 'hoo [*,_1,zaz]',
  );
  keys %Lexicon; # dodges the 'used only once' warning
}

ok defined( $lh = Woozle->get_handle('elx') ) && ref($lh);
ok $lh && $lh->maketext('d2', 7), "hum 14"      ;
ok $lh && $lh->maketext('d3', 7), "hoo 7 zazen" ;
ok $lh && $lh->maketext('d4', 7), "hoo 7 zazen" ;

print "# Byebye!\n";
ok 1;

