#!/usr/bin/perl

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = '../lib';
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir 't';

use Test::More;

BEGIN {
	if ($^O =~ /cygwin/i) {
		plan tests => 13;
	} else {
		plan skip_all => "This is not cygwin";
	}
}

use Config;
use File::Spec;
use ExtUtils::MM;

use_ok( 'ExtUtils::MM_Cygwin' );

# test canonpath
my $path = File::Spec->canonpath('/a/../../c');
is( MM->canonpath('/a/../../c'), $path,
	'canonpath() method should work just like the one in File::Spec' );

# test cflags, with the fake package below
my $args = bless({
	CFLAGS	=> 'fakeflags',
	CCFLAGS	=> '',
}, MM);

# with CFLAGS set, it should be returned
is( $args->cflags(), 'fakeflags',
	'cflags() should return CFLAGS member data, if set' );

delete $args->{CFLAGS};

# ExtUtils::MM_Cygwin::cflags() calls this, fake the output
{
    local $SIG{__WARN__} = sub { 
        # no warnings 'redefine';
        warn @_ unless $_[0] =~ /^Subroutine .* redefined/;
    };
    sub ExtUtils::MM_Unix::cflags { return $_[1] };
}

# respects the config setting, should ignore whitespace around equal sign
my $ccflags = $Config{useshrplib} eq 'true' ? ' -DUSEIMPORTLIB' : '';
{
    local $args->{NEEDS_LINKING} = 1;
    $args->cflags(<<FLAGS);
OPTIMIZE = opt
PERLTYPE  =pt
FLAGS
}

like( $args->{CFLAGS}, qr/OPTIMIZE = opt/, '... should set OPTIMIZE' );
like( $args->{CFLAGS}, qr/PERLTYPE = pt/, '... should set PERLTYPE' );
like( $args->{CFLAGS}, qr/CCFLAGS = $ccflags/, '... should set CCFLAGS' );

# test manifypods
$args = bless({
	NOECHO => 'noecho',
	MAN3PODS => {},
	MAN1PODS => {},
    MAKEFILE => 'Makefile',
}, 'MM');
like( $args->manifypods(), qr/pure_all\n\tnoecho/,
	'manifypods() should return without PODS values set' );

$args->{MAN3PODS} = { foo => 1 };
my $out = tie *STDOUT, 'FakeOut';
{
    local $SIG{__WARN__} = sub {
        # no warnings 'redefine';
        warn @_ unless $_[0] =~ /used only once/;
    };
    no warnings 'once';
    local *MM::perl_script = sub { return };
    my $res = $args->manifypods();
    like( $$out, qr/could not locate your pod2man/,
          '... should warn if pod2man cannot be located' );
    like( $res, qr/POD2MAN_EXE = -S pod2man/,
          '... should use default pod2man target' );
    like( $res, qr/pure_all.+foo/, '... should add MAN3PODS targets' );
}

SKIP: {
    skip "Only relevent in the core", 2 unless $ENV{PERL_CORE};
    $args->{PERL_SRC} = File::Spec->updir;
    $args->{MAN1PODS} = { bar => 1 };
    $$out = '';
    $res = $args->manifypods();
    is( $$out, '', '... should not warn if PERL_SRC provided' );
    like( $res, qr/bar \\\n\t1 \\\n\tfoo/,
          '... should join MAN1PODS and MAN3PODS');
}

# test perl_archive
my $libperl = $Config{libperl} || 'libperl.a';
$libperl =~ s/\.a/.dll.a/;
is( $args->perl_archive(), "\$(PERL_INC)/$libperl",
	'perl_archive() should respect libperl setting' );


package FakeOut;

sub TIEHANDLE {
	bless(\(my $scalar), $_[0]);
}

sub PRINT {
	my $self = shift;
	$$self .= shift;
}
