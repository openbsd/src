#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

require './test.pl';
plan( tests => 23 );

# test various operations on @_

sub new1 { bless \@_ }
{
    my $x = new1("x");
    my $y = new1("y");
    is("@$y","y");
    is("@$x","x");
}

sub new2 { splice @_, 0, 0, "a", "b", "c"; return \@_ }
{
    my $x = new2("x");
    my $y = new2("y");
    is("@$x","a b c x");
    is("@$y","a b c y");
}

sub new3 { goto &new1 }
{
    my $x = new3("x");
    my $y = new3("y");
    is("@$y","y");
    is("@$x","x");
}

sub new4 { goto &new2 }
{
    my $x = new4("x");
    my $y = new4("y");
    is("@$x","a b c x");
    is("@$y","a b c y");
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
pass();

# bug #21542 local $_[0] causes reify problems and coredumps

sub local1 { local $_[0] }
my $foo = 'foo'; local1($foo); local1($foo);
print "got [$foo], expected [foo]\nnot " if $foo ne 'foo';
pass();

sub local2 { local $_[0]; last L }
L: { local2 }
pass();

# the following test for local(@_) used to be in t/op/nothr5005.t (because it
# failed with 5005threads)

$|=1;

sub foo { local(@_) = ('p', 'q', 'r'); }
sub bar { unshift @_, 'D'; @_ }
sub baz { push @_, 'E'; return @_ }
for (1..3) { 
    is(join('',foo('a', 'b', 'c')),'pqr');
    is(join('',bar('d')),'Dd');
    is(join('',baz('e')),'eE');
} 

# [perl #28032] delete $_[0] was freeing things too early

{
    my $flag = 0;
    sub X::DESTROY { $flag = 1 }
    sub f {
	delete $_[0];
	ok(!$flag, 'delete $_[0] : in f');
    }
    {
	my $x = bless [], 'X';
	f($x);
	ok(!$flag, 'delete $_[0] : after f');
    }
    ok($flag, 'delete $_[0] : outside block');
}

	
