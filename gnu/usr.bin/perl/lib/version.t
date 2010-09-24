#! /usr/local/perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

use Test::More qw(no_plan);
use Data::Dumper;
require Test::Harness;
no warnings 'once';
*Verbose = \$Test::Harness::Verbose;
use POSIX qw/locale_h/;
use File::Temp qw/tempfile/;
use File::Basename;

BEGIN {
    use_ok("version", 0.77);
    # If we made it this far, we are ok.
}

my $Verbose;

diag "Tests with base class" unless $ENV{PERL_CORE};

BaseTests("version","new","qv");
BaseTests("version","new","declare");
BaseTests("version","parse", "qv");
BaseTests("version","parse", "declare");

# dummy up a redundant call to satify David Wheeler
local $SIG{__WARN__} = sub { die $_[0] };
eval 'use version;';
unlike ($@, qr/^Subroutine main::declare redefined/,
    "Only export declare once per package (to prevent redefined warnings)."); 

package version::Bad;
use base 'version';
sub new { my($self,$n)=@_;  bless \$n, $self }

package main;

my $warning;
local $SIG{__WARN__} = sub { $warning = $_[0] };
my ($fh, $filename) = tempfile('tXXXXXXX', SUFFIX => '.pm', UNLINK => 1);
(my $package = basename($filename)) =~ s/\.pm$//;
print $fh <<"EOF";
# This is an empty subclass
package $package;
use base 'version';
use vars '\$VERSION';
\$VERSION=0.001;
EOF
close $fh;

sub main_reset {
    delete $main::INC{'$package'};
    undef &qv; undef *::qv; # avoid 'used once' warning
    undef &declare; undef *::declare; # avoid 'used once' warning
}

diag "Tests with empty derived class"  unless $ENV{PERL_CORE};

use_ok($package, 0.001);
my $testobj = $package->new(1.002_003);
isa_ok( $testobj, $package );
ok( $testobj->numify == 1.002003, "Numified correctly" );
ok( $testobj->stringify eq "1.002003", "Stringified correctly" );
ok( $testobj->normal eq "v1.2.3", "Normalified correctly" );

my $verobj = version::->new("1.2.4");
ok( $verobj > $testobj, "Comparison vs parent class" );

BaseTests($package, "new", "qv");
main_reset;
use_ok($package, 0.001, "declare");
BaseTests($package, "new", "declare");
main_reset;
use_ok($package, 0.001);
BaseTests($package, "parse", "qv");
main_reset;
use_ok($package, 0.001, "declare");
BaseTests($package, "parse", "declare");

diag "tests with bad subclass"  unless $ENV{PERL_CORE};
$testobj = version::Bad->new(1.002_003);
isa_ok( $testobj, "version::Bad" );
eval { my $string = $testobj->numify };
like($@, qr/Invalid version object/,
    "Bad subclass numify");
eval { my $string = $testobj->normal };
like($@, qr/Invalid version object/,
    "Bad subclass normal");
eval { my $string = $testobj->stringify };
like($@, qr/Invalid version object/,
    "Bad subclass stringify");
eval { my $test = ($testobj > 1.0) };
like($@, qr/Invalid version object/,
    "Bad subclass vcmp");
strict_lax_tests();

