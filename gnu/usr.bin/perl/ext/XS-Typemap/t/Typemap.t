BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bXS\/Typemap\b/) {
        print "1..0 # Skip: XS::Typemap was not built\n";
        exit 0;
    }
}

use Test;
BEGIN { plan tests => 84 }

use strict;
use warnings;
use XS::Typemap;

ok(1);

# Some inheritance trees to check ISA relationships
BEGIN {
  package intObjPtr::SubClass;
  use base qw/ intObjPtr /;
  sub xxx { 1; }
}

BEGIN {
  package intRefIvPtr::SubClass;
  use base qw/ intRefIvPtr /;
  sub xxx { 1 }
}

# T_SV - standard perl scalar value
print "# T_SV\n";

my $sv = "Testing T_SV";
ok( T_SV($sv), $sv);

# T_SVREF - reference to Scalar
print "# T_SVREF\n";

$sv .= "REF";
my $svref = \$sv;
ok( T_SVREF($svref), $svref );

# Now test that a non reference is rejected
# the typemaps croak
eval { T_SVREF( "fail - not ref" ) };
ok( $@ );

# T_AVREF - reference to a perl Array
print "# T_AVREF\n";

my @array;
ok( T_AVREF(\@array), \@array);

# Now test that a non array ref is rejected
eval { T_AVREF( \$sv ) };
ok( $@ );

# T_HVREF - reference to a perl Hash
print "# T_HVREF\n";

my %hash;
ok( T_HVREF(\%hash), \%hash);

# Now test that a non hash ref is rejected
eval { T_HVREF( \@array ) };
ok( $@ );


# T_CVREF - reference to perl subroutine
print "# T_CVREF\n";
my $sub = sub { 1 };
ok( T_CVREF($sub), $sub );

# Now test that a non code ref is rejected
eval { T_CVREF( \@array ) };
ok( $@ );

# T_SYSRET - system return values
print "# T_SYSRET\n";

# first check success
ok( T_SYSRET_pass );

# ... now failure
ok( T_SYSRET_fail, undef);

# T_UV - unsigned integer
print "# T_UV\n";

ok( T_UV(5), 5 );    # pass
ok( T_UV(-4) != -4); # fail

# T_IV - signed integer
print "# T_IV\n";

ok( T_IV(5), 5);
ok( T_IV(-4), -4);
ok( T_IV(4.1), int(4.1));
ok( T_IV("52"), "52");
ok( T_IV(4.5) != 4.5); # failure


# Skip T_INT

# T_ENUM - enum list
print "# T_ENUM\n";

ok( T_ENUM() ); # just hope for a true value

# T_BOOL - boolean
print "# T_BOOL\n";

ok( T_BOOL(52) );
ok( ! T_BOOL(0) );
ok( ! T_BOOL('') );
ok( ! T_BOOL(undef) );

# Skip T_U_INT

# Skip T_SHORT

# T_U_SHORT aka U16

print "# T_U_SHORT\n";

ok( T_U_SHORT(32000), 32000);
if ($Config{shortsize} == 2) {
  ok( T_U_SHORT(65536) != 65536); # probably dont want to test edge cases
} else {
  ok(1); # e.g. Crays have shortsize 4 (T3X) or 8 (CXX and SVX)
}

# T_U_LONG aka U32

print "# T_U_LONG\n";

ok( T_U_LONG(65536), 65536);
ok( T_U_LONG(-1) != -1);

# T_CHAR

print "# T_CHAR\n";

ok( T_CHAR("a"), "a");
ok( T_CHAR("-"), "-");
ok( T_CHAR(chr(128)),chr(128));
ok( T_CHAR(chr(256)) ne chr(256));

# T_U_CHAR

print "# T_U_CHAR\n";

ok( T_U_CHAR(127), 127);
ok( T_U_CHAR(128), 128);
ok( T_U_CHAR(-1) != -1);
ok( T_U_CHAR(300) != 300);

# T_FLOAT
print "# T_FLOAT\n";

# limited precision
ok( sprintf("%6.3f",T_FLOAT(52.345)), sprintf("%6.3f",52.345));

# T_NV
print "# T_NV\n";

ok( T_NV(52.345), 52.345);

# T_DOUBLE
print "# T_DOUBLE\n";

ok( sprintf("%6.3f",T_DOUBLE(52.345)), sprintf("%6.3f",52.345));

# T_PV
print "# T_PV\n";

