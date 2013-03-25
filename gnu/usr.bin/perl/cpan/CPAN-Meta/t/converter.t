use strict;
use warnings;
use Test::More 0.88;
use utf8;

use CPAN::Meta;
use CPAN::Meta::Validator;
use CPAN::Meta::Converter;
use File::Spec;
use File::Basename qw/basename/;
use IO::Dir;
use Parse::CPAN::Meta 1.4400;
use version;

delete $ENV{$_} for qw/PERL_JSON_BACKEND PERL_YAML_BACKEND/; # use defaults

# mock file object
package
  File::StringObject;

use overload q{""} => sub { ${$_[0]} }, fallback => 1;

sub new {
  my ($class, $file) = @_;
  bless \$file, $class;
}

package main;

my $data_dir = IO::Dir->new( 't/data' );
my @files = sort grep { /^\w/ } $data_dir->read;

sub _spec_version { return $_[0]->{'meta-spec'}{version} || "1.0" }

#use Data::Dumper;

for my $f ( reverse sort @files ) {
  my $path = File::Spec->catfile('t','data',$f);
  my $original = Parse::CPAN::Meta->load_file( $path  );
  ok( $original, "loaded $f" );
  my $original_v = _spec_version($original);
  # UPCONVERSION
  {
    my $cmc = CPAN::Meta::Converter->new( $original );
    my $converted = $cmc->convert( version => 2 );
    is ( _spec_version($converted), 2, "up converted spec version $original_v to spec version 2");
    my $cmv = CPAN::Meta::Validator->new( $converted );
    ok ( $cmv->is_valid, "up converted META is valid" )
      or diag( "ERRORS:\n" . join( "\n", $cmv->errors )
#      . "\nMETA:\n" . Dumper($converted)
    );
  }
  # UPCONVERSION - partial
  if ( _spec_version( $original ) < 2 ) {
    my $cmc = CPAN::Meta::Converter->new( $original );
    my $converted = $cmc->convert( version => '1.4' );
    is ( _spec_version($converted), 1.4, "up converted spec version $original_v to spec version 1.4");
    my $cmv = CPAN::Meta::Validator->new( $converted );
    ok ( $cmv->is_valid, "up converted META is valid" )
      or diag( "ERRORS:\n" . join( "\n", $cmv->errors )
#      . "\nMETA:\n" . Dumper($converted)
    );
  }
  # DOWNCONVERSION - partial
  if ( _spec_version( $original ) >= 1.2 ) {
    my $cmc = CPAN::Meta::Converter->new( $original );
    my $converted = $cmc->convert( version => '1.2' );
    is ( _spec_version($converted), '1.2', "down converted spec version $original_v to spec version 1.2");
    my $cmv = CPAN::Meta::Validator->new( $converted );
    ok ( $cmv->is_valid, "down converted META is valid" )
      or diag( "ERRORS:\n" . join( "\n", $cmv->errors )
#      . "\nMETA:\n" . Dumper($converted)
    );

    if (_spec_version( $original ) == 2) {
      is_deeply(
        $converted->{build_requires},
        {
          'Test::More'      => '0.88',
          'Build::Requires' => '1.1',
          'Test::Requires'  => '1.2',
        },
        "downconversion from 2 merge test and build requirements",
      );
    }
  }
  # DOWNCONVERSION
  {
    my $cmc = CPAN::Meta::Converter->new( $original );
    my $converted = $cmc->convert( version => '1.0' );
    is ( _spec_version($converted), '1.0', "down converted spec version $original_v to spec version 1.0");
    my $cmv = CPAN::Meta::Validator->new( $converted );
    ok ( $cmv->is_valid, "down converted META is valid" )
      or diag( "ERRORS:\n" . join( "\n", $cmv->errors )
#      . "\nMETA:\n" . Dumper($converted)
    );

    unless ($original_v eq '1.0') {
      like ( $converted->{generated_by},
        qr(\Q$original->{generated_by}\E, CPAN::Meta::Converter version \S+$),
        "added converter mark to generated_by",
      );
    }
  }
}

# specific test for custom key handling
{
  my $path = File::Spec->catfile('t','data','META-1_4.yml');
  my $original = Parse::CPAN::Meta->load_file( $path  );
  ok( $original, "loaded META-1_4.yml" );
  my $cmc = CPAN::Meta::Converter->new( $original );
  my $up_converted = $cmc->convert( version => 2 );
  ok ( $up_converted->{x_whatever} && ! $up_converted->{'x-whatever'},
    "up converted 'x-' to 'x_'"
  );
  ok ( $up_converted->{x_whatelse},
    "up converted 'x_' as 'x_'"
  );
  ok ( $up_converted->{x_WhatNow} && ! $up_converted->{XWhatNow},
    "up converted 'XFoo' to 'x_Foo'"
  ) or diag join("\n", keys %$up_converted);
}

