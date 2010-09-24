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

# get a Module::Build object and test with it
my $mb;
stdout_stderr_of( sub { $mb = $dist->new_from_context('verbose' => 1) } );
isa_ok( $mb, "Module::Build" );
like( $mb->cpan_client, qr/^\Q$^X\E/, "cpan_client is mocked with perl" );

my $out = stdout_of( sub {
    $dist->run_build('installdeps')
});
ok( length($out), "ran mocked Build installdeps");
my $expected = quotemeta(Module::Build->find_command($^X));
like( $out, qr/$expected/i, "relative cpan_client resolved relative to \$^X" );
like( $out, qr/File::Spec/, "saw File::Spec prereq" );
like( $out, qr/Getopt::Long/, "saw Getopt::Long prereq" );

$out = stdout_stderr_of( sub {
    $dist->run_build('installdeps', '--cpan_client', 'ADLKASJDFLASDJ')
});
like( $out, qr/cpan_client .* is not executable/,
  "Build installdeps with bad cpan_client dies"
);

# vim:ts=2:sw=2:et:sta:sts=2
