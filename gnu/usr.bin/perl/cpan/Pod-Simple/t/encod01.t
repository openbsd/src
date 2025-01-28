# encoding nonesuch
use strict;
use warnings;

use Test::More tests => 5;
use File::Spec;

#use Pod::Simple::Debug (10);

use Pod::Simple;
use Pod::Simple::DumpAsXML;

my $thefile;

use File::Spec;
use Cwd ();
use File::Basename ();

BEGIN {
  my $corpusdir = File::Spec->catdir(File::Basename::dirname(Cwd::abs_path(__FILE__)), 'corpus');
  $thefile = File::Spec->catfile($corpusdir, 'nonesuch.txt');
}

print "# Testing that $thefile parses right.\n";
my $outstring;
{
  my $p = Pod::Simple::DumpAsXML->new;
  $p->output_string( \$outstring );
  $p->parse_file( $thefile );
  undef $p;
}
ok 1 ; # make sure it parsed at all
ok( $outstring && length($outstring) ); # make sure it parsed to something.
#print $outstring;
like( $outstring, qr/Blorp/ );
like( $outstring, qr/errata/ );
like( $outstring, qr/unsupported/ );
