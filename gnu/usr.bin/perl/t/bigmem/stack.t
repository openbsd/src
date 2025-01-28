#!perl
BEGIN {
    chdir 't' if -d 't';
    @INC = "../lib";
    require './test.pl';
}

use strict;
use Config qw(%Config);
use XS::APItest;

# memory usage checked with top
$ENV{PERL_TEST_MEMORY} >= 60
    or skip_all("Need ~60GB for this test");
$Config{ptrsize} >= 8
    or skip_all("Need 64-bit pointers for this test");
XS::APItest::wide_marks()
    or skip_all("Not configured for SSize_t marks");

my @x;
$x[0x8000_0000] = "Hello";

my $arg_count;

my @tests =
  (
      [ mark => sub
        {
            # unlike the grep example this avoids the mark manipulation done by grep
            # so it's more of a pure mark type test
            # it also fails/succeeds a lot faster
            my $count = () =  (x(), z());
            is($count, 0x8000_0002, "got expected (large) list size");
        },
      ],
      [ xssize => sub
        {
            # check XS gets the right numbers in our predefined variables
            # returned ~ -2G before fix
            my $count = XS::APItest::xs_items(x(), z());
            is($count, 0x8000_0002, "got expected XS list size");
        }
      ],
      [ listsub => sub
        {
            my $last = ( x() )[-1];
            is($last, "Hello", "list subscripting");

            my ($first, $last2, $last1) = ( "first", x(), "Goodbye" )[0, -2, -1];
            is($first, "first", "list subscripting in list context (0)");
            is($last2, "Hello", "list subscripting in list context (-2)");
            is($last1, "Goodbye", "list subscripting in list context (-1)");
        }
      ],
      [ iterctx => sub
        {
            # the iter context had an I32 stack offset
            my $last = ( x(), iter() )[-1];
            is($last, "abc", "check iteration not confused");
        }
      ],
      [ split => sub
        {
            # split had an I32 base offset
            # this paniced with "Split loop"
            my $count = () = ( x(), do_split("ABC") );
            is($count, 0x8000_0004, "split base index");
            # it would be nice to test split returning >2G (or >4G) items, but
            # I don't have the memory needed
        }
      ],
      [ xsload => sub
        {
            # I expect this to crash if buggy
            my $count = () = (x(), loader());
            is($count, 0x8000_0001, "check loading XS with large stack");
        }
      ],
      [ pp_list => sub
        {
            my $l = ( x(), list2() )[-1];
            is($l, 2, "pp_list mark handling");
        }
       ],
      [
          chomp_av => sub {
              # not really stack related, but is 32-bit related
              local $x[-1] = "Hello\n";
              chomp(@x);
              is($x[-1], "Hello", "chomp on a large array");
          }
         ],
      [
          grepwhile => sub {
            SKIP: {
                  skip "This test is even slower - define PERL_RUN_SLOW_TESTS to run me", 1
                    unless $ENV{PERL_RUN_SLOW_TESTS};
                  # grep ..., @x used too much memory
                  my $count = grep 1, ( (undef) x 0x7FFF_FFFF, 1, 1 );
                  is($count, 0x8000_0001, "grepwhile item count");
              }
          }
      ],
      [
          repeat => sub {
            SKIP:
              {
                  $ENV{PERL_TEST_MEMORY} >= 70
                       or skip "repeat test needs 70GB", 2;
                  # pp_repeat would throw an unable to allocate error
                  my ($lastm1, $middle) = ( ( x() ) x 2 )[-1, @x-1];
                  is($lastm1, "Hello", "repeat lastm1");
                  is($middle, "Hello", "repeat middle");
              }
          },
      ],
      [
          tiescalar => sub {
            SKIP:
              {
                  # this swaps unless you have actually 80GB RAM, since
                  # most of the memory is touched
                  $ENV{PERL_TEST_MEMORY} >= 80
                    or skip "tiescalar second test needs 80GB", 2;
                  my $x;
                  ok(ref( ( x(), tie($x, "ScalarTie", 1..5))[-1]),
                     "tied with deep stack");
                  is($x, 6, "check arguments received");
                  untie $x;
                  ok(tie($x, "ScalarTie", x()), "tie scalar with long argument list");
                  is($x, 1+scalar(@x), "check arguments received");
                  untie $x;
                SKIP:
                  {
                      skip "This test is even slower - define PERL_RUN_SLOW_TESTS to run me", 1
                        unless $ENV{PERL_RUN_SLOW_TESTS};
                      my $o = bless {}, "ScalarTie";
                      # this was news to me
                      ok(tie($x, $o, x(), 1), "tie scalar via object with long argument list");
                      is($x, 2+scalar(@x), "check arguments received");
                      untie $x;
                  }
              }
          }
      ],
      [
          apply => sub {
            SKIP:
              {
                  skip "2**31 system calls take a very long time - define PERL_RUN_SLOW_TESTS to run me", 1
                    unless $ENV{PERL_RUN_SLOW_TESTS};
                  my $mode = (stat $0)[2];
                  my $tries = 0x8000_0001;
                  my $count = chmod $mode, ( $0 ) x $tries;
                  is($count, $tries, "chmod with 2G files");
              }
          }
      ],
      [
          join => sub {
              no warnings 'uninitialized';
              my $joined = join "", @x, "!";
              is($joined, "Hello!", "join");
          },
      ],
      [
          class_construct => sub {
              use experimental 'class';
              class Foo {
                  field $x :param;
              };
              my $y = Foo->new((x => 1) x 0x4000_0001);
              ok($y, "construct class based object with 2G parameters");
          },
      ],
      [
          eval_sv_count => sub {
            SKIP:
              {
                  $ENV{PERL_TEST_MEMORY} >= 70
                    or skip "eval_sv_count test needs 70GB", 2;

                  my $count = ( @x, XS::APItest::eval_sv('@x', G_LIST) )[-1];
                  is($count, scalar @x, "check eval_sv result/mark handling");
              }
          }
      ],
      [
          call_sv_args => sub {
              undef $arg_count;
              my $ret_count = XS::APItest::call_sv(\&arg_count, G_LIST, x());
              is($ret_count, 0, "call_sv with 2G args - arg_count() returns nothing");
              is($arg_count, scalar @x, "check call_sv argument handling - argument count");
          },
      ],
      [
          call_sv_mark => sub {
              my $ret_count = ( x(), XS::APItest::call_sv(\&list, G_LIST) )[-1];
              is($ret_count, 2, "call_sv with deep stack - returned value count");
          },
      ],
     );

# these tests are slow, let someone debug them one at a time
my %enabled = map { $_ => 1 } @ARGV;
for my $test (@tests) {
    my ($id, $code) = @$test;
    if (!@ARGV || $enabled{$id}) {
        note($id);
        $code->();
    }
}

done_testing();

sub x { @x }

sub z { 1 }

sub iter {
    my $result = '';
    my $count = 0;
    for my $item (qw(a b c)) {
        $result .= $item;
        die "iteration bug" if ++$count > 5;
    }
    $result;
}

sub do_split {
    return split //, $_[0];
}

sub loader {
    require Cwd;
    ();
}

sub list2 {
    scalar list(1);
}

sub list {
    # ensure this continues to use a pp_list op
    # if you change it.
    return shift() ? (1, 2) : (2, 1);
}

sub arg_count {
    $arg_count = @_;
    ();
}

package ScalarTie;

sub TIESCALAR {
    ::note("TIESCALAR $_[0]");
    bless { count => scalar @_ }, __PACKAGE__;
}

sub FETCH {
    $_[0]{count};
}
