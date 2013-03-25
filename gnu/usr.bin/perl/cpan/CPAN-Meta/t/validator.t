use strict;
use warnings;
use Test::More 0.88;

use CPAN::Meta;
use CPAN::Meta::Validator;
use File::Spec;
use IO::Dir;
use Parse::CPAN::Meta 1.4400;

delete $ENV{$_} for qw/PERL_JSON_BACKEND PERL_YAML_BACKEND/; # use defaults

{
  my $data_dir = IO::Dir->new( 't/data' );
  my @files = sort grep { /^\w/ } $data_dir->read;

  for my $f ( @files ) {
    my $meta = Parse::CPAN::Meta->load_file( File::Spec->catfile('t','data',$f) );
    my $cmv = CPAN::Meta::Validator->new({%$meta});
    ok( $cmv->is_valid, "$f validates" )
      or diag( "ERRORS:\n" . join( "\n", $cmv->errors ) );
  }
}

{
  my $data_dir = IO::Dir->new( 't/data-fail' );
  my @files = sort grep { /^\w/ } $data_dir->read;

  for my $f ( @files ) {
    my $meta = Parse::CPAN::Meta->load_file( File::Spec->catfile('t','data-fail',$f) );
    my $cmv = CPAN::Meta::Validator->new({%$meta});
    ok( ! $cmv->is_valid, "invalid $f doesn't validate" );
  }
}

done_testing;