# do strict lax tests in a sub to isolate a package to test importing
sub strict_lax_tests {
  package temp12345;
  # copied from perl core test t/op/packagev.t
  # format: STRING STRICT_OK LAX_OK
  my $strict_lax_data = << 'CASE_DATA';
1.00		pass	pass
1.00001		pass	pass
0.123		pass	pass
12.345		pass	pass
42		pass	pass
0		pass	pass
0.0		pass	pass
v1.2.3		pass	pass
v1.2.3.4	pass	pass
v0.1.2		pass	pass
v0.0.0		pass	pass
01		fail	pass
01.0203		fail	pass
v01		fail	pass
v01.02.03	fail	pass
.1		fail	pass
.1.2		fail	pass
1.		fail	pass
1.a		fail	fail
1._		fail	fail
1.02_03		fail	pass
v1.2_3		fail	pass
v1.02_03	fail	pass
v1.2_3_4	fail	fail
v1.2_3.4	fail	fail
1.2_3.4		fail	fail
0_		fail	fail
1_		fail	fail
1_.		fail	fail
1.1_		fail	fail
1.02_03_04	fail	fail
1.2.3		fail	pass
v1.2		fail	pass
v0		fail	pass
v1		fail	pass
v.1.2.3		fail	fail
v		fail	fail
v1.2345.6	fail	pass
undef		fail	pass
1a		fail	fail
1.2a3		fail	fail
bar		fail	fail
_		fail	fail
CASE_DATA

  require version;
  version->import( qw/is_strict is_lax/ );
  for my $case ( split qr/\n/, $strict_lax_data ) {
    my ($v, $strict, $lax) = split qr/\t+/, $case;
    main::ok( $strict eq 'pass' ? is_strict($v) : ! is_strict($v), "is_strict($v) [$strict]" );
    main::ok( $strict eq 'pass' ? version::is_strict($v) : ! version::is_strict($v), "version::is_strict($v) [$strict]" );
    main::ok( $lax eq 'pass' ? is_lax($v) : ! is_lax($v), "is_lax($v) [$lax]" );
    main::ok( $lax eq 'pass' ? version::is_lax($v) : ! version::is_lax($v), "version::is_lax($v) [$lax]" );
  }
}

