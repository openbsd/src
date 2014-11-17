use strict;
use warnings;
use Test::More 0.88;

use CPAN::Meta;
use File::Spec;
use IO::Dir;

sub _slurp { do { local(@ARGV,$/)=shift(@_); <> } }

delete $ENV{$_} for qw/PERL_JSON_BACKEND PERL_YAML_BACKEND/; # use defaults

my $data_dir = IO::Dir->new( 't/data-fixable' );
my @files = sort grep { /^\w/ } $data_dir->read;

for my $f ( sort @files ) {
  my $path = File::Spec->catfile('t','data-fixable',$f);
  my $meta = eval { CPAN::Meta->load_file( $path ) };
  ok( defined $meta, "load_file('$f')" ) or diag $@;
  my $string = _slurp($path);
  my $method =  $path =~ /\.json/ ? "load_json_string" : "load_yaml_string";
  my $meta2 = eval { CPAN::Meta->$method( $string, { fix_errors => 1 } ) };
  ok( defined $meta2, "$method(slurp('$f'))" ) or diag $@;
}

done_testing;

