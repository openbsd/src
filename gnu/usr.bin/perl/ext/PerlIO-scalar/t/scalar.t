#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    unless (find PerlIO::Layer 'perlio') {
	print "1..0 # Skip: not perlio\n";
	exit 0;
    }
    require Config;
    if (($Config::Config{'extensions'} !~ m!\bPerlIO/scalar\b!) ){
        print "1..0 # Skip -- Perl configured without PerlIO::scalar module\n";
        exit 0;
    }
}

use Fcntl qw(SEEK_SET SEEK_CUR SEEK_END); # Not 0, 1, 2 everywhere.

$| = 1;

use Test::More tests => 55;

my $fh;
my $var = "aaa\n";
ok(open($fh,"+<",\$var));

is(<$fh>, $var);

ok(eof($fh));

ok(seek($fh,0,SEEK_SET));
ok(!eof($fh));

ok(print $fh "bbb\n");
is($var, "bbb\n");
$var = "foo\nbar\n";
ok(seek($fh,0,SEEK_SET));
ok(!eof($fh));
is(<$fh>, "foo\n");
ok(close $fh, $!);

# Test that semantics are similar to normal file-based I/O
# Check that ">" clobbers the scalar
$var = "Something";
open $fh, ">", \$var;
is($var, "");
#  Check that file offset set to beginning of scalar
my $off = tell($fh);
is($off, 0);
# Check that writes go where they should and update the offset
$var = "Something";
print $fh "Brea";
$off = tell($fh);
is($off, 4);
is($var, "Breathing");
close $fh;

# Check that ">>" appends to the scalar
$var = "Something ";
open $fh, ">>", \$var;
$off = tell($fh);
is($off, 10);
is($var, "Something ");
#  Check that further writes go to the very end of the scalar
$var .= "else ";
is($var, "Something else ");

$off = tell($fh);
is($off, 10);

print $fh "is here";
is($var, "Something else is here");
close $fh;

# Check that updates to the scalar from elsewhere do not
# cause problems
$var = "line one\nline two\line three\n";
open $fh, "<", \$var;
while (<$fh>) {
    $var = "foo";
}
close $fh;
is($var, "foo");

# Check that dup'ing the handle works

$var = '';
open $fh, "+>", \$var;
print $fh "xxx\n";
open $dup,'+<&',$fh;
print $dup "yyy\n";
seek($dup,0,SEEK_SET);
is(<$dup>, "xxx\n");
is(<$dup>, "yyy\n");
close($fh);
close($dup);

open $fh, '<', \42;
is(<$fh>, "42", "reading from non-string scalars");
close $fh;

{ package P; sub TIESCALAR {bless{}} sub FETCH { "shazam" } }
tie $p, P; open $fh, '<', \$p;
is(<$fh>, "shazam", "reading from magic scalars");

{
    use warnings;
    my $warn = 0;
    local $SIG{__WARN__} = sub { $warn++ };
    open my $fh, '>', \my $scalar;
    print $fh "foo";
    close $fh;
    is($warn, 0, "no warnings when writing to an undefined scalar");
}

{
    use warnings;
    my $warn = 0;
    local $SIG{__WARN__} = sub { $warn++ };
    for (1..2) {
        open my $fh, '>', \my $scalar;
        close $fh;
    }
    is($warn, 0, "no warnings when reusing a lexical");
}

{
    use warnings;
    my $warn = 0;
    local $SIG{__WARN__} = sub { $warn++ };

    my $fetch = 0;
    {
        package MgUndef;
        sub TIESCALAR { bless [] }
        sub FETCH { $fetch++; return undef }
    }
    tie my $scalar, MgUndef;

    open my $fh, '<', \$scalar;
    close $fh;
    is($warn, 0, "no warnings reading a magical undef scalar");
    is($fetch, 1, "FETCH only called once");
}

{
    use warnings;
    my $warn = 0;
    local $SIG{__WARN__} = sub { $warn++ };
    my $scalar = 3;
    undef $scalar;
    open my $fh, '<', \$scalar;
    close $fh;
    is($warn, 0, "no warnings reading an undef, allocated scalar");
}

my $data = "a non-empty PV";
$data = undef;
open(MEM, '<', \$data) or die "Fail: $!\n";
my $x = join '', <MEM>;
is($x, '');

{
    # [perl #35929] verify that works with $/ (i.e. test PerlIOScalar_unread)
    my $s = <<'EOF';
line A
line B
a third line
EOF
    open(F, '<', \$s) or die "Could not open string as a file";
    local $/ = "";
    my $ln = <F>;
    close F;
    is($ln, $s, "[perl #35929]");
}

# [perl #40267] PerlIO::scalar doesn't respect readonly-ness
{
    ok(!(defined open(F, '>', \undef)), "[perl #40267] - $!");
    close F;

    my $ro = \43;
    ok(!(defined open(F, '>', $ro)), $!);
    close F;
    # but we can read from it
    ok(open(F, '<', $ro), $!);
    is(<F>, 43);
    close F;
}

{
    # Check that we zero fill when needed when seeking,
    # and that seeking negative off the string does not do bad things.

    my $foo;

    ok(open(F, '>', \$foo));

    # Seeking forward should zero fill.

    ok(seek(F, 50, SEEK_SET));
    print F "x";
    is(length($foo), 51);
    like($foo, qr/^\0{50}x$/);

    is(tell(F), 51);
    ok(seek(F, 0, SEEK_SET));
    is(length($foo), 51);

    # Seeking forward again should zero fill but only the new bytes.

    ok(seek(F, 100, SEEK_SET));
    print F "y";
    is(length($foo), 101);
    like($foo, qr/^\0{50}x\0{49}y$/);
    is(tell(F), 101);

    # Seeking back and writing should not zero fill.

    ok(seek(F, 75, SEEK_SET));
    print F "z";
    is(length($foo), 101);
    like($foo, qr/^\0{50}x\0{24}z\0{24}y$/);
    is(tell(F), 76);

    # Seeking negative should not do funny business.

    ok(!seek(F,  -50, SEEK_SET), $!);
    ok(seek(F, 0, SEEK_SET));
    ok(!seek(F,  -50, SEEK_CUR), $!);
    ok(!seek(F, -150, SEEK_END), $!);
}