sub BaseTests {

    my ($CLASS, $method, $qv_declare) = @_;
    my $warning;
    local $SIG{__WARN__} = sub { $warning = $_[0] };
    
    # Insert your test code below, the Test module is use()ed here so read
    # its man page ( perldoc Test ) for help writing this test script.
    
    # Test bare number processing
    diag "tests with bare numbers" unless $ENV{PERL_CORE};
    $version = $CLASS->$method(5.005_03);
    is ( "$version" , "5.00503" , '5.005_03 eq 5.00503' );
    $version = $CLASS->$method(1.23);
    is ( "$version" , "1.23" , '1.23 eq "1.23"' );
    
    # Test quoted number processing
    diag "tests with quoted numbers" unless $ENV{PERL_CORE};
    $version = $CLASS->$method("5.005_03");
    is ( "$version" , "5.005_03" , '"5.005_03" eq "5.005_03"' );
    $version = $CLASS->$method("v1.23");
    is ( "$version" , "v1.23" , '"v1.23" eq "v1.23"' );
    
    # Test stringify operator
    diag "tests with stringify" unless $ENV{PERL_CORE};
    $version = $CLASS->$method("5.005");
    is ( "$version" , "5.005" , '5.005 eq "5.005"' );
    $version = $CLASS->$method("5.006.001");
    is ( "$version" , "5.006.001" , '5.006.001 eq v5.6.1' );
    unlike ($warning, qr/v-string without leading 'v' deprecated/, 'No leading v');
    $version = $CLASS->$method("v1.2.3_4");
    is ( "$version" , "v1.2.3_4" , 'alpha version 1.2.3_4 eq v1.2.3_4' );
    
    # test illegal formats
    diag "test illegal formats" unless $ENV{PERL_CORE};
    eval {$version = $CLASS->$method("1.2_3_4")};
    like($@, qr/multiple underscores/,
	"Invalid version format (multiple underscores)");
    
    eval {$version = $CLASS->$method("1.2_3.4")};
    like($@, qr/underscores before decimal/,
	"Invalid version format (underscores before decimal)");
    
    eval {$version = $CLASS->$method("1_2")};
    like($@, qr/alpha without decimal/,
	"Invalid version format (alpha without decimal)");
    
    eval { $version = $CLASS->$method("1.2b3")};
    like($@, qr/non-numeric data/,
	"Invalid version format (non-numeric data)");

    # from here on out capture the warning and test independently
    {
    eval{$version = $CLASS->$method("99 and 44/100 pure")};

    like($@, qr/non-numeric data/,
	"Invalid version format (non-numeric data)");
    
    eval{$version = $CLASS->$method("something")};
    like($@, qr/non-numeric data/,
	"Invalid version format (non-numeric data)");
    
    # reset the test object to something reasonable
    $version = $CLASS->$method("1.2.3");
    
    # Test boolean operator
    ok ($version, 'boolean');
    
    # Test class membership
    isa_ok ( $version, $CLASS );
    
    # Test comparison operators with self
    diag "tests with self" unless $ENV{PERL_CORE};
    is ( $version <=> $version, 0, '$version <=> $version == 0' );
    ok ( $version == $version, '$version == $version' );
    
    # Test Numeric Comparison operators
    # test first with non-object
    $version = $CLASS->$method("5.006.001");
    $new_version = "5.8.0";
    diag "numeric tests with non-objects" unless $ENV{PERL_CORE};
    ok ( $version == $version, '$version == $version' );
    ok ( $version < $new_version, '$version < $new_version' );
    ok ( $new_version > $version, '$new_version > $version' );
    ok ( $version != $new_version, '$version != $new_version' );
    
    # now test with existing object
    $new_version = $CLASS->$method($new_version);
    diag "numeric tests with objects" unless $ENV{PERL_CORE};
    ok ( $version < $new_version, '$version < $new_version' );
    ok ( $new_version > $version, '$new_version > $version' );
    ok ( $version != $new_version, '$version != $new_version' );
    
    # now test with actual numbers
    diag "numeric tests with numbers" unless $ENV{PERL_CORE};
    ok ( $version->numify() == 5.006001, '$version->numify() == 5.006001' );
    ok ( $version->numify() <= 5.006001, '$version->numify() <= 5.006001' );
    ok ( $version->numify() < 5.008, '$version->numify() < 5.008' );
    #ok ( $version->numify() > v5.005_02, '$version->numify() > 5.005_02' );
    
    # test with long decimals
    diag "Tests with extended decimal versions" unless $ENV{PERL_CORE};
    $version = $CLASS->$method(1.002003);
    ok ( $version == "1.2.3", '$version == "1.2.3"');
    ok ( $version->numify == 1.002003, '$version->numify == 1.002003');
    $version = $CLASS->$method("2002.09.30.1");
    ok ( $version == "2002.9.30.1",'$version == 2002.9.30.1');
    ok ( $version->numify == 2002.009030001,
	'$version->numify == 2002.009030001');
    
    # now test with alpha version form with string
    $version = $CLASS->$method("1.2.3");
    $new_version = "1.2.3_4";
    diag "numeric tests with alpha-style non-objects" unless $ENV{PERL_CORE};
    ok ( $version < $new_version, '$version < $new_version' );
    ok ( $new_version > $version, '$new_version > $version' );
    ok ( $version != $new_version, '$version != $new_version' );
    
    $version = $CLASS->$method("1.2.4");
    diag "numeric tests with alpha-style non-objects"
	unless $ENV{PERL_CORE};
    ok ( $version > $new_version, '$version > $new_version' );
    ok ( $new_version < $version, '$new_version < $version' );
    ok ( $version != $new_version, '$version != $new_version' );
    
    # now test with alpha version form with object
    $version = $CLASS->$method("1.2.3");
    $new_version = $CLASS->$method("1.2.3_4");
    diag "tests with alpha-style objects" unless $ENV{PERL_CORE};
    ok ( $version < $new_version, '$version < $new_version' );
    ok ( $new_version > $version, '$new_version > $version' );
    ok ( $version != $new_version, '$version != $new_version' );
    ok ( !$version->is_alpha, '!$version->is_alpha');
    ok ( $new_version->is_alpha, '$new_version->is_alpha');
    
    $version = $CLASS->$method("1.2.4");
    diag "tests with alpha-style objects" unless $ENV{PERL_CORE};
    ok ( $version > $new_version, '$version > $new_version' );
    ok ( $new_version < $version, '$new_version < $version' );
    ok ( $version != $new_version, '$version != $new_version' );
    
    $version = $CLASS->$method("1.2.3.4");
    $new_version = $CLASS->$method("1.2.3_4");
    diag "tests with alpha-style objects with same subversion"
	unless $ENV{PERL_CORE};
    ok ( $version > $new_version, '$version > $new_version' );
    ok ( $new_version < $version, '$new_version < $version' );
    ok ( $version != $new_version, '$version != $new_version' );
    
    diag "test implicit [in]equality" unless $ENV{PERL_CORE};
    $version = $CLASS->$method("v1.2.3");
    $new_version = $CLASS->$method("1.2.3.0");
    ok ( $version == $new_version, '$version == $new_version' );
    $new_version = $CLASS->$method("1.2.3_0");
    ok ( $version == $new_version, '$version == $new_version' );
    $new_version = $CLASS->$method("1.2.3.1");
    ok ( $version < $new_version, '$version < $new_version' );
    $new_version = $CLASS->$method("1.2.3_1");
    ok ( $version < $new_version, '$version < $new_version' );
    $new_version = $CLASS->$method("1.1.999");
    ok ( $version > $new_version, '$version > $new_version' );
    
    # that which is not expressly permitted is forbidden
    diag "forbidden operations" unless $ENV{PERL_CORE};
    ok ( !eval { ++$version }, "noop ++" );
    ok ( !eval { --$version }, "noop --" );
    ok ( !eval { $version/1 }, "noop /" );
    ok ( !eval { $version*3 }, "noop *" );
    ok ( !eval { abs($version) }, "noop abs" );

SKIP: {
    skip "version require'd instead of use'd, cannot test $qv_declare", 3
    	unless defined $qv_declare;
    # test the $qv_declare() sub
    diag "testing $qv_declare" unless $ENV{PERL_CORE};
    $version = $CLASS->$qv_declare("1.2");
    is ( "$version", "v1.2", $qv_declare.'("1.2") == "1.2.0"' );
    $version = $CLASS->$qv_declare(1.2);
    is ( "$version", "v1.2", $qv_declare.'(1.2) == "1.2.0"' );
    isa_ok( $CLASS->$qv_declare('5.008'), $CLASS );
}

    # test creation from existing version object
    diag "create new from existing version" unless $ENV{PERL_CORE};
    ok (eval {$new_version = $CLASS->$method($version)},
	    "new from existing object");
    ok ($new_version == $version, "class->$method($version) identical");
    $new_version = $version->$method();
    isa_ok ($new_version, $CLASS );
    is ($new_version, "0", "version->$method() doesn't clone");
    $new_version = $version->$method("1.2.3");
    is ($new_version, "1.2.3" , '$version->$method("1.2.3") works too');

    # test the CVS revision mode
    diag "testing CVS Revision" unless $ENV{PERL_CORE};
    $version = new $CLASS qw$Revision: 1.2$;
    ok ( $version == "1.2.0", 'qw$Revision: 1.2$ == 1.2.0' );
    $version = new $CLASS qw$Revision: 1.2.3.4$;
    ok ( $version == "1.2.3.4", 'qw$Revision: 1.2.3.4$ == 1.2.3.4' );
    
    # test the CPAN style reduced significant digit form
    diag "testing CPAN-style versions" unless $ENV{PERL_CORE};
    $version = $CLASS->$method("1.23_01");
    is ( "$version" , "1.23_01", "CPAN-style alpha version" );
    ok ( $version > 1.23, "1.23_01 > 1.23");
    ok ( $version < 1.24, "1.23_01 < 1.24");

    # test reformed UNIVERSAL::VERSION
    diag "Replacement UNIVERSAL::VERSION tests" unless $ENV{PERL_CORE};

    my $error_regex = $] < 5.006
	? 'version \d required'
	: 'does not define \$t.{7}::VERSION';
    
    {
	my ($fh, $filename) = tempfile('tXXXXXXX', SUFFIX => '.pm', UNLINK => 1);
	(my $package = basename($filename)) =~ s/\.pm$//;
	print $fh "package $package;\n\$$package\::VERSION=0.58;\n1;\n";
	close $fh;

	$version = 0.58;
	eval "use lib '.'; use $package $version";
	unlike($@, qr/$package version $version/,
		'Replacement eval works with exact version');
	
	# test as class method
	$new_version = $package->VERSION;
	cmp_ok($new_version,'==',$version, "Called as class method");

	eval "print Completely::Unknown::Module->VERSION";
	if ( $] < 5.008 ) {
	    unlike($@, qr/$error_regex/,
		"Don't freak if the module doesn't even exist");
	}
	else {
	    unlike($@, qr/defines neither package nor VERSION/,
		"Don't freak if the module doesn't even exist");
	}

	# this should fail even with old UNIVERSAL::VERSION
	$version += 0.01;
	eval "use lib '.'; use $package $version";
	like($@, qr/$package version $version/,
		'Replacement eval works with incremented version');
	
	$version =~ s/0+$//; #convert to string and remove trailing 0's
	chop($version);	# shorten by 1 digit, should still succeed
	eval "use lib '.'; use $package $version";
	unlike($@, qr/$package version $version/,
		'Replacement eval works with single digit');
	
	# this would fail with old UNIVERSAL::VERSION
	$version += 0.1;
	eval "use lib '.'; use $package $version";
	like($@, qr/$package version $version/,
		'Replacement eval works with incremented digit');
	unlink $filename;
    }

    { # dummy up some variously broken modules for testing
	my ($fh, $filename) = tempfile('tXXXXXXX', SUFFIX => '.pm', UNLINK => 1);
	(my $package = basename($filename)) =~ s/\.pm$//;
	print $fh "1;\n";
	close $fh;

	eval "use lib '.'; use $package 3;";
	if ( $] < 5.008 ) {
	    like($@, qr/$error_regex/,
		'Replacement handles modules without package or VERSION'); 
	}
	else {
	    like($@, qr/defines neither package nor VERSION/,
		'Replacement handles modules without package or VERSION'); 
	}
	eval "use lib '.'; use $package; \$version = $package->VERSION";
	unlike ($@, qr/$error_regex/,
	    'Replacement handles modules without package or VERSION'); 
	ok (!defined($version), "Called as class method");
	unlink $filename;
    }
    
    { # dummy up some variously broken modules for testing
	my ($fh, $filename) = tempfile('tXXXXXXX', SUFFIX => '.pm', UNLINK => 1);
	(my $package = basename($filename)) =~ s/\.pm$//;
	print $fh "package $package;\n#look ma no VERSION\n1;\n";
	close $fh;
	eval "use lib '.'; use $package 3;";
	like ($@, qr/$error_regex/,
	    'Replacement handles modules without VERSION'); 
	eval "use lib '.'; use $package; print $package->VERSION";
	unlike ($@, qr/$error_regex/,
	    'Replacement handles modules without VERSION'); 
	unlink $filename;
    }

    { # dummy up some variously broken modules for testing
	my ($fh, $filename) = tempfile('tXXXXXXX', SUFFIX => '.pm', UNLINK => 1);
	(my $package = basename($filename)) =~ s/\.pm$//;
	print $fh "package $package;\n\@VERSION = ();\n1;\n";
	close $fh;
	eval "use lib '.'; use $package 3;";
	like ($@, qr/$error_regex/,
	    'Replacement handles modules without VERSION'); 
	eval "use lib '.'; use $package; print $package->VERSION";
	unlike ($@, qr/$error_regex/,
	    'Replacement handles modules without VERSION'); 
	unlink $filename;
    }

