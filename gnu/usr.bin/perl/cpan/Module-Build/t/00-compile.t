use strict;
use warnings;
use lib 't/lib';
use MBTest;
use File::Find qw/find/;

my @files;
find( sub { -f && /\.pm$/ && push @files, $File::Find::name }, 'lib' );

plan tests => scalar @files;

for my $f ( sort @files ) {
  my $ec;
  my $output = stdout_stderr_of( sub { $ec = system( $^X, '-c', $f ) } );
  ok( ! $ec, "compiling $f" ) or diag $output;
}

