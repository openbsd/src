# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'
# Time-stamp: "2001-06-20 02:12:53 MDT"
######################### We start with some black magic to print on failure.

# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..5\n"; }
END {print "fail 1\n" unless $loaded;}
use Locale::Maketext 1.01;
print "# Perl v$], Locale::Maketext v$Locale::Maketext::VERSION\n";
$loaded = 1;
print "ok 1\n";
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
}

$lh = Woozle->get_handle('elx');
if($lh) {
  print "ok 2\n";

  my $x;

  $x = $lh->maketext('d2', 7);
  if($x eq "hum 14") {
    print "ok 3\n";
  } else {
    print "fail 3 #  (got \"$x\")\n";
  }

  $x = $lh->maketext('d3', 7);
  if($x eq "hoo 7 zazen") {
    print "ok 4\n";
  } else {
    print "fail 4 #  (got \"$x\")\n";
  }

  $x = $lh->maketext('d4', 7);
  if($x eq "hoo 7 zazen") {
    print "ok 5\n";
  } else {
    print "fail 5 #  (got \"$x\")\n";
  }

  
} else {
  print "fail 2\n";
}
#Shazam!
