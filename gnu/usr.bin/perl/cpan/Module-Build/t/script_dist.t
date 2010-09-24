#!/usr/bin/perl -w
# -*- mode: cperl; tab-width: 8; indent-tabs-mode: nil; basic-offset: 2 -*-
# vim:ts=8:sw=2:et:sta:sts=2

use strict;
use lib 't/lib';
use MBTest 'no_plan';

use DistGen qw(undent);

blib_load('Module::Build');
blib_load('Module::Build::ConfigData');

# XXX DistGen shouldn't be assuming module-ness?
my $dist = DistGen->new(dir => MBTest->tmpdir);
$dist->add_file('bin/foo', undent(<<'  ---'));
  #!/usr/bin/perl

  package bin::foo;
  $VERSION = 0.01;

  =head1 NAME

  foo - does stuff

  =head1 AUTHOR

  A. U. Thor, a.u.thor@a.galaxy.far.far.away

  =cut

  print "hello world\n";
  ---

my %details = (
  dist_name => 'bin-foo',
  dist_version_from => 'bin/foo',
  dist_author => ['A. U. Thor, a.u.thor@a.galaxy.far.far.away'],
  dist_version => '0.01',
);
my %meta_provides = (
  'bin-foo' => {
    file => 'bin/foo',
    version => '0.01',
  }
);
$dist->change_build_pl({
  # TODO need to get all of this data out of the program itself
  ! $ENV{EXTRA_TEST} ? (
    %details, meta_merge => { provides => \%meta_provides, },
  ) : (),
  program_name        => 'bin/foo',
  license             => 'perl',
});

# hmm... the old assumption of what a dist looks like is wrong here
$dist->remove_file('lib/Simple.pm'); $dist->regen;

$dist->chdir_in;
rmdir('lib');

#system('konsole');
my $mb = Module::Build->new_from_context;
ok($mb);
is($mb->program_name, 'bin/foo');
is($mb->license, 'perl');
is($mb->dist_name, 'bin-foo');
is($mb->dist_version, '0.01');
is_deeply($mb->dist_author,
  ['A. U. Thor, a.u.thor@a.galaxy.far.far.away']);
ok $mb->dispatch('distmeta');

SKIP: {
  skip( 'YAML_support feature is not enabled', 1 )
      unless Module::Build::ConfigData->feature('YAML_support');
  require YAML::Tiny;
  my $yml = YAML::Tiny::LoadFile('META.yml');
  is_deeply($yml->{provides}, \%meta_provides);
}
$dist->chdir_original if $dist->did_chdir;
