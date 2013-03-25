#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest tests => 18;

blib_load('Module::Build');
blib_load('Module::Build::ConfigData');

use DistGen;


############################## ACTION distmeta works without a MANIFEST file

{
  my $dist = DistGen->new( no_manifest => 1 )->chdir_in->regen;

  ok ! -e 'MANIFEST';

  my $mb;
  stderr_of( sub { $mb = Module::Build->new_from_context } );

  my $out;
  $out = eval { stderr_of(sub{$mb->dispatch('distmeta')}) };
  is $@, '';

  like $out, qr/Nothing to enter for 'provides'/;

  ok -e 'META.yml';

}


############################## Check generation of README file

# TODO: We need to test faking the absence of Pod::Readme when present
#       so Pod::Text will be used. Also fake the absence of both to
#       test that we fail gracefully.

my $provides; # Used a bunch of times below

my $pod_text = <<'---';
=pod

=head1 NAME

Simple - A simple module

=head1 AUTHOR

Simple Simon <simon@simple.sim>

=cut
---

my $dist = DistGen->new->chdir_in;

$dist->change_build_pl
({
    module_name         => $dist->name,
    dist_version        => '3.14159265',
    license             => 'perl',
    create_readme       => 1,
});

# .pm File with pod
#

$dist->change_file( 'lib/Simple.pm', <<'---' . $pod_text);
package Simple;
$VERSION = '1.23';
---
$dist->regen( clean => 1 );
ok( -e "lib/Simple.pm", "Creating Simple.pm" );
my $mb = Module::Build->new_from_context;
$mb->do_create_readme;
like( slurp("README"), qr/NAME/,
    "Generating README from .pm");
is( $mb->dist_author->[0], 'Simple Simon <simon@simple.sim>',
    "Extracting AUTHOR from .pm");
is( $mb->dist_abstract, "A simple module",
    "Extracting abstract from .pm");

# .pm File with pod in separate file
#

$dist->change_file( 'lib/Simple.pm', <<'---');
package Simple;
$VERSION = '1.23';
---
$dist->change_file( 'lib/Simple.pod', $pod_text );
$dist->regen( clean => 1 );

ok( -e "lib/Simple.pm", "Creating Simple.pm" );
ok( -e "lib/Simple.pod", "Creating Simple.pod" );
$mb = Module::Build->new_from_context;
$mb->do_create_readme;
like( slurp("README"), qr/NAME/, "Generating README from .pod");
is( $mb->dist_author->[0], 'Simple Simon <simon@simple.sim>',
    "Extracting AUTHOR from .pod");
is( $mb->dist_abstract, "A simple module",
    "Extracting abstract from .pod");

# .pm File with pod and separate pod file
#

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23';

=pod

=head1 DONT USE THIS FILE FOR POD

=cut
---
$dist->change_file( 'lib/Simple.pod', $pod_text );
$dist->regen( clean => 1 );
ok( -e "lib/Simple.pm", "Creating Simple.pm" );
ok( -e "lib/Simple.pod", "Creating Simple.pod" );
$mb = Module::Build->new_from_context;
$mb->do_create_readme;
like( slurp("README"), qr/NAME/, "Generating README from .pod over .pm");
is( $mb->dist_author->[0], 'Simple Simon <simon@simple.sim>',
    "Extracting AUTHOR from .pod over .pm");
is( $mb->dist_abstract, "A simple module",
    "Extracting abstract from .pod over .pm");

