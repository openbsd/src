# $arla: test.pl,v 1.1 2002/06/03 02:03:44 lha Exp $
use AAFS;
use Data::Dumper;

my $cell = AAFS::aafs_cell_create("e.kth.se");

my $vol = AAFS::aafs_volume_create($cell, "root.afs");

my $vldb = AAFS::aafs_volume_examine_nvldb($vol);

print "name ->>", $vldb->{'name'}, "<<- \n";

print Dumper($vldb);

my $info = AAFS::aafs_volume_examine_info($vol);

print Dumper($info);

my $query = {
    -server=>'mim.e.kth.se',
    -partition=>'/vicepe',
};

my $vldblist = AAFS::aafs_vldb_query($cell, $query);

print "Found ", $#$vldblist, " entries.\n";

foreach my $v (@$vldblist) {
    my $q = AAFS::aafs_volume_examine_nvldb($$v);
    print $q->{'name'}, ",";
}
print "\n";
