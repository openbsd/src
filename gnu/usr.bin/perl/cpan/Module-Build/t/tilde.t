#!/usr/bin/perl -w

# Test ~ expansion from command line arguments.

use strict;
use lib 't/lib';
use MBTest tests => 16;

blib_load('Module::Build');

my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new( dir => $tmp );
$dist->regen;

$dist->chdir_in;


sub run_sample {
    my @args = @_;

    local $Test::Builder::Level = $Test::Builder::Level + 1;

    $dist->clean;

    my $mb;
    stdout_of( sub {
      $mb = Module::Build->new_from_context( @args );
    } );

    return $mb;
}


my $p = 'install_base';

SKIP: {
    my $home = $ENV{HOME} ? $ENV{HOME} : undef;

    if ($^O eq 'VMS') {
        # Convert the path to UNIX format, trim off the trailing slash
        $home = VMS::Filespec::unixify($home);
        $home =~ s#/$##;
    }

    unless (defined $home) {
      my @info = eval { getpwuid $> };
      skip "No home directory for tilde-expansion tests", 15 if $@
        or !defined $info[7];
      $home = $info[7];
    }

    is( run_sample( $p => '~'     )->$p(),  $home );

    is( run_sample( $p => '~/fooxzy' )->$p(),  "$home/fooxzy" );

    is( run_sample( $p => '~/ fooxzy')->$p(),  "$home/ fooxzy" );

    is( run_sample( $p => '~/fo o')->$p(),  "$home/fo o" );

    is( run_sample( $p => 'fooxzy~'  )->$p(),  'fooxzy~' );

    is( run_sample( prefix => '~' )->prefix,
	$home );

    # Test when HOME is different from getpwuid(), as in sudo.
    {
        local $ENV{HOME} = '/wibble/whomp';

        is( run_sample( $p => '~' )->$p(),    "/wibble/whomp" );
    }

    my $mb = run_sample( install_path => { html => '~/html',
					   lib  => '~/lib'   }
		       );
    is( $mb->install_destination('lib'),  "$home/lib" );
    # 'html' is translated to 'binhtml' & 'libhtml'
    is( $mb->install_destination('binhtml'), "$home/html" );
    is( $mb->install_destination('libhtml'), "$home/html" );

    $mb = run_sample( install_path => { lib => '~/lib' } );
    is( $mb->install_destination('lib'),  "$home/lib" );

    $mb = run_sample( destdir => '~' );
    is( $mb->destdir,           $home );

    $mb->$p('~');
    is( $mb->$p(),      '~', 'API does not expand tildes' );

    skip "On OS/2 EMX all users are equal", 2 if $^O eq 'os2';
    is( run_sample( $p => '~~'    )->$p(),  '~~' );
    is( run_sample( $p => '~ fooxzy' )->$p(),  '~ fooxzy' );
}

# Again, with named users
SKIP: {
    my @info = eval { getpwuid $> };
    skip "No home directory for tilde-expansion tests", 1 if $@
        or !defined $info[7] or !defined $info[0];
    my ($me, $home) = @info[0,7];

    if ($^O eq 'VMS') {
        # Convert the path to UNIX format and trim off the trailing slash.
        # Also, the fake module we're in has mangled $ENV{HOME} for its own
        # purposes; getpwuid doesn't know about that but _detildefy does.
        $home = VMS::Filespec::unixify($ENV{HOME});
        $home =~ s#/$##;
    }
    my $expected = "$home/fooxzy";

    like( run_sample( $p => "~$me/fooxzy")->$p(),  qr(\Q$expected\E)i );
}

