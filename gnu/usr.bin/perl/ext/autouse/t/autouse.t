#!./perl

BEGIN {
    require Config;
    if ($Config::Config{'extensions'} !~ m!\bList/Util\b!){
	print "1..0 # Skip -- Perl configured without List::Util module\n";
	exit 0;
    }
}

use Test::More tests => 12;

BEGIN {
    require autouse;
    eval {
        "autouse"->import('List::Util' => 'List::Util::first(&@)');
    };
    ok( !$@ );

    eval {
        "autouse"->import('List::Util' => 'Foo::min');
    };
    ok( $@, qr/^autouse into different package attempted/ );

    "autouse"->import('List::Util' => qw(max first(&@)));
}

my @a = (1,2,3,4,5.5);
is( max(@a), 5.5);


# first() has a prototype of &@.  Make sure that's preserved.
is( (first { $_ > 3 } @a), 4);


# Example from the docs.
use autouse 'Carp' => qw(carp croak);

{
    my @warning;
    local $SIG{__WARN__} = sub { push @warning, @_ };
    carp "this carp was predeclared and autoused\n";
    is( scalar @warning, 1 );
    like( $warning[0], qr/^this carp was predeclared and autoused\n/ );

    eval { croak "It is but a scratch!" };
    like( $@, qr/^It is but a scratch!/);
}


# Test that autouse's lazy module loading works.
use autouse 'Errno' => qw(EPERM);

my $mod_file = 'Errno.pm';   # just fine and portable for %INC
ok( !exists $INC{$mod_file} );
ok( EPERM ); # test if non-zero
ok( exists $INC{$mod_file} );

use autouse Env => "something";
eval { something() };
like( $@, qr/^\Qautoused module Env has unique import() method/ );

# Check that UNIVERSAL.pm doesn't interfere with modules that don't use
# Exporter and have no import() of their own.
require UNIVERSAL;
require File::Spec;
unshift @INC, File::Spec->catdir('t', 'lib'), 'lib';
autouse->import("MyTestModule" => 'test_function');
my $ret = test_function();
is( $ret, 'works' );

