#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir 't';

use Test::More;
use ExtUtils::MakeMaker;

my $Has_Version = eval 'require version; "version"->import; 1';

my %versions = (q[$VERSION = '1.00']            => '1.00',
                q[*VERSION = \'1.01']           => '1.01',
                q[($VERSION) = q$Revision: 1.4 $ =~ /(\d+)/g;] => 32208,
                q[$FOO::VERSION = '1.10';]      => '1.10',
                q[*FOO::VERSION = \'1.11';]     => '1.11',
                '$VERSION = 0.02'               => 0.02,
                '$VERSION = 0.0'                => 0.0,
                '$VERSION = -1.0'               => -1.0,
                '$VERSION = undef'              => 'undef',
                '$wibble  = 1.0'                => 'undef',
                q[my $VERSION = '1.01']         => 'undef',
                q[local $VERISON = '1.02']      => 'undef',
                q[local $FOO::VERSION = '1.30'] => 'undef',
                q[if( $Foo::VERSION >= 3.00 ) {]=> 'undef',
                q[our $VERSION = '1.23';]       => '1.23',

                '$Something::VERSION == 1.0'    => 'undef',
                '$Something::VERSION <= 1.0'    => 'undef',
                '$Something::VERSION >= 1.0'    => 'undef',
                '$Something::VERSION != 1.0'    => 'undef',

                qq[\$Something::VERSION == 1.0\n\$VERSION = 2.3\n]                     => '2.3',
                qq[\$Something::VERSION == 1.0\n\$VERSION = 2.3\n\$VERSION = 4.5\n]    => '2.3',

                '$VERSION = sprintf("%d.%03d", q$Revision: 1.4 $ =~ /(\d+)\.(\d+)/);' => '3.074',
                '$VERSION = substr(q$Revision: 1.4 $, 10) + 2 . "";'                   => '4.8',
               );

if( $Has_Version ) {
    $versions{q[use version; $VERSION = qv("1.2.3");]} = qv("1.2.3");
    $versions{q[$VERSION = qv("1.2.3")]}               = qv("1.2.3");
}

plan tests => (2 * keys %versions) + 4;

while( my($code, $expect) = each %versions ) {
    is( parse_version_string($code), $expect, $code );
}


sub parse_version_string {
    my $code = shift;

    open(FILE, ">VERSION.tmp") || die $!;
    print FILE "$code\n";
    close FILE;

    $_ = 'foo';
    my $version = MM->parse_version('VERSION.tmp');
    is( $_, 'foo', '$_ not leaked by parse_version' );
    
    unlink "VERSION.tmp";
    
    return $version;
}


# This is a specific test to see if a version subroutine in the $VERSION
# declaration confuses later calls to the version class.
# [rt.cpan.org 30747]
SKIP: {
    skip "need version.pm", 4 unless $Has_Version;
    is parse_version_string(q[ $VERSION = '1.00'; sub version { $VERSION } ]),
       '1.00';
    is parse_version_string(q[ use version; $VERSION = version->new("1.2.3") ]),
       qv("1.2.3");
}
