#!./perl -w
#
#  Copyright 2002, Larry Wall.
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

sub BEGIN {
    chdir('t') if -d 't';
    if ($ENV{PERL_CORE}){
	@INC = ('.', '../lib', '../ext/Storable/t');
        require Config;
        if ($Config::Config{'extensions'} !~ /\bStorable\b/) {
            print "1..0 # Skip: Storable was not built\n";
            exit 0;
        }
    } else {
        unless (eval "require Hash::Util") {
            if ($@ =~ /Can\'t locate Hash\/Util\.pm in \@INC/) {
                print "1..0 # Skip: No Hash::Util\n";
                exit 0;
            } else {
                die;
            }
        }
	unshift @INC, 't';
    }
    require 'st-dump.pl';
}


use Storable qw(dclone);
use Hash::Util qw(lock_hash unlock_value);

print "1..16\n";

my %hash = (question => '?', answer => 42, extra => 'junk', undef => undef);
lock_hash %hash;
unlock_value %hash, 'answer';
unlock_value %hash, 'extra';
delete $hash{'extra'};

my $test;

package Restrict_Test;

sub me_second {
  return (undef, $_[0]);
}

package main;

sub testit {
  my $hash = shift;
  my $copy = dclone $hash;

  my @in_keys = sort keys %$hash;
  my @out_keys = sort keys %$copy;
  unless (ok ++$test, "@in_keys" eq "@out_keys") {
    print "# Failed: keys mis-match after deep clone.\n";
    print "# Original keys: @in_keys\n";
    print "# Copy's keys: @out_keys\n";
  }

  # $copy = $hash;	# used in initial debug of the tests

  ok ++$test, Internals::SvREADONLY(%$copy), "cloned hash restricted?";

  ok ++$test, Internals::SvREADONLY($copy->{question}),
    "key 'question' not locked in copy?";

  ok ++$test, !Internals::SvREADONLY($copy->{answer}),
    "key 'answer' not locked in copy?";

  eval { $copy->{extra} = 15 } ;
  unless (ok ++$test, !$@, "Can assign to reserved key 'extra'?") {
    my $diag = $@;
    $diag =~ s/\n.*\z//s;
    print "# \$\@: $diag\n";
  }

  eval { $copy->{nono} = 7 } ;
  ok ++$test, $@, "Can not assign to invalid key 'nono'?";

  ok ++$test, exists $copy->{undef},
    "key 'undef' exists";

  ok ++$test, !defined $copy->{undef},
    "value for key 'undef' is undefined";
}

for $Storable::canonical (0, 1) {
  print "# \$Storable::canonical = $Storable::canonical\n";
  testit (\%hash);
  my $object = \%hash;
  # bless {}, "Restrict_Test";
}
