BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
}

# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..2\n"; }
END {print "not ok 1\n" unless $loaded;}
use Class::ISA;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

  @Food::Fishstick::ISA = qw(Food::Fish  Life::Fungus  Chemicals);
  @Food::Fish::ISA = qw(Food);
  @Food::ISA = qw(Matter);
  @Life::Fungus::ISA = qw(Life);
  @Chemicals::ISA = qw(Matter);
  @Life::ISA = qw(Matter);
  @Matter::ISA = qw();

  use Class::ISA;
  my @path = Class::ISA::super_path('Food::Fishstick');
  my $flat_path = join ' ', @path;
  print "# Food::Fishstick path is:\n# $flat_path\n";
  print "not " unless
   "Food::Fish Food Matter Life::Fungus Life Chemicals" eq $flat_path;
  print "ok 2\n";
