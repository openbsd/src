#!/usr/bin/perl -w
################################################################################
#
#  buildperl.pl -- build various versions of perl automatically
#
################################################################################
#
#  $Revision: 1.1.1.2 $
#  $Author: millert $
#  $Date: 2006/03/28 18:47:58 $
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
use Pod::Usage;
use File::Find;
use File::Path;
use Data::Dumper;
use IO::File;
use Cwd;

my %opt = (
  prefix => '/tmp/perl/install/<config>/<perl>',
  build  => '/tmp/perl/build/<config>',
  source => '/tmp/perl/source',
  force  => 0,
);

my %config = (
  default     => {
	           config_args => '-des',
                 },
  thread      => {
	           config_args     => '-des -Dusethreads',
	           masked_versions => [ qr/^perl5\.00[01234]/ ],
                 },
  thread5005  => {
	           config_args     => '-des -Duse5005threads',
	           masked_versions => [ qr/^perl5\.00[012345]|^perl-5.(9|\d\d)/ ],
                 },
  debug       => {
	           config_args => '-des -Doptimize=-g',
                 },
);

my @patch = (
  {
    perl => [
              qr/^perl5\.00[01234]/,
              qw/
                perl5.005
                perl5.005_01
                perl5.005_02
                perl5.005_03
              /,
            ],
    subs => [
              [ \&patch_db, 1 ],
            ],
  },
  {
    perl => [
     	      qw/
                perl-5.6.0
                perl-5.6.1
                perl-5.7.0
                perl-5.7.1
                perl-5.7.2
                perl-5.7.3
                perl-5.8.0
     	      /,
            ],
    subs => [
              [ \&patch_db, 3 ],
            ],
  },
  {
    perl => [
              qr/^perl5\.004_0[1234]/,
            ],
    subs => [
              [ \&patch_doio ],
            ],
  },
);

my(%perl, @perls);

GetOptions(\%opt, qw(
  config=s@
  prefix=s
  source=s
  perl=s@
  force
)) or pod2usage(2);

if (exists $opt{config}) {
  for my $cfg (@{$opt{config}}) {
    exists $config{$cfg} or die "Unknown configuration: $cfg\n";
  }
}
else {
  $opt{config} = [sort keys %config];
}

find(sub {
  /^(perl-?(5\..*))\.tar.gz$/ or return;
  $perl{$1} = { version => $2, source => $File::Find::name };
}, $opt{source});

if (exists $opt{perl}) {
  for my $perl (@{$opt{perl}}) {
    my $p = $perl;
    exists $perl{$p} or $p = "perl$perl";
    exists $perl{$p} or $p = "perl-$perl";
    exists $perl{$p} or die "Cannot find perl: $perl\n";
    push @perls, $p;
  }
}
else {
  @perls = sort keys %perl;
}

$ENV{PATH} = "~/bin:$ENV{PATH}";  # use ccache

my %current;

for my $cfg (@{$opt{config}}) {
  for my $perl (@perls) {
    my $config = $config{$cfg};
    %current = (config => $cfg, perl => $perl);

    if (is($config->{masked_versions}, $perl)) {
      print STDERR "skipping $perl for configuration $cfg (masked)\n";
      next;
    }

    if (-d expand($opt{prefix}) and !$opt{force}) {
      print STDERR "skipping $perl for configuration $cfg (already installed)\n";
      next;
    }

    my $cwd = cwd;

    my $build = expand($opt{build});
    -d $build or mkpath($build);
    chdir $build or die "chdir $build: $!\n";

    print STDERR "building $perl with configuration $cfg\n";
    buildperl($perl, $config);

    chdir $cwd or die "chdir $cwd: $!\n";
  }
}

sub expand
{
  my $in = shift;
  $in =~ s/(<(\w+)>)/exists $current{$2} ? $current{$2} : $1/eg;
  return $in;
}

sub is
{
  my($s1, $s2) = @_;

  defined $s1 != defined $s2 and return 0;

  ref $s2 and ($s1, $s2) = ($s2, $s1);

  if (ref $s1) {
    if (ref $s1 eq 'ARRAY') {
      is($_, $s2) and return 1 for @$s1;
      return 0;
    }
    return $s2 =~ $s1;
  }

  return $s1 eq $s2;
}

sub buildperl
{
  my($perl, $cfg) = @_;

  my $d = extract_source($perl{$perl});
  chdir $d or die "chdir $d: $!\n";

  patch_source($perl);

  build_and_install($perl{$perl});
}

sub extract_source
{
  my $perl = shift;
  my $target = "perl-$perl->{version}";

  for my $dir ("perl$perl->{version}", "perl-$perl->{version}") {
    if (-d $dir) {
      print "removing old build directory $dir\n";
      rmtree($dir);
    }
  }

  print "extracting $perl->{source}\n";

  run_or_die("tar xzf $perl->{source}");

  if ($perl->{version} !~ /^\d+\.\d+\.\d+/ && -d "perl-$perl->{version}") {
    $target = "perl$perl->{version}";
    rename "perl-$perl->{version}", $target or die "rename: $!\n";
  }

  -d $target or die "$target not found\n";

  return $target;
}

sub patch_source
{
  my $perl = shift;

  for my $p (@patch) {
    if (is($p->{perl}, $perl)) {
      for my $s (@{$p->{subs}}) {
        my($sub, @args) = @$s;
        $sub->(@args);
      }
    }
  }
}

sub build_and_install
{
  my $perl = shift;
  my $prefix = expand($opt{prefix});

  print "building perl $perl->{version} ($current{config})\n";

  run_or_die("./Configure $config{$current{config}}{config_args} -Dusedevel -Uinstallusrbinperl -Dprefix=$prefix");
  run_or_die("sed -i -e '/^.*<built-in>/d' -e '/^.*<command line>/d' makefile x2p/makefile");
  run_or_die("make all");
  # run("make test");
  run_or_die("make install");
}

sub patch_db
{
  my $ver = shift;
  print "patching DB_File\n";
  run_or_die("sed -i -e 's/<db.h>/<db$ver\\/db.h>/' ext/DB_File/DB_File.xs");
}

sub patch_doio
{
  patch('doio.c', <<'END');
--- doio.c.org	2004-06-07 23:14:45.000000000 +0200
+++ doio.c	2003-11-04 08:03:03.000000000 +0100
@@ -75,6 +75,16 @@
 #  endif
 #endif

+#if _SEM_SEMUN_UNDEFINED
+union semun
+{
+  int val;
+  struct semid_ds *buf;
+  unsigned short int *array;
+  struct seminfo *__buf;
+};
+#endif
+
 bool
 do_open(gv,name,len,as_raw,rawmode,rawperm,supplied_fp)
 GV *gv;
END
}

sub patch
{
  my($file, $patch) = @_;
  print "patching $file\n";
  my $diff = "$file.diff";
  write_or_die($diff, $patch);
  run_or_die("patch -s -p0 <$diff");
  unlink $diff or die "unlink $diff: $!\n";
}

sub write_or_die
{
  my($file, $data) = @_;
  my $fh = new IO::File ">$file" or die "$file: $!\n";
  $fh->print($data);
}

sub run_or_die
{
  # print "[running @_]\n";
  system "@_" and die "@_: $?\n";
}

sub run
{
  # print "[running @_]\n";
  system "@_" and warn "@_: $?\n";
}
