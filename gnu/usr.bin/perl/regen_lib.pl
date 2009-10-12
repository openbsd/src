#!/usr/bin/perl -w
use strict;
use vars qw($Needs_Write $Verbose @Changed);
use File::Compare;
use Symbol;

# Common functions needed by the regen scripts

$Needs_Write = $^O eq 'cygwin' || $^O eq 'os2' || $^O eq 'MSWin32';

$Verbose = 0;
@ARGV = grep { not($_ eq '-q' and $Verbose = -1) }
  grep { not($_ eq '-v' and $Verbose = 1) } @ARGV;

END {
  print STDOUT "Changed: @Changed\n" if @Changed;
}

sub safer_unlink {
  my @names = @_;
  my $cnt = 0;

  my $name;
  foreach $name (@names) {
    next unless -e $name;
    chmod 0777, $name if $Needs_Write;
    ( CORE::unlink($name) and ++$cnt
      or warn "Couldn't unlink $name: $!\n" );
  }
  return $cnt;
}

sub safer_rename_silent {
  my ($from, $to) = @_;

  # Some dosish systems can't rename over an existing file:
  safer_unlink $to;
  chmod 0600, $from if $Needs_Write;
  rename $from, $to;
}

sub rename_if_different {
  my ($from, $to) = @_;

  if (compare($from, $to) == 0) {
      warn "no changes between '$from' & '$to'\n" if $Verbose > 0;
      safer_unlink($from);
      return;
  }
  warn "changed '$from' to '$to'\n" if $Verbose > 0;
  push @Changed, $to unless $Verbose < 0;
  safer_rename_silent($from, $to) or die "renaming $from to $to: $!";
}

# Saf*er*, but not totally safe. And assumes always open for output.
sub safer_open {
    my $name = shift;
    my $fh = gensym;
    open $fh, ">$name" or die "Can't create $name: $!";
    *{$fh}->{SCALAR} = $name;
    binmode $fh;
    $fh;
}

sub safer_close {
    my $fh = shift;
    close $fh or die 'Error closing ' . *{$fh}->{SCALAR} . ": $!";
}

1;
