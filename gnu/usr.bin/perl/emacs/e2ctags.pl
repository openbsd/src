
##e2ctags.pl
##Convert an Emacs-style TAGS file to a standard ctags file.
##Runs in a single pass over the TAGS file and keeps the first
##tag entry found, and the file name and line number the tag can
##be found on.
##Then it opens all relevant files and builds the regular expression
##for ctags.
##Run over a few test files and compared with a real ctags file shows
##only extra tags in the translated file, which probably won't hurt
##vi.
##

use strict;

my $filename;
my ($tag,$line_no,$line);
my %tags = ();
my %filetags = ();
my %files = ();
my @lines = ();

while (<>) {
  if ($_ eq "\x0C\n") {
    ##Grab next line and parse it for the filename
    $_ = <>;
    chomp;
    s/,\d+$//;
    $filename = $_;
    ++$files{$filename};
    next;
  }
  ##Figure out how many records in this line and
  ##extract the tag name and the line that it is found on
  next if /struct/;
  if (/\x01/) {
    ($tag,$line_no) = /\x7F(\w+)\x01(\d+)/;
  }
  else {
    tr/(//d;
    ($tag,$line_no) = /(\w+)\s*\x7F(\d+),/;
  }
  next unless $tag;
  ##Take only the first entry per tag
  next if defined($tags{$tag});
  $tags{$tag}{FILE} = $filename;
  $tags{$tag}{LINE_NO} = $line_no;
  push @{$filetags{$filename}}, $tag;
}

foreach $filename (keys %files) {
  open FILE, $filename or die "Couldn't open $filename: $!\n";
  @lines = <FILE>;
  close FILE;
  chomp @lines;
  foreach $tag ( @{$filetags{$filename}} ) {
    $line = $lines[$tags{$tag}{LINE_NO}-1];
    if (length($line) >= 50) {
      $line = substr($line,0,50);
    }
    else {
      $line .= '$';
    }
    $line =~ s#\\#\\\\#;
    $tags{$tag}{LINE} = join '', '/^',$line,'/';
  }
}

foreach $tag ( sort keys %tags ) {
  print "$tag\t$tags{$tag}{FILE}\t$tags{$tag}{LINE}\n";
}
