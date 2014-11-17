#!/usr/bin/perl -w
# -*- mode: cperl; tab-width: 8; indent-tabs-mode: nil; basic-offset: 2 -*-
# vim:ts=8:sw=2:et:sta:sts=2

use strict;
use warnings;
use lib 't/lib';
use IO::File;
use MBTest;

my $undef;

# parse various module $VERSION lines
# these will be reversed later to create %modules
my @modules = (
  $undef => <<'---', # no $VERSION line
package Simple;
---
  $undef => <<'---', # undefined $VERSION
package Simple;
our $VERSION;
---
  '1.23' => <<'---', # declared & defined on same line with 'our'
package Simple;
our $VERSION = '1.23';
---
  '1.23' => <<'---', # declared & defined on separate lines with 'our'
package Simple;
our $VERSION;
$VERSION = '1.23';
---
  '1.23' => <<'---', # commented & defined on same line
package Simple;
our $VERSION = '1.23'; # our $VERSION = '4.56';
---
  '1.23' => <<'---', # commented & defined on separate lines
package Simple;
# our $VERSION = '4.56';
our $VERSION = '1.23';
---
  '1.23' => <<'---', # use vars
package Simple;
use vars qw( $VERSION );
$VERSION = '1.23';
---
  '1.23' => <<'---', # choose the right default package based on package/file name
package Simple::_private;
$VERSION = '0';
package Simple;
$VERSION = '1.23'; # this should be chosen for version
---
  '1.23' => <<'---', # just read the first $VERSION line
package Simple;
$VERSION = '1.23'; # we should see this line
$VERSION = eval $VERSION; # and ignore this one
---
  '1.23' => <<'---', # just read the first $VERSION line in reopened package (1)
package Simple;
$VERSION = '1.23';
package Error::Simple;
$VERSION = '2.34';
package Simple;
---
  '1.23' => <<'---', # just read the first $VERSION line in reopened package (2)
package Simple;
package Error::Simple;
$VERSION = '2.34';
package Simple;
$VERSION = '1.23';
---
  '1.23' => <<'---', # mentions another module's $VERSION
package Simple;
$VERSION = '1.23';
if ( $Other::VERSION ) {
    # whatever
}
---
  '1.23' => <<'---', # mentions another module's $VERSION in a different package
package Simple;
$VERSION = '1.23';
package Simple2;
if ( $Simple::VERSION ) {
    # whatever
}
---
  '1.23' => <<'---', # $VERSION checked only in assignments, not regexp ops
package Simple;
$VERSION = '1.23';
if ( $VERSION =~ /1\.23/ ) {
    # whatever
}
---
  '1.23' => <<'---', # $VERSION checked only in assignments, not relational ops
package Simple;
$VERSION = '1.23';
if ( $VERSION == 3.45 ) {
    # whatever
}
---
  '1.23' => <<'---', # $VERSION checked only in assignments, not relational ops
package Simple;
$VERSION = '1.23';
package Simple2;
if ( $Simple::VERSION == 3.45 ) {
    # whatever
}
---
  '1.23' => <<'---', # Fully qualified $VERSION declared in package
package Simple;
$Simple::VERSION = 1.23;
---
  '1.23' => <<'---', # Differentiate fully qualified $VERSION in a package
package Simple;
$Simple2::VERSION = '999';
$Simple::VERSION = 1.23;
---
  '1.23' => <<'---', # Differentiate fully qualified $VERSION and unqualified
package Simple;
$Simple2::VERSION = '999';
$VERSION = 1.23;
---
  '1.23' => <<'---', # $VERSION declared as package variable from within 'main' package
$Simple::VERSION = '1.23';
{
  package Simple;
  $x = $y, $cats = $dogs;
}
---
  '1.23' => <<'---', # $VERSION wrapped in parens - space inside
package Simple;
( $VERSION ) = '1.23';
---
  '1.23' => <<'---', # $VERSION wrapped in parens - no space inside
package Simple;
($VERSION) = '1.23';
---
  '1.23' => <<'---', # $VERSION follows a spurious 'package' in a quoted construct
package Simple;
__PACKAGE__->mk_accessors(qw(
    program socket proc
    package filename line codeline subroutine finished));

our $VERSION = "1.23";
---
  '1.23' => <<'---', # $VERSION using version.pm
  package Simple;
  use version; our $VERSION = version->new('1.23');
---
  '1.23' => <<'---', # $VERSION using version.pm and qv()
  package Simple;
  use version; our $VERSION = qv('1.230');
---
  '1.23' => <<'---', # Two version assignments, should ignore second one
  $Simple::VERSION = '1.230';
  $Simple::VERSION = eval $Simple::VERSION;
---
  '1.23' => <<'---', # declared & defined on same line with 'our'
package Simple;
our $VERSION = '1.23_00_00';
---
  '1.23' => <<'---', # package NAME VERSION
  package Simple 1.23;
---
  '1.23_01' => <<'---', # package NAME VERSION
  package Simple 1.23_01;
---
  'v1.2.3' => <<'---', # package NAME VERSION
  package Simple v1.2.3;
---
  'v1.2_3' => <<'---', # package NAME VERSION
  package Simple v1.2_3;
---
  '1.23' => <<'---', # trailing crud
  package Simple;
  our $VERSION;
  $VERSION = '1.23-alpha';
---
  '1.23' => <<'---', # trailing crud
  package Simple;
  our $VERSION;
  $VERSION = '1.23b';
---
  '1.234' => <<'---', # multi_underscore
  package Simple;
  our $VERSION;
  $VERSION = '1.2_3_4';
---
  '0' => <<'---', # non-numeric
  package Simple;
  our $VERSION;
  $VERSION = 'onetwothree';
---
  $undef => <<'---', # package NAME BLOCK, undef $VERSION
package Simple {
  our $VERSION;
}
---
  '1.23' => <<'---', # package NAME BLOCK, with $VERSION
package Simple {
  our $VERSION = '1.23';
}
---
  '1.23' => <<'---', # package NAME VERSION BLOCK
package Simple 1.23 {
  1;
}
---
  'v1.2.3_4' => <<'---', # package NAME VERSION BLOCK
package Simple v1.2.3_4 {
  1;
}
---
  '0' => <<'---', # set from separately-initialised variable
package Simple;
  our $CVSVERSION   = '$Revision: 1.7 $';
  our ($VERSION)    = ($CVSVERSION =~ /(\d+\.\d+)/);
}
---
);
my %modules = reverse @modules;

