
require 5;
use Test;
BEGIN { plan tests => 11; }
use Locale::Maketext 1.01;
print "# Hi there...\n";
ok 1;

print "# --- Making sure that get_handle works ---\n";

# declare some classes...
{
  package Woozle;
  @ISA = ('Locale::Maketext');
  sub dubbil   { return $_[1] * 2 }
  sub numerate { return $_[2] . 'en' }
}
{
  package Woozle::eu_mt;
  @ISA = ('Woozle');
  %Lexicon = (
   'd2' => 'hum [dubbil,_1]',
   'd3' => 'hoo [quant,_1,zaz]',
   'd4' => 'hoo [*,_1,zaz]',
  );
  keys %Lexicon; # dodges the 'used only once' warning
}

my $lh;
print "# Basic sanity:\n";
ok defined( $lh = Woozle->get_handle('eu-mt') ) && ref($lh);
ok $lh && $lh->maketext('d2', 7), "hum 14"      ;



print "# Make sure we can assign to ENV entries\n",
      "# (Otherwise we can't run the subsequent tests)...\n";
$ENV{'MYORP'}   = 'Zing';
ok $ENV{'MYORP'}, 'Zing';
$ENV{'SWUZ'}   = 'KLORTHO HOOBOY';
ok $ENV{'SWUZ'}, 'KLORTHO HOOBOY';

delete $ENV{'MYORP'};
delete $ENV{'SWUZ'};

print "# Test LANG...\n";
$ENV{'REQUEST_METHOD'} = '';
$ENV{'LANG'}     = 'Eu_MT';
$ENV{'LANGUAGE'} = '';
ok defined( $lh = Woozle->get_handle() ) && ref($lh);

print "# Test LANGUAGE...\n";
$ENV{'LANG'}     = '';
$ENV{'LANGUAGE'} = 'Eu-MT';
ok defined( $lh = Woozle->get_handle() ) && ref($lh);

print "# Test HTTP_ACCEPT_LANGUAGE...\n";
$ENV{'REQUEST_METHOD'}       = 'GET';
$ENV{'HTTP_ACCEPT_LANGUAGE'} = 'eu-MT';
ok defined( $lh = Woozle->get_handle() ) && ref($lh);
$ENV{'HTTP_ACCEPT_LANGUAGE'} = 'x-plorp, zaz, eu-MT, i-klung';
ok defined( $lh = Woozle->get_handle() ) && ref($lh);
$ENV{'HTTP_ACCEPT_LANGUAGE'} = 'x-plorp, zaz, eU-Mt, i-klung';
ok defined( $lh = Woozle->get_handle() ) && ref($lh);


print "# Byebye!\n";
ok 1;

