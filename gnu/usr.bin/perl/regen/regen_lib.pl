#!/usr/bin/perl -w
use strict;
our (@Changed, $TAP);
use File::Compare;
use Symbol;
use Carp;
use Text::Wrap();

# Common functions needed by the regen scripts

our $Needs_Write = $^O eq 'cygwin' || $^O eq 'os2' || $^O eq 'MSWin32';

our $Verbose = 0;
@ARGV = grep { not($_ eq '-q' and $Verbose = -1) }
  grep { not($_ eq '--tap' and $TAP = 1) }
  grep { not($_ eq '-v' and $Verbose = 1) } @ARGV;

END {
  print STDOUT "Changed: @Changed\n" if @Changed;
}

sub safer_unlink {
  my @names = @_;
  my $cnt = 0;

  my $name;
  foreach $name (@names) {
    next unless -e $name;
    chmod 0777, $name if $Needs_Write;
    ( CORE::unlink($name) and ++$cnt
      or warn "Couldn't unlink $name: $!\n" );
  }
  return $cnt;
}

# Open a new file.
sub open_new {
    my ($final_name, $mode, $header, $force) = @_;
    my $name = $final_name . '-new';
    my $lang =
        $final_name =~ /\.pod\z/ ? 'Pod' :
        $final_name =~ /\.(?:c|h|inc|tab|act)\z/ ? 'C' :
        $final_name =~ /\.gitignore\z/ ? 'None' :
        'Perl';
    if ($force && -e $final_name) {
        chmod 0777, $name if $Needs_Write;
        CORE::unlink $final_name
                or die "Couldn't unlink $final_name: $!\n";
    }
    my $fh = gensym;
    if (!defined $mode or $mode eq '>') {
        if (-f $name) {
            unlink $name or die "$name exists but can't unlink: $!";
        }
        open $fh, '>', $name or die "Can't create $name: $!";
    } elsif ($mode eq '>>') {
        open $fh, '>>', $name or die "Can't append to $name: $!";
    } else {
        die "Unhandled open mode '$mode'";
    }
    @{*$fh}{qw(name final_name lang force)}
        = ($name, $final_name, $lang, $force);
    binmode $fh;
    print {$fh} read_only_top(lang => $lang, %$header) if $header;
    $fh;
}

sub close_and_rename {
    my $fh = shift;
    my ($name, $final_name, $force) = @{*{$fh}}{qw(name final_name force)};
    close $fh or die "Error closing $name: $!";

    if ($TAP) {
        # Don't use compare because if there are errors it doesn't give any
        # way to generate diagnostics about what went wrong.
        # These files are small enough to read into memory.
        local $/;
        # This is the file we just closed, so it should open cleanly:
        open $fh, '<', $name
            or die "Can't open '$name': $!";
        my $want = <$fh>;
        die "Can't read '$name': $!"
            unless defined $want;
        close $fh
            or die "Can't close '$name': $!";

        my $fail;
        if (!open $fh, '<', $final_name) {
            $fail = "Can't open '$final_name': $!";
        } else {
            my $have = <$fh>;
            if (!defined $have) {
                $fail = "Can't read '$final_name': $!";
                close $fh;
            } elsif (!close $fh) {
                $fail = "Can't close '$final_name': $!";
            } elsif ($want ne $have) {
                $fail = "'$name' and '$final_name' differ";
            }
        }
        # If someone wants to run t/porting/regen.t and keep the
        # changes then they can set this env var, otherwise we
        # unlink the generated file regardless.
        my $keep_changes= $ENV{"REGEN_T_KEEP_CHANGES"};
        safer_unlink($name) unless $keep_changes;
        if ($fail) {
            print STDOUT "not ok - $0 $final_name\n";
            die "$fail\n";
        } else {
            print STDOUT "ok - $0 $final_name\n";
        }
        # If we get here then the file hasn't changed, and we should
        # delete the new version if they have requested we keep changes
        # as we wont have deleted it above like we would normally.
        safer_unlink($name) if $keep_changes;
        return;
    }
    unless ($force) {
        if (compare($name, $final_name) == 0) {
            warn "no changes between '$name' & '$final_name'\n" if $Verbose > 0;
            safer_unlink($name);
            return;
        }
        warn "changed '$name' to '$final_name'\n" if $Verbose > 0;
        push @Changed, $final_name unless $Verbose < 0;
    }

    # Some DOSish systems can't rename over an existing file:
    safer_unlink $final_name;
    chmod 0600, $name if $Needs_Write;
    rename $name, $final_name or die "renaming $name to $final_name: $!";
}

