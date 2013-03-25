#!./perl

BEGIN {
    if ($^O eq 'VMS') {
        print "1..0 # Skip: needs porting, perhaps imitating Win32 mechanisms\n";
	exit 0;
    }
    require "../../t/test.pl";
}
use strict;
use warnings;

plan 3;

# [perl #76474]
{
  my $stderr = runperl(
     switches => ['-MIPC::Open3', '-w'],
     prog => 'open STDIN, q _Makefile_ or die $!; open3(q _<&0_, my $out, undef, $ENV{PERLEXE}, q _-e0_)',
     stderr => 1,
  );

  is $stderr, '',
   "dup STDOUT in a child process by using its file descriptor";
}

{
  my $want = qr/\A# This Makefile is for the IPC::Open3 extension to perl\.\r?\z/;
  open my $fh, '<', 'Makefile' or die "Can't open MAKEFILE: $!";
  my $have = <$fh>;
  chomp $have;
  like($have, $want, 'No surprises from MakeMaker');
  close $fh;

  fresh_perl_like(<<'EOP',
use IPC::Open3;
open FOO, 'Makefile' or die $!;
open3('<&' . fileno FOO, my $out, undef, $ENV{PERLEXE}, '-eprint scalar <STDIN>');
print <$out>;
EOP
		  $want,
		  undef,
		  'Numeric file handles are duplicated correctly'
      );
}
