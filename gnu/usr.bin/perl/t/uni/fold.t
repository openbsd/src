BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use File::Spec;

my $CF = File::Spec->catfile(File::Spec->catdir(File::Spec->updir,
					       "lib", "unicore"),
			    "CaseFolding.txt");

use constant EBCDIC => ord 'A' == 193;

if (open(CF, $CF)) {
    my @CF;

    while (<CF>) {
	# Skip S since we are going for 'F'ull case folding.  I is obsolete starting
	# with Unicode 3.2, but leaving it in does no harm, and allows backward
	# compatibility
        if (/^([0-9A-F]+); ([CFI]); ((?:[0-9A-F]+)(?: [0-9A-F]+)*); \# (.+)/) {
	    next if EBCDIC && hex $1 < 0x100;
	    push @CF, [$1, $2, $3, $4];
	}
    }

    close(CF);

    die qq[$0: failed to find casefoldings from "$CF"\n] unless @CF;

    print "1..", scalar @CF, "\n";

    my $i = 0;
    for my $cf (@CF) {
	my ($code, $status, $mapping, $name) = @$cf;
	$i++;
	my $a = pack("U0U*", hex $code);
	my $b = pack("U0U*", map { hex } split " ", $mapping);
	my $t0 = ":$a:" =~ /:$a:/    ? 1 : 0;
	my $t1 = ":$a:" =~ /:$a:/i   ? 1 : 0;
	my $t2 = ":$a:" =~ /:[$a]:/  ? 1 : 0;
	my $t3 = ":$a:" =~ /:[$a]:/i ? 1 : 0;
	my $t4 = ":$a:" =~ /:$b:/i   ? 1 : 0;
	my $t5 = ":$a:" =~ /:[$b]:/i ? 1 : 0;
	my $t6 = ":$b:" =~ /:$a:/i   ? 1 : 0;
	my $t7 = ":$b:" =~ /:[$a]:/i ? 1 : 0;
	print $t0 && $t1 && $t2 && $t3 && $t4 && $t5 && $t6 && $t7 ?
	    "ok $i \# - $code - $name - $mapping - $status\n" :
	    "not ok $i \# - $code - $name - $mapping - $status - $t0 $t1 $t2 $t3 $t4 $t5 $t6 $t7\n";
    }
} else {
    die qq[$0: failed to open "$CF": $!\n];
}
