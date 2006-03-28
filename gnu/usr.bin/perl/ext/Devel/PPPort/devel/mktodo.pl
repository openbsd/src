#!/usr/bin/perl -w
################################################################################
#
#  mktodo.pl -- generate baseline and todo files
#
################################################################################
#
#  $Revision: 1.2 $
#  $Author: millert $
#  $Date: 2006/03/28 19:23:02 $
#
################################################################################
#
#  Version 3.x, Copyright (C) 2004-2005, Marcus Holland-Moritz.
#  Version 2.x, Copyright (C) 2001, Paul Marquess.
#  Version 1.x, Copyright (C) 1999, Kenneth Albanowski.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

use strict;
use Getopt::Long;
use Data::Dumper;
use IO::File;
use IO::Select;

my %opt = (
  debug => 0,
  base  => 0,
);

print "\n$0 @ARGV\n\n";

GetOptions(\%opt, qw(
            perl=s todo=s version=s debug base
          )) or die;

my $fullperl = `which $opt{perl}`;
chomp $fullperl;

regen_all();

my %sym;
for (`nm $fullperl`) {
  chomp;
  /\s+T\s+(\w+)\s*$/ and $sym{$1}++;
}
keys %sym >= 50 or die "less than 50 symbols found in $fullperl\n";

my %all = %{load_todo($opt{todo}, $opt{version})};
my @recheck;

for (;;) {
  my $retry = 1;
  regen_apicheck();
retry:
  my $r = run(qw(make test));
  $r->{didnotrun} and die "couldn't run make test: $!\n";
  $r->{status} == 0 and last;
  my(@new, @tmp, %seen);
  for my $l (@{$r->{stderr}}) {
    if ($l =~ /_DPPP_test_(\w+)/) {
      if (!$seen{$1}++) {
        my @s = grep { exists $sym{$_} } $1, "Perl_$1", "perl_$1";
        if (@s) {
          push @tmp, [$1, "E (@s)"];
        }
        else {
          push @new, [$1, "E"];
        }
      }
    }
    if ($l =~ /undefined symbol: (?:[Pp]erl_)?(\w+)/) {
      if (!$seen{$1}++) {
        my @s = grep { exists $sym{$_} } $1, "Perl_$1", "perl_$1";
        push @new, [$1, @s ? "U (@s)" : "U"];
      }
    }
  }
  @new = grep !$all{$_->[0]}, @new;
  unless (@new) {
    @new = grep !$all{$_->[0]}, @tmp;
    # TODO: @recheck was here, find a better way to get recheck syms
    #       * we definitely don't have to check (U) symbols
    #       * try to grep out warnings before making symlist ?
  }
  unless (@new) {
    if ($retry > 0) {
      $retry--;
      regen_all();
      goto retry;
    }
    print Dumper($r);
    die "no new TODO symbols found...";
  }
  push @recheck, map { $_->[0] } @new;
  for (@new) {
    printf "[$opt{version}] new symbol: %-30s # %s\n", @$_;
    $all{$_->[0]} = $_->[1];
  }
  write_todo($opt{todo}, $opt{version}, \%all);
}

for my $sym (@recheck) {
  my $cur = delete $all{$sym};
  printf "[$opt{version}] chk symbol: %-30s # %s\n", $sym, $cur;
  write_todo($opt{todo}, $opt{version}, \%all);
  regen_all();
  my $r = run(qw(make test));
  $r->{didnotrun} and die "couldn't run make test: $!\n";
  if ($r->{status} == 0) {
    printf "[$opt{version}] del symbol: %-30s # %s\n", $sym, $cur;
  }
  else {
    $all{$sym} = $cur;
  }
}

write_todo($opt{todo}, $opt{version}, \%all);

run(qw(make realclean));

exit 0;

sub regen_all
{
  my @mf_arg = qw( --with-apicheck OPTIMIZE=-O0 );
  push @mf_arg, qw( DEFINE=-DDPPP_APICHECK_NO_PPPORT_H ) if $opt{base};

  # just to be sure
  run(qw(make realclean));
  run($fullperl, "Makefile.PL", @mf_arg)->{status} == 0
      or die "cannot run Makefile.PL: $!\n";
}

sub regen_apicheck
{
  unlink qw(apicheck.c apicheck.o);
  system "$fullperl apicheck_c.PL >/dev/null";
}

sub load_todo
{
  my($file, $expver) = @_;

  if (-e $file) {
    my $f = new IO::File $file or die "cannot open $file: $!\n";
    my $ver = <$f>;
    chomp $ver;
    if ($ver eq $expver) {
      my %sym;
      while (<$f>) {
        chomp;
        /^(\w+)\s+#\s+(.*)/ or goto nuke_file;
        exists $sym{$1} and goto nuke_file;
        $sym{$1} = $2;
      }
      return \%sym;
    }

nuke_file:
    undef $f;
    unlink $file or die "cannot remove $file: $!\n";
  }

  return {};
}

sub write_todo
{
  my($file, $ver, $sym) = @_;
  my $f;

  $f = new IO::File ">$file" or die "cannot open $file: $!\n";
  $f->print("$ver\n");

  for (sort keys %$sym) {
    $f->print(sprintf "%-30s # %s\n", $_, $sym->{$_});
  }
}

sub run
{
  my $prog = shift;
  my @args = @_;

  # print "[$prog @args]\n";

  system "$prog @args >tmp.out 2>tmp.err";

  my $out = new IO::File "tmp.out" || die "tmp.out: $!\n";
  my $err = new IO::File "tmp.err" || die "tmp.err: $!\n";

  my %rval = (
    status    => $? >> 8,
    stdout    => [<$out>],
    stderr    => [<$err>],
    didnotrun => 0,
  );

  unlink "tmp.out", "tmp.err";

  $? & 128 and $rval{core}   = 1;
  $? & 127 and $rval{signal} = $? & 127;

  \%rval;
}

