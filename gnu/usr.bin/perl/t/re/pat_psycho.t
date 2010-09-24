#!./perl
#
# This is a home for regular expression tests that don't fit into
# the format supported by re/regexp.t.  If you want to add a test
# that does fit that format, add it to re/re_tests, not here.

use strict;
use warnings;
use 5.010;


sub run_tests;

$| = 1;


BEGIN {
    chdir 't' if -d 't';
    @INC = ('../lib','.');
    do "re/ReTest.pl" or die $@;
}


plan tests => 11;  # Update this when adding/deleting tests.

run_tests() unless caller;

#
# Tests start here.
#
sub run_tests {

  SKIP:
    {
        print "# Set PERL_SKIP_PSYCHO_TEST to skip this test\n";
        my @normal = qw [the are some normal words];

        skip "Skipped Psycho", 2 * @normal if $ENV {PERL_SKIP_PSYCHO_TEST};

        local $" = "|";

        my @psycho = (@normal, map chr $_, 255 .. 20000);
        my $psycho1 = "@psycho";
        for (my $i = @psycho; -- $i;) {
            my $j = int rand (1 + $i);
            @psycho [$i, $j] = @psycho [$j, $i];
        }
        my $psycho2 = "@psycho";

        foreach my $word (@normal) {
            ok $word =~ /($psycho1)/ && $1 eq $word, 'Psycho';
            ok $word =~ /($psycho2)/ && $1 eq $word, 'Psycho';
        }
    }


  SKIP:
    {
        # stress test CURLYX/WHILEM.
        #
        # This test includes varying levels of nesting, and according to
        # profiling done against build 28905, exercises every code line in the
        # CURLYX and WHILEM blocks, except those related to LONGJMP, the
        # super-linear cache and warnings. It executes about 0.5M regexes

        skip "No psycho tests" if $ENV {PERL_SKIP_PSYCHO_TEST};
        print "# Set PERL_SKIP_PSYCHO_TEST to skip this test\n";
        my $r = qr/^
                    (?:
                        ( (?:a|z+)+ )
                        (?:
                            ( (?:b|z+){3,}? )
                            (
                                (?:
                                    (?:
                                        (?:c|z+){1,1}?z
                                    )?
                                    (?:c|z+){1,1}
                                )*
                            )
                            (?:z*){2,}
                            ( (?:z+|d)+ )
                            (?:
                                ( (?:e|z+)+ )
                            )*
                            ( (?:f|z+)+ )
                        )*
                        ( (?:z+|g)+ )
                        (?:
                            ( (?:h|z+)+ )
                        )*
                        ( (?:i|z+)+ )
                    )+
                    ( (?:j|z+)+ )
                    (?:
                        ( (?:k|z+)+ )
                    )*
                    ( (?:l|z+)+ )
              $/x;
          
        my $ok = 1;
        my $msg = "CURLYX stress test";
        OUTER:
          for my $a ("x","a","aa") {
            for my $b ("x","bbb","bbbb") {
              my $bs = $a.$b;
              for my $c ("x","c","cc") {
                my $cs = $bs.$c;
                for my $d ("x","d","dd") {
                  my $ds = $cs.$d;
                  for my $e ("x","e","ee") {
                    my $es = $ds.$e;
                    for my $f ("x","f","ff") {
                      my $fs = $es.$f;
                      for my $g ("x","g","gg") {
                        my $gs = $fs.$g;
                        for my $h ("x","h","hh") {
                          my $hs = $gs.$h;
                          for my $i ("x","i","ii") {
                            my $is = $hs.$i;
                            for my $j ("x","j","jj") {
                              my $js = $is.$j;
                              for my $k ("x","k","kk") {
                                my $ks = $js.$k;
                                for my $l ("x","l","ll") {
                                  my $ls = $ks.$l;
                                  if ($ls =~ $r) {
                                    if ($ls =~ /x/) {
                                      $msg .= ": unexpected match for [$ls]";
                                      $ok = 0;
                                      last OUTER;
                                    }
                                    my $cap = "$1$2$3$4$5$6$7$8$9$10$11$12";
                                    unless ($ls eq $cap) {
                                      $msg .= ": capture: [$ls], got [$cap]";
                                      $ok = 0;
                                      last OUTER;
                                    }
                                  }
                                  else {
                                    unless ($ls =~ /x/) {
                                      $msg = ": failed for [$ls]";
                                      $ok = 0;
                                      last OUTER;
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
        }
        ok($ok, $msg);
    }
} # End of sub run_tests

1;
