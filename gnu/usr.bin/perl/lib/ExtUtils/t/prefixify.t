#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use Test::More;

if( $^O eq 'VMS' ) {
    plan skip_all => 'prefixify works differently on VMS';
}
else {
    plan tests => 3;
}
use Config;
use File::Spec;
use ExtUtils::MM;

my $mm = bless {}, 'MM';

my $default = File::Spec->catdir(qw(this that));

$mm->prefixify('installbin', 'wibble', 'something', $default);
is( $mm->{INSTALLBIN}, $Config{installbin},
                                            'prefixify w/defaults');

$mm->{ARGS}{PREFIX} = 'foo';
$mm->prefixify('installbin', 'wibble', 'something', $default);
is( $mm->{INSTALLBIN}, File::Spec->catdir('something', $default),
                                            'prefixify w/defaults and PREFIX');

{
    undef *ExtUtils::MM_Unix::Config;
    $ExtUtils::MM_Unix::Config{wibble} = 'C:\opt\perl\wibble';
    $mm->prefixify('wibble', 'C:\opt\perl', 'C:\yarrow');

    is( $mm->{WIBBLE}, 'C:\yarrow\wibble',  'prefixify Win32 paths' );
    { package ExtUtils::MM_Unix;  Config->import }
}
