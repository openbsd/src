#!/usr/bin/perl -w
#
# This script uses embedded formatting directives in the lynx.cfg file to
# guide it in extracting comments and related information to construct a
# set of HTML files.  Comments begin with '#', and directives with '.'.
# Directives implemented:
#
#	h1 {Text}
#		major heading.  You may specify the same major heading in
#		more than one place.
#	h2 {Text}
#		minor heading, i.e. a keyword.
#	ex [number]
#		the following line(s) are an example.  The [number] defaults
#		to 1.
#	nf [number]
#		turn justification off for the given number of lines, defaulting
#		to the remainder of the file.
#	fi
#		turn justification back on
#

require "getopts.pl";

# Options:
#	-a	show all options, not only those that are available.
#	-m	mark unavailable options with an '*'.  Data for this is read
#		from standard input.
#	-s	sort entries in body.html
&Getopts('ams');

if ( defined $opt_m ) {
	@settings_ = <STDIN>;
	%settings_avail = ();
	foreach $l (@settings_) {
		chop $l;
		if ($l =~ /^[a-zA-Z_][a-zA-Z_0-9]*$/) {
			$settings_avail{uc $l} = 1;
		}
	}
} else {
	$opt_a = 1;
}

# This sub tells whether the support for the given setting was enabled at
# compile time.
sub ok {
	local ($name) = @_;
	local ($ret) = defined $opt_a || defined($settings_avail{uc $name})+0;
	$ret;
}


if ( $#ARGV < 0 ) {
	&doit("lynx.cfg");
} else {
	while ( $#ARGV >= 0 ) {
		&doit ( shift @ARGV );
	}
}
exit (0);


# process a Lynx configuration-file
sub doit {
	local ($name) = @_;

	# Ignore our own backup files
	if ( $name =~ ".*~" ) {
		return;
	}

	# Read the file into an array in memory.
	open(FP,$name) || do {
		print STDERR "Can't open $name: $!\n";
		return;
	};
	local(@input) = <FP>;
	close(FP);

	for $n (0..$#input) {
		chop $input[$n]; # trim newlines
		$input[$n] =~ s/\s*$//; # trim trailing blanks
		$input[$n] =~ s/^\s*//; # trim leading blanks
	}

	&gen_alphatoc(@input);
	@cats = &gen_cattoc(@input);
	&gen_body(@input);
}

sub gen_alphatoc {
	local(@input) = @_;
	local (@minor);
	local ($n, $m, $c, $d);
	local ($output="alphatoc.html");
	open(FP,">$output") || do {
		print STDERR "Can't open $output: $!\n";
		return;
	};
	print FP <<'EOF';
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">
<html>
<head>
<link rev="made" href="mailto:lynx-dev@sig.net">
<title>Settings by name</title>
</head>
<body>
<h1>Alphabetical table of settings</h1>
EOF
	$m=0;
	for $n (0..$#input) {
		if ( $input[$n] =~ /^\.h2\s*[A-Z][A-Z0-9_]*$/ ) {
			$minor[$m] = $input[$n];
			$minor[$m] =~ s/^.h2\s*//;
			$m++ if (ok($minor[$m]) || defined $opt_a);
		}
	}
	@minor = sort @minor;
	# index by the first character of each keyword
	$c=' ';
	for $n (0..$#minor) {
		$d = substr($minor[$n],0,1);
		if ($d ne $c) {
			printf FP "<a href=\"#%s\">%s</a> \n", $d, $d;
			$c=$d;
		}
	}
	# index by the first character of each keyword
	$c=' ';
	for $n (0..$#minor) {
		$d = substr($minor[$n],0,1);
		if ($d ne $c) {
			printf FP "<h2><a name=%s>%s</a></h2>\n", $d, $d;
			$c=$d;
		}
		local ($avail = ok($minor[$n]));
		local ($mark = !$avail && defined $opt_m ? "*" : "");
		if (defined $opt_a || $avail) {
		    printf FP "<a href=\"body.html#%s\">%s</a>&nbsp;&nbsp;\n", $minor[$n], $minor[$n] . $mark;
		};
	}
	$str = <<'EOF'
<p>
<a href=cattoc.html>To list of settings by category</a>
EOF
. (defined $opt_a && defined $opt_m ?
"<p>Support for all settings suffixed with '*' was disabled at compile time.\n" :
 "") . <<'EOF'
</body>
</html>
EOF
	;print FP $str;
	close(FP);
}