SKIP: 	{
	skip 'Cannot test bare v-strings with Perl < 5.6.0', 4
		if $] < 5.006_000; 
	diag "Tests with v-strings" unless $ENV{PERL_CORE};
	$version = $CLASS->$method(1.2.3);
	ok("$version" == "v1.2.3", '"$version" == 1.2.3');
	$version = $CLASS->$method(1.0.0);
	$new_version = $CLASS->$method(1);
	ok($version == $new_version, '$version == $new_version');
	skip "version require'd instead of use'd, cannot test declare", 1
	    unless defined $qv_declare;
	$version = &$qv_declare(1.2.3);
	ok("$version" == "v1.2.3", 'v-string initialized $qv_declare()');
    }

    diag "Tests with real-world (malformed) data" unless $ENV{PERL_CORE};

    # trailing zero testing (reported by Andreas Koenig).
    $version = $CLASS->$method("1");
    ok($version->numify eq "1.000", "trailing zeros preserved");
    $version = $CLASS->$method("1.0");
    ok($version->numify eq "1.000", "trailing zeros preserved");
    $version = $CLASS->$method("1.0.0");
    ok($version->numify eq "1.000000", "trailing zeros preserved");
    $version = $CLASS->$method("1.0.0.0");
    ok($version->numify eq "1.000000000", "trailing zeros preserved");
    
    # leading zero testing (reported by Andreas Koenig).
    $version = $CLASS->$method(".7");
    ok($version->numify eq "0.700", "leading zero inferred");

    # leading space testing (reported by Andreas Koenig).
    $version = $CLASS->$method(" 1.7");
    ok($version->numify eq "1.700", "leading space ignored");

    # RT 19517 - deal with undef and 'undef' initialization
    ok("$version" ne 'undef', "Undef version comparison #1");
    ok("$version" ne undef, "Undef version comparison #2");
    $version = $CLASS->$method('undef');
    unlike($warning, qr/^Version string 'undef' contains invalid data/,
	"Version string 'undef'");

    $version = $CLASS->$method(undef);
    like($warning, qr/^Use of uninitialized value/,
	"Version string 'undef'");
    ok($version == 'undef', "Undef version comparison #3");
    ok($version ==  undef,  "Undef version comparison #4");
    eval "\$version = \$CLASS->$method()"; # no parameter at all
    unlike($@, qr/^Bizarre copy of CODE/, "No initializer at all");
    ok($version == 'undef', "Undef version comparison #5");
    ok($version ==  undef,  "Undef version comparison #6");

    $version = $CLASS->$method(0.000001);
    unlike($warning, qr/^Version string '1e-06' contains invalid data/,
    	"Very small version objects");
    }

