#!/usr/bin/perl -w
# $LynxId: cfg2html.pl,v 1.21 2014/01/08 22:49:46 tom Exp $
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
#	url text
#		embed an HREF to external site.
#
use strict;

use Getopt::Std;

use vars qw($opt_a $opt_m $opt_s);

use vars qw(@cats);
use vars qw(%cats);

use vars qw(@settings_avail);
use vars qw(%settings_avail);

# Options:
#	-a	show all options, not only those that are available.
#	-m	mark unavailable options with an '*'.  Data for this is read
#		from standard input.
#	-s	sort entries in body.html
&getopts('ams');

if ( defined $opt_m ) {
    my $l;
    my @settings_ = <STDIN>;
    %settings_avail = ();
    foreach $l (@settings_) {
        chop $l;
        if ( $l =~ /^[[:alpha:]_][[:alnum:]_]*$/ ) {
            $settings_avail{ uc $l } = 1;
        }
    }
}
else {
    $opt_a = 1;
}

# This sub tells whether the support for the given setting was enabled at
# compile time.
sub ok {
    my ($name) = @_;
    my $ret = defined( $settings_avail{ uc $name } ) + 0;
    $ret;
}

if ( $#ARGV < 0 ) {
    &doit("lynx.cfg");
}
else {
    while ( $#ARGV >= 0 ) {
        &doit( shift @ARGV );
    }
}
exit(0);

# process a Lynx configuration-file
sub doit {
    my ($name) = @_;
    my $n;

    # Ignore our own backup files
    if ( $name =~ ".*~" ) {
        return;
    }

    # Read the file into an array in memory.
    open( FP, $name ) || do {
        print STDERR "Can't open $name: $!\n";
        return;
    };
    my (@input) = <FP>;
    close(FP);

    for $n ( 0 .. $#input ) {
        chop $input[$n];    # trim newlines
        $input[$n] =~ s/\s*$//;    # trim trailing blanks
        $input[$n] =~ s/^\s*//;    # trim leading blanks
    }

    &gen_alphatoc(@input);
    @cats = &gen_cattoc(@input);
    &gen_body(@input);
}

sub gen_alphatoc {
    my (@input) = @_;
    my @minor;
    my ( $n, $m, $c, $d, $need_p );
    my $output = "alphatoc.html";
    open( FP, ">$output" ) || do {
        print STDERR "Can't open $output: $!\n";
        return;
    };
    print FP <<'EOF';
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
<link rev="made" href="mailto:lynx-dev@nongnu.org">
<title>lynx.cfg settings by name</title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<meta name="description" content=
"This is a table of Lynx's settings in lynx.cfg, listed alphabetically.  Some settings are disabled at compile-time.">
</head>
<body>
EOF
    $m = 0;
    for $n ( 0 .. $#input ) {
        if ( $input[$n] =~ /^\.h2\s*[[:upper:]][[:upper:][:digit:]_]*$/ ) {
            $minor[$m] = $input[$n];
            $minor[$m] =~ s/^.h2\s*//;
            $m++ if ( ok( $minor[$m] ) || defined $opt_a );
        }
    }
    @minor = sort @minor;

    # index by the first character of each keyword
    $c      = ' ';
    $need_p = 1;
    for $n ( 0 .. $#minor ) {
        $d = substr( $minor[$n], 0, 1 );
        if ( $d ne $c ) {
            if ($need_p) {
                printf FP "<p>";
                $need_p = 0;
            }
            printf FP "<a href=\"#%s\">%s</a> \n", $d, $d;
            $c = $d;
        }
    }

    # index by the first character of each keyword
    $c = ' ';
    for $n ( 0 .. $#minor ) {
        $d = substr( $minor[$n], 0, 1 );
        if ( $d ne $c ) {
            printf FP "<h2><a name=%s>%s</a></h2>\n", $d, $d;
            $need_p = 1;
            $c      = $d;
        }
        my $avail = ok( $minor[$n] );
        my $mark = ( !$avail && defined $opt_m ) ? "*" : "";
        if ( defined $opt_a || $avail ) {
            if ($need_p) {
                printf FP "<p>";
                $need_p = 0;
            }
            printf FP "<a href=\"body.html#%s\">%s</a>&nbsp;&nbsp;\n",
              $minor[$n], $minor[$n] . $mark;
        }
    }
    my $str = <<'EOF'
<p>
<a href=cattoc.html>To list of settings by category</a>
EOF
      . (
        defined $opt_a && defined $opt_m
        ? "<p>Support for all settings suffixed with '*' was disabled at compile time.\n"
        : ""
      )
      . <<'EOF'
</body>
</html>
EOF
      ;
    print FP $str;
    close(FP);
}

