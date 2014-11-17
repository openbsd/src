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
  my @data_dirs = qw( t/data-test t/data-valid );
  my @files = sort map {
        my $d = $_;
        map { "$d/$_" } grep { substr($_,0,1) ne '.' } IO::Dir->new($d)->read
  } @data_dirs;

  for my $f ( @files ) {
    my $meta = Parse::CPAN::Meta->load_file( File::Spec->catfile($f) );
    my $cmv = CPAN::Meta::Validator->new({%$meta});
    ok( $cmv->is_valid, "$f validates" )
      or diag( "ERRORS:\n" . join( "\n", $cmv->errors ) );
  }
}

{
  my @data_dirs = qw( t/data-fail t/data-fixable );
  my @files = sort map {
        my $d = $_;
        map { "$d/$_" } grep { substr($_,0,1) ne '.' } IO::Dir->new($d)->read
  } @data_dirs;

  for my $f ( @files ) {
    my $meta = Parse::CPAN::Meta->load_file( File::Spec->catfile($f) );
    my $cmv = CPAN::Meta::Validator->new({%$meta});
    ok( ! $cmv->is_valid, "$f shouldn't validate" );
  }
}

done_testing;