SKIP: {
	my $warning;
	local $SIG{__WARN__} = sub { $warning = $_[0] };
	# dummy up a legal module for testing RT#19017
	my ($fh, $filename) = tempfile('tXXXXXXX', SUFFIX => '.pm', UNLINK => 1);
	(my $package = basename($filename)) =~ s/\.pm$//;
	print $fh <<"EOF";
package $package;
use $CLASS; \$VERSION = ${CLASS}->new('0.0.4');
1;
EOF
	close $fh;

	eval "use lib '.'; use $package 0.000008;";
	like ($@, qr/^$package version 0.000008 required/,
	    "Make sure very small versions don't freak"); 
	eval "use lib '.'; use $package 1;";
	like ($@, qr/^$package version 1 required/,
	    "Comparing vs. version with no decimal"); 
	eval "use lib '.'; use $package 1.;";
	like ($@, qr/^$package version 1 required/,
	    "Comparing vs. version with decimal only"); 
	if ( $] < 5.006_000 ) {
	    skip 'Cannot "use" extended versions with Perl < 5.6.0', 3; 
	}
	eval "use lib '.'; use $package v0.0.8;";
	my $regex = "^$package version v0.0.8 required";
	like ($@, qr/$regex/, "Make sure very small versions don't freak"); 

	$regex =~ s/8/4/; # set for second test
	eval "use lib '.'; use $package v0.0.4;";
	unlike($@, qr/$regex/, 'Succeed - required == VERSION');
	cmp_ok ( $package->VERSION, 'eq', '0.0.4', 'No undef warnings' );
	unlink $filename;
    }

