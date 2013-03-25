#!./perl

# There are few filetest operators that are portable enough to test.
# See pod/perlport.pod for details.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use Config;
plan(tests => 47 + 27*14);

ok( -d 'op' );
ok( -f 'TEST' );
ok( !-f 'op' );
ok( !-d 'TEST' );
ok( -r 'TEST' );

# Make a read only file
my $ro_file = tempfile();

{
    open my $fh, '>', $ro_file or die "open $fh: $!";
    close $fh or die "close $fh: $!";
}

chmod 0555, $ro_file or die "chmod 0555, '$ro_file' failed: $!";

$oldeuid = $>;		# root can read and write anything
eval '$> = 1';		# so switch uid (may not be implemented)

print "# oldeuid = $oldeuid, euid = $>\n";

SKIP: {
    if (!$Config{d_seteuid}) {
	skip('no seteuid');
    } 
    else {
	ok( !-w $ro_file );
    }
}

# Scripts are not -x everywhere so cannot test that.

eval '$> = $oldeuid';	# switch uid back (may not be implemented)

# this would fail for the euid 1
# (unless we have unpacked the source code as uid 1...)
ok( -r 'op' );

# this would fail for the euid 1
# (unless we have unpacked the source code as uid 1...)
SKIP: {
    if ($Config{d_seteuid}) {
	ok( -w 'op' );
    } else {
	skip('no seteuid');
    }
}

ok( -x 'op' ); # Hohum.  Are directories -x everywhere?

is( "@{[grep -r, qw(foo io noo op zoo)]}", "io op" );

# Test stackability of filetest operators

ok( defined( -f -d 'TEST' ) && ! -f -d _ );
ok( !defined( -e 'zoo' ) );
ok( !defined( -e -d 'zoo' ) );
ok( !defined( -f -e 'zoo' ) );
ok( -f -e 'TEST' );
ok( -e -f 'TEST' );
ok( defined(-d -e 'TEST') );
ok( defined(-e -d 'TEST') );
ok( ! -f -d 'op' );
ok( -x -d -x 'op' );
ok( (-s -f 'TEST' > 1), "-s returns real size" );
ok( -f -s 'TEST' == 1 );

# now with an empty file
my $tempfile = tempfile();
open my $fh, ">", $tempfile;
close $fh;
ok( -f $tempfile );
is( -s $tempfile, 0 );
is( -f -s $tempfile, 0 );
is( -s -f $tempfile, 0 );
unlink_all $tempfile;

# stacked -l
eval { -l -e "TEST" };
like $@, qr/^The stat preceding -l _ wasn't an lstat at /,
  'stacked -l non-lstat error with warnings off';
{
 local $^W = 1;
 eval { -l -e "TEST" };
 like $@, qr/^The stat preceding -l _ wasn't an lstat at /,
  'stacked -l non-lstat error with warnings on';
}
# Make sure -l is using the previous stat buffer, and not using the previ-
# ous op’s return value as a file name.
SKIP: {
 use Perl::OSType 'os_type';
 if (os_type ne 'Unix') { skip "Not Unix", 2 }
 if (-l "TEST") { skip "TEST is a symlink", 2 }
 chomp(my $ln = `which ln`);
 if ( ! -e $ln ) { skip "No ln"   , 2 }
 lstat "TEST";
 `ln -s TEST 1`;
 ok ! -l -e _, 'stacked -l uses previous stat, not previous retval';
 unlink 1;

 # Since we already have our skip block set up, we might as well put this
 # test here, too:
 # -l always treats a non-bareword argument as a file name
 system qw "ln -s TEST", \*foo;
 local $^W = 1;
 ok -l \*foo, '-l \*foo is a file name';
 unlink \*foo;
}

# test that _ is a bareword after filetest operators

-f 'TEST';
ok( -f _ );
sub _ { "this is not a file name" }
ok( -f _ );

my $over;
{
    package OverFtest;

    use overload 
	fallback => 1,
        -X => sub { 
            $over = [qq($_[0]), $_[1]];
            "-$_[1]"; 
        };
}
{
    package OverString;

    # No fallback. -X should fall back to string overload even without
    # it.
    use overload q/""/ => sub { $over = 1; "TEST" };
}
{
    package OverBoth;

    use overload
        q/""/   => sub { "TEST" },
        -X      => sub { "-$_[1]" };
}
{
    package OverNeither;

    # Need fallback. Previous versions of perl required 'fallback' to do
    # -X operations on an object with no "" overload.
    use overload 
        '+' => sub { 1 },
        fallback => 1;
}

my $ft = bless [], "OverFtest";
my $ftstr = qq($ft);
my $str = bless [], "OverString";
my $both = bless [], "OverBoth";
my $neither = bless [], "OverNeither";
my $nstr = qq($neither);

open my $gv, "<", "TEST";
bless $gv, "OverString";
open my $io, "<", "TEST";
$io = *{$io}{IO};
bless $io, "OverString";

my $fcntl_not_available;
eval { require Fcntl } or $fcntl_not_available = 1;

