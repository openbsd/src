#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest tests => 51;

blib_load('Module::Build');
blib_load('Module::Build::ConfigData');

my $tmp = MBTest->tmpdir;

my %metadata =
  (
   module_name   => 'Simple',
   dist_version  => '3.14159265',
   dist_author   => [ 'Simple Simon <ss\@somewhere.priv>' ],
   dist_abstract => 'Something interesting',
   license       => 'perl',
   meta_add => {
		keywords  => [qw(super duper something)],
		resources => {homepage => 'http://foo.example.com'},
	       },
  );


use DistGen;
my $dist = DistGen->new( dir => $tmp );
$dist->change_build_pl( \%metadata );
$dist->regen;

my $simple_file = 'lib/Simple.pm';
my $simple2_file = 'lib/Simple2.pm';

   # Traditional VMS will return the file in in lower case, and is_deeply
   # does exact case comparisons.
   # When ODS-5 support is active for preserved case file names we do not
   # change the case.
   if ($^O eq 'VMS') {
       my $lower_case_expect = 1;
       my $vms_efs_case = 0;
       if (eval 'require VMS::Feature') {
           $vms_efs_case = VMS::Feature::current("efs_case_preserve");
       } else {
           my $efs_case = $ENV{'DECC$EFS_CASE_PRESERVE'} || '';
           $vms_efs_case = $efs_case =~ /^[ET1]/i;
       }
       $lower_case_expect = 0 if $vms_efs_case;
       if ($lower_case_expect) {
           $simple_file = lc($simple_file);
           $simple2_file = lc($simple2_file);
       }
   }


$dist->chdir_in;

my $mb = Module::Build->new_from_context;

##################################################
#
# Test for valid META.yml

{
  my $mb_prereq = { 'Module::Build' => $Module::Build::VERSION };
  my $mb_config_req = {
    'Module::Build' => int($Module::Build::VERSION * 100)/100
  };
  my $node = $mb->get_metadata( );

  # exists() doesn't seem to work here
  is $node->{name}, $metadata{module_name};
  is $node->{version}, $metadata{dist_version};
  is $node->{abstract}, $metadata{dist_abstract};
  is_deeply $node->{author}, $metadata{dist_author};
  is $node->{license}, $metadata{license};
  is_deeply $node->{configure_requires}, $mb_config_req, 'Add M::B to configure_requires';
  like $node->{generated_by}, qr{Module::Build};
  ok defined( $node->{'meta-spec'}{version} ),
      "'meta-spec' -> 'version' field present in META.yml";
  ok defined( $node->{'meta-spec'}{url} ),
      "'meta-spec' -> 'url' field present in META.yml";
  is_deeply $node->{keywords}, $metadata{meta_add}{keywords};
  is_deeply $node->{resources}, $metadata{meta_add}{resources};
}

{
  my $mb_prereq = { 'Module::Build' => 0 };
  $mb->configure_requires( $mb_prereq );
  my $node = $mb->get_metadata( );


  # exists() doesn't seem to work here
  is_deeply $node->{configure_requires}, $mb_prereq, 'Add M::B to configure_requires';
}

$dist->clean;


##################################################
#
# Tests to ensure that the correct packages and versions are
# recorded for the 'provides' field of META.yml

my $provides; # Used a bunch of times below

sub new_build { return Module::Build->new_from_context( quiet => 1, @_ ) }

############################## Single Module

# File with corresponding package (w/ or w/o version)
# Simple.pm => Simple v1.23

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23';
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Simple' => {file => $simple_file,
			version => '1.23'}});

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Simple' => {file => $simple_file}});

# File with no corresponding package (w/ or w/o version)
# Simple.pm => Foo::Bar v1.23

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Foo::Bar;
$VERSION = '1.23';
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Foo::Bar' => { file => $simple_file,
			   version => '1.23' }});

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Foo::Bar;
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Foo::Bar' => { file => $simple_file}});


# Single file with multiple differing packages (w/ or w/o version)
# Simple.pm => Simple
# Simple.pm => Foo::Bar

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23';
package Foo::Bar;
$VERSION = '1.23';
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Simple'   => { file => $simple_file,
			   version => '1.23' },
	   'Foo::Bar' => { file => $simple_file,
			   version => '1.23' }});

{
  $dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = version->new('0.60.' . (qw$Revision: 128 $)[1]);
package Simple::Simon;
$VERSION = version->new('0.61.' . (qw$Revision: 129 $)[1]);
---
  $dist->regen;
  my $provides = new_build()->get_metadata()->{provides};
  is $provides->{'Simple'}{version}, 'v0.60.128', "Check version";
  is $provides->{'Simple::Simon'}{version}, 'v0.61.129', "Check version";
  is ref($provides->{'Simple'}{version}), '', "Versions from get_metadata() aren't refs";
  is ref($provides->{'Simple::Simon'}{version}), '', "Versions from get_metadata() aren't refs";
}


