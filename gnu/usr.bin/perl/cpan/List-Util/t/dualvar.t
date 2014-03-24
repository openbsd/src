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

use Scalar::Util ();
use Test::More  (grep { /dualvar/ } @Scalar::Util::EXPORT_FAIL)
			? (skip_all => 'dualvar requires XS version')
			: (tests => 41);
use Config;

Scalar::Util->import('dualvar');
Scalar::Util->import('isdual');

$var = dualvar( 2.2,"string");

ok( isdual($var),	'Is a dualvar');
ok( $var == 2.2,	'Numeric value');
ok( $var eq "string",	'String value');

$var2 = $var;

ok( isdual($var2),	'Is a dualvar');
ok( $var2 == 2.2,	'copy Numeric value');
ok( $var2 eq "string",	'copy String value');

$var++;

ok( ! isdual($var),	'No longer dualvar');
ok( $var == 3.2,	'inc Numeric value');
ok( $var ne "string",	'inc String value');

my $numstr = "10.2";
my $numtmp = int($numstr); # use $numstr as an int

$var = dualvar($numstr, "");

ok( isdual($var),	'Is a dualvar');
ok( $var == $numstr,	'NV');

SKIP: {
  skip("dualvar with UV value known to fail with $]",2) if $] < 5.006_001;
  my $bits = ($Config{'use64bitint'}) ? 63 : 31;
  $var = dualvar(1<<$bits, "");
  ok( isdual($var),		'Is a dualvar');
  ok( $var == (1<<$bits),	'UV 1');
  ok( $var > 0,			'UV 2');
}

# Create a dualvar "the old fashioned way"
$var = "10";
ok( ! isdual($var),	'Not a dualvar');
my $foo = $var + 0;
ok( isdual($var),	'Is a dualvar');

{
  package Tied;

  sub TIESCALAR { bless {} }
  sub FETCH { 7.5 }
}

tie my $tied, 'Tied';
$var = dualvar($tied, "ok");
ok(isdual($var),	'Is a dualvar');
ok($var == 7.5,		'Tied num');
ok($var eq 'ok',	'Tied str');


SKIP: {
  skip("need utf8::is_utf8",3) unless defined &utf8::is_utf8;
  ok(!!utf8::is_utf8(dualvar(1,chr(400))), 'utf8');
  ok( !utf8::is_utf8(dualvar(1,"abc")),    'not utf8');
}


SKIP: {
  skip("Perl not compiled with 'useithreads'",20) unless ($Config{'useithreads'});
  require threads; import threads;
  require threads::shared; import threads::shared;
  skip("Requires threads::shared v1.42 or later",20) unless ($threads::shared::VERSION >= 1.42);

  my $siv :shared = dualvar(42, 'Fourty-Two');
  my $snv :shared = dualvar(3.14, 'PI');
  my $bits = ($Config{'use64bitint'}) ? 63 : 31;
  my $suv :shared = dualvar(1<<$bits, 'Large unsigned int');

  ok($siv == 42, 'Shared IV number preserved');
  ok($siv eq 'Fourty-Two', 'Shared string preserved');
  ok(isdual($siv), 'Is a dualvar');
  ok($snv == 3.14, 'Shared NV number preserved');
  ok($snv eq 'PI', 'Shared string preserved');
  ok(isdual($snv), 'Is a dualvar');
  ok($suv == (1<<$bits), 'Shared UV number preserved');
  ok($suv > 0, 'Shared UV number preserved');
  ok($suv eq 'Large unsigned int', 'Shared string preserved');
  ok(isdual($suv), 'Is a dualvar');

  my @ary :shared;
  $ary[0] = $siv;
  $ary[1] = $snv;
  $ary[2] = $suv;

  ok($ary[0] == 42, 'Shared IV number preserved');
  ok($ary[0] eq 'Fourty-Two', 'Shared string preserved');
  ok(isdual($ary[0]), 'Is a dualvar');
  ok($ary[1] == 3.14, 'Shared NV number preserved');
  ok($ary[1] eq 'PI', 'Shared string preserved');
  ok(isdual($ary[1]), 'Is a dualvar');
  ok($ary[2] == (1<<$bits), 'Shared UV number preserved');
  ok($ary[2] > 0, 'Shared UV number preserved');
  ok($ary[2] eq 'Large unsigned int', 'Shared string preserved');
  ok(isdual($ary[2]), 'Is a dualvar');
}

