#!/usr/bin/perl -w
use strict;
use vars qw($Is_W32 $Is_OS2 $Is_Cygwin $Is_NetWare $Needs_Write);
use Config; # Remember, this is running using an existing perl

# Common functions needed by the regen scripts

$Is_W32 = $^O eq 'MSWin32';
$Is_OS2 = $^O eq 'os2';
$Is_Cygwin = $^O eq 'cygwin';
$Is_NetWare = $Config{osname} eq 'NetWare';
if ($Is_NetWare) {
  $Is_W32 = 0;
}

$Needs_Write = $Is_OS2 || $Is_W32 || $Is_Cygwin || $Is_NetWare;

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

sub safer_rename {
  my ($from, $to) = @_;
  safer_rename_silent($from, $to) or die "renaming $from to $to: $!";
}
1;
