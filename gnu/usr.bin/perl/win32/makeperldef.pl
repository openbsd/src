my $CCTYPE = "";
print "EXPORTS\n";
foreach (@ARGV) {
	if (/CCTYPE=(.*)$/) {
		$CCTYPE = $1;
		next;
	}
	emit_symbol("boot_$_");
}

sub emit_symbol {
	my $symbol = shift;
	if ($CCTYPE eq "BORLAND") {
		# workaround Borland quirk by export both the straight
		# name and a name with leading underscore
		print "\t$symbol=_$symbol\n";
		print "\t_$symbol\n";
	}
	else {
		print "\t$symbol\n";
	}
}

