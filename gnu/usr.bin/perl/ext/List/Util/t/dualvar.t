#!./perl

BEGIN {
    unless (-d 'blib') {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	keys %Config; # Silence warning
	if ($Config{extensions} !~ /\bList\/Util\b/) {
	    print "1..0 # Skip: List::Util was not built\n";
	    exit 0;
	}
    }
}

use vars qw($skip);

BEGIN {
  require Scalar::Util;

  if (grep { /dualvar/ } @Scalar::Util::EXPORT_FAIL) {
    print "1..0\n";
    $skip=1;
  }
}

eval <<'EOT' unless $skip;
use Scalar::Util qw(dualvar);

print "1..11\n";

$var = dualvar 2.2,"string";

print "not " unless $var == 2.2;
print "ok 1\n";

print "not " unless $var eq "string";
print "ok 2\n";

$var2 = $var;

$var++;

print "not " unless $var == 3.2;
print "ok 3\n";

print "not " unless $var ne "string";
print "ok 4\n";

print "not " unless $var2 == 2.2;
print "ok 5\n";

print "not " unless $var2 eq "string";
print "ok 6\n";

my $numstr = "10.2";
my $numtmp = sprintf("%d", $numstr);
$var = dualvar $numstr, "";
print "not " unless $var == $numstr;
print "ok 7\n";

$var = dualvar 1<<31, "";
print "not " unless $var == 1<<31;
print "ok 8\n";
print "not " unless $var > 0;
print "ok 9\n";

tie my $tied, 'Tied';
$var = dualvar $tied, "ok";
print "not " unless $var == 7.5;
print "ok 10\n";
print "not " unless $var eq "ok";
print "ok 11\n";

EOT

package Tied;

sub TIESCALAR { bless {} }
sub FETCH { 7.5 }