# This uses the associative array $cats{} to store HREF values pointing into
# the cattoc file.
#
# We could generate this file in alphabetic order as well, but choose to use
# the order of entries in lynx.cfg, since some people expect that arrangement.
sub gen_body {
    my @input = @_;
    my ( $n, $c );
    my @h2;
    my $output = "body.html";
    open( FP, ">$output" ) || do {
        print STDERR "Can't open $output: $!\n";
        return;
    };
    print FP <<'EOF';
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
<link rev="made" href="mailto:lynx-dev@nongnu.org">
<title>Description of settings in lynx configuration file</title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<meta name="description" content=
"This is a list of each of Lynx's settings in lynx.cfg, with full description and their default values indicated.">
</head>
<body>
EOF
    my $l;
    my $t;
    my $d     = -1;
    my $p     = 0;
    my $m     = 0;
    my $h1    = "";
    my $sp    = ' ';
    my $ex    = 0;
    my $nf    = 0;
    my $any   = 0;
    my $first = 0;
    my $next  = 0;
    my $left  = 0;
    my $needp = 0;
    my %keys;
    undef %keys;

    my @optnames;
    my %optname_to_fname;    #this maps optname to fname - will be used
                             #for alphabetical output of the content
    my $curfilename = "tmp000";    #will be incremented each time
    my $tmpdir      = "./";        #temp files will be created there
    close(FP);

    for $n ( 0 .. $#input ) {
        if ($next) {
            $next--;
            next;
        }
        $c = $input[$n];
        my $count = $#input;
        my $once  = 1;
        if ( $c =~ /^\.h1\s/ ) {
            $h1 = 1;
            $h1 = $c;
            $h1 =~ s/^.h1\s*//;
            $m     = 0;
            $first = 1;
            undef %keys;
            next;
        }
        elsif ( $c =~ /^\.h2\s/ ) {
            $c =~ s/^.h2\s*//;
            $h2[$m] = $c;
            $keys{$c} = 1;
            $m++;
            next;
        }
        elsif ( $c =~ /^\./ ) {
            my $s = $c;
            $s =~ s/^\.[[:lower:]]+\s//;
            if ( $s =~ /^[[:digit:]]+$/ ) {
                $count = $s;
                $once  = $s;
            }
        }
        if ( $c =~ /^\.ex/ ) {
            $ex = $once;
            printf FP "<h3><em>Example%s:</em></h3>\n", $ex > 1 ? "s" : "";
            $needp = 1;
        }
        elsif ( $c =~ /^\.url/ ) {
            my $url = $c;
            $url =~ s/^\.url\s+//;
            printf FP "<blockquote><p><a href=\"%s\">%s</a></p></blockquote>\n",
              $url, $url;
            $needp = 1;
        }
        elsif ( $c =~ /^\.nf/ ) {
            $needp = 0;
            printf FP "<pre>\n";
            $nf = $count;
        }
        elsif ( $c =~ /^\.fi/ ) {
            printf FP "</pre>\n";
            $nf    = 0;
            $needp = 1;
        }
        elsif ( $c =~ /^$/ ) {
            if ( $m > 1 ) {
                my $j;
                for $j ( 1 .. $#h2 ) {
                    close(FP);
                    ++$curfilename;
                    push @optnames, $h2[$j];
                    open( FP, ">$tmpdir/$curfilename" ) || do {
                        print STDERR "Can't open tmpfile: $!\n";
                        return;
                    };
                    $optname_to_fname{ $h2[$j] } = $curfilename;

                    printf FP "<hr>\n";
                    printf FP "<h2><kbd><a name=\"%s\">%s</a></kbd>\n", $h2[$j],
                      $h2[$j];
                    if ( $h1 ne "" ) {
                        printf FP " &ndash; <a href=\"cattoc.html#%s\">%s</a>",
                          $cats{$h1}, $h1;
                    }
                    printf FP "</h2>\n";
                    printf FP "<h3><em>Description</em></h3>\n";
                    printf FP "<p>Please see the description of "
                      . "<a href=\"#%s\">%s</a>\n",
                      $h2[0], $h2[0];
                    $needp = 0;
                }
                @h2 = "";
            }
            $m     = 0;
            $first = 1;
        }
        elsif ( $c =~ /^[#[:alpha:]]/ && $m != 0 ) {
            if ($first) {
                close(FP);
                ++$curfilename;
                push @optnames, $h2[0];
                open( FP, ">$tmpdir/$curfilename" ) || do {
                    print STDERR "Can't open tmpfile: $!\n";
                    return;
                };
                $optname_to_fname{ $h2[0] } = $curfilename;

                if ($any) {
                    printf FP "<hr>\n";
                }
                printf FP "<h2><kbd><a name=\"%s\">%s</a></kbd>\n", $h2[0],
                  $h2[0];
                if ( $h1 ne "" ) {
                    printf FP " &ndash; <a href=\"cattoc.html#%s\">%s</a>",
                      $cats{$h1}, $h1;
                }
                printf FP "</h2>\n";
                printf FP "<h3><em>Description</em></h3>\n";
                $needp = 1;
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

            my $k = 0;
            if ( $c =~ /^[[:alpha:]_][[:alnum:]_]+:/ ) {
                $t = $c;
                $t =~ s/:.*//;
                $k = $keys{$t};
            }

            if ( $c =~ /^$/ ) {
                if ($nf) {
                    printf FP "\n";
                }
                else {
                    $p = 1;
                }
            }
            elsif ( $ex != 0 ) {
                printf FP "%s", $needp ? "<p>" : "<br>";
                $needp = 0;
                printf FP "<code>%s</code><br>\n", $c;
                $ex--;
            }
            elsif ($k) {
                if ( $d != $n && !$nf ) {
                    printf FP "<h3><em>Default value</em></h3>\n";
                    printf FP "<p>";
                    $needp = 0;
                }
                $c =~ s/:$/:<em>none<\/em>/;
                $c =~ s/:/<\/code>:<code>/;
                $c = "<code>" . $c . "</code>";
                if ( !$nf ) {
                    $c .= "<br>";
                }
                printf FP "%s\n", $c;
                $d = $n + 1;
            }
            else {
                if ( $p && !$nf ) {
                    printf FP "<p>\n";
                    $needp = 0;
                }
                $p = 0;
                if ( $input[ $n + 1 ] =~ /^#\s*==/ ) {
                    $c = "<br><em>$c</em>";
                    if ( !$nf ) {
                        $c .= "<br>";
                    }
                    $next++;
                }
                printf FP "<p>" if $needp;
                $needp = 0;
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
    open( FP, ">>$output" ) || do {
        print STDERR "Can't open $output: $!\n";
        return;
    };
    {
        my @ordered =
          ( defined $opt_s ? ( sort keys(%optname_to_fname) ) : @optnames );
        printf FP "<p>";
        if ( defined $opt_s ) {
            print FP "Options are sorted by name.\n";
        }
        else {
            print FP "Options are in the same order as lynx.cfg.\n";
        }
        foreach $l (@ordered) {
            my $fnm = $tmpdir . $optname_to_fname{$l};
            open( FP1, "<$fnm" ) || do {
                print STDERR "Can't open $fnm: $!\n";
                return;
            };
            my $avail = ok($l);
            if ( defined $opt_a || $avail ) {
                my @lines = <FP1>;
                print FP @lines;
                if ( !$avail && defined $opt_m ) {
                    print FP <<'EOF';
<p>Support for this setting was disabled at compile-time.
EOF
                }
            }
            close(FP1);
        }
        foreach $l ( values(%optname_to_fname) ) {
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
    my @input = @_;
    my @major;
    my %descs;
    my %index;
    my ( $n, $m, $c, $d, $found, $h1, $nf, $ex, $count, $once );
    my $output = "cattoc.html";

    open( FP, ">$output" ) || do {
        print STDERR "Can't open $output: $!\n";
        return;
    };
    print FP <<'EOF';
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
<link rev="made" href="mailto:lynx-dev@nongnu.org">
<title>lynx.cfg settings by category</title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<meta name="description" content=
"These are the categories for Lynx's settings in lynx.cfg, with summary descriptions and links to each setting.">
</head>
<body>
<p>These are the major categories of configuration settings in Lynx:
<ul>
EOF
    $m  = -1;
    $h1 = 0;
    $nf = 0;
    for $n ( 0 .. $#input ) {
        my $count = $#input;
        my $once  = 1;
        $c = $input[$n];
        if ( $input[$n] =~ /^\.h1\s/ ) {
            $h1 = 1;
            $c =~ s/^.h1\s*//;
            $m     = $#major + 1;
            $d     = 0;
            $found = 0;
            while ( $d <= $#major && !$found ) {
                if ( $major[$d] eq $c ) {
                    $m     = $d;
                    $found = 1;
                }
                $d++;
            }
            if ( !$found ) {
                $major[$m]           = $c;
                $descs{ $major[$m] } = "";
                $index{ $major[$m] } = "";
            }
            next;
        }
        elsif ( $h1 != 0 ) {
            if ( $c =~ /^\.(nf|ex)/ ) {
                my $s = $c;
                $s =~ s/^\.[[:lower:]]+\s//;
                if ( $s =~ /^[[:digit:]]+$/ ) {
                    $count = $s;
                    $once  = $s;
                }
            }
            if ( $input[$n] =~ /^$/ ) {
                $h1 = 0;
            }
            elsif ( $input[$n] =~ /^\.nf/ ) {
                $descs{ $major[$m] } .= "<pre>" . "\n";
                $nf = $count;
            }
            elsif ( $input[$n] =~ /^\.fi/ ) {
                $descs{ $major[$m] } .= "</pre>" . "\n";
                $nf = 0;
            }
            elsif ( $input[$n] =~ /^\.ex/ ) {
                $ex = $once;
                $descs{ $major[$m] } .=
                    "<h3><em>Example"
                  . ( $ex > 1 ? "s" : "" )
                  . ":</em></h3>\n" . "\n";
            }
            elsif ( $input[$n] =~ /^\s*#/ ) {
                $c = $input[$n];
                $c =~ s/^\s*#\s*//;
                $descs{ $major[$m] } .= $c . "\n";
            }
        }
        if ( $m >= 0 && $input[$n] =~ /^\.h2\s/ ) {
            $c = $input[$n];
            $c =~ s/^.h2\s*//;
            $index{ $major[$m] } .= $c . "\n"
              if ( defined $opt_a || ok($c) );
            $h1 = 0;
        }
        if ( $nf != 0 && $nf-- == 0 ) {
            $descs{ $major[$m] } .= "</pre>\n";
        }
    }
    @major = sort @major;
    for $n ( 0 .. $#major ) {
        $cats{ $major[$n] } = sprintf( "header%03d", $n );
        printf FP "<li><a href=\"#%s\">%s</a>\n", $cats{ $major[$n] },
          $major[$n];
    }
    printf FP "</ul>\n";
    for $n ( 0 .. $#major ) {
        printf FP "\n";
        printf FP "<h2><a name=\"%s\">%s</a></h2>\n", $cats{ $major[$n] },
          $major[$n];
        if ( $descs{ $major[$n] } !~ /^$/ ) {
            printf FP "<h3>Description</h3>\n<p>%s\n", $descs{ $major[$n] };
        }
        $c = $index{ $major[$n] };
        if ( $c ne "" ) {
            my @c = split( /\n/, $c );
            @c = sort @c;
            printf FP
              "<p>Here is a list of settings that belong to this category\n";
            printf FP "<ul>\n";
            for $m ( 0 .. $#c ) {
                my $avail = ok( $c[$m] );
                my $mark = ( !$avail && defined $opt_m ) ? "*" : "";
                printf FP "<li><a href=\"body.html#%s\">%s</a>\n", $c[$m],
                  $c[$m] . $mark;
            }
            printf FP "</ul>\n";
        }
    }
    my $str = <<'EOF'
<p>
<a href=alphatoc.html>To list of settings by name</a>
EOF
      . (
        defined $opt_a && defined $opt_m
        ? "<p>Support for all settings suffixed with '*' was disabled at compile time."
        : ""
      )
      . <<'EOF'
</body>
</html>
EOF
      ;
    print FP $str;
    close(FP);
    return @cats;
}