SKIP: {
    skip 'Cannot test "use base qw(version)"  when require is used', 3
    	unless defined $qv_declare;
    my ($fh, $filename) = tempfile('tXXXXXXX', SUFFIX => '.pm', UNLINK => 1);
    (my $package = basename($filename)) =~ s/\.pm$//;
    print $fh <<"EOF";
package $package;
use base qw(version);
1;
EOF
    close $fh;
    # need to eliminate any other $qv_declare()'s
    undef *{"main\::$qv_declare"};
    ok(!defined(&{"main\::$qv_declare"}), "make sure we cleared $qv_declare() properly");
    eval "use lib '.'; use $package qw/declare qv/;";
    ok(defined(&{"main\::$qv_declare"}), "make sure we exported $qv_declare() properly");
    isa_ok( &$qv_declare(1.2), $package);
    unlink $filename;
}

SKIP: {
	if ( $] < 5.006_000 ) {
	    skip 'Cannot "use" extended versions with Perl < 5.6.0', 3; 
	}
	my ($fh, $filename) = tempfile('tXXXXXXX', SUFFIX => '.pm', UNLINK => 1);
	(my $package = basename($filename)) =~ s/\.pm$//;
	print $fh <<"EOF";
package $package;
\$VERSION = 1.0;
1;
EOF
	close $fh;
	eval "use lib '.'; use $package 1.001;";
	like ($@, qr/^$package version 1.001 required/,
	    "User typed numeric so we error with numeric"); 
	eval "use lib '.'; use $package v1.1.0;";
	like ($@, qr/^$package version v1.1.0 required/,
	    "User typed extended so we error with extended"); 
	unlink $filename;
    }

