use DirHandle;
use AutoSplit;

sub splitthis {
my ($top,$base,$dest) = @_;
my $d = new DirHandle $base;
if (defined $d) {
	while (defined($_ = $d->read)) {
		next if $_ eq ".";
		next if $_ eq "..";
		my $entry = "$base\\$_";
		my $entrywithouttop = $entry;
		$entrywithouttop =~ s/^$top//;
		if (-d $entry) {splitthis ($top,$entry,$dest);}
		else { 
			next unless ($entry=~/pm$/i);
			#print "Will run autosplit on $entry to $dest\n";
			autosplit($entry,$dest,0,1,1);
			};
		};
	};
}

splitthis $ARGV[0],$ARGV[0],$ARGV[1];
