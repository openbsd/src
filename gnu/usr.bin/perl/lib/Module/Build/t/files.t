#!/usr/bin/perl -w

use strict;
use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
use MBTest tests => 6;

use Cwd ();
my $cwd = Cwd::cwd;
my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new( dir => $tmp );
$dist->regen;

chdir( $dist->dirname ) or die "Can't chdir to '@{[$dist->dirname]}': $!";


use IO::File;


use Module::Build;
my $mb = Module::Build->new_from_context;
my @files;

{
  # Make sure copy_if_modified() can handle spaces in filenames
  
  my @tmp;
  foreach (1..2) {
    my $tmp = File::Spec->catdir('t', "tmp$_");
    $mb->add_to_cleanup($tmp);
    push @files, $tmp;
    unless (-d $tmp) {
      mkdir($tmp, 0777) or die "Can't create $tmp: $!";
    }
    ok -d $tmp;
    $tmp[$_] = $tmp;
  }
  
  my $filename = 'file with spaces.txt';
  
  my $file = File::Spec->catfile($tmp[1], $filename);
  my $fh = IO::File->new($file, '>') or die "Can't create $file: $!";
  print $fh "Foo\n";
  $fh->close;
  ok -e $file;
  
  
  my $file2 = $mb->copy_if_modified(from => $file, to_dir => $tmp[2]);
  ok $file2;
  ok -e $file2;
}

{
  # Try some dir_contains() combinations
  my $first  = File::Spec->catdir('', 'one', 'two');
  my $second = File::Spec->catdir('', 'one', 'two', 'three');
  
  ok( Module::Build->dir_contains($first, $second) );
}

# cleanup
chdir( $cwd ) or die "Can''t chdir to '$cwd': $!";
$dist->remove;

use File::Path;
rmtree( $tmp );