my %lang_opener = (
    Perl => '# ',
    Pod  => '',
    C    => '/* ',
    None => '# ',
);

sub read_only_top {
    my %args = @_;
    my $lang = $args{lang};
    die "Missing language argument" unless defined $lang;
    die "Unknown language argument '$lang'"
        unless exists $lang_opener{$lang};
    my $style = $args{style} ? " $args{style} " : '   ';

    # Generate the "modeline" for syntax highlighting based on the language
    my $raw = "-*- " . ($lang eq 'None' ? "" : "mode: $lang; ") . "buffer-read-only: t -*-\n";

    if ($args{file}) {
        $raw .= "\n   $args{file}\n";
    }
    if ($args{copyright}) {
        local $" = ', ';
         $raw .= wrap(75, '   ', '   ', <<"EOM") . "\n";

Copyright (C) @{$args{copyright}} by\0Larry\0Wall\0and\0others

You may distribute under the terms of either the GNU General Public
License or the Artistic License, as specified in the README file.
EOM
    }

    $raw .= "!!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!\n";

    if ($args{by}) {
        $raw .= "This file is built by $args{by}";
        if ($args{from}) {
            my @from = ref $args{from} eq 'ARRAY' ? @{$args{from}} : $args{from};
            my $last = pop @from;
            if (@from) {
                $raw .= ' from ' . join (', ', @from) . " and $last";
            } else {
                $raw .= " from $last";
            }
        }
        $raw .= ".\n";
    }
    $raw .= "Any changes made here will be lost!\n";
    $raw .= $args{final} if $args{final};

    my $cooked = $lang eq 'C'
        ? wrap(78, '/* ', $style, $raw) . " */\n\n"
        : wrap(78, $lang_opener{$lang}, $lang_opener{$lang}, $raw) . "\n";
    $cooked =~ tr/\0/ /; # Don't break Larry's name etc
    $cooked =~ s/ +$//mg; # Remove all trailing spaces
    $cooked =~ s! \*/\n!$args{quote}!s if $args{quote};
    return $cooked;
}

sub read_only_bottom_close_and_rename {
    my ($fh, $sources) = @_;
    my ($name, $lang, $final_name) = @{*{$fh}}{qw(name lang final_name)};
    confess "bad fh in read_only_bottom_close_and_rename" unless $name;
    die "No final name specified at open time for $name"
        unless $final_name;

    my $comment;
    if ($sources) {
        $comment = "Generated from:\n";
        foreach my $file (sort @$sources) {
            my $digest = (-e $file)
                         ? digest($file)
                           # Use a random number that won't match the real
                           # digest, so will always show as out-of-date, so
                           # Porting tests likely will fail drawing attention
                           # to the problem.
                         : int(rand(1_000_000));
            $comment .= "$digest $file\n";
        }
    }
    $comment .= "ex: set ro" . ($lang eq 'None' ? "" : " ft=\L$lang\E") . ":";

    if ($lang eq 'Pod') {
        # nothing
    } elsif ($lang eq 'C') {
        $comment =~ s/^/ * /mg;
        $comment =~ s! \* !/* !;
        $comment .= " */";
    } else {
        $comment =~ s/^/# /mg;
    }
    print $fh "\n$comment\n";

    close_and_rename($fh);

    return;
}

sub tab {
    no warnings 'numeric';
    my ($l, $t) = @_;
    $t .= "\t" x ($l - (length($t) + 1) / 8);
    $t;
}

sub digest {
    my $file = shift;
    # Need to defer loading this, as the main regen scripts work back to 5.004,
    # and likely we don't even have this module on every 5.8 install yet:
    require Digest::SHA;

    local ($/, *FH);
    open FH, '<', $file or die "Can't open $file: $!";
    my $raw = <FH>;
    close FH or die "Can't close $file: $!";
    return Digest::SHA::sha256_hex($raw);
};

