#!./perl

BEGIN {
  chdir 't' if -d 't';
  @INC = '../lib';
}

BEGIN {
  our @TEST = stat "TEST";
  our @README = stat "README";
  unless (@TEST && @README) {
    print "1..0 # Skip: no file TEST or README\n";
    exit 0;
  }
}

print "1..13\n";

use File::Compare qw(compare compare_text);

print "ok 1\n";

# named files, same, existing but different, cause an error
print "not " unless compare("README","README") == 0;
print "ok 2\n";

print "not " unless compare("TEST","README") == 1;
print "ok 3\n";

print "not " unless compare("README","HLAGHLAG") == -1;
                               # a file which doesn't exist
print "ok 4\n";

# compare_text, the same file, different but existing files
# cause error, test sub form.
print "not " unless compare_text("README","README") == 0;
print "ok 5\n";

print "not " unless compare_text("TEST","README") == 1;
print "ok 6\n";

print "not " unless compare_text("TEST","HLAGHLAG") == -1;
print "ok 7\n";

print "not " unless
  compare_text("README","README",sub {$_[0] ne $_[1]}) == 0;
print "ok 8\n";

# filehandle and same file
{
  my $fh;
  open ($fh, "<README") or print "not ";
  binmode($fh);
  print "not " unless compare($fh,"README") == 0;
  print "ok 9\n";
  close $fh;
}

# filehandle and different (but existing) file.
{
  my $fh;
  open ($fh, "<README") or print "not ";
  binmode($fh);
  print "not " unless compare_text($fh,"TEST") == 1;
  print "ok 10\n";
  close $fh;
}

# Different file with contents of known file,
# will use File::Temp to do this, skip rest of
# tests if this doesn't seem to work

my @donetests;
eval {
  require File::Temp; import File::Temp qw/ tempfile unlink0 /;

  my($tfh,$filename) = tempfile('fcmpXXXX', TMPDIR => 1);
  # NB. The trailing space is intentional (see [perl #37716])
  my $whsp = get_valid_whitespace();
  open my $tfhSP, ">", "$filename$whsp"
      or die "Could not open '$filename$whsp' for writing: $!";
  binmode($tfhSP);
  {
    local $/; #slurp
    my $fh;
    open($fh,'README');
    binmode($fh);
    my $data = <$fh>;
    print $tfh $data;
    close($fh);
    print $tfhSP $data;
    close($tfhSP);
  }
  seek($tfh,0,0);
  $donetests[0] = compare($tfh, 'README');
  if ($^O eq 'VMS') {
      unlink0($tfh,$filename);  # queue for later removal
      close $tfh;               # may not be opened shared
  }
  $donetests[1] = compare($filename, 'README');
  unlink0($tfh,$filename);
  $donetests[2] = compare('README', "$filename$whsp");
  unlink "$filename$whsp";
};
print "# problem '$@' when testing with a temporary file\n" if $@;

if (@donetests == 3) {
  print "not " unless $donetests[0] == 0;
  print "ok 11 # fh/file [$donetests[0]]\n";
  print "not " unless $donetests[1] == 0;
  print "ok 12 # file/file [$donetests[1]]\n";
  print "not " unless $donetests[2] == 0;
  print "ok 13 # ";
  print "TODO" if $^O eq "cygwin"; # spaces after filename silently trunc'd
  print " file/fileCR [$donetests[2]]\n";
}
else {
  print "ok 11# Skip\nok 12 # Skip\nok 13 # Skip Likely due to File::Temp\n";
}

sub get_valid_whitespace {
    return ' ' unless $^O eq 'VMS';
    return (exists $ENV{'DECC$EFS_CHARSET'} && $ENV{'DECC$EFS_CHARSET'} =~ /^[ET1]/i) 
           ? ' '
           : '_';  # traditional mode eats spaces in filenames
}
