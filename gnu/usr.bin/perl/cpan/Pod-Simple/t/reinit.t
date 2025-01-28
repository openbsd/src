use strict;
use warnings;
use Test::More tests => 5;

use File::Spec;
use Cwd ();
use File::Basename ();

use Pod::Simple::Text;
$Pod::Simple::Text::FREAKYMODE = 1;

my $parser  = Pod::Simple::Text->new();

foreach my $file (
  "junk1.pod",
  "junk2.pod",
  "perlcyg.pod",
  "perlfaq.pod",
  "perlvar.pod",
) {
    my $full_file = File::Spec->catfile(File::Basename::dirname(Cwd::abs_path(__FILE__)), $file);

    unless(-e $full_file) {
        ok 0;
        print "# But $full_file doesn't exist!!\n";
        next;
    }

    my $precooked = $full_file;
    my $outstring;
    my $compstring;
    $precooked =~ s<\.pod><o.txt>s;
    $parser->reinit;
    $parser->output_string(\$outstring);
    $parser->parse_file($full_file);

    open(IN, $precooked) or die "Can't read-open $precooked: $!";
    {
      local $/;
      $compstring = <IN>;
    }
    close(IN);

    for ($outstring,$compstring) { s/\s+/ /g; s/^\s+//s; s/\s+$//s; }

    if($outstring eq $compstring) {
      ok 1;
      next;
    } elsif( do{
      for ($outstring, $compstring) { tr/ //d; };
      $outstring eq $compstring;
    }){
      print "# Differ only in whitespace.\n";
      ok 1;
      next;
    } else {

      my $x = $outstring ^ $compstring;
      $x =~ m/^(\x00*)/s or die;
      my $at = length($1);
      print "# Difference at byte $at...\n";
      if($at > 10) {
        $at -= 5;
      }
      {
        print "# ", substr($outstring,$at,20), "\n";
        print "# ", substr($compstring,$at,20), "\n";
        print "#      ^...";
      }

      ok 0;
      printf "# Unequal lengths %s and %s\n", length($outstring), length($compstring);
      next;
    }
  }
