# This will put installed perl files into some other location
# Note that we cannot put hashbang to be extproc to make Configure work.

use Config;

$dir = shift;
$dir =~ s|/|\\|g ;
$nowarn = 1, $dir = shift if $dir eq '-n';

die <<EOU unless defined $dir and -d $dir;
usage:	$^X $0 [-n] directory-to-install
  -n	do not check whether the directory is not on path
EOU

@path = split /;/, $ENV{PATH};
$idir = $Config{installbin};
$indir =~ s|\\|/|g ;

my %seen;

foreach $file (<$idir/*>) {
  next if $file =~ /\.(exe|bak)/i;
  $base = $file;
  $base =~ s/\.$//;		# just in case...
  $base =~ s|.*/||;
  $base =~ s|\.pl$||;
  #$file =~ s|/|\\|g ;
  warn "Clashing output name for $file, skipping" if $seen{$base}++;
  print "Processing $file => $dir\\$base.cmd\n";
  open IN, '<', $file or warn, next;
  open OUT, '>', "$dir/$base.cmd" or warn, next;
  my $firstline = <IN>;
  my $flags = '';
  $flags = $2 if $firstline =~ /^#!\s*(\S+)\s+-([^#]+?)\s*(#|$)/;
  print OUT "extproc perl -S$flags\n$firstline";
  print OUT $_ while <IN>;
  close IN or warn, next;
  close OUT or warn, next;
}

