#!./perl 

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    $ENV{PERL5LIB} = '../lib';
    require './test.pl';
}

$| = 1;

my $Is_VMS = $^O eq 'VMS';
my $Is_MSWin32 = $^O eq 'MSWin32';
my $Is_NetWare = $^O eq 'NetWare';
my $i = 0 ;

my @prgs = () ;

foreach (sort glob("lib/strict/*")) {

    next if -d || /(~|\.orig|,v)$/;

    open F, "<$_" or die "Cannot open $_: $!\n" ;
    while (<F>) {
	last if /^__END__/ ;
    }

    {
        local $/ = undef;
        @prgs = (@prgs, split "\n########\n", <F>) ;
    }
    close F or die "Could not close: $!" ;
}

undef $/;

print "1.." . (@prgs + 4) . "\n";
 
 
for (@prgs){
    my $switch = "";
    my @temps = () ;
    if (s/^\s*-\w+//){
        $switch = $&;
    }
    my($prog,$expected) = split(/\nEXPECT\n/, $_);
    if ( $prog =~ /--FILE--/) {
        my(@files) = split(/\n--FILE--\s*([^\s\n]*)\s*\n/, $prog) ;
	shift @files ;
	die "Internal error test $i didn't split into pairs, got " . 
		scalar(@files) . "[" . join("%%%%", @files) ."]\n"
	    if @files % 2 ;
	while (@files > 2) {
	    my $filename = shift @files ;
	    my $code = shift @files ;
    	    push @temps, $filename ;
	    open F, ">$filename" or die "Cannot open $filename: $!\n" ;
	    print F $code ;
	    close F or die "Could not close: $!" ;
	}
	shift @files ;
	$prog = shift @files ;
    }
    my $tmpfile = tempfile();
    open TEST, ">$tmpfile" or die "Could not open: $!";
    print TEST $prog,"\n";
    close TEST or die "Could not close: $!";
    my $results = $Is_MSWin32 ?
	              `.\\perl -I../lib $switch $tmpfile 2>&1` :
                  $^O eq 'NetWare' ?
		      `perl -I../lib $switch $tmpfile 2>&1` :
                  `$^X $switch $tmpfile 2>&1`;
    my $status = $?;
    $results =~ s/\n+$//;
    # allow expected output to be written as if $prog is on STDIN
    $results =~ s/tmp\d+[A-Z][A-Z]?/-/g;
    $results =~ s/\n%[A-Z]+-[SIWEF]-.*$// if $Is_VMS;  # clip off DCL status msg
    $expected =~ s/\n+$//;
    my $prefix = ($results =~ s/^PREFIX\n//) ;
    my $TODO = $prog =~ m/^#\s*TODO:/;
    if ( $results =~ s/^SKIPPED\n//) {
	print "$results\n" ;
    }
    elsif (($prefix and $results !~ /^\Q$expected/) or
	   (!$prefix and $results ne $expected)){
        if (! $TODO) {
            print STDERR "PROG: $switch\n$prog\n";
            print STDERR "EXPECTED:\n$expected\n";
            print STDERR "GOT:\n$results\n";
        }
        print "not ";
    }
    print "ok " . ++$i . ($TODO ? " # TODO" : "") . "\n";
    foreach (@temps) 
	{ unlink $_ if $_ } 
}

eval qq(use strict 'garbage');
print +($@ =~ /^Unknown 'strict' tag\(s\) 'garbage'/)
	? "ok ".++$i."\n" : "not ok ".++$i."\t# $@";

eval qq(no strict 'garbage');
print +($@ =~ /^Unknown 'strict' tag\(s\) 'garbage'/)
	? "ok ".++$i."\n" : "not ok ".++$i."\t# $@";

eval qq(use strict qw(foo bar));
print +($@ =~ /^Unknown 'strict' tag\(s\) 'foo bar'/)
	? "ok ".++$i."\n" : "not ok ".++$i."\t# $@";

eval qq(no strict qw(foo bar));
print +($@ =~ /^Unknown 'strict' tag\(s\) 'foo bar'/)
	? "ok ".++$i."\n" : "not ok ".++$i."\t# $@";
