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
	if ($^O =~ /beos/i) {
		plan tests => 2;
	} else {
		plan skip_all => 'This is not BeOS';
	}
}

use Config;
use File::Spec;
use File::Basename;

# tels - Taken from MM_Win32.t - I must not understand why this works, right?
# Does this mimic ExtUtils::MakeMaker ok?
{
    @MM::ISA = qw(
        ExtUtils::MM_Unix 
        ExtUtils::Liblist::Kid 
        ExtUtils::MakeMaker
    );
    # MM package faked up by messy MI entanglement
    package MM;
    sub DESTROY {}
}

require_ok( 'ExtUtils::MM_BeOS' );

# perl_archive()
{
    my $libperl = $Config{libperl} || 'libperl.a';
    is( MM->perl_archive(), File::Spec->catfile('$(PERL_INC)', $libperl ),
	    'perl_archive() should respect libperl setting' );
}
