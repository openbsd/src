#!/usr/bin/perl -w

# Test ~ expansion from command line arguments.

use strict;
use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
use MBTest tests => 15;

use Cwd ();
my $cwd = Cwd::cwd;
my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new( dir => $tmp );
$dist->regen;

chdir( $dist->dirname ) or die "Can't chdir to '@{[$dist->dirname]}': $!";


use Module::Build;

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
    skip "Needs case and syntax tweaks for VMS", 14 if $^O eq 'VMS';
    unless (defined $home) {
      my @info = eval { getpwuid $> };
      skip "No home directory for tilde-expansion tests", 14 if $@;
      $home = $info[7];
    }

    is( run_sample( $p => '~'     )->$p(),  $home );

    is( run_sample( $p => '~/foo' )->$p(),  "$home/foo" );

    is( run_sample( $p => '~~'    )->$p(),  '~~' );

    is( run_sample( $p => '~ foo' )->$p(),  '~ foo' );

    is( run_sample( $p => '~/ foo')->$p(),  "$home/ foo" );
      
    is( run_sample( $p => '~/fo o')->$p(),  "$home/fo o" );

    is( run_sample( $p => 'foo~'  )->$p(),  'foo~' );

    is( run_sample( prefix => '~' )->prefix,
	$home );

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
}

# Again, with named users
SKIP: {
    skip "Needs case and syntax tweaks for VMS", 1 if $^O eq 'VMS';
    my @info = eval { getpwuid $> };
    skip "No home directory for tilde-expansion tests", 1 if $@;
    my ($me, $home) = @info[0,7];
    
    is( run_sample( $p => "~$me/foo")->$p(),  "$home/foo" );
}


# cleanup
chdir( $cwd ) or die "Can''t chdir to '$cwd': $!";
$dist->remove;

use File::Path;
rmtree( $tmp );