# Single file with multiple differing packages, no corresponding package
# Simple.pm => Foo
# Simple.pm => Foo::Bar

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Foo;
$VERSION = '1.23';
package Foo::Bar;
$VERSION = '1.23';
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Foo'      => { file => $simple_file,
			   version => '1.23' },
	   'Foo::Bar' => { file => $simple_file,
			   version => '1.23' }});


# Single file with same package appearing multiple times, no version
#   only record a single instance
# Simple.pm => Simple
# Simple.pm => Simple

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
package Simple;
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Simple' => { file => $simple_file }});


# Single file with same package appearing multiple times, single
# version 1st package:
# Simple.pm => Simple v1.23
# Simple.pm => Simple

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23';
package Simple;
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Simple' => { file => $simple_file,
			 version => '1.23' }});


# Single file with same package appearing multiple times, single
# version 2nd package
# Simple.pm => Simple
# Simple.pm => Simple v1.23

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
package Simple;
$VERSION = '1.23';
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Simple' => { file => $simple_file,
			 version => '1.23' }});


# Single file with same package appearing multiple times, conflicting versions
# Simple.pm => Simple v1.23
# Simple.pm => Simple v2.34

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23';
package Simple;
$VERSION = '2.34';
---
$dist->regen( clean => 1 );
my $err = '';
$err = stderr_of( sub { $mb = new_build() } );
$err = stderr_of( sub { $provides = $mb->find_dist_packages } );
is_deeply($provides,
	  {'Simple' => { file => $simple_file,
			 version => '1.23' }}); # XXX should be 2.34?
like( $err, qr/already declared/, '  with conflicting versions reported' );


# (Same as above three cases except with no corresponding package)
# Simple.pm => Foo v1.23
# Simple.pm => Foo v2.34

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Foo;
$VERSION = '1.23';
package Foo;
$VERSION = '2.34';
---
$dist->regen( clean => 1 );
$err = stderr_of( sub { $mb = new_build() } );
$err = stderr_of( sub { $provides = $mb->find_dist_packages } );
is_deeply($provides,
	  {'Foo' => { file => $simple_file,
		      version => '1.23' }}); # XXX should be 2.34?
like( $err, qr/already declared/, '  with conflicting versions reported' );



############################## Multiple Modules

# Multiple files with same package, no version
# Simple.pm  => Simple
# Simple2.pm => Simple

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
---
$dist->add_file( 'lib/Simple2.pm', <<'---' );
package Simple;
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Simple' => { file => $simple_file }});
$dist->remove_file( 'lib/Simple2.pm' );


# Multiple files with same package, single version in corresponding package
# Simple.pm  => Simple v1.23
# Simple2.pm => Simple

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23';
---
$dist->add_file( 'lib/Simple2.pm', <<'---' );
package Simple;
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Simple' => { file => $simple_file,
			 version => '1.23' }});
$dist->remove_file( 'lib/Simple2.pm' );


# Multiple files with same package,
#   single version in non-corresponding package
# Simple.pm  => Simple
# Simple2.pm => Simple v1.23

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
---
$dist->add_file( 'lib/Simple2.pm', <<'---' );
package Simple;
$VERSION = '1.23';
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Simple' => { file => $simple2_file,
			 version => '1.23' }});
$dist->remove_file( 'lib/Simple2.pm' );


# Multiple files with same package, conflicting versions
# Simple.pm  => Simple v1.23
# Simple2.pm => Simple v2.34

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23';
---
$dist->add_file( 'lib/Simple2.pm', <<'---' );
package Simple;
$VERSION = '2.34';
---
$dist->regen( clean => 1 );
stderr_of( sub { $mb = new_build(); } );
$err = stderr_of( sub { $provides = $mb->find_dist_packages } );
is_deeply($provides,
	  {'Simple' => { file => $simple_file,
			 version => '1.23' }});
like( $err, qr/Found conflicting versions for package/,
      '  with conflicting versions reported' );
$dist->remove_file( 'lib/Simple2.pm' );


# Multiple files with same package, multiple agreeing versions
# Simple.pm  => Simple v1.23
# Simple2.pm => Simple v1.23

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23';
---
$dist->add_file( 'lib/Simple2.pm', <<'---' );
package Simple;
$VERSION = '1.23';
---
$dist->regen( clean => 1 );
$mb = new_build();
$err = stderr_of( sub { $provides = $mb->find_dist_packages } );
is_deeply($provides,
	  {'Simple' => { file => $simple_file,
			 version => '1.23' }});
$dist->remove_file( 'lib/Simple2.pm' );


############################################################
#
# (Same as above five cases except with non-corresponding package)
#

# Multiple files with same package, no version
# Simple.pm  => Foo
# Simple2.pm => Foo

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Foo;
---
$dist->add_file( 'lib/Simple2.pm', <<'---' );
package Foo;
---
$dist->regen( clean => 1 );
$mb = new_build();
$provides = $mb->find_dist_packages;
ok( exists( $provides->{Foo} ) ); # it exist, can't predict which file
$dist->remove_file( 'lib/Simple2.pm' );


