#!/usr/bin/perl

# Check whether there are naming conflicts when names are truncated to
# the DOSish case-ignoring 8.3 format, plus other portability no-nos.

# The "8.3 rule" is loose: "if reducing the directory entry names
# within one directory to lowercase and 8.3-truncated causes
# conflicts, that's a bad thing".  So the rule is NOT the strict
# "no filename shall be longer than eight and a suffix if present
# not longer than three".

# TODO: this doesn't actually check for *directory entries*, what this
# does is to check for *MANIFEST entries*, which are only files, not
# directories.  In other words, a 8.3 conflict between a directory
# "abcdefghx" and a file "abcdefghy" wouldn't be noticed-- or even for
# a directory "abcdefgh" and a file "abcdefghy".

sub eight_dot_three {
    my ($dir, $base, $ext) = ($_[0] =~ m!^(?:(.+)/)?([^/.]+)(?:\.([^/.]+))?$!);
    my $file = $base . defined $ext ? ".$ext" : "";
    $base = substr($base, 0, 8);
    $ext  = substr($ext,  0, 3) if defined $ext;
    if ($dir =~ /\./)  {
	warn "$dir: directory name contains '.'\n";
    }
    if ($file =~ /[^A-Za-z0-9\._-]/) {
	warn "$file: filename contains non-portable characters\n";
    }
    if (length $file > 30) {
	warn "$file: filename longer than 30 characters\n"; # make up a limit
    }
    if (defined $dir) {
	return ($dir, defined $ext ? "$dir/$base.$ext" : "$dir/$base");
    } else {
	return ('.', defined $ext ? "$base.$ext" : $base);
    }
}

my %dir;

if (open(MANIFEST, "MANIFEST")) {
    while (<MANIFEST>) {
	chomp;
	s/\s.+//;
	unless (-f) {
	    warn "$_: missing\n";
	    next;
	}
	if (tr/././ > 1) {
	    print "$_: more than one dot\n";
	    next;
	}
	my ($dir, $edt) = eight_dot_three($_);
	($dir, $edt) = map { lc } ($dir, $edt);
	push @{$dir{$dir}->{$edt}}, $_;
    }
} else {
    die "$0: MANIFEST: $!\n";
}

for my $dir (sort keys %dir) {
    for my $edt (keys %{$dir{$dir}}) {
	my @files = @{$dir{$dir}->{$edt}};
	if (@files > 1) {
	    print "@files: directory $dir conflict $edt\n";
	}
    }
}
