#!./perl

print "1..9\n";

# test various operations on @_

my $ord = 0;
sub new1 { bless \@_ }
{
    my $x = new1("x");
    my $y = new1("y");
    ++$ord;
    print "# got [@$y], expected [y]\nnot " unless "@$y" eq "y";
    print "ok $ord\n";
    ++$ord;
    print "# got [@$x], expected [x]\nnot " unless "@$x" eq "x";
    print "ok $ord\n";
}

sub new2 { splice @_, 0, 0, "a", "b", "c"; return \@_ }
{
    my $x = new2("x");
    my $y = new2("y");
    ++$ord;
    print "# got [@$x], expected [a b c x]\nnot " unless "@$x" eq "a b c x";
    print "ok $ord\n";
    ++$ord;
    print "# got [@$y], expected [a b c y]\nnot " unless "@$y" eq "a b c y";
    print "ok $ord\n";
}

sub new3 { goto &new1 }
{
    my $x = new3("x");
    my $y = new3("y");
    ++$ord;
    print "# got [@$y], expected [y]\nnot " unless "@$y" eq "y";
    print "ok $ord\n";
    ++$ord;
    print "# got [@$x], expected [x]\nnot " unless "@$x" eq "x";
    print "ok $ord\n";
}

sub new4 { goto &new2 }
{
    my $x = new4("x");
    my $y = new4("y");
    ++$ord;
    print "# got [@$x], expected [a b c x]\nnot " unless "@$x" eq "a b c x";
    print "ok $ord\n";
    ++$ord;
    print "# got [@$y], expected [a b c y]\nnot " unless "@$y" eq "a b c y";
    print "ok $ord\n";
}

# see if POPSUB gets to see the right pad across a dounwind() with
# a reified @_

sub methimpl {
    my $refarg = \@_;
    die( "got: @_\n" );
}

sub method {
    &methimpl;
}

sub try {
    eval { method('foo', 'bar'); };
    print "# $@" if $@;
}

for (1..5) { try() }
++$ord;
print "ok $ord\n";

# These tests disabled because the change #19064 was retracted.
# http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2003-08/msg01485.html
if (0) {
# bug #21542 local $_[0] causes reify problems and coredumps

sub local1 { local $_[0] }
my $foo = 'foo'; local1($foo); local1($foo);
print "got [$foo], expected [foo]\nnot " if $foo ne 'foo';
$ord++;
print "ok $ord\n";

sub local2 { local $_[0]; last L }
L: { local2 }
$ord++;
print "ok $ord\n";
}
