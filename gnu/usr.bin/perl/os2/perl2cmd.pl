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

foreach $file (<$idir/*>) {
  next if $file =~ /\.exe/i;
  $base = $file;
  $base =~ s/\.$//;		# just in case...
  $base =~ s|.*/||;
  $file =~ s|/|\\|g ;
  print "Processing $file => $dir\\$base.cmd\n";
  system 'cmd.exe', '/c', "echo extproc perl -S>$dir\\$base.cmd";
  system 'cmd.exe', '/c', "type $file >> $dir\\$base.cmd";
}

