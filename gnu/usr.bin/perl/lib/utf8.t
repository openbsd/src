#!./perl 

my $has_perlio;

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
    unless ($has_perlio = find PerlIO::Layer 'perlio') {
	print <<EOF;
# Since you don't have perlio you might get failures with UTF-8 locales.
EOF
    }
}

no utf8; # Ironic, no?

# NOTE!
#
# Think carefully before adding tests here.  In general this should be
# used only for about three categories of tests:
#
# (1) tests that absolutely require 'use utf8', and since that in general
#     shouldn't be needed as the utf8 is being obsoleted, this should
#     have rather few tests.  If you want to test Unicode and regexes,
#     you probably want to go to op/regexp or op/pat; if you want to test
#     split, go to op/split; pack, op/pack; appending or joining,
#     op/append or op/join, and so forth
#
# (2) tests that have to do with Unicode tokenizing (though it's likely
#     that all the other Unicode tests sprinkled around the t/**/*.t are
#     going to catch that)
#
# (3) complicated tests that simultaneously stress so many Unicode features
#     that deciding into which other test script the tests should go to
#     is hard -- maybe consider breaking up the complicated test
#
#

plan tests => 94;

{
    # bug id 20001009.001

    my ($a, $b);

    { use bytes; $a = "\xc3\xa4" }
    { use utf8;  $b = "\xe4"     }

    my $test = 68;

    ok($a ne $b);

    { use utf8; ok($a ne $b) }
}


{
    # bug id 20000730.004

    my $smiley = "\x{263a}";

    for my $s ("\x{263a}",
	       $smiley,
		
	       "" . $smiley,
	       "" . "\x{263a}",

	       $smiley    . "",
	       "\x{263a}" . "",
	       ) {
	my $length_chars = length($s);
	my $length_bytes;
	{ use bytes; $length_bytes = length($s) }
	my @regex_chars = $s =~ m/(.)/g;
	my $regex_chars = @regex_chars;
	my @split_chars = split //, $s;
	my $split_chars = @split_chars;
	ok("$length_chars/$regex_chars/$split_chars/$length_bytes" eq
	   "1/1/1/3");
    }

    for my $s ("\x{263a}" . "\x{263a}",
	       $smiley    . $smiley,

	       "\x{263a}\x{263a}",
	       "$smiley$smiley",
	       
	       "\x{263a}" x 2,
	       $smiley    x 2,
	       ) {
	my $length_chars = length($s);
	my $length_bytes;
	{ use bytes; $length_bytes = length($s) }
	my @regex_chars = $s =~ m/(.)/g;
	my $regex_chars = @regex_chars;
	my @split_chars = split //, $s;
	my $split_chars = @split_chars;
	ok("$length_chars/$regex_chars/$split_chars/$length_bytes" eq
	   "2/2/2/6");
    }
}


{
    my $w = 0;
    local $SIG{__WARN__} = sub { print "#($_[0])\n"; $w++ };
    my $x = eval q/"\\/ . "\x{100}" . q/"/;;
   
    ok($w == 0 && $x eq "\x{100}");
}

