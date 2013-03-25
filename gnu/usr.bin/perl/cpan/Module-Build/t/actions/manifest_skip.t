use strict;
use lib 't/lib';
use MBTest;
use DistGen;

plan tests => 7;

# Ensure any Module::Build modules are loaded from correct directory
blib_load('Module::Build');

# create dist object in a temp directory
# enter the directory and generate the skeleton files
my $dist = DistGen->new->chdir_in;
$dist->change_build_pl(
  module_name => $dist->name,
  requires => {
    'File::Spec' => 9999,
  },
  build_requires => {
    'Getopt::Long' => 9998,
  },
  cpan_client => $^X . ' -le print($_)for($^X,@ARGV)',
)->regen;

ok( ! -e 'MANIFEST.SKIP', "MANIFEST.SKIP doesn't exist at start" );

# get a Module::Build object and test with it
my $mb;
stdout_stderr_of( sub { $mb = $dist->new_from_context('verbose' => 1) } );
isa_ok( $mb, "Module::Build" );

my ($out, $err) = stdout_stderr_of( sub {
    $dist->run_build('manifest_skip')
});
ok( -e 'MANIFEST.SKIP', "'Build manifest_skip' creates MANIFEST.SKIP" );
like( $out, qr/Creating a new MANIFEST.SKIP file/, "Saw creation message");

# shouldn't overwrite
my $old_mtime = -M 'MANIFEST.SKIP';
($out, $err) = stdout_stderr_of( sub {
    $dist->run_build('manifest_skip')
});
like( $err, qr/MANIFEST.SKIP already exists/, 
  "Running it again warns about pre-existing MANIFEST.SKIP"
);
is( -M 'MANIFEST.SKIP', $old_mtime, "File does not appear modified" );

# cleanup
($out, $err) = stdout_stderr_of( sub {
    $dist->run_build('distclean')
});
ok( -e 'MANIFEST.SKIP', "MANIFEST.SKIP still exists after distclean" );

# vim:ts=2:sw=2:et:sta:sts=2
