use strict;
use warnings;
package GeneratePackage;
# vim:ts=8:sw=2:et:sta:sts=2

use base 'Exporter';
our @EXPORT = qw(tmpdir generate_file);

use Cwd;
use File::Spec;
use File::Path;
use File::Temp;
use IO::File;

sub tmpdir {
  File::Temp::tempdir(
    'MMD-XXXXXXXX',
    CLEANUP => 1,
    DIR => ($ENV{PERL_CORE} ? File::Spec->rel2abs(Cwd::cwd) : File::Spec->tmpdir),
  );
}

sub generate_file {
  my ($dir, $rel_filename, $content) = @_;

  File::Path::mkpath($dir) or die "failed to create '$dir'";
  my $abs_filename = File::Spec->catfile($dir, $rel_filename);

  Test::More::note("working on $abs_filename");

  my $fh = IO::File->new(">$abs_filename") or die "Can't write '$abs_filename'\n";
  print $fh $content;
  close $fh;

  return $abs_filename;
}

1;
