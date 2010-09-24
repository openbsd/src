#!/usr/bin/perl -w
################################################################################
#
#  mktodo.pl -- generate baseline and todo files
#
################################################################################
#
#  $Revision: 16 $
#  $Author: mhx $
#  $Date: 2009/01/18 14:10:51 +0100 $
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

use strict;
use Getopt::Long;
use Data::Dumper;
use IO::File;
use IO::Select;
use Config;
use Time::HiRes qw( gettimeofday tv_interval );

require 'devel/devtools.pl';

our %opt = (
  debug   => 0,
  base    => 0,
  verbose => 0,
  check   => 1,
  shlib   => 'blib/arch/auto/Devel/PPPort/PPPort.so',
);

GetOptions(\%opt, qw(
            perl=s todo=s version=s shlib=s debug base verbose check!
          )) or die;

identify();

print "\n", ident_str(), "\n\n";

my $fullperl = `which $opt{perl}`;
chomp $fullperl;

$ENV{SKIP_SLOW_TESTS} = 1;

regen_all();

my %sym;
for (`$Config{nm} $fullperl`) {
  chomp;
  /\s+T\s+(\w+)\s*$/ and $sym{$1}++;
}
keys %sym >= 50 or die "less than 50 symbols found in $fullperl\n";

my %all = %{load_todo($opt{todo}, $opt{version})};
my @recheck;

my $symmap = get_apicheck_symbol_map();

for (;;) {
  my $retry = 1;
  my $trynm = 1;
  regen_apicheck();

retry:
  my(@new, @tmp, %seen);

  my $r = run(qw(make));
  $r->{didnotrun} and die "couldn't run make: $!\n";

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
  }

  if ($r->{status} == 0) {
    my @u;
    my @usym;

    if ($trynm) {
      @u = eval { find_undefined_symbols($fullperl, $opt{shlib}) };
      warn "warning: $@" if $@;
      $trynm = 0;
    }

    unless (@u) {
      $r = run(qw(make test));
      $r->{didnotrun} and die "couldn't run make test: $!\n";
      $r->{status} == 0 and last;

      for my $l (@{$r->{stderr}}) {
        if ($l =~ /undefined symbol: (\w+)/) {
          push @u, $1;
        }
      }
    }

    for my $u (@u) {
      for my $m (keys %{$symmap->{$u}}) {
        if (!$seen{$m}++) {
          my $pl = $m;
          $pl =~ s/^[Pp]erl_//;
          my @s = grep { exists $sym{$_} } $pl, "Perl_$pl", "perl_$pl";
          push @new, [$m, @s ? "U (@s)" : "U"];
        }
      }
    }
  }

  @new = grep !$all{$_->[0]}, @new;

  unless (@new) {
    @new = grep !$all{$_->[0]}, @tmp;
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

  # don't recheck undefined symbols reported by the dynamic linker
  push @recheck, map { $_->[0] } grep { $_->[1] !~ /^U/ } @new;

  for (@new) {
    sym('new', @$_);
    $all{$_->[0]} = $_->[1];
  }

  write_todo($opt{todo}, $opt{version}, \%all);
}

if ($opt{check}) {
  my $ifmt = '%' . length(scalar @recheck) . 'd';
  my $t0 = [gettimeofday];
  
  RECHECK: for my $i (0 .. $#recheck) {
    my $sym = $recheck[$i];
    my $cur = delete $all{$sym};
  
    sym('chk', $sym, $cur, sprintf(" [$ifmt/$ifmt, ETA %s]",
               $i + 1, scalar @recheck, eta($t0, $i, scalar @recheck)));
  
    write_todo($opt{todo}, $opt{version}, \%all);
  
    if ($cur eq "E (Perl_$sym)") {
      # we can try a shortcut here
      regen_apicheck($sym);
  
      my $r = run(qw(make test));
  
      if (!$r->{didnotrun} && $r->{status} == 0) {
        sym('del', $sym, $cur);
        next RECHECK;
      }
    }
  
    # run the full test
    regen_all();
  
    my $r = run(qw(make test));
  
    $r->{didnotrun} and die "couldn't run make test: $!\n";
  
    if ($r->{status} == 0) {
      sym('del', $sym, $cur);
    }
    else {
      $all{$sym} = $cur;
    }
  }
}

write_todo($opt{todo}, $opt{version}, \%all);

run(qw(make realclean));

exit 0;

sub sym
{
  my($what, $sym, $reason, $extra) = @_;
  $extra ||= '';
  my %col = (
    'new' => 'bold red',
    'chk' => 'bold magenta',
    'del' => 'bold green',
  );
  $what = colored("$what symbol", $col{$what});

  printf "[%s] %s %-30s # %s%s\n",
         $opt{version}, $what, $sym, $reason, $extra;
}

sub regen_all
{
  my @mf_arg = ('--with-apicheck', 'OPTIMIZE=-O0 -w');
  push @mf_arg, qw( DEFINE=-DDPPP_APICHECK_NO_PPPORT_H ) if $opt{base};

  # just to be sure
  run(qw(make realclean));
  run($fullperl, "Makefile.PL", @mf_arg)->{status} == 0
      or die "cannot run Makefile.PL: $!\n";
}

sub regen_apicheck
{
  unlink qw(apicheck.c apicheck.o);
  runtool({ out => '/dev/null' }, $fullperl, 'apicheck_c.PL', map { "--api=$_" } @_)
      or die "cannot regenerate apicheck.c\n";
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

sub find_undefined_symbols
{
  my($perl, $shlib) = @_;

  my $ps = read_sym(file => $perl,  options => [qw( --defined-only   )]);
  my $ls = read_sym(file => $shlib, options => [qw( --undefined-only )]);

  my @undefined;

  for my $sym (keys %$ls) {
    unless (exists $ps->{$sym}) {
      if ($sym !~ /\@/ and $sym !~ /^_/) {
        push @undefined, $sym;
      }
    }
  }

  return @undefined;
}

sub read_sym
{
  my %opt = ( options => [], @_ );

  my $r = run($Config{nm}, @{$opt{options}}, $opt{file});

  if ($r->{didnotrun} or $r->{status}) {
    die "cannot run $Config{nm}";
  }

  my %sym;

  for (@{$r->{stdout}}) {
    chomp;
    my($adr, $fmt, $sym) = /^\s*([[:xdigit:]]+)?\s+([ABCDGINRSTUVW?-])\s+(\S+)\s*$/i
                           or die "cannot parse $Config{nm} output:\n[$_]\n";
    $sym{$sym} = { format => $fmt };
    $sym{$sym}{address} = $adr if defined $adr;
  }

  return \%sym;
}

sub get_apicheck_symbol_map
{
  my $r = run(qw(make apicheck.i));
  
  if ($r->{didnotrun} or $r->{status}) {
    die "cannot run make apicheck.i";
  }

  my $fh = IO::File->new('apicheck.i')
           or die "cannot open apicheck.i: $!";

  local $_;
  my %symmap;
  my $cur;

  while (<$fh>) {
    next if /^#/;
    if (defined $cur) {
      for my $sym (/\b([A-Za-z_]\w+)\b/g) {
        $symmap{$sym}{$cur}++;
      }
      undef $cur if /^}$/;
    }
    else {
      /_DPPP_test_(\w+)/ and $cur = $1;
    }
  }

  return \%symmap;
}
