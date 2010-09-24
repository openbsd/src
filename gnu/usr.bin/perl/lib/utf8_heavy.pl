package utf8;
use strict;
use warnings;

sub DEBUG () { 0 }

sub DESTROY {}

my %Cache;

sub croak { require Carp; Carp::croak(@_) }

##
## "SWASH" == "SWATCH HASH". A "swatch" is a swatch of the Unicode landscape.
## It's a data structure that encodes a set of Unicode characters.
##

{
    # If a floating point number is within this distance from the value of a
    # fraction, it is considered to be that fraction, even if many more digits
    # are specified that don't exactly match.
    my $min_floating_slop;

    sub SWASHNEW {
        my ($class, $type, $list, $minbits, $none) = @_;
        local $^D = 0 if $^D;

        print STDERR __LINE__, ": ", join(", ", @_), "\n" if DEBUG;

        ##
        ## Get the list of codepoints for the type.
        ## Called from swash_init (see utf8.c) or SWASHNEW itself.
        ##
        ## Callers of swash_init:
        ##     op.c:pmtrans             -- for tr/// and y///
        ##     regexec.c:regclass_swash -- for /[]/, \p, and \P
        ##     utf8.c:is_utf8_common    -- for common Unicode properties
        ##     utf8.c:to_utf8_case      -- for lc, uc, ucfirst, etc. and //i
        ##
        ## Given a $type, our goal is to fill $list with the set of codepoint
        ## ranges. If $type is false, $list passed is used.
        ##
        ## $minbits:
        ##     For binary properties, $minbits must be 1.
        ##     For character mappings (case and transliteration), $minbits must
        ##     be a number except 1.
        ##
        ## $list (or that filled according to $type):
        ##     Refer to perlunicode.pod, "User-Defined Character Properties."
        ##     
        ##     For binary properties, only characters with the property value
        ##     of True should be listed. The 3rd column, if any, will be ignored
        ##
        ## $none is undocumented, so I'm (khw) trying to do some documentation
        ## of it now.  It appears to be if there is a mapping in an input file
        ## that maps to 'XXXX', then that is replaced by $none+1, expressed in
        ## hexadecimal.  The only place I found it possibly used was in
        ## S_pmtrans in op.c.
        ##
        ## To make the parsing of $type clear, this code takes the a rather
        ## unorthodox approach of last'ing out of the block once we have the
        ## info we need. Were this to be a subroutine, the 'last' would just
        ## be a 'return'.
        ##
        my $file; ## file to load data from, and also part of the %Cache key.
        my $ListSorted = 0;

        # Change this to get a different set of Unicode tables
        my $unicore_dir = 'unicore';

        if ($type)
        {
            $type =~ s/^\s+//;
            $type =~ s/\s+$//;

            print STDERR __LINE__, ": type = $type\n" if DEBUG;

        GETFILE:
            {
                ##
                ## It could be a user-defined property.
                ##

                my $caller1 = $type =~ s/(.+)::// ? $1 : caller(1);

                if (defined $caller1 && $type =~ /^(?:\w+)$/) {
                    my $prop = "${caller1}::$type";
                    if (exists &{$prop}) {
                        no strict 'refs';
                        
                        $list = &{$prop};
                        last GETFILE;
                    }
                }

                require "$unicore_dir/Heavy.pl";

                # Everything is caseless matching
                my $property_and_table = lc $type;
                print STDERR __LINE__, ": $property_and_table\n" if DEBUG;

                # See if is of the compound form 'property=value', where the
                # value indicates the table we should use.
                my ($property, $table, @remainder) =
                                    split /\s*[:=]\s*/, $property_and_table, -1;
                return $type if @remainder;

                my $prefix;
                if (! defined $table) {
                        
                    # Here, is the single form.  The property becomes empty, and
                    # the whole value is the table.
                    $table = $property;
                    $prefix = $property = "";
                } else {
                    print STDERR __LINE__, ": $property\n" if DEBUG;

                    # Here it is the compound property=table form.  The property
                    # name is always loosely matched, which means remove any of
                    # these:
                    $property =~ s/[_\s-]//g;

                    # And convert to canonical form.  Quit if not valid.
                    $property = $utf8::loose_property_name_of{$property};
                    return $type unless defined $property;

                    $prefix = "$property=";

                    # If the rhs looks like it is a number...
                    print STDERR __LINE__, ": table=$table\n" if DEBUG;
                    if ($table =~ qr{ ^ [ \s 0-9 _  + / . -]+ $ }x) {
                        print STDERR __LINE__, ": table=$table\n" if DEBUG;

                        # Don't allow leading nor trailing slashes 
                        return $type if $table =~ / ^ \/ | \/ $ /x;

                        # Split on slash, in case it is a rational, like \p{1/5}
                        my @parts = split qr{ \s* / \s* }x, $table, -1;
                        print __LINE__, ": $type\n" if @parts > 2 && DEBUG;

                        # Can have maximum of one slash
                        return $type if @parts > 2;

                        foreach my $part (@parts) {
                            print __LINE__, ": part=$part\n" if DEBUG;

                            $part =~ s/^\+\s*//;    # Remove leading plus
                            $part =~ s/^-\s*/-/;    # Remove blanks after unary
                                                    # minus

                            # Remove underscores between digits.
                            $part =~ s/( ?<= [0-9] ) _ (?= [0-9] ) //xg;

                            # No leading zeros (but don't make a single '0'
                            # into a null string)
                            $part =~ s/ ^ ( -? ) 0+ /$1/x;
                            $part .= '0' if $part eq '-' || $part eq "";

                            # No trailing zeros after a decimal point
                            $part =~ s/ ( \. .*? ) 0+ $ /$1/x;

                            # Begin with a 0 if a leading decimal point
                            $part =~ s/ ^ ( -? ) \. /${1}0./x;

                            # Ensure not a trailing decimal point: turn into an
                            # integer
                            $part =~ s/ \. $ //x;

                            print STDERR __LINE__, ": part=$part\n" if DEBUG;
                            #return $type if $part eq "";
                            
                            # Result better look like a number.  (This test is
                            # needed because, for example could have a plus in
                            # the middle.)
                            return $type if $part
                                            !~ / ^ -? [0-9]+ ( \. [0-9]+)? $ /x;
                        }

                        #  If a rational...
                        if (@parts == 2) {

                            # If denominator is negative, get rid of it, and ...
                            if ($parts[1] =~ s/^-//) {

                                # If numerator is also negative, convert the
                                # whole thing to positive, or move the minus to
                                # the numerator
                                if ($parts[0] !~ s/^-//) {
                                    $parts[0] = '-' . $parts[0];
                                }
                            }
                            $table = join '/', @parts;
                        }
                        elsif ($property ne 'nv' || $parts[0] !~ /\./) {

                            # Here is not numeric value, or doesn't have a
                            # decimal point.  No further manipulation is
                            # necessary.  (Note the hard-coded property name.
                            # This could fail if other properties eventually
                            # had fractions as well; perhaps the cjk ones
                            # could evolve to do that.  This hard-coding could
                            # be fixed by mktables generating a list of
                            # properties that could have fractions.)
                            $table = $parts[0];
                        } else {

                            # Here is a floating point numeric_value.  Try to
                            # convert to rational.  First see if is in the list
                            # of known ones.
                            if (exists $utf8::nv_floating_to_rational{$parts[0]}) {
                                $table = $utf8::nv_floating_to_rational{$parts[0]};
                            } else {

                                # Here not in the list.  See if is close
                                # enough to something in the list.  First
                                # determine what 'close enough' means.  It has
                                # to be as tight as what mktables says is the
                                # maximum slop, and as tight as how many
                                # digits we were passed.  That is, if the user
                                # said .667, .6667, .66667, etc.  we match as
                                # many digits as they passed until get to
                                # where it doesn't matter any more due to the
                                # machine's precision.  If they said .6666668,
                                # we fail.
                                (my $fraction = $parts[0]) =~ s/^.*\.//;
                                my $epsilon = 10 ** - (length($fraction));
                                if ($epsilon > $utf8::max_floating_slop) {
                                    $epsilon = $utf8::max_floating_slop;
                                }

                                # But it can't be tighter than the minimum
                                # precision for this machine.  If haven't
                                # already calculated that minimum, do so now.
                                if (! defined $min_floating_slop) {

                                    # Keep going down an order of magnitude
                                    # until find that adding this quantity to
                                    # 1 remains 1; but put an upper limit on
                                    # this so in case this algorithm doesn't
                                    # work properly on some platform, that we
                                    # won't loop forever.
                                    my $count = 0;
                                    $min_floating_slop = 1;
                                    while (1+ $min_floating_slop != 1
                                           && $count++ < 50)
                                    {
                                        my $next = $min_floating_slop / 10;
                                        last if $next == 0; # If underflows,
                                                            # use previous one
                                        $min_floating_slop = $next;
                                        print STDERR __LINE__, ": min_float_slop=$min_floating_slop\n" if DEBUG;
                                    }

                                    # Back off a couple orders of magnitude,
                                    # just to be safe.
                                    $min_floating_slop *= 100;
                                }
                                    
                                if ($epsilon < $min_floating_slop) {
                                    $epsilon = $min_floating_slop;
                                }
                                print STDERR __LINE__, ": fraction=.$fraction; epsilon=$epsilon\n" if DEBUG;

                                undef $table;

                                # And for each possible rational in the table,
                                # see if it is within epsilon of the input.
                                foreach my $official
                                        (keys %utf8::nv_floating_to_rational)
                                {
                                    print STDERR __LINE__, ": epsilon=$epsilon, official=$official, diff=", abs($parts[0] - $official), "\n" if DEBUG;
                                    if (abs($parts[0] - $official) < $epsilon) {
                                      $table =
                                      $utf8::nv_floating_to_rational{$official};
                                        last;
                                    }
                                }

                                # Quit if didn't find one.
                                return $type unless defined $table;
                            }
                        }
                        print STDERR __LINE__, ": $property=$table\n" if DEBUG;
                    }
                }

                # Combine lhs (if any) and rhs to get something that matches
                # the syntax of the lookups.
                $property_and_table = "$prefix$table";
                print STDERR __LINE__, ": $property_and_table\n" if DEBUG;

                # First try stricter matching.
                $file = $utf8::stricter_to_file_of{$property_and_table};

                # If didn't find it, try again with looser matching by editing
                # out the applicable characters on the rhs and looking up
                # again.
                if (! defined $file) {
                    $table =~ s/ [_\s-] //xg;
                    $property_and_table = "$prefix$table";
                    print STDERR __LINE__, ": $property_and_table\n" if DEBUG;
                    $file = $utf8::loose_to_file_of{$property_and_table};
                }

                # Add the constant and go fetch it in.
                if (defined $file) {
                    if ($utf8::why_deprecated{$file}) {
                        warnings::warnif('deprecated', "Use of '$type' in \\p{} or \\P{} is deprecated because: $utf8::why_deprecated{$file};");
                    }
                    $file= "$unicore_dir/lib/$file.pl";
                    last GETFILE;
                }
                print STDERR __LINE__, ": didn't find $property_and_table\n" if DEBUG;

                ##
                ## See if it's a user-level "To".
                ##

                my $caller0 = caller(0);

                if (defined $caller0 && $type =~ /^To(?:\w+)$/) {
                    my $map = $caller0 . "::" . $type;

                    if (exists &{$map}) {
                        no strict 'refs';
                        
                        $list = &{$map};
                        last GETFILE;
                    }
                }

                ##
                ## Last attempt -- see if it's a standard "To" name
                ## (e.g. "ToLower")  ToTitle is used by ucfirst().
                ## The user-level way to access ToDigit() and ToFold()
                ## is to use Unicode::UCD.
                ##
                if ($type =~ /^To(Digit|Fold|Lower|Title|Upper)$/) {
                    $file = "$unicore_dir/To/$1.pl";
                    ## would like to test to see if $file actually exists....
                    last GETFILE;
                }

                ##
                ## If we reach this line, it's because we couldn't figure
                ## out what to do with $type. Ouch.
                ##

                return $type;
            }

            if (defined $file) {
                print STDERR __LINE__, ": found it (file='$file')\n" if DEBUG;

                ##
                ## If we reach here, it was due to a 'last GETFILE' above
                ## (exception: user-defined properties and mappings), so we
                ## have a filename, so now we load it if we haven't already.
                ## If we have, return the cached results. The cache key is the
                ## class and file to load.
                ##
                my $found = $Cache{$class, $file};
                if ($found and ref($found) eq $class) {
                    print STDERR __LINE__, ": Returning cached '$file' for \\p{$type}\n" if DEBUG;
                    return $found;
                }

                local $@;
                local $!;
                $list = do $file; die $@ if $@;
            }

            $ListSorted = 1; ## we know that these lists are sorted
        }

        my $extras;
        my $bits = $minbits;

        my $ORIG = $list;
        if ($list) {
            my @tmp = split(/^/m, $list);
            my %seen;
            no warnings;
            $extras = join '', grep /^[^0-9a-fA-F]/, @tmp;
            $list = join '',
                map  { $_->[1] }
                sort { $a->[0] <=> $b->[0] }
                map  { /^([0-9a-fA-F]+)/; [ CORE::hex($1), $_ ] }
                grep { /^([0-9a-fA-F]+)/ and not $seen{$1}++ } @tmp; # XXX doesn't do ranges right
        }

        if ($none) {
            my $hextra = sprintf "%04x", $none + 1;
            $list =~ s/\tXXXX$/\t$hextra/mg;
        }

        if ($minbits != 1 && $minbits < 32) { # not binary property
            my $top = 0;
            while ($list =~ /^([0-9a-fA-F]+)(?:[\t]([0-9a-fA-F]+)?)(?:[ \t]([0-9a-fA-F]+))?/mg) {
                my $min = CORE::hex $1;
                my $max = defined $2 ? CORE::hex $2 : $min;
                my $val = defined $3 ? CORE::hex $3 : 0;
                $val += $max - $min if defined $3;
                $top = $val if $val > $top;
            }
            my $topbits =
                $top > 0xffff ? 32 :
                $top > 0xff ? 16 : 8;
            $bits = $topbits if $bits < $topbits;
        }

        my @extras;
        if ($extras) {
            for my $x ($extras) {
                pos $x = 0;
                while ($x =~ /^([^0-9a-fA-F\n])(.*)/mg) {
                    my $char = $1;
                    my $name = $2;
                    print STDERR __LINE__, ": $1 => $2\n" if DEBUG;
                    if ($char =~ /[-+!&]/) {
                        my ($c,$t) = split(/::/, $name, 2);	# bogus use of ::, really
                        my $subobj;
                        if ($c eq 'utf8') {
                            $subobj = utf8->SWASHNEW($t, "", $minbits, 0);
                        }
                        elsif (exists &$name) {
                            $subobj = utf8->SWASHNEW($name, "", $minbits, 0);
                        }
                        elsif ($c =~ /^([0-9a-fA-F]+)/) {
                            $subobj = utf8->SWASHNEW("", $c, $minbits, 0);
                        }
                        return $subobj unless ref $subobj;
                        push @extras, $name => $subobj;
                        $bits = $subobj->{BITS} if $bits < $subobj->{BITS};
                    }
                }
            }
        }

        if (DEBUG) {
            print STDERR __LINE__, ": CLASS = $class, TYPE => $type, BITS => $bits, NONE => $none";
            print STDERR "\nLIST =>\n$list" if defined $list;
            print STDERR "\nEXTRAS =>\n$extras" if defined $extras;
            print STDERR "\n";
        }

        my $SWASH = bless {
            TYPE => $type,
            BITS => $bits,
            EXTRAS => $extras,
            LIST => $list,
            NONE => $none,
            @extras,
        } => $class;

        if ($file) {
            $Cache{$class, $file} = $SWASH;
        }

        return $SWASH;
    }
}

# Now SWASHGET is recasted into a C function S_swash_get (see utf8.c).

1;
