#!perl -T

BEGIN {
    use Config;
    use Test::More;
    plan skip_all => "POSIX is unavailable" 
        if $Config{'extensions'} !~ m!\bPOSIX\b!;
}
use strict;
use POSIX;
BEGIN {
    plan skip_all => "POSIX::Termios not implemented" 
        if  !eval "POSIX::Termios->new;1"
        and $@=~/not implemented/;
}


my @getters = qw(getcflag getiflag getispeed getlflag getoflag getospeed);

plan tests => 3 + 2 * (3 + NCCS() + @getters);

my $r;

# create a new object
my $termios = eval { POSIX::Termios->new };
is( $@, '', "calling POSIX::Termios->new" );
ok( defined $termios, "\tchecking if the object is defined" );
isa_ok( $termios, "POSIX::Termios", "\tchecking the type of the object" );

# testing getattr()

SKIP: {
    -t STDIN or skip("STDIN not a tty", 2);
    $r = eval { $termios->getattr(0) };
    is( $@, '', "calling getattr(0)" );
    ok( defined $r, "\tchecking if the returned value is defined: $r" );
}

SKIP: {
    -t STDOUT or skip("STDOUT not a tty", 2);
    $r = eval { $termios->getattr(1) };
    is( $@, '', "calling getattr(1)" );
    ok( defined $r, "\tchecking if the returned value is defined: $r" );
}

SKIP: {
    -t STDERR or skip("STDERR not a tty", 2);
    $r = eval { $termios->getattr(2) };
    is( $@, '', "calling getattr(2)" );
    ok( defined $r, "\tchecking if the returned value is defined: $r" );
}

# testing getcc()
for my $i (0..NCCS()-1) {
    $r = eval { $termios->getcc($i) };
    is( $@, '', "calling getcc($i)" );
    ok( defined $r, "\tchecking if the returned value is defined: $r" );
}

# testing getcflag()
for my $method (@getters) {
    $r = eval { $termios->$method() };
    is( $@, '', "calling $method()" );
    ok( defined $r, "\tchecking if the returned value is defined: $r" );
}

