use strict;
use warnings;
use Test::More 0.88;

use CPAN::Meta;
use CPAN::Meta::Validator;
use CPAN::Meta::Converter;
use File::Spec;
use IO::Dir;
use Parse::CPAN::Meta 1.4400;

delete $ENV{$_} for qw/PERL_JSON_BACKEND PERL_YAML_BACKEND/; # use defaults

my $data_dir = IO::Dir->new( 't/data-fail' );
my @files = sort grep { /^\w/ } $data_dir->read;

sub _spec_version { return $_[0]->{'meta-spec'}{version} || "1.0" }

use Data::Dumper;

for my $f ( reverse sort @files ) {
  my $path = File::Spec->catfile('t','data-fail',$f);
  my $original = Parse::CPAN::Meta->load_file( $path  );
  ok( $original, "loaded invalid $f" );
  my $original_v = _spec_version($original);
  # UPCONVERSION
  if ( _spec_version( $original ) lt '2' ) {
    my $cmc = CPAN::Meta::Converter->new( $original );
    eval { $cmc->convert( version => 2 ) };
    ok ( $@, "error thrown up converting" );
  }
  # DOWNCONVERSION
  if ( _spec_version( $original ) gt '1.0' ) {
    my $cmc = CPAN::Meta::Converter->new( $original );
    eval { $cmc->convert( version => '1.0' ) };
    ok ( $@, "error thrown down converting" );
  }
}

done_testing;