# Multiple files with same package, version in first file
# Simple.pm  => Foo v1.23
# Simple2.pm => Foo

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Foo;
$VERSION = '1.23';
---
$dist->add_file( 'lib/Simple2.pm', <<'---' );
package Foo;
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Foo' => { file => $simple_file,
		      version => '1.23' }});
$dist->remove_file( 'lib/Simple2.pm' );


# Multiple files with same package, version in second file
# Simple.pm  => Foo
# Simple2.pm => Foo v1.23

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Foo;
---
$dist->add_file( 'lib/Simple2.pm', <<'---' );
package Foo;
$VERSION = '1.23';
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Foo' => { file => $simple2_file,
		      version => '1.23' }});
$dist->remove_file( 'lib/Simple2.pm' );


# Multiple files with same package, conflicting versions
# Simple.pm  => Foo v1.23
# Simple2.pm => Foo v2.34

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Foo;
$VERSION = '1.23';
---
$dist->add_file( 'lib/Simple2.pm', <<'---' );
package Foo;
$VERSION = '2.34';
---
$dist->regen( clean => 1 );
stderr_of( sub { $mb = new_build(); } );
$err = stderr_of( sub { $provides = $mb->find_dist_packages } );
# XXX Should 'Foo' exist ??? Can't predict values for file & version
ok( exists( $provides->{Foo} ) );
like( $err, qr/Found conflicting versions for package/,
      '  with conflicting versions reported' );
$dist->remove_file( 'lib/Simple2.pm' );


# Multiple files with same package, multiple agreeing versions
# Simple.pm  => Foo v1.23
# Simple2.pm => Foo v1.23

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Foo;
$VERSION = '1.23';
---
$dist->add_file( 'lib/Simple2.pm', <<'---' );
package Foo;
$VERSION = '1.23';
---
$dist->regen( clean => 1 );
$mb = new_build();
$err = stderr_of( sub { $provides = $mb->find_dist_packages } );
ok( exists( $provides->{Foo} ) );
is( $provides->{Foo}{version}, '1.23' );
ok( exists( $provides->{Foo}{file} ) ); # Can't predict which file
is( $err, '', '  no conflicts reported' );
$dist->remove_file( 'lib/Simple2.pm' );

############################################################
# Conflicts among primary & multiple alternatives

# multiple files, conflicting version in corresponding file
$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23';
---
$dist->add_file( 'lib/Simple2.pm', <<'---' );
package Simple;
$VERSION = '2.34';
---
$dist->add_file( 'lib/Simple3.pm', <<'---' );
package Simple;
$VERSION = '2.34';
---
$dist->regen( clean => 1 );
$err = stderr_of( sub {
  $mb = new_build();
} );
$err = stderr_of( sub { $provides = $mb->find_dist_packages } );
is_deeply($provides,
	  {'Simple' => { file => $simple_file,
			 version => '1.23' }});
like( $err, qr/Found conflicting versions for package/,
      '  corresponding package conflicts with multiple alternatives' );
$dist->remove_file( 'lib/Simple2.pm' );
$dist->remove_file( 'lib/Simple3.pm' );

# multiple files, conflicting version in non-corresponding file
$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23';
---
$dist->add_file( 'lib/Simple2.pm', <<'---' );
package Simple;
$VERSION = '1.23';
---
$dist->add_file( 'lib/Simple3.pm', <<'---' );
package Simple;
$VERSION = '2.34';
---
$dist->regen( clean => 1 );
$err = stderr_of( sub {
  $mb = new_build();
} );
$err = stderr_of( sub { $provides = $mb->find_dist_packages } );
is_deeply($provides,
	  {'Simple' => { file => $simple_file,
			 version => '1.23' }});
like( $err, qr/Found conflicting versions for package/,
      '  only one alternative conflicts with corresponding package' );
$dist->remove_file( 'lib/Simple2.pm' );
$dist->remove_file( 'lib/Simple3.pm' );


############################################################
# Don't record private packages (beginning with underscore)
# Simple.pm => Simple::_private
# Simple.pm => Simple::_private::too

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23';
package Simple::_private;
$VERSION = '2.34';
package Simple::_private::too;
$VERSION = '3.45';
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages,
	  {'Simple' => { file => $simple_file,
			 version => '1.23' }});


############################################################
# Files with no packages?

# Simple.pm => <empty>

$dist->change_file( 'lib/Simple.pm', '' );
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply( $mb->find_dist_packages, {} );

# Simple.pm => =pod..=cut (no package declaration)
$dist->change_file( 'lib/Simple.pm', <<'---' );
=pod

=head1 NAME

Simple - Pure Documentation

=head1 DESCRIPTION

Doesn't do anything.

=cut
---
$dist->regen( clean => 1 );
$mb = new_build();
is_deeply($mb->find_dist_packages, {});