my @pkg_names = (
  [ 'Simple' ] => <<'---', # package NAME
package Simple;
---
  [ 'Simple::Edward' ] => <<'---', # package NAME::SUBNAME
package Simple::Edward;
---
  [ 'Simple::Edward::' ] => <<'---', # package NAME::SUBNAME::
package Simple::Edward::;
---
  [ "Simple'Edward" ] => <<'---', # package NAME'SUBNAME
package Simple'Edward;
---
  [ "Simple'Edward::" ] => <<'---', # package NAME'SUBNAME::
package Simple'Edward::;
---
  [ 'Simple::::Edward' ] => <<'---', # package NAME::::SUBNAME
package Simple::::Edward;
---
  [ '::Simple::Edward' ] => <<'---', # package ::NAME::SUBNAME
package ::Simple::Edward;
---
  [ 'main' ] => <<'---', # package NAME:SUBNAME (fail)
package Simple:Edward;
---
  [ 'main' ] => <<'---', # package NAME' (fail)
package Simple';
---
  [ 'main' ] => <<'---', # package NAME::SUBNAME' (fail)
package Simple::Edward';
---
  [ 'main' ] => <<'---', # package NAME''SUBNAME (fail)
package Simple''Edward;
---
  [ 'main' ] => <<'---', # package NAME-SUBNAME (fail)
package Simple-Edward;
---
);
my %pkg_names = reverse @pkg_names;

plan tests => 54 + (2 * keys( %modules )) + (2 * keys( %pkg_names ));

require_ok('Module::Metadata');

# class method C<find_module_by_name>
my $module = Module::Metadata->find_module_by_name(
               'Module::Metadata' );
ok( -e $module, 'find_module_by_name() succeeds' );

#########################

my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new( dir => $tmp );
$dist->regen;

$dist->chdir_in;


# fail on invalid module name
my $pm_info = Module::Metadata->new_from_module(
		'Foo::Bar', inc => [] );
ok( !defined( $pm_info ), 'fail if can\'t find module by module name' );


# fail on invalid filename
my $file = File::Spec->catfile( 'Foo', 'Bar.pm' );
$pm_info = Module::Metadata->new_from_file( $file, inc => [] );
ok( !defined( $pm_info ), 'fail if can\'t find module by file name' );