sub wrap {
    local $Text::Wrap::columns = shift;
    local $Text::Wrap::unexpand = 0;
    Text::Wrap::wrap(@_);
}

sub columnarize_list {
    my $listp = shift;
    my $max_width = shift;

    # Returns the list (pointed to by 'listp') of text items, but arranged
    # tabularly, like the default output of the 'ls' command.  The first few
    # items of the list will be in the first column; the next batch in the
    # second, etc, for as many columns as can fit in 'maxwidth' bytes, and as
    # many rows as necessary.

    use integer;

    # Real data is unlikely to be able to fit more columns than this in a
    # typical 80 byte window.  But obviously this could be changed or passed
    # in.
    my $max_columns = 7;

    my $min_spacer = 2;     # Need this much space between columns
    my $columns;
    my $rows;
    my @col_widths;

  COLUMN:
    # We start with more columns, and work down until we find a number that
    # can accommodate all the data.  This algorithm doesn't require the
    # resulting columns to all have the same width.  This can allow for
    # as tight of packing as the data will possibly allow.
    for ($columns = 7; $columns >= 1; $columns--) {

        # For this many columns, we will need this many rows (final row might
        # not be completely filled)
        $rows = ($listp->@* + $columns - 1) / $columns;

        # We only need to execute this final iteration to calculate the number
        # of rows, as we can't get fewer than a single column.
        last if $columns == 1;

        my $row_width = 0;
        my $i = 0;  # Which element of the input list

        # For each column ...
        for my $col (0 .. $columns - 1) {

            # Calculate how wide the column needs to be, which is based on the
            # widest element in it
            $col_widths[$col] = 0;

            # Look through all the rows to find the widest element
            for my $row (0 .. $rows - 1) {

                # Skip if this row doesn't have an entry for this column
                last if $i >= $listp->@*;

                # This entry occupies this many bytes.
                my $this_width = length $listp->[$i];

                # All but the final column need a spacer between it and the
                # next column over.
                $this_width += $min_spacer if $col < $columns - 1;


                # This column will need to have enough width to accommodate
                # this element
                if ($this_width > $col_widths[$col]) {

                    # We can't have this many columns if the total width
                    # exceeds the available; bail now and try fewer columns
                    next COLUMN if $row_width + $this_width > $max_width;

                    $col_widths[$col] = $this_width;
                }

                $i++;   # The next row will contain the next item
            }

            $row_width += $col_widths[$col];
            next COLUMN if $row_width > $max_width;
        }

        # If we get this far, this many columns works
        last;
    }

    # Assemble the output
    my $text = "";
    for my $row (0 .. $rows - 1) {
        for my $col (0 .. $columns - 1) {
            my $index = $row + $rows * $col;  # Convert 2 dimensions to 1

            # Skip if this row doesn't have an entry for this column
            next if $index >= $listp->@*;

            my $element = $listp->[$index];
            $text .= $element;

            # Add alignment spaces for all but final column
            $text .= " " x ($col_widths[$col] - length $element)
                                                        if $col < $columns - 1;
        }

        $text .= "\n";  # End of row
    }

    return $text;
}

# return the perl version as defined in patchlevel.h.
# (we may be being run by another perl, so $] won't be right)
# return e.g. (5, 14, 3, "5.014003")

sub perl_version {
    my $plh = 'patchlevel.h';
    open my $fh, "<", $plh or die "can't open '$plh': $!\n";
    my ($v1,$v2,$v3);
    while (<$fh>) {
        $v1 = $1 if /PERL_REVISION\s+(\d+)/;
        $v2 = $1 if /PERL_VERSION\s+(\d+)/;
        $v3 = $1 if /PERL_SUBVERSION\s+(\d+)/;
    }
    die "can't locate PERL_REVISION in '$plh'"   unless defined $v1;
    die "can't locate PERL_VERSION in '$plh'"    unless defined $v2;
    die "can't locate PERL_SUBVERSION in '$plh'" unless defined $v3;
    return ($v1,$v2,$v3, sprintf("%d.%03d%03d", $v1, $v2, $v3));
}


1;
