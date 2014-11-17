#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest;
use Config;

my $tmp;

blib_load('Module::Build');

{
  my ($have_c_compiler, $tmp_exec) = check_compiler();

  if ( !$have_c_compiler ) {
    plan skip_all => 'No compiler found';
  } elsif ( $^O eq 'VMS' ) {
    plan skip_all => 'Child test output confuses harness';
  } elsif ( !$Config{usedl} ) {
    plan skip_all => 'Perl not compiled for dynamic loading'
  } else {
    plan tests => 22;
  }
  require Cwd;
  $tmp = MBTest->tmpdir( $tmp_exec ? () : (DIR => Cwd::cwd) );
}



#########################

use DistGen;
my $dist = DistGen->new( dir => $tmp, xs => 1 )->chdir_in->regen;

my $mb = $dist->new_from_context;

eval {$mb->dispatch('clean')};
is $@, '';

eval {$mb->dispatch('build')};
is $@, '';

{
  # Make sure it actually works: that we can call methods in the XS module

  # Unfortunately, We must do this is a subprocess because some OS will not
  # release the handle on a dynamic lib until the attaching process terminates

  ok $mb->run_perl_command(['-Mblib', '-M'.$dist->name, '-e1']);

  like stdout_of( sub {$mb->run_perl_command([
       '-Mblib', '-M'.$dist->name,
       '-we', "print @{[$dist->name]}::okay()"])}), qr/ok$/;

  like stdout_of( sub {$mb->run_perl_command([
       '-Mblib', '-M'.$dist->name,
       '-we', "print @{[$dist->name]}::version()"])}), qr/0.01$/;

  like stdout_of( sub {$mb->run_perl_command([
       '-Mblib', '-M'.$dist->name,
       '-we', "print @{[$dist->name]}::xs_version()"])}), qr/0.01$/;

}

{
  # Try again in a subprocess
  eval {$mb->dispatch('clean')};
  is $@, '';


  $mb->create_build_script;
  my $script = $mb->build_script;
  ok -e $script;

  eval {$mb->run_perl_script($script)};
  is $@, '';
}

# We can't be verbose in the sub-test, because Test::Harness will
# think that the output is for the top-level test.
stdout_stderr_of( sub { eval {$mb->dispatch('test')} });
is $@, '';

eval {$mb->dispatch('clean')};
is $@, '';


SKIP: {
  skip( "skipping a Unixish-only tests", 1 )
      unless $mb->is_unixish;

  $mb->{config}->push(ld => "FOO=BAR ".$mb->config('ld'));
  eval {$mb->dispatch('build')};
  is $@, '';
  $mb->{config}->pop('ld');
}

eval {$mb->dispatch('realclean')};
is $@, '';

# Make sure blib/ is gone after 'realclean'
ok ! -e 'blib';

########################################

# Try a XS distro with a deep namespace


$dist->reset( name => 'Simple::With::Deep::Name', dir => $tmp, xs => 1 );
$dist->chdir_in->regen;

$mb = $dist->new_from_context;

eval { $mb->dispatch('build') };
is $@, '';

stdout_stderr_of( sub { eval { $mb->dispatch('test') } } );
is $@, '';

eval { $mb->dispatch('clean') };

eval { $mb->dispatch('build', 'pureperl_only' => 1) };
like $@, qr/\ACan\'t build xs files under --pureperl-only/, 'Can\'t build xs under pureperl';

eval { $mb->dispatch('build', pureperl_only => 1, allow_pureperl => 1) };
is $@, '', 'Can\'t build xs under pureperl, unless allow_pureperl';

eval { $mb->dispatch('realclean') };
is $@, '';

########################################

# Try a XS distro using a flat directory structure
# and a 'dist_name' instead of a 'module_name'

$dist->reset( name => 'Dist-Name', dir => $tmp, xs => 1 )->chdir_in;

$dist->remove_file('lib/Dist-Name.pm');
$dist->remove_file('lib/Dist-Name.xs');

$dist->change_build_pl
  ({
    dist_name         => 'Dist-Name',
    dist_version_from => 'Simple.pm',
    pm_files => { 'Simple.pm' => 'lib/Simple.pm' },
    xs_files => { 'Simple.xs' => 'lib/Simple.xs' },
  });

$dist->add_file('Simple.xs', <<"---");
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

MODULE = Simple         PACKAGE = Simple

SV *
okay()
    CODE:
        RETVAL = newSVpv( "ok", 0 );
    OUTPUT:
        RETVAL
---

$dist->add_file( 'Simple.pm', <<"---" );
package Simple;

\$VERSION = '0.01';

require Exporter;
require DynaLoader;

\@ISA = qw( Exporter DynaLoader );
\@EXPORT_OK = qw( okay );

bootstrap Simple \$VERSION;

1;

__END__

=head1 NAME

Simple - Perl extension for blah blah blah

=head1 DESCRIPTION

Stub documentation for Simple.

=head1 AUTHOR

A. U. Thor, a.u.thor\@a.galaxy.far.far.away

=cut
---
$dist->change_file('t/basic.t', <<"---");
use Test::More tests => 2;
use strict;

use Simple;
ok( 1 );

ok( Simple::okay() eq 'ok' );
---

$dist->regen;

$mb = $dist->new_from_context;

eval { $mb->dispatch('build') };
is $@, '';

stdout_of( sub { eval { $mb->dispatch('test') } } );
is $@, '';

eval { $mb->dispatch('realclean') };
is $@, '';