# construct from module filename
$file = File::Spec->catfile( 'lib', split( /::/, $dist->name ) ) . '.pm';
$pm_info = Module::Metadata->new_from_file( $file );
ok( defined( $pm_info ), 'new_from_file() succeeds' );

# construct from filehandle
my $handle = IO::File->new($file);
$pm_info = Module::Metadata->new_from_handle( $handle, $file );
ok( defined( $pm_info ), 'new_from_handle() succeeds' );
$pm_info = Module::Metadata->new_from_handle( $handle );
is( $pm_info, undef, "new_from_handle() without filename returns undef" );
close($handle);

# construct from module name, using custom include path
$pm_info = Module::Metadata->new_from_module(
	     $dist->name, inc => [ 'lib', @INC ] );
ok( defined( $pm_info ), 'new_from_module() succeeds' );


foreach my $module ( sort keys %modules ) {
    my $expected = $modules{$module};
 SKIP: {
    skip( "No our() support until perl 5.6", 2 )
        if $] < 5.006 && $module =~ /\bour\b/;
    skip( "No package NAME VERSION support until perl 5.11.1", 2 )
        if $] < 5.011001 && $module =~ /package\s+[\w\:\']+\s+v?[0-9._]+/;

    $dist->change_file( 'lib/Simple.pm', $module );
    $dist->regen;

    my $warnings = '';
    local $SIG{__WARN__} = sub { $warnings .= $_ for @_ };
    my $pm_info = Module::Metadata->new_from_file( $file );

    # Test::Builder will prematurely numify objects, so use this form
    my $errs;
    my $got = $pm_info->version;
    if ( defined $expected ) {
        ok( $got eq $expected,
            "correct module version (expected '$expected')" )
            or $errs++;
    } else {
        ok( !defined($got),
            "correct module version (expected undef)" )
            or $errs++;
    }
    is( $warnings, '', 'no warnings from parsing' ) or $errs++;
    diag "Got: '$got'\nModule contents:\n$module" if $errs;
  }
}

# revert to pristine state
$dist->regen( clean => 1 );

foreach my $pkg_name ( sort keys %pkg_names ) {
    my $expected = $pkg_names{$pkg_name};

    $dist->change_file( 'lib/Simple.pm', $pkg_name );
    $dist->regen;

    my $warnings = '';
    local $SIG{__WARN__} = sub { $warnings .= $_ for @_ };
    my $pm_info = Module::Metadata->new_from_file( $file );

    # Test::Builder will prematurely numify objects, so use this form
    my $errs;
    my @got = $pm_info->packages_inside();
    is_deeply( \@got, $expected,
               "correct package names (expected '" . join(', ', @$expected) . "')" )
            or $errs++;
    is( $warnings, '', 'no warnings from parsing' ) or $errs++;
    diag "Got: '" . join(', ', @got) . "'\nModule contents:\n$pkg_name" if $errs;
}

# revert to pristine state
$dist->regen( clean => 1 );

# Find each package only once
$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23';
package Error::Simple;
$VERSION = '2.34';
package Simple;
---

$dist->regen;

$pm_info = Module::Metadata->new_from_file( $file );

my @packages = $pm_info->packages_inside;
is( @packages, 2, 'record only one occurence of each package' );


# Module 'Simple.pm' does not contain package 'Simple';
# constructor should not complain, no default module name or version
$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple::Not;
$VERSION = '1.23';
---

$dist->regen;
$pm_info = Module::Metadata->new_from_file( $file );

is( $pm_info->name, undef, 'no default package' );
is( $pm_info->version, undef, 'no version w/o default package' );

# Module 'Simple.pm' contains an alpha version
# constructor should report first $VERSION found
$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '1.23_01';
$VERSION = eval $VERSION;
---

$dist->regen;
$pm_info = Module::Metadata->new_from_file( $file );

is( $pm_info->version, '1.23_01', 'alpha version reported');

# NOTE the following test has be done this way because Test::Builder is
# too smart for our own good and tries to see if the version object is a
# dual-var, which breaks with alpha versions:
#    Argument "1.23_0100" isn't numeric in addition (+) at
#    /usr/lib/perl5/5.8.7/Test/Builder.pm line 505.

ok( $pm_info->version > 1.23, 'alpha version greater than non');