for my $op (split //, "rwxoRWXOezsfdlpSbctugkTMBAC") {
    $over = [];
    ok( my $rv = eval "-$op \$ft",  "overloaded -$op succeeds" )
        or diag( $@ );
    is( $over->[0], $ftstr,         "correct object for overloaded -$op" );
    is( $over->[1], $op,            "correct op for overloaded -$op" );
    is( $rv,        "-$op",         "correct return value for overloaded -$op");

    my ($exp, $is) = (1, "is");
    if (
	!$fcntl_not_available and (
        $op eq "u" and not eval { Fcntl::S_ISUID() } or
        $op eq "g" and not eval { Fcntl::S_ISGID() } or
        $op eq "k" and not eval { Fcntl::S_ISVTX() }
	)
    ) {
        ($exp, $is) = (0, "not");
    }

    $over = 0;
    $rv = eval "-$op \$str";
    ok( !$@,                        "-$op succeeds with string overloading" )
        or diag( $@ );
    is( $rv, eval "-$op 'TEST'",    "correct -$op on string overload" );
    is( $over,      $exp,           "string overload $is called for -$op" );

    ($exp, $is) = $op eq "l" ? (1, "is") : (0, "not");

    $over = 0;
    eval "-$op \$gv";
    is( $over,      $exp,   "string overload $is called for -$op on GLOB" );

    # IO refs always get string overload called. This might be a bug.
    $op eq "t" || $op eq "T" || $op eq "B"
        and ($exp, $is) = (1, "is");

    $over = 0;
    eval "-$op \$io";
    is( $over,      $exp,   "string overload $is called for -$op on IO");

    $rv = eval "-$op \$both";
    is( $rv,        "-$op",         "correct -$op on string/-X overload" );

    $rv = eval "-$op \$neither";
    ok( !$@,                        "-$op succeeds with random overloading" )
        or diag( $@ );
    is( $rv, eval "-$op \$nstr",    "correct -$op with random overloading" );

    is( eval "-r -$op \$ft", "-r",      "stacked overloaded -$op" );
    is( eval "-$op -r \$ft", "-$op",    "overloaded stacked -$op" );
}

# -l stack corruption: this bug occurred from 5.8 to 5.14
{
 push my @foo, "bar", -l baz;
 is $foo[0], "bar", '-l bareword does not corrupt the stack';
}

# -l and fatal warnings
stat "test.pl";
eval { use warnings FATAL => io; -l cradd };
ok !stat _,
  'fatal warnings do not prevent -l HANDLE from setting stat status';

# File test ops should not call get-magic on the topmost SV on the stack if
# it belongs to another op.
{
  my $w;
  sub oon::TIESCALAR{bless[],'oon'}
  sub oon::FETCH{$w++}
  tie my $t, 'oon';
  push my @a, $t, -t;
  is $w, 1, 'file test does not call FETCH on stack item not its own';
}

# -T and -B

my $Perl = which_perl();

SKIP: {
    skip "no -T on filehandles", 8 unless eval { -T STDERR; 1 };

    # Test that -T HANDLE sets the last stat type
    -l "perl.c";   # last stat type is now lstat
    -T STDERR;     # should set it to stat, since -T does a stat
    eval { -l _ }; # should die, because the last stat type is not lstat
    like $@, qr/^The stat preceding -l _ wasn't an lstat at /,
	'-T HANDLE sets the stat type';

    # statgv should be cleared when freed
    fresh_perl_is
	'open my $fh, "test.pl"; -r $fh; undef $fh; open my $fh2, '
	. "q\0$Perl\0; print -B _",
	'',
	{ switches => ['-l'] },
	'PL_statgv should not point to freed-and-reused SV';

    # or coerced into a non-glob
    fresh_perl_is
	'open Fh, "test.pl"; -r($h{i} = *Fh); $h{i} = 3; undef %h;'
	. 'open my $fh2, ' . "q\0" . which_perl() . "\0; print -B _",
	'',
	{ switches => ['-l'] },
	'PL_statgv should not point to coerced-freed-and-reused GV';

    # -T _ should work after stat $ioref
    open my $fh, 'test.pl';
    stat $Perl; # a binary file
    stat *$fh{IO};
    ok -T _, '-T _ works after stat $ioref';

    # and after -r $ioref
    -r *$fh{IO};
    ok -T _, '-T _ works after -r $ioref';

    # -T _ on closed filehandle should still reset stat info
    stat $fh;
    close $fh;
    -T _;
    ok !stat _, '-T _ on closed filehandle resets stat info';

    lstat "test.pl";
    -T $fh; # closed
    eval { lstat _ };
    like $@, qr/^The stat preceding lstat\(\) wasn't an lstat at /,
	'-T on closed handle resets last stat type';

    # Fatal warnings should not affect the setting of errno.
    $! = 7;
    -T cradd;
    my $errno = $!;
    $! = 7;
    eval { use warnings FATAL => unopened; -T cradd };
    my $errno2 = $!;
    is $errno2, $errno,
	'fatal warnings do not affect errno after -T BADHADNLE';
}

is runperl(prog => '-T _', switches => ['-w'], stderr => 1), "",
  'no uninit warnings from -T with no preceding stat';

SKIP: {
    my $rand_file_name = 'filetest-' . rand =~ y/.//dr;
    if (-e $rand_file_name) { skip "File $rand_file_name exists", 1 }
    stat 'test.pl';
    -T $rand_file_name;
    ok !stat _, '-T "nonexistent" resets stat success status';
}

# Unsuccessful filetests on filehandles should leave stat buffers in the
# same state whether fatal warnings are on or off.
{
    stat "test.pl";
    # This GV has no IO
    -r *phlon;
    my $failed_stat1 = stat _;

    stat "test.pl";
    eval { use warnings FATAL => unopened; -r *phlon };
    my $failed_stat2 = stat _;

    is $failed_stat2, $failed_stat1,
	'failed -r($gv_without_io) with and w/out fatal warnings';

    stat "test.pl";
    -r cength;  # at compile time autovivifies IO, but with no fp
    $failed_stat1 = stat _;

    stat "test.pl";
    eval { use warnings FATAL => unopened; -r cength };
    $failed_stat2 = stat _;
    
    is $failed_stat2, $failed_stat1,
	'failed -r($gv_with_io_but_no_fp) with and w/out fatal warnings';
} 
