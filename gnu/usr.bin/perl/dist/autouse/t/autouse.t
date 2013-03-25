#!./perl

BEGIN {
    require Config;
    if ($Config::Config{'extensions'} !~ m!\bList/Util\b!){
	print "1..0 # Skip -- Perl configured without List::Util module\n";
	exit 0;
    }
}

use Test::More tests => 15;

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

# Test that autouse is exempt from all methods of triggering the subroutine
# redefinition warning.
SKIP: {
    skip "Fails in 5.15.5 and below (perl bug)", 2 if $] < 5.0150051;
    use warnings; local $^W = 1; no warnings 'once';
    my $w;
    local $SIG{__WARN__} = sub { $w .= shift };
    use autouse MyTestModule2 => 'test_function2';
    *MyTestModule2::test_function2 = \&test_function2;
    require MyTestModule2;
    is $w, undef,
       'no redefinition warning when clobbering autouse stub with new sub';
    undef $w;
    import MyTestModule2 'test_function2';
    is $w, undef,
       'no redefinition warning when clobbering autouse stub via *a=\&b';
}
SKIP: {
    skip "Fails from 5.10 to 5.15.5 (perl bug)", 1
	if $] < 5.0150051 and $] > 5.0099;
    use Config;
    skip "no B", 1 unless $Config{extensions} =~ /\bB\b/;
    use warnings; local $^W = 1; no warnings 'once';
    my $w;
    local $SIG{__WARN__} = sub { $w .= shift };
    use autouse B => "sv_undef";
    *B::sv_undef = \&sv_undef;
    require B;
    is $w, undef,
      'no redefinition warning when clobbering autouse stub with new XSUB';
}