ok( T_PV("a string"), "a string");
ok( T_PV(52), 52);

# T_PTR
print "# T_PTR\n";

my $t = 5;
my $ptr = T_PTR_OUT($t);
ok( T_PTR_IN( $ptr ), $t );

# T_PTRREF
print "# T_PTRREF\n";

$t = -52;
$ptr = T_PTRREF_OUT( $t );
ok( ref($ptr), "SCALAR");
ok( T_PTRREF_IN( $ptr ), $t );

# test that a non-scalar ref is rejected
eval { T_PTRREF_IN( $t ); };
ok( $@ );

# T_PTROBJ
print "# T_PTROBJ\n";

$t = 256;
$ptr = T_PTROBJ_OUT( $t );
ok( ref($ptr), "intObjPtr");
ok( $ptr->T_PTROBJ_IN, $t );

# check that normal scalar refs fail
eval {intObjPtr::T_PTROBJ_IN( \$t );};
ok( $@ );

# check that inheritance works
bless $ptr, "intObjPtr::SubClass";
ok( ref($ptr), "intObjPtr::SubClass");
ok( $ptr->T_PTROBJ_IN, $t );

# Skip T_REF_IV_REF

# T_REF_IV_PTR
print "# T_REF_IV_PTR\n";

$t = -365;
$ptr = T_REF_IV_PTR_OUT( $t );
ok( ref($ptr), "intRefIvPtr");
ok( $ptr->T_REF_IV_PTR_IN(), $t);

# inheritance should not work
bless $ptr, "intRefIvPtr::SubClass";
eval { $ptr->T_REF_IV_PTR_IN };
ok( $@ );

# Skip T_PTRDESC

# Skip T_REFREF

# Skip T_REFOBJ

# T_OPAQUEPTR
print "# T_OPAQUEPTR\n";

$t = 22;
my $p = T_OPAQUEPTR_IN( $t );
ok( T_OPAQUEPTR_OUT($p), $t);

# T_OPAQUEPTR with a struct
print "# T_OPAQUEPTR with a struct\n";

my @test = (5,6,7);
$p = T_OPAQUEPTR_IN_struct(@test);
my @result = T_OPAQUEPTR_OUT_struct($p);
ok(scalar(@result),scalar(@test));
for (0..$#test) {
  ok($result[$_], $test[$_]);
}

# T_OPAQUE
print "# T_OPAQUE\n";

$t = 48;
$p = T_OPAQUE_IN( $t );
ok(T_OPAQUEPTR_OUT_short( $p ), $t); # Test using T_OPAQUEPTR
ok(T_OPAQUE_OUT( $p ), $t );         # Test using T_OPQAQUE

# T_OPAQUE_array
print "# A packed  array\n";

my @opq = (2,4,8);
my $packed = T_OPAQUE_array(@opq);
my @uopq = unpack("i*",$packed);
ok(scalar(@uopq), scalar(@opq));
for (0..$#opq) {
  ok( $uopq[$_], $opq[$_]);
}

# Skip T_PACKED

# Skip T_PACKEDARRAY

# Skip T_DATAUNIT

# Skip T_CALLBACK

# T_ARRAY
print "# T_ARRAY\n";
my @inarr = (1,2,3,4,5,6,7,8,9,10);
my @outarr = T_ARRAY( 5, @inarr );
ok(scalar(@outarr), scalar(@inarr));

for (0..$#inarr) {
  ok($outarr[$_], $inarr[$_]);
}



# T_STDIO
print "# T_STDIO\n";

# open a file in XS for write
my $testfile= "stdio.tmp";
my $fh = T_STDIO_open( $testfile );
ok( $fh );

# write to it using perl
if (defined $fh) {

  my @lines = ("NormalSTDIO\n", "PerlIO\n");

  # print to it using FILE* through XS
  ok( T_STDIO_print($fh, $lines[0]), length($lines[0]));

  # print to it using normal perl
  ok(print $fh "$lines[1]");

  # close it using XS if using perlio, using Perl otherwise
  ok( $Config{useperlio} ? T_STDIO_close( $fh ) : close( $fh ) );

  # open from perl, and check contents
  open($fh, "< $testfile");
  ok($fh);
  my $line = <$fh>;
  ok($line,$lines[0]);
  $line = <$fh>;
  ok($line,$lines[1]);

  ok(close($fh));
  ok(unlink($testfile));

} else {
  for (1..8) {
    skip("Skip Test not relevant since file was not opened correctly",0);
  }
}