# specific test for custom key handling
{
  my $path = File::Spec->catfile('t','data','META-2.json');
  my $original = Parse::CPAN::Meta->load_file( $path  );
  ok( $original, "loaded META-2.json" );
  my $cmc = CPAN::Meta::Converter->new( $original );
  my $down_converted = $cmc->convert( version => 1.4 );
  ok ( $down_converted->{x_whatever},
    "down converted 'x_' as 'x_'"
  );
}

# specific test for generalization of unclear licenses
{
  my $path = File::Spec->catfile('t','data','gpl-1_4.yml');
  my $original = Parse::CPAN::Meta->load_file( $path  );
  ok( $original, "loaded gpl-1_4.yml" );
  my $cmc = CPAN::Meta::Converter->new( $original );
  my $up_converted = $cmc->convert( version => 2 );
  is_deeply ( $up_converted->{license},
    [ "open_source" ],
    "up converted 'gpl' to 'open_source'"
  );
}

# specific test for upconverting resources
{
  my $path = File::Spec->catfile('t','data','resources.yml');
  my $original = Parse::CPAN::Meta->load_file( $path  );
  ok( $original, "loaded resources.yml" );
  my $cmc = CPAN::Meta::Converter->new( $original );
  my $converted = $cmc->convert( version => 2 );
  is_deeply(
    $converted->{resources},
    { x_MailingList => 'http://groups.google.com/group/www-mechanize-users',
      x_Repository  => 'http://code.google.com/p/www-mechanize/source',
      homepage      => 'http://code.google.com/p/www-mechanize/',
      bugtracker    => {web => 'http://code.google.com/p/www-mechanize/issues/list',},
      license       => ['http://dev.perl.org/licenses/'],
    },
    "upconversion of resources"
  );
}

# specific test for object conversion
{
  my $path = File::Spec->catfile('t','data','resources.yml');
  my $original = Parse::CPAN::Meta->load_file( $path  );
  ok( $original, "loaded resources.yml" );
  $original->{version} = version->new("1.64");
  $original->{no_index}{file} = File::StringObject->new(".gitignore");
  pass( "replaced some data fields with objects" );
  my $cmc = CPAN::Meta::Converter->new( $original );
  ok( my $converted = $cmc->convert( version => 2 ), "conversion successful" );
}

# specific test for UTF-8 handling
{
  my $path = File::Spec->catfile('t','data','unicode.yml');
  my $original = CPAN::Meta->load_file( $path  )
    or die "Couldn't load $path";
  ok( $original, "unicode.yml" );
  my @authors = $original->authors;
  like( $authors[0], qr/Williåms/, "Unicode characters preserved in authors" );
}

# specific test for version ranges
{
  my @prereq_keys = qw(
    prereqs requires build_requires configure_requires
    recommends conflicts
  );
  for my $case ( qw/ 2 1_4 / ) {
    my $suffix = $case eq 2 ? "$case.json" : "$case.yml";
    my $version = $case;
    $version =~ tr[_][.];
    my $path = File::Spec->catfile('t','data','version-ranges-' . $suffix);
    my $original = Parse::CPAN::Meta->load_file( $path  );
    ok( $original, "loaded " . basename $path );
    my $cmc = CPAN::Meta::Converter->new( $original );
    my $converted = $cmc->convert( version => $version );
    for my $h ( $original, $converted ) {
      delete $h->{generated_by};
      delete $h->{'meta-spec'}{url};
      for my $k ( @prereq_keys ) {
        _normalize_reqs($h->{$k}) if exists $h->{$k};
      }
    }
    is_deeply( $converted, $original, "version ranges preserved in conversion" );
  }
}

# specific test for version numbers
{
  my $path = File::Spec->catfile('t','data','version-not-normal.json');
  my $original = Parse::CPAN::Meta->load_file( $path  );
  ok( $original, "loaded " . basename $path );
  my $cmc = CPAN::Meta::Converter->new( $original );
  my $converted = $cmc->convert( version => 2 );
  is( $converted->{prereqs}{runtime}{requires}{'File::Find'}, "v0.1.0", "normalize v0.1");
  is( $converted->{prereqs}{runtime}{requires}{'File::Path'}, "v1.0.0", "normalize v1.0.0");
}

# CMR standardizes stuff in a way that makes it hard to test original vs final
# so we remove spaces and >= to make them compare the same
sub _normalize_reqs {
  my $hr = shift;
  for my $k ( keys %$hr ) {
    if (ref $hr->{$k} eq 'HASH') {
      _normalize_reqs($hr->{$k});
    }
    elsif ( ! ref $hr->{$k} ) {
      $hr->{$k} =~ s{\s+}{}g;
      $hr->{$k} =~ s{>=\s*}{}g;
    }
  }
}

done_testing;
