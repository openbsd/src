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

*_spec_version = \&CPAN::Meta::Converter::_extract_spec_version;

use Data::Dumper;

for my $f ( reverse sort @files ) {
  my $path = File::Spec->catfile('t','data-fail',$f);
  my $original = Parse::CPAN::Meta->load_file( $path  );
  ok( $original, "loaded invalid $f" );
  my $original_v = _spec_version($original);
  # UPCONVERSION
  if ( $original_v lt '2' ) {
    my $cmc = CPAN::Meta::Converter->new( $original );
    my $fixed = eval { $cmc->convert( version => 2 ) };
    ok ( $@, "error thrown up converting" );
  }
  # DOWNCONVERSION
  if ( $original_v gt '1.0' ) {
    my $cmc = CPAN::Meta::Converter->new( $original );
    my $fixed = eval { $cmc->convert( version => '1.0' ) };
    ok ( $@, "error thrown down converting" );
  }
}

done_testing;

