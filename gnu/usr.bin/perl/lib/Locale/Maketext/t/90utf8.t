
require 5;
use Test;
BEGIN { plan tests => 4; }
use Locale::Maketext 1.01;
print "# Hi there...\n";
ok 1;


print "# --- Making sure that get_handle works with utf8 ---\n";
use utf8;

# declare some classes...
{
  package Woozle;
  @ISA = ('Locale::Maketext');
  sub dubbil   { return $_[1] * 2  .chr(2000)}
  sub numerate { return $_[2] . 'en'  }
}
{
  package Woozle::eu_mt;
  @ISA = ('Woozle');
  %Lexicon = (
   'd2' => chr(1000) . 'hum [dubbil,_1]',
   'd3' => chr(1000) . 'hoo [quant,_1,zaz]',
   'd4' => chr(1000) . 'hoo [*,_1,zaz]',
  );
  keys %Lexicon; # dodges the 'used only once' warning
}

my $lh;
print "# Basic sanity:\n";
ok defined( $lh = Woozle->get_handle('eu-mt') ) && ref($lh);
ok $lh && $lh->maketext('d2', 7), chr(1000)."hum 14".chr(2000)   ;


print "# Byebye!\n";
ok 1;

