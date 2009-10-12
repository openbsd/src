################################################################################
#
#  devtools.pl -- various utility functions
#
################################################################################
#
#  $Revision: 1.2 $
#  $Author: millert $
#  $Date: 2009/10/12 18:24:26 $
#
################################################################################
#
#  Version 3.x, Copyright (C) 2004-2009, Marcus Holland-Moritz.
#  Version 2.x, Copyright (C) 2001, Paul Marquess.
#  Version 1.x, Copyright (C) 1999, Kenneth Albanowski.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

use IO::File;

eval "use Term::ANSIColor";
$@ and eval "sub colored { pop; @_ }";

my @argvcopy = @ARGV;

sub verbose
{
  if ($opt{verbose}) {
    my @out = @_;
    s/^(.*)/colored("($0) ", 'bold blue').colored($1, 'blue')/eg for @out;
    print STDERR @out;
  }
}

sub ddverbose
{
  return $opt{verbose} ? ('--verbose') : ();
}

sub runtool
{
  my $opt = ref $_[0] ? shift @_ : {};
  my($prog, @args) = @_;
  my $sysstr = join ' ', map { "'$_'" } $prog, @args;
  $sysstr .= " >$opt->{'out'}"  if exists $opt->{'out'};
  $sysstr .= " 2>$opt->{'err'}" if exists $opt->{'err'};
  verbose("running $sysstr\n");
  my $rv = system $sysstr;
  verbose("$prog => exit code $rv\n");
  return not $rv;
}

sub runperl
{
  my $opt = ref $_[0] ? shift @_ : {};
  runtool($opt, $^X, @_);
}

sub run
{
  my $prog = shift;
  my @args = @_;

  runtool({ 'out' => 'tmp.out', 'err' => 'tmp.err' }, $prog, @args);

  my $out = IO::File->new("tmp.out") or die "tmp.out: $!\n";
  my $err = IO::File->new("tmp.err") or die "tmp.err: $!\n";

  my %rval = (
    status    => $? >> 8,
    stdout    => [<$out>],
    stderr    => [<$err>],
    didnotrun => 0,
  );

  unlink "tmp.out", "tmp.err";

  $? & 128 and $rval{core}   = 1;
  $? & 127 and $rval{signal} = $? & 127;

  return \%rval;
}

sub ident_str
{
  return "$^X $0 @argvcopy";
}

sub identify
{
  verbose(ident_str() . "\n");
}

sub ask($)
{
  my $q = shift;
  my $a;
  local $| = 1;
  print "\n$q [y/n] ";
  do { $a = <>; } while ($a !~ /^\s*([yn])\s*$/i);
  return lc $1 eq 'y';
}

sub quit_now
{
  print "\nSorry, cannot continue.\n\n";
  exit 1;
}

sub ask_or_quit
{
  quit_now unless &ask;
}

sub eta
{
  my($start, $i, $n) = @_;
  return "--:--:--" if $i < 3;
  my $elapsed = tv_interval($start);
  my $h = int($elapsed*($n-$i)/$i);
  my $s = $h % 60; $h /= 60;
  my $m = $h % 60; $h /= 60;
  return sprintf "%02d:%02d:%02d", $h, $m, $s;
}

1;