# revert to pristine state
$dist->regen( clean => 1 );

# parse $VERSION lines scripts for package main
my @scripts = (
  <<'---', # package main declared
#!perl -w
package main;
$VERSION = '0.01';
---
  <<'---', # on first non-comment line, non declared package main
#!perl -w
$VERSION = '0.01';
---
  <<'---', # after non-comment line
#!perl -w
use strict;
$VERSION = '0.01';
---
  <<'---', # 1st declared package
#!perl -w
package main;
$VERSION = '0.01';
package _private;
$VERSION = '999';
---
  <<'---', # 2nd declared package
#!perl -w
package _private;
$VERSION = '999';
package main;
$VERSION = '0.01';
---
  <<'---', # split package
#!perl -w
package main;
package _private;
$VERSION = '999';
package main;
$VERSION = '0.01';
---
  <<'---', # define 'main' version from other package
package _private;
$::VERSION = 0.01;
$VERSION = '999';
---
  <<'---', # define 'main' version from other package
package _private;
$VERSION = '999';
$::VERSION = 0.01;
---
);

my ( $i, $n ) = ( 1, scalar( @scripts ) );
foreach my $script ( @scripts ) {
  $dist->change_file( 'bin/simple.plx', $script );
  $dist->regen;
  $pm_info = Module::Metadata->new_from_file(
	       File::Spec->catfile( 'bin', 'simple.plx' ) );

  is( $pm_info->version, '0.01', "correct script version ($i of $n)" );
  $i++;
}


# examine properties of a module: name, pod, etc
$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '0.01';
package Simple::Ex;
$VERSION = '0.02';

=head1 NAME

Simple - It's easy.

=head1 AUTHOR

Simple Simon

You can find me on the IRC channel
#simon on irc.perl.org.

=cut
---
$dist->regen;

$pm_info = Module::Metadata->new_from_module(
             $dist->name, inc => [ 'lib', @INC ] );

is( $pm_info->name, 'Simple', 'found default package' );
is( $pm_info->version, '0.01', 'version for default package' );

# got correct version for secondary package
is( $pm_info->version( 'Simple::Ex' ), '0.02',
    'version for secondary package' );

my $filename = $pm_info->filename;
ok( defined( $filename ) && -e $filename,
    'filename() returns valid path to module file' );

@packages = $pm_info->packages_inside;
is( @packages, 2, 'found correct number of packages' );
is( $packages[0], 'Simple', 'packages stored in order found' );

# we can detect presence of pod regardless of whether we are collecting it
ok( $pm_info->contains_pod, 'contains_pod() succeeds' );

my @pod = $pm_info->pod_inside;
is_deeply( \@pod, [qw(NAME AUTHOR)], 'found all pod sections' );

is( $pm_info->pod('NONE') , undef,
    'return undef() if pod section not present' );

is( $pm_info->pod('NAME'), undef,
    'return undef() if pod section not collected' );


# collect_pod
$pm_info = Module::Metadata->new_from_module(
             $dist->name, inc => [ 'lib', @INC ], collect_pod => 1 );

{
  my %pod;
  for my $section (qw(NAME AUTHOR)) {
    my $content = $pm_info->pod( $section );
    if ( $content ) {
      $content =~ s/^\s+//;
      $content =~ s/\s+$//;
    }
    $pod{$section} = $content;
  }
  my %expected = (
    NAME   => q|Simple - It's easy.|,
    AUTHOR => <<'EXPECTED'
Simple Simon

You can find me on the IRC channel
#simon on irc.perl.org.
EXPECTED
  );
  for my $text (values %expected) {
    $text =~ s/^\s+//;
    $text =~ s/\s+$//;
  }
  is( $pod{NAME},   $expected{NAME},   'collected NAME pod section' );
  is( $pod{AUTHOR}, $expected{AUTHOR}, 'collected AUTHOR pod section' );
}

{
  # test things that look like POD, but aren't
$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;

=YES THIS STARTS POD

our $VERSION = '999';

=cute

our $VERSION = '666';

=cut

*foo
=*no_this_does_not_start_pod;

our $VERSION = '1.23';

---
  $dist->regen;
  $pm_info = Module::Metadata->new_from_file('lib/Simple.pm');
  is( $pm_info->name, 'Simple', 'found default package' );
  is( $pm_info->version, '1.23', 'version for default package' );
}

