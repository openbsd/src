use strict;
use warnings;

use Test::More tests => 1;

use File::Basename;
use File::Spec;
use Cwd qw(abs_path);
my $THISDIR;
BEGIN {
    $THISDIR = dirname(abs_path(__FILE__));
    unshift @INC, $THISDIR;
    require "testcmp.pl";
    TestCompare->import;
}

use Pod::Text;

my $infile = File::Spec->catfile($THISDIR, (File::Spec->updir) x 2, 'scripts', 'pod2usage.PL');
my $cmpfile = File::Spec->catfile($THISDIR, 'pod2usage.xr');
my $outfile = File::Spec->catfile($THISDIR, 'pod2usage.OUT');

my $text_parser = Pod::Text->new;
$text_parser->parse_from_file($infile, $outfile);

my %opts = map +($_ => 1), @ARGV;

if ($opts{'-xrgen'}) {
    if ($opts{'-force'} or ! -e $cmpfile) {
        print "# Creating expected result for \"pod2usage\"" .
              " pod2text test ...\n";
        $text_parser->parse_from_file($infile, $cmpfile);
    }
    else {
        print "# File $cmpfile already exists" .
              " (use '-force' to regenerate it).\n";
    }
}

ok !testcmp( $outfile, $cmpfile ), "$outfile matches $cmpfile";
unlink $outfile;
