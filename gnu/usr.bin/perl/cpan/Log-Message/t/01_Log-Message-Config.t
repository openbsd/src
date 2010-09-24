### Log::Message::Config test suite ###
BEGIN { 
    if( $ENV{PERL_CORE} ) {
        chdir '../lib/Log/Message' if -d '../lib/Log/Message';
        unshift @INC, '../../..';
    }
} 

BEGIN { chdir 't' if -d 't' }

use strict;
use lib qw[../lib conf];
use Test::More tests => 6;
use File::Spec;
use File::Basename qw[dirname];

use_ok( 'Log::Message::Config'    ) or diag "Config.pm not found.  Dying", die;
use_ok( 'Log::Message'            ) or diag "Module.pm not found.  Dying", die;

{
    my $default = {
        private => undef,
        verbose => 1,
        tag     => 'NONE',
        level   => 'log',
        remove  => 0,
        chrono  => 1,
    };

    my $log = Log::Message->new();

    is_deeply( $default, $log->{CONFIG}, q[Config creation from default] );
}

{
    my $config = {
        private => 1,
        verbose => 1,
        tag     => 'TAG',
        level   => 'carp',
        remove  => 0,
        chrono  => 1,
    };

    my $log = Log::Message->new( %$config );

    is_deeply( $config, $log->{CONFIG}, q[Config creation from options] );
}

{
    my $file = {
        private => 1,
        verbose => 0,
        tag     => 'SOME TAG',
        level   => 'carp',
        remove  => 1,
        chrono  => 0,
    };

    my $log = Log::Message->new(
                    config  => File::Spec->catfile( qw|conf config_file| )
                );

    is_deeply( $file, $log->{CONFIG}, q[Config creation from file] );
}

{

    my $mixed = {
        private => 1,
        verbose => 0,
        remove  => 1,
        chrono  => 0,
        tag     => 'MIXED',
        level   => 'die',
    };
    my $log = Log::Message->new(
                    config  => File::Spec->catfile( qw|conf config_file| ),
                    tag     => 'MIXED',
                    level   => 'die',
                );
    is_deeply( $mixed, $log->{CONFIG}, q[Config creation from file & options] );
}
           
