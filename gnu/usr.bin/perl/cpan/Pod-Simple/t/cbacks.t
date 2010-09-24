BEGIN {
    if($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }
}

use strict;
use Test;
BEGIN { plan tests => 8 };

my $d;
#use Pod::Simple::Debug (\$d, 0);

ok 1;

use Pod::Simple::XMLOutStream;
use Pod::Simple::DumpAsXML;
use Pod::Simple::DumpAsText;

my @from = (
 'Pod::Simple::XMLOutStream'
  => '<Document><head1>I LIKE PIE</head1></Document>',
   
 'Pod::Simple::DumpAsXML'
  => "<Document>\n  <head1>\n    I LIKE PIE\n  </head1>\n</Document>\n",
   
 'Pod::Simple::DumpAsText'
  => "++Document\n  ++head1\n    * \"I LIKE PIE\"\n  --head1\n--Document\n",

);


# Might as well test all the classes...
while(@from) {
  my($x => $expected) = splice(@from, 0,2);
  my $more = '';
  print "#Testing via class $x, version ", $x->VERSION(), "\n";
  my $p = $x->new;
  my($got, $exp);
  ok scalar($got = $x->_out(
    # Mutor:
    sub {
     $_[0]->code_handler(sub { $more .= $_[1] . ":" . $_[0] . "\n"       } );
     $_[0]->cut_handler( sub { $more .= "~" . $_[1] . ":" .  $_[0]. "\n" } );
    } => join "\n",
    "",
    "\t# This is handy...",
    "=head1 I  LIKE   PIE",
    "",
    "=cut",
    "use Test::Harness;",
    "runtests(sort glob 't/*.t');",
    "",
    "",
   ))
    => scalar($exp = $expected);
  ;
  unless($got eq $exp) {
    print '# Got vs exp:\n# ', Pod::Simple::BlackBox::pretty($got),
     "\n# ",Pod::Simple::BlackBox::pretty($exp),"\n";
  }
  
  ok scalar($got = $more), scalar($exp = join "\n" =>
   "1:",
   "2:\t# This is handy...",
   "~5:=cut",
   "6:use Test::Harness;",
   "7:runtests(sort glob 't/*.t');",
   "8:",
   "",
  );
  unless($got eq $exp) {
   print '# Got vs exp:\n# ', Pod::Simple::BlackBox::pretty($got),
    "\n# ",Pod::Simple::BlackBox::pretty($exp),"\n";
  }
}


print "# Wrapping up... one for the road...\n";
ok 1;
print "# --- Done with ", __FILE__, " --- \n";