# This uses the associative array $cats{} to store HREF values pointing into
# the cattoc file.
#
# We could generate this file in alphabetic order as well, but choose to use
# the order of entries in lynx.cfg, since some people expect that arrangement.
sub gen_body {
	local(@input) = @_;
	local ($n, $m, $c, $p, $h1, $h2, $any);
	local ($output="body.html");
	open(FP,">$output") || do {
		print STDERR "Can't open $output: $!\n";
		return;
	};
	print FP <<'EOF';
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">
<html>
<head>
<link rev="made" href="mailto:lynx-dev@sig.net">
<title>Description of settings in lynx configuration file</title>
</head>
<body>
EOF
	$d = -1;
	$p = 0;
	$m = 0;
	$h1 = "";
	$sp = ' ';
	$ex = 0;
	$nf = 0;
	$any = 0;
	$next = 0;
	$left = 0;
	undef %keys;

	local (@optnames);
	local (%optname_to_fname);#this maps optname to fname - will be used
	    #for alphabetical output of the content
	local ($curfilename = "tmp000");#will be incremented each time
	local ($tmpdir = "./");#temp files will be created there
	close(FP);

	for $n (0..$#input) {
		if ( $next ) {
			$next--;
			next;
		}
		$c = $input[$n];
		$count = $#input;
		$once = 1;
		if ( $c =~ /^\.h1\s/ ) {
			$h1 = 1;
			$h1 = $c;
			$h1 =~ s/^.h1\s*//;
			$m = 0;
			$first = 1;
			undef %keys;
			next;
		} elsif ( $c =~ /^\.h2\s/ ) {
			$c =~ s/^.h2\s*//;
			$h2[$m] = $c;
			$keys{$c} = 1;
			$m++;
			next;
		} elsif ( $c =~ /^\./ ) {
			$s = $c;
			$s =~ s/^\.[a-z]+\s//;
			if ( $s =~ /^[0-9]+$/ ) {
				$count = $s;
				$once = $s;
			}
		}
		if ( $c =~ /^\.ex/ ) {
			$ex = $once;
			printf FP "<h3><em>Example%s:</em></h3>\n", $ex > 1 ? "s" : "";
		} elsif ( $c =~ /^\.nf/ ) {
			printf FP "<pre>\n";
			$nf = $count;
		} elsif ( $c =~ /^\.fi/ ) {
			printf FP "</pre>\n";
			$nf = 0;
		} elsif ( $c =~ /^$/ ) {
			if ( $m > 1 ) {
				for $j (1..$#h2) {
					close(FP);++$curfilename;
					push @optnames,$h2[$j];
					open(FP,">$tmpdir/$curfilename") || do {
						print STDERR "Can't open tmpfile: $!\n";
						return;
					};
					$optname_to_fname{$h2[$j]} = $curfilename;

					printf FP "<hr>\n";
					printf FP "<h2><kbd><a name=\"%s\">%s</a></kbd>\n", $h2[$j], $h2[$j];
					if ( $h1 ne "" ) {
						printf FP " - <a href=\"cattoc.html#%s\">%s</a>", $cats{$h1}, $h1;
					}
					printf FP "</h2>\n";
					printf FP "<h3><em>Description</em></h3>\n";
					printf FP "Please see the description of <a href=\"#%s\">%s</a>\n", $h2[0], $h2[0];
				}
				@h2 = "";
			}
			$m = 0;
			$first = 1;
		} elsif ( $c =~ /^[#A-Za-z]/ && $m != 0 ) {
			if ( $first ) {
				close(FP);++$curfilename;
				push @optnames,$h2[0];
				open(FP,">$tmpdir/$curfilename") || do {
				    print STDERR "Can't open tmpfile: $!\n";
				    return;
				};
				$optname_to_fname{$h2[0]} = $curfilename;

				if ( $any ) {
					printf FP "<hr>\n";
				}
				printf FP "<h2><kbd><a name=\"%s\">%s</a></kbd>\n", $h2[0], $h2[0];
				if ( $h1 ne "" ) {
					printf FP " - <a href=\"cattoc.html#%s\">%s</a>", $cats{$h1}, $h1;
				}
				printf FP "</h2>\n";
				printf FP "<h3><em>Description</em></h3>\n";
				$any++;
				$first = 0;
			}

			# Convert tabs first, to retain relative alignment
			$c =~ s#^\t#' 'x8#e;
			while ( $c =~ /\t/ ) {
				$c =~ s#(^[^\t]+)\t#$1 . $sp x (9 - (length($1) % 8 ))#e;
			}

			# Strip off the comment marker
			$c =~ s/^#//;

			# and convert simple expressions:
			$c =~ s/&/&amp;/g;
			$c =~ s/>/&gt;/g;
			$c =~ s/</&lt;/g;
			#hvv - something wrong was with next statement
			$c =~ s/'([^ ])'/"<strong>$1<\/strong>"/g;

			# Do a line-break each time the margin changes.  We 
			# could get fancier, but HTML doesn't really support
			# text-formatting, and we'll use what it does have to
			# do wrapping.
			if ( ! $nf ) {
				$t = $c;
				$t =~ s/(\s*).*/$1/;
				$t = length $t;
				if ( $t != $left ) {
					$left = $t;
					printf FP "<br>\n";
				}
			}

			$k = 0;
			if ( $c =~ /^[a-zA-Z_]+:/ ) {
				$t = $c;
				$t =~ s/:.*//;
				$k = $keys{$t};
			}

			if ( $c =~ /^$/ ) {
				if ( $nf ) {
					printf FP "\n";
				} else {
					$p = 1;
				}
			} elsif ( $ex != 0 ) {
				printf FP "<br><code>%s</code><br>\n", $c;
				$ex--;
			} elsif ( $k ) {
				if ( $d != $n && ! $nf ) {
					printf FP "<h3><em>Default value</em></h3>\n";
				}
				$c =~ s/:$/:<em>none<\/em>/;
				$c =~ s/:/<\/code>:<code>/;
				$c = "<code>" . $c . "</code>";
				if ( ! $nf ) {
					$c .= "<br>";
				}
				printf FP "%s\n", $c;
				$d = $n + 1;
			} else {
				if ( $p && ! $nf ) {
					printf FP "<p>\n";
				}
				$p = 0;
				if ( $input[$n+1] =~ /^#\s*==/ ) {
					$c = "<br><em>$c</em>";
					if ( ! $nf ) {
						$c .= "<br>";
					}
					$next++;
				}
				printf FP "%s\n", $c;
			}
			if ( $nf != 0 && $nf-- == 0 ) {
				printf FP "</pre>\n";
			}
		}
	}
	close(FP);
	# Here we collect files with description of needed lynx.cfg
	# options in the proper (natural or sorted) order.
	open(FP,">>$output") || do {
		print STDERR "Can't open $output: $!\n";
		return;
	};
	{
	    local (@ordered = (defined $opt_s ? (sort keys(%optname_to_fname)) : @optnames));
	    if (defined $opt_s) {
		print FP "Options are sorted by name.\n";
	    } else {
		print FP "Options are in the same order as lynx.cfg.\n";
	    }
	    foreach $l (@ordered) {
		local ($fnm = $tmpdir . $optname_to_fname{$l});
		open(FP1,"<$fnm") || do {
		    print STDERR "Can't open $fnm: $!\n";
		    return;
		};
		local ($avail = ok($l));
		if (defined $opt_a || $avail) {
		    local(@lines) = <FP1>;
		    print FP @lines;
		    if (!$avail && defined $opt_m) {
			print FP <<'EOF';
<p>Support for this setting was disabled at compile-time.
EOF
		    }
		}
		close(FP1);
	    }
	    foreach $l (values(%optname_to_fname)) {
		unlink $l;
	    }
	}

	print FP <<'EOF';
</body>
</html>
EOF
	close(FP);
}

sub gen_cattoc {
	local(@input) = @_;
	local (@major);
	local (@descs);
	local (@index);
	local ($n, $m, $c, $d, $found, $h1);
	local ($output="cattoc.html");

	open(FP,">$output") || do {
		print STDERR "Can't open $output: $!\n";
		return;
	};
	print FP <<'EOF';
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">
<html>
<head>
<link rev="made" href="mailto:lynx-dev@sig.net">
<title>Settings by category</title>
</head>
<body>
<h1>Settings by category</h1>
These are the major categories of configuration settings in Lynx:
<ul>
EOF
	$m = -1;
	$h1 = 0;
	for $n (0..$#input) {
		if ( $input[$n] =~ /^\.h1\s/ ) {
			$h1 = 1;
			$c = $input[$n];
			$c =~ s/^.h1\s*//;
			$m = $#major + 1;
			$d = 0;
			$found = 0;
			while ( $d <= $#major && ! $found ) {
				if ( $major[$d] eq $c ) {
					$m = $d;
					$found = 1;
				}
				$d++;
			}
			if ( ! $found ) {
				$major[$m] = $c;
				$descs{$major[$m]} = "";
				$index{$major[$m]} = "";
			}
		} elsif ( $h1 != 0 ) {
			if ( $input[$n] =~ /^$/ ) {
				$h1 = 0;
			} elsif ( $input[$n] =~ /^\s*#/ ) {
				$c = $input[$n];
				$c =~ s/^\s*#\s*//;
				$descs{$major[$m]} .= $c . "\n";
			}
		}
		if ( $m >= 0 && $input[$n] =~ /^\.h2\s/ ) {
			$c = $input[$n];
			$c =~ s/^.h2\s*//;
			$index{$major[$m]} .= $c . "\n"
			    if (defined $opt_a || ok($c));
		}
	}
	@major = sort @major;
	for $n (0..$#major) {
		$cats{$major[$n]} = sprintf("header%03d", $n);
		printf FP "<li><a href=\"#%s\">%s</a>\n", $cats{$major[$n]}, $major[$n];
	}
	printf FP "</ul>\n";
	for $n (0..$#major) {
		printf FP "\n";
		printf FP "<h2><a name=\"%s\">%s</a></h2>\n", $cats{$major[$n]}, $major[$n];
		if ($descs{$major[$n]} !~ /^$/) {
			printf FP "<h3>Description</h3>\n%s\n", $descs{$major[$n]};
		}
		$c = $index{$major[$n]};
		if ( $c ne "" ) {
			@c = split(/\n/, $c);
			@c = sort @c;
			printf FP "<p>Here is a list of settings that belong to this category\n";
			printf FP "<ul>\n";
			for $m (0..$#c) {
				local($avail = ok($c[$m]));
				local($mark = !$avail && defined $opt_m ? "*" : "");
				printf FP "<li><a href=\"body.html#%s\">%s</a>\n", $c[$m], $c[$m] . $mark;
			}
			printf FP "</ul>\n";
		}
	}
	$str = <<'EOF'
<p>
<a href=alphatoc.html>To list of settings by name</a>
EOF
. (defined $opt_a && defined $opt_m ?
"<p>Support for all settings suffixed with '*' was disabled at compile time." :
 "") . <<'EOF'
</body>
</html>
EOF
	;print FP $str;
	close(FP);
	return @cats;
}
