package OpenBSD::Getopt;
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(getopts);

sub handle_option
{
	my ($opt, $hash, $params) = @_;

	$params = 1 unless defined $params;
	if (defined $hash->{$opt} and ref($hash->{$opt}) eq 'CODE') {
		&{$hash->{$opt}}($params);
	} else {
		${"opt_$opt"} = $params;
		push(@EXPORT, "\$opt_$opt");
		$hash->{$opt} = $params;
	}
}

sub getopts($;$) 
{
    my ($args, $hash) = @_;

    $hash = {} unless defined $hash;
    local @EXPORT;

    while ($_ = shift @ARGV) {
    	last if /^--$/;
    	unless (m/^-(.)(.*)/s) {
		unshift @ARGV, $_;
		last;
	}
	my ($opt, $other) = ($1, $2);
	if ($args =~ m/\Q$opt\E(\:)?/) {
		if ($1 eq ':') {
			if ($other eq '') {
				die "no argument for option $opt" unless @ARGV;
				$other = shift @ARGV;
			}
			handle_option($opt, $hash, $other);
		} else {
			handle_option($opt, $hash);
			if ($other ne '') {
				$_ = "-$other";
				redo;
			}
		}
	} else {
		die "Unknown option $opt";
	}
    }
    local $Exporter::ExportLevel = 1;
    import OpenBSD::Getopt;
    return $hash;
}

1;