SKIP: {
	# test locale handling
	my $warning;
	local $SIG{__WARN__} = sub { $warning = $_[0] };

$DB::single = 1;
	my $v = eval { $CLASS->$method('1,7') };
#	is( $@, "", 'Directly test comma as decimal compliance');

	my $ver = 1.23;  # has to be floating point number
	my $orig_loc = setlocale( LC_ALL );
	my $loc;
	while (<DATA>) {
	    chomp;
	    $loc = setlocale( LC_ALL, $_);
	    last if localeconv()->{decimal_point} eq ',';
	}
	skip 'Cannot test locale handling without a comma locale', 4
	    unless ( $loc and ($ver eq '1,23') );

	diag ("Testing locale handling with $loc") unless $ENV{PERL_CORE};

	$v = $CLASS->$method($ver);
	unlike($warning, qr/Version string '1,23' contains invalid data/,
	    "Process locale-dependent floating point");
	is ($v, "1.23", "Locale doesn't apply to version objects");
	ok ($v == $ver, "Comparison to locale floating point");

	setlocale( LC_ALL, $orig_loc); # reset this before possible skip
	skip 'Cannot test RT#46921 with Perl < 5.008', 1
	    if ($] < 5.008);
	skip 'Cannot test RT#46921 with pure Perl module', 1
	    if exists $INC{'version/vpp.pm'};
	my ($fh, $filename) = tempfile('tXXXXXXX', SUFFIX => '.pm', UNLINK => 1);
	(my $package = basename($filename)) =~ s/\.pm$//;
	print $fh <<"EOF";
package $package;
use POSIX qw(locale_h);
\$^W = 1;
use $CLASS;
setlocale (LC_ALL, '$loc');
use $CLASS ;
eval "use Socket 1.7";
setlocale( LC_ALL, '$orig_loc');
1;
EOF
	close $fh;

	eval "use lib '.'; use $package;";
	unlike($warning, qr"Version string '1,7' contains invalid data",
	    'Handle locale action-at-a-distance');
    }

    eval 'my $v = $CLASS->$method("1._1");';
    unlike($@, qr/^Invalid version format \(alpha with zero width\)/,
    	"Invalid version format 1._1");

    {
	my $warning;
	local $SIG{__WARN__} = sub { $warning = $_[0] };
	eval 'my $v = $CLASS->$method(~0);';
	unlike($@, qr/Integer overflow in version/, "Too large version");
	like($warning, qr/Integer overflow in version/, "Too large version");
    }

    {
	# http://rt.cpan.org/Public/Bug/Display.html?id=30004
	my $v1 = $CLASS->$method("v0.1_1");
	(my $alpha1 = Dumper($v1)) =~ s/.+'alpha' => ([^,]+),.+/$1/ms;
	my $v2 = $CLASS->$method($v1);
	(my $alpha2 = Dumper($v2)) =~ s/.+'alpha' => ([^,]+),.+/$1/ms;
	is $alpha2, $alpha1, "Don't fall for Data::Dumper's tricks";
    }

    {
	# http://rt.perl.org/rt3/Ticket/Display.html?id=56606
	my $badv = bless { version => [1,2,3] }, "version";
	is $badv, '1.002003', "Deal with badly serialized versions from YAML";	
	my $badv2 = bless { qv => 1, version => [1,2,3] }, "version";
	is $badv2, 'v1.2.3', "Deal with badly serialized versions from YAML ";	
    }
}