{
  # Make sure processing stops after __DATA__
  $dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '0.01';
__DATA__
*UNIVERSAL::VERSION = sub {
  foo();
};
---
  $dist->regen;

  $pm_info = Module::Metadata->new_from_file('lib/Simple.pm');
  is( $pm_info->name, 'Simple', 'found default package' );
  is( $pm_info->version, '0.01', 'version for default package' );
  my @packages = $pm_info->packages_inside;
  is_deeply(\@packages, ['Simple'], 'packages inside');
}

{
  # Make sure we handle version.pm $VERSIONs well
  $dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = version->new('0.60.' . (qw$Revision: 128 $)[1]);
package Simple::Simon;
$VERSION = version->new('0.61.' . (qw$Revision: 129 $)[1]);
---
  $dist->regen;

  $pm_info = Module::Metadata->new_from_file('lib/Simple.pm');
  is( $pm_info->name, 'Simple', 'found default package' );
  is( $pm_info->version, '0.60.128', 'version for default package' );
  my @packages = $pm_info->packages_inside;
  is_deeply([sort @packages], ['Simple', 'Simple::Simon'], 'packages inside');
  is( $pm_info->version('Simple::Simon'), '0.61.129', 'version for embedded package' );
}

# check that package_versions_from_directory works

$dist->change_file( 'lib/Simple.pm', <<'---' );
package Simple;
$VERSION = '0.01';
package Simple::Ex;
$VERSION = '0.02';
{
  package main; # should ignore this
}
{
  package DB; # should ignore this
}
{
  package Simple::_private; # should ignore this
}

=head1 NAME

Simple - It's easy.

=head1 AUTHOR

Simple Simon

=cut
---
$dist->regen;

my $exp_pvfd = {
  'Simple' => {
    'file' => 'Simple.pm',
    'version' => '0.01'
  },
  'Simple::Ex' => {
    'file' => 'Simple.pm',
    'version' => '0.02'
  }
};

my $got_pvfd = Module::Metadata->package_versions_from_directory('lib');

is_deeply( $got_pvfd, $exp_pvfd, "package_version_from_directory()" )
  or diag explain $got_pvfd;

{
  my $got_provides = Module::Metadata->provides(dir => 'lib', version => 2);
  my $exp_provides = {
    'Simple' => {
      'file' => 'lib/Simple.pm',
      'version' => '0.01'
    },
    'Simple::Ex' => {
      'file' => 'lib/Simple.pm',
      'version' => '0.02'
    }
  };

  is_deeply( $got_provides, $exp_provides, "provides()" )
    or diag explain $got_provides;
}

{
  my $got_provides = Module::Metadata->provides(dir => 'lib', prefix => 'other', version => 1.4);
  my $exp_provides = {
    'Simple' => {
      'file' => 'other/Simple.pm',
      'version' => '0.01'
    },
    'Simple::Ex' => {
      'file' => 'other/Simple.pm',
      'version' => '0.02'
    }
  };

  is_deeply( $got_provides, $exp_provides, "provides()" )
    or diag explain $got_provides;
}

# Check package_versions_from_directory with regard to case-sensitivity
{
  $dist->change_file( 'lib/Simple.pm', <<'---' );
package simple;
$VERSION = '0.01';
---
  $dist->regen;

  $pm_info = Module::Metadata->new_from_file('lib/Simple.pm');
  is( $pm_info->name, undef, 'no default package' );
  is( $pm_info->version, undef, 'version for default package' );
  is( $pm_info->version('simple'), '0.01', 'version for lower-case package' );
  is( $pm_info->version('Simple'), undef, 'version for capitalized package' );

  $dist->change_file( 'lib/Simple.pm', <<'---' );
package simple;
$VERSION = '0.01';
package Simple;
$VERSION = '0.02';
package SiMpLe;
$VERSION = '0.03';
---
  $dist->regen;

  $pm_info = Module::Metadata->new_from_file('lib/Simple.pm');
  is( $pm_info->name, 'Simple', 'found default package' );
  is( $pm_info->version, '0.02', 'version for default package' );
  is( $pm_info->version('simple'), '0.01', 'version for lower-case package' );
  is( $pm_info->version('Simple'), '0.02', 'version for capitalized package' );
  is( $pm_info->version('SiMpLe'), '0.03', 'version for mixed-case package' );
}