{
    use warnings;
    use strict;

    my $show = q(
                 sub show {
                   my $result;
                   $result .= '>' . join (',', map {ord} split //, $_) . '<'
                     foreach @_;
                   $result;
                 }
                 1;
                );
    eval $show or die $@; # We don't expect this sub definition to fail.
    my $progfile = 'utf' . $$;
    END {unlink_all $progfile}

    # If I'm right 60 is '>' in ASCII, ' ' in EBCDIC
    # 173 is not punctuation in either ASCII or EBCDIC
    my (@char);
    foreach (60, 173, 257, 65532) {
      my $char = chr $_;
      utf8::encode($char);
      # I don't want to use map {ord} and I've no need to hardcode the UTF
      # version
      my $charsubst = $char;
      $charsubst =~ s/(.)/ord ($1) . ','/ge;
      chop $charsubst;
      # Not testing this one against map {ord}
      my $char_as_ord
          = join " . ", map {sprintf 'chr (%d)', ord $_} split //, $char;
      push @char, [$_, $char, $charsubst, $char_as_ord];
    }
    # Now we've done all the UTF8 munching hopefully we're safe
    my @tests = (
             ['check our detection program works',
              'my @a = ("'.chr(60).'\x2A", ""); $b = show @a', qr/^>60,42<><$/],
             ['check literal 8 bit input',
              '$a = "' . chr (173) . '"; $b = show $a', qr/^>173<$/],
             ['check no utf8; makes no change',
              'no utf8; $a = "' . chr (173) . '"; $b = show $a', qr/^>173<$/],
             # Now we do the real byte sequences that are valid UTF8
             (map {
               ["the utf8 sequence for chr $_->[0]",
                qq{\$a = "$_->[1]"; \$b = show \$a}, qr/^>$_->[2]<$/],
               ["no utf8; for the utf8 sequence for chr $_->[0]",
                qq(no utf8; \$a = "$_->[1]"; \$b = show \$a), qr/^>$_->[2]<$/],
               ["use utf8; for the utf8 sequence for chr $_->[0]",
                qq(use utf8; \$a = "$_->[1]"; \$b = show \$a), qr/^>$_->[0]<$/],
              } @char),
             # Interpolation of hex characters needs to take place now, as we're
             # testing feeding malformed utf8 into perl. Bug now fixed was an
             # "out of memory" error. We really need the "" [rather than qq()
             # or q()] to get the best explosion.
             ["!Feed malformed utf8 into perl.", <<"BANG",
    use utf8; %a = ("\xE1\xA0"=>"sterling");
    print 'start'; printf '%x,', ord \$_ foreach keys %a; print "end\n";
BANG
	      qr/^Malformed UTF-8 character \(\d bytes?, need \d, .+\).*start\d+,end$/sm
	     ],
            );
    foreach (@tests) {
        my ($why, $prog, $expect) = @$_;
        open P, ">$progfile" or die "Can't open '$progfile': $!";
        binmode(P, ":bytes") if $has_perlio;
	print P $show, $prog, '; print $b'
            or die "Print to 'progfile' failed: $!";
        close P or die "Can't close '$progfile': $!";
        if ($why =~ s/^!//) {
            print "# Possible delay...\n";
        } else {
            print "# $prog\n";
        }
        my $result = runperl ( stderr => 1, progfile => $progfile );
        like ($result, $expect, $why);
    }
    print
        "# Again! Again! [but this time as eval, and not the explosive one]\n";
    # and now we've safely done them all as separate files, check that the
    # evals do the same thing. Hopefully doing it later sucessfully decouples
    # the previous tests from anything messy that may go wrong with the evals.
    foreach (@tests) {
        my ($why, $prog, $expect) = @$_;
        next if $why =~ m/^!/; # Goes bang.
        my $result = eval $prog;
        if ($@) {
            print "# prog is $prog\n";
            print "# \$\@=", _qq($@), "\n";
        }
        like ($result, $expect, $why);
    }

    # See what the tokeniser does with hash keys.
    print "# What does the tokeniser do with utf8 hash keys?\n";
    @tests = (map {
        # This is the control - I don't expect it to fail
        ["assign utf8 for chr $_->[0] to a hash",
         qq(my \$a = "$_->[1]"; my %h; \$h{\$a} = 1;
            my \$b = show keys %h; \$b .= 'F' unless \$h{$_->[3]}; \$b),
         qr/^>$_->[2]<$/],
        ["no utf8; assign utf8 for chr $_->[0] to a hash",
         qq(no utf8; my \$a = "$_->[1]"; my %h; \$h{\$a} = 1;
            my \$b = show keys %h; \$b .= 'F' unless \$h{$_->[3]}; \$b),
         qr/^>$_->[2]<$/],
        ["use utf8; assign utf8 for chr $_->[0] to a hash",
         qq(use utf8; my \$a = "$_->[1]"; my %h; \$h{\$a} = 1;
            my \$b = show keys %h; \$b .= 'F' unless \$h{chr $_->[0]}; \$b),
         qr/^>$_->[0]<$/],
        # Now check literal $h{"x"} constructions.
        ["\$h{\"x\"} construction, where x is utf8 for chr $_->[0]",
         qq(my \$a = "$_->[1]"; my %h; \$h{"$_->[1]"} = 1;
            my \$b = show keys %h; \$b .= 'F' unless \$h{$_->[3]}; \$b),
         qr/^>$_->[2]<$/],
        ["no utf8; \$h{\"x\"} construction, where x is utf8 for chr $_->[0]",
         qq(no utf8; my \$a = "$_->[1]"; my %h; \$h{"$_->[1]"} = 1;
            my \$b = show keys %h; \$b .= 'F' unless \$h{$_->[3]}; \$b),
         qr/^>$_->[2]<$/],
        ["use utf8; \$h{\"x\"} construction, where x is utf8 for chr $_->[0]",
         qq(use utf8; my \$a = "$_->[1]"; my %h; \$h{"$_->[1]"} = 1;
            my \$b = show keys %h; \$b .= 'F' unless \$h{chr $_->[0]}; \$b),
         qr/^>$_->[0]<$/],
        # Now check "x" => constructions.
        ["assign \"x\"=>1 to a hash, where x is utf8 for chr $_->[0]",
         qq(my \$a = "$_->[1]"; my %h; %h = ("$_->[1]" => 1);
            my \$b = show keys %h; \$b .= 'F' unless \$h{$_->[3]}; \$b),
         qr/^>$_->[2]<$/],
        ["no utf8; assign \"x\"=>1 to a hash, where x is utf8 for chr $_->[0]",
         qq(no utf8; my \$a = "$_->[1]"; my %h; %h = ("$_->[1]" => 1);
            my \$b = show keys %h; \$b .= 'F' unless \$h{$_->[3]}; \$b),
         qr/^>$_->[2]<$/],
        ["use utf8; assign \"x\"=>1 to a hash, where x is utf8 for chr $_->[0]",
         qq(use utf8; my \$a = "$_->[1]"; my %h; %h = ("$_->[1]" => 1);
            my \$b = show keys %h; \$b .= 'F' unless \$h{chr $_->[0]}; \$b),
         qr/^>$_->[0]<$/],
        # Check copies of hashes made from literal utf8 keys
        ["assign utf8 for chr $_->[0] to a hash, then copy it",
         qq(my \$a = "$_->[1]"; my %i; \$i{\$a} = 1; my %h = %i;
            my \$b = show keys %h; \$b .= 'F' unless \$h{$_->[3]}; \$b),
         qr/^>$_->[2]<$/],
        ["no utf8; assign utf8 for chr $_->[0] to a hash, then copy it",
         qq(no utf8; my \$a = "$_->[1]"; my %i; \$i{\$a} = 1;; my %h = %i;
            my \$b = show keys %h; \$b .= 'F' unless \$h{$_->[3]}; \$b),
         qr/^>$_->[2]<$/],
        ["use utf8; assign utf8 for chr $_->[0] to a hash, then copy it",
         qq(use utf8; my \$a = "$_->[1]"; my %i; \$i{\$a} = 1; my %h = %i;
            my \$b = show keys %h; \$b .= 'F' unless \$h{chr $_->[0]}; \$b),
         qr/^>$_->[0]<$/],
     } @char);
    foreach (@tests) {
        my ($why, $prog, $expect) = @$_;
        # print "# $prog\n";
        my $result = eval $prog;
        like ($result, $expect, $why);
    }
}
