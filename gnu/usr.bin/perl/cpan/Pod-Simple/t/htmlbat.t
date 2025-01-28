# Testing HTMLBatch
use strict;
use warnings;

use Test::More tests => 15;

#sub Pod::Simple::HTMLBatch::DEBUG () {5};

my $DEBUG = 0;

require Pod::Simple::HTMLBatch;;

use File::Spec;
use Cwd;
my $cwd = cwd();
print "# CWD: $cwd\n" if $DEBUG;

use File::Spec;
use Cwd ();
use File::Basename ();

my $t_dir = File::Basename::dirname(Cwd::abs_path(__FILE__));
my $corpus_dir = File::Spec->catdir($t_dir, 'testlib1');

print "# OK, found the test corpus as $corpus_dir\n" if $DEBUG;

my $outdir;
while(1) {
  my $rand = sprintf "%05x", rand( 0x100000 );
  $outdir = File::Spec->catdir( $t_dir, "delme-$rand-out" );
  last unless -e $outdir;
}

END {
    use File::Path;
    rmtree $outdir, 0, 0;
}

ok 1;
print "# Output dir: $outdir\n" if $DEBUG;

mkdir $outdir, 0777 or die "Can't mkdir $outdir: $!";

print "# Converting $corpus_dir => $outdir\n" if $DEBUG;
my $conv = Pod::Simple::HTMLBatch->new;
$conv->verbose(0);
$conv->index(1);
$conv->batch_convert( [$corpus_dir], $outdir );
ok 1;
print "# OK, back from converting.\n" if $DEBUG;

my @files;
use File::Find;
find( sub {
      push @files, $File::Find::name;
      if (/[.]html$/ && $_ !~ /perl|index/) {
          # Make sure an index was generated.
          open HTML, $_ or die "Cannot open $_: $!\n";
          my $html = do { local $/; <HTML> };
          close HTML;
          like $html, qr/<div class='indexgroup'>/;
      }
      return;
}, $outdir );

{
  my $long = ( grep m/zikzik\./i, @files )[0];
  ok($long) or print "# How odd, no zikzik file in $outdir!?\n";
  if($long) {
    $long =~ s{zikzik\.html?$}{}s;
    for(@files) { substr($_, 0, length($long)) = '' }
    @files = grep length($_), @files;
  }
}

if ($DEBUG) {
    print "#Produced in $outdir ...\n";
    foreach my $f (sort @files) {
        print "#   $f\n";
    }
    print "# (", scalar(@files), " items total)\n";
}

# Some minimal sanity checks:
ok scalar(grep m/\.css/i, @files) > 5;
ok scalar(grep m/\.html?/i, @files) > 5;
ok scalar grep m{squaa\W+Glunk.html?}i, @files;

if (my @long = grep { /^[^.]{9,}/ } map { s{^[^/]/}{} } @files) {
    ok 0;
    print "#    File names too long:\n",
        map { "#         $_\n" } @long;
} else {
    ok 1;
}

# use Pod::Simple;
# *pretty = \&Pod::Simple::BlackBox::pretty;
