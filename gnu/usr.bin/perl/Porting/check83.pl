#!/usr/bin/perl -w

use strict;

# Check whether there are naming conflicts when names are truncated to
# the DOSish case-ignoring 8.3 format, plus other portability no-nos.

# The "8.3 rule" is loose: "if reducing the directory entry names
# within one directory to lowercase and 8.3-truncated causes
# conflicts, that's a bad thing".  So the rule is NOT the strict
# "no filename shall be longer than eight and a suffix if present
# not longer than three".

# The 8-level depth rule is for older VMS systems that likely won't
# even be able to unpack the tarball if more than eight levels 
# (including the top of the source tree) are present.

my %seen;
my $maxl = 30; # make up a limit for a maximum filename length

sub eight_dot_three {
    return () if $seen{$_[0]}++;
    my ($dir, $base, $ext) = ($_[0] =~ m{^(?:(.+)/)?([^/.]*)(?:\.([^/.]+))?$});
    my $file = $base . ( defined $ext ? ".$ext" : "" );
    $base = substr($base, 0, 8);
    $ext  = substr($ext,  0, 3) if defined $ext;
    if (defined $dir && $dir =~ /\./)  {
	print "directory name contains '.': $dir\n";
    }
    if ($base eq "") {
	print "filename starts with dot: $_[0]\n";
    }
    if ($file =~ /[^A-Za-z0-9\._-]/) {
	print "filename contains non-portable characters: $_[0]\n";
    }
    if (length $file > $maxl) {
	print "filename longer than $maxl characters: $file\n";
    }
    if (defined $dir) {
	return ($dir, defined $ext ? "$dir/$base.$ext" : "$dir/$base");
    } else {
	return ('.', defined $ext ? "$base.$ext" : $base);
    }
}

my %dir;

if (open(MANIFEST, '<', 'MANIFEST')) {
    while (<MANIFEST>) {
	chomp;
	s/\s.+//;
	unless (-f) {
	    print "missing: $_\n";
	    next;
	}
	if (tr/././ > 1) {
	    print "more than one dot: $_\n";
	    next;
	}
	if ((my $slashes = $_ =~ tr|\/|\/|) > 7) {
	    print "more than eight levels deep: $_\n";
	    next;
	}
	while (m!/|\z!g) {
	    my ($dir, $edt) = eight_dot_three("$`");
	    next unless defined $dir;
	    ($dir, $edt) = map { lc } ($dir, $edt);
	    push @{$dir{$dir}->{$edt}}, $_;
	}
    }
} else {
    die "$0: MANIFEST: $!\n";
}

for my $dir (sort keys %dir) {
    for my $edt (keys %{$dir{$dir}}) {
	my @files = @{$dir{$dir}{$edt}};
	if (@files > 1) {
	    print "conflict on filename $edt:\n", map "    $_\n", @files;
	}
    }
}