1;

__DATA__
af_ZA
af_ZA.utf8
an_ES
an_ES.utf8
az_AZ.utf8
be_BY
be_BY.utf8
bg_BG
bg_BG.utf8
br_FR
br_FR@euro
br_FR.utf8
bs_BA
bs_BA.utf8
ca_ES
ca_ES@euro
ca_ES.utf8
cs_CZ
cs_CZ.utf8
da_DK
da_DK.utf8
de_AT
de_AT@euro
de_AT.utf8
de_BE
de_BE@euro
de_BE.utf8
de_DE
de_DE@euro
de_DE.utf8
de_LU
de_LU@euro
de_LU.utf8
el_GR
el_GR.utf8
en_DK
en_DK.utf8
es_AR
es_AR.utf8
es_BO
es_BO.utf8
es_CL
es_CL.utf8
es_CO
es_CO.utf8
es_EC
es_EC.utf8
es_ES
es_ES@euro
es_ES.utf8
es_PY
es_PY.utf8
es_UY
es_UY.utf8
es_VE
es_VE.utf8
et_EE
et_EE.iso885915
et_EE.utf8
eu_ES
eu_ES@euro
eu_ES.utf8
fi_FI
fi_FI@euro
fi_FI.utf8
fo_FO
fo_FO.utf8
fr_BE
fr_BE@euro
fr_BE.utf8
fr_CA
fr_CA.utf8
fr_CH
fr_CH.utf8
fr_FR
fr_FR@euro
fr_FR.utf8
fr_LU
fr_LU@euro
fr_LU.utf8
gl_ES
gl_ES@euro
gl_ES.utf8
hr_HR
hr_HR.utf8
hu_HU
hu_HU.utf8
id_ID
id_ID.utf8
is_IS
is_IS.utf8
it_CH
it_CH.utf8
it_IT
it_IT@euro
it_IT.utf8
ka_GE
ka_GE.utf8
kk_KZ
kk_KZ.utf8
kl_GL
kl_GL.utf8
lt_LT
lt_LT.utf8
lv_LV
lv_LV.utf8
mk_MK
mk_MK.utf8
mn_MN
mn_MN.utf8
nb_NO
nb_NO.utf8
nl_BE
nl_BE@euro
nl_BE.utf8
nl_NL
nl_NL@euro
nl_NL.utf8
nn_NO
nn_NO.utf8
no_NO
no_NO.utf8
oc_FR
oc_FR.utf8
pl_PL
pl_PL.utf8
pt_BR
pt_BR.utf8
pt_PT
pt_PT@euro
pt_PT.utf8
ro_RO
ro_RO.utf8
ru_RU
ru_RU.koi8r
ru_RU.utf8
ru_UA
ru_UA.utf8
se_NO
se_NO.utf8
sh_YU
sh_YU.utf8
sk_SK
sk_SK.utf8
sl_SI
sl_SI.utf8
sq_AL
sq_AL.utf8
sr_CS
sr_CS.utf8
sv_FI
sv_FI@euro
sv_FI.utf8
sv_SE
sv_SE.iso885915
sv_SE.utf8
tg_TJ
tg_TJ.utf8
tr_TR
tr_TR.utf8
tt_RU.utf8
uk_UA
uk_UA.utf8
vi_VN
vi_VN.tcvn
wa_BE
wa_BE@euro
wa_BE.utf8

