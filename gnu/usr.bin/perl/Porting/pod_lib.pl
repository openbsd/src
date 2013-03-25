#!/usr/bin/perl -w

use strict;
use Digest::MD5 'md5';
use File::Find;

# make it clearer when we haven't run to completion, as we can be quite
# noisy when things are working ok

sub my_die {
    print STDERR "$0: ", @_;
    print STDERR "\n" unless $_[-1] =~ /\n\z/;
    print STDERR "ABORTED\n";
    exit 255;
}

sub open_or_die {
    my $filename = shift;
    open my $fh, '<', $filename or my_die "Can't open $filename: $!";
    return $fh;
}

sub slurp_or_die {
    my $filename = shift;
    my $fh = open_or_die($filename);
    binmode $fh;
    local $/;
    my $contents = <$fh>;
    die "Can't read $filename: $!" unless defined $contents and close $fh;
    return $contents;
}

sub write_or_die {
    my ($filename, $contents) = @_;
    open my $fh, '>', $filename or die "Can't open $filename for writing: $!";
    binmode $fh;
    print $fh $contents or die "Can't write to $filename: $!";
    close $fh or die "Can't close $filename: $!";
}

sub pods_to_install {
    # manpages not to be installed
    my %do_not_install = map { ($_ => 1) }
        qw(Pod::Functions XS::APItest XS::Typemap);

    my (%done, %found);

    File::Find::find({no_chdir=>1,
                      wanted => sub {
                          if (m!/t\z!) {
                              ++$File::Find::prune;
                              return;
                          }

                          # $_ is $File::Find::name when using no_chdir
                          return unless m!\.p(?:m|od)\z! && -f $_;
                          return if m!lib/Net/FTP/.+\.pm\z!; # Hi, Graham! :-)
                          # Skip .pm files that have corresponding .pod files
                          return if s!\.pm\z!.pod! && -e $_;
                          s!\.pod\z!!;
                          s!\Alib/!!;
                          s!/!::!g;

                          my_die("Duplicate files for $_, '$done{$_}' and '$File::Find::name'")
                              if exists $done{$_};
                          $done{$_} = $File::Find::name;

                          return if $do_not_install{$_};
                          return if is_duplicate_pod($File::Find::name);
                          $found{/\A[a-z]/ ? 'PRAGMA' : 'MODULE'}{$_}
                              = $File::Find::name;
                      }}, 'lib');
    return \%found;
}

my %state = (
             # Don't copy these top level READMEs
             ignore => {
                        micro => 1,
                        # vms => 1,
                       },
            );

{
    my (%Lengths, %MD5s);

    sub is_duplicate_pod {
        my $file = shift;
        local $_;

        # Initialise the list of possible source files on the first call.
        unless (%Lengths) {
            __prime_state() unless $state{master};
            foreach (@{$state{master}}) {
                next unless $_->[2]{dual};
                # This is a dual-life perl*.pod file, which will have be copied
                # to lib/ by the build process, and hence also found there.
                # These are the only pod files that might become duplicated.
                ++$Lengths{-s $_->[1]};
                ++$MD5s{md5(slurp_or_die($_->[1]))};
            }
        }

        # We are a file in lib. Are we a duplicate?
        # Don't bother calculating the MD5 if there's no interesting file of
        # this length.
        return $Lengths{-s $file} && $MD5s{md5(slurp_or_die($file))};
    }
}

sub __prime_state {
    my $source = 'perldelta.pod';
    my $filename = "pod/$source";
    my $contents = slurp_or_die($filename);
    my @want =
        $contents =~ /perldelta - what is new for perl v(5)\.(\d+)\.(\d+)\n/;
    die "Can't extract version from $filename" unless @want;
    my $delta_leaf = join '', 'perl', @want, 'delta';
    $state{delta_target} = "$delta_leaf.pod";
    $state{delta_version} = \@want;

    # This way round so that keys can act as a MANIFEST skip list
    # Targets will always be in the pod directory. Currently we can only cope
    # with sources being in the same directory.
    $state{copies}{$state{delta_target}} = $source;

    # The default flags if none explicitly set for the current file.
    my $current_flags = '';
    my (%flag_set, @paths);

    my $master = open_or_die('pod/perl.pod');

    while (<$master>) {
        last if /^=begin buildtoc$/;
    }
    die "Can't find '=begin buildtoc':" if eof $master;

    while (<$master>) {
        next if /^$/ or /^#/;
        last if /^=end buildtoc/;
        my ($command, @args) = split ' ';
        if ($command eq 'flag') {
            # For the named pods, use these flags, instead of $current_flags
            my $flags = shift @args;
            my_die("Malformed flag $flags")
                unless $flags =~ /\A=([a-z]*)\z/;
            $flag_set{$_} = $1 foreach @args;
        } elsif ($command eq 'path') {
            # If the pod's name matches the regex, prepend the given path.
            my_die("Malformed path for /$args[0]/")
                unless @args == 2;
            push @paths, [qr/\A$args[0]\z/, $args[1]];
        } elsif ($command eq 'aux') {
            # The contents of perltoc.pod's "AUXILIARY DOCUMENTATION" section
            $state{aux} = [sort @args];
        } else {
            my_die("Unknown buildtoc command '$command'");
        }
    }

    foreach (<$master>) {
        next if /^$/ or /^#/;
        next if /^=head2/;
        last if /^=for buildtoc __END__$/;

        if (my ($action, $flags) = /^=for buildtoc flag ([-+])([a-z]+)/) {
            if ($action eq '+') {
                $current_flags .= $flags;
            } else {
                my_die("Attempt to unset [$flags] failed - flags are '$current_flags")
                    unless $current_flags =~ s/[\Q$flags\E]//g;
            }
        } elsif (my ($leafname, $desc) = /^\s+(\S+)\s+(.*)/) {
            my $podname = $leafname;
            my $filename = "pod/$podname.pod";
            foreach (@paths) {
                my ($re, $path) = @$_;
                if ($leafname =~ $re) {
                    $podname = $path . $leafname;
                    $filename = "$podname.pod";
                    last;
                }
            }

            # Keep this compatible with pre-5.10
            my $flags = delete $flag_set{$leafname};
            $flags = $current_flags unless defined $flags;

            my %flags;
            $flags{toc_omit} = 1 if $flags =~ tr/o//d;
            $flags{dual} = $podname ne $leafname;

            $state{generated}{"$podname.pod"}++ if $flags =~ tr/g//d;

            if ($flags =~ tr/r//d) {
                my $readme = $podname;
                $readme =~ s/^perl//;
                $state{readmes}{$readme} = $desc;
                $flags{readme} = 1;
            } else {
                $state{pods}{$podname} = $desc;
            }
            my_die "Unknown flag found in section line: $_" if length $flags;

            push @{$state{master}},
                [$leafname, $filename, \%flags];

            if ($podname eq 'perldelta') {
                local $" = '.';
                push @{$state{master}},
                    [$delta_leaf, "pod/$state{delta_target}"];
                $state{pods}{$delta_leaf} = "Perl changes in version @want";
            }

        } else {
            my_die("Malformed line: $_");
        }
    }
    close $master or my_die("close pod/perl.pod: $!");
    # This has to be special-cased somewhere. Turns out this is cleanest:
    push @{$state{master}}, ['a2p', 'x2p/a2p.pod', {toc_omit => 1}];

    my_die("perl.pod sets flags for unknown pods: "
           . join ' ', sort keys %flag_set)
        if keys %flag_set;
}

sub get_pod_metadata {
    # Do we expect to find generated pods on disk?
    my $permit_missing_generated = shift;
    # Do they want a consistency report?
    my $callback = shift;
    local $_;

    __prime_state() unless $state{master};
    return \%state unless $callback;

    my %BuildFiles;

    foreach my $path (@_) {
        $path =~ m!([^/]+)$!;
        ++$BuildFiles{$1};
    }

    # Sanity cross check

    my (%disk_pods, %manipods, %manireadmes);
    my (%cpanpods, %cpanpods_leaf);
    my (%our_pods);

    # There are files that we don't want to list in perl.pod.
    # Maybe the various stub manpages should be listed there.
    my %ignoredpods = map { ( "$_.pod" => 1 ) } qw( );

    # Convert these to a list of filenames.
    ++$our_pods{"$_.pod"} foreach keys %{$state{pods}};
    foreach (@{$state{master}}) {
        ++$our_pods{"$_->[0].pod"}
            if $_->[2]{readme};
    }

    opendir my $dh, 'pod';
    while (defined ($_ = readdir $dh)) {
        next unless /\.pod\z/;
        ++$disk_pods{$_};
    }

    # Things we copy from won't be in perl.pod
    # Things we copy to won't be in MANIFEST

    my $mani = open_or_die('MANIFEST');
    while (<$mani>) {
        chomp;
        s/\s+.*$//;
        if (m!^pod/([^.]+\.pod)!i) {
            ++$manipods{$1};
        } elsif (m!^README\.(\S+)!i) {
            next if $state{ignore}{$1};
            ++$manireadmes{"perl$1.pod"};
        } elsif (exists $our_pods{$_}) {
            ++$cpanpods{$_};
            m!([^/]+)$!;
            ++$cpanpods_leaf{$1};
            $disk_pods{$_}++
                if -e $_;
        }
    }
    close $mani or my_die "close MANIFEST: $!\n";

    # Are we running before known generated files have been generated?
    # (eg in a clean checkout)
    my %not_yet_there;
    if ($permit_missing_generated) {
        # If so, don't complain if these files aren't yet in place
        %not_yet_there = (%manireadmes, %{$state{generated}}, %{$state{copies}})
    }

    my @inconsistent;
    foreach my $i (sort keys %disk_pods) {
        push @inconsistent, "$0: $i exists but is unknown by buildtoc\n"
            unless $our_pods{$i} || $ignoredpods{$i};
        push @inconsistent, "$0: $i exists but is unknown by MANIFEST\n"
            if !$BuildFiles{'MANIFEST'} # Ignore if we're rebuilding MANIFEST
                && !$manipods{$i} && !$manireadmes{$i} && !$state{copies}{$i}
                    && !$state{generated}{$i} && !$cpanpods{$i};
    }
    foreach my $i (sort keys %our_pods) {
        push @inconsistent, "$0: $i is known by buildtoc but does not exist\n"
            unless $disk_pods{$i} or $BuildFiles{$i} or $not_yet_there{$i};
    }
    unless ($BuildFiles{'MANIFEST'}) {
        # Again, ignore these if we're about to rebuild MANIFEST
        foreach my $i (sort keys %manipods) {
            push @inconsistent, "$0: $i is known by MANIFEST but does not exist\n"
                unless $disk_pods{$i};
            push @inconsistent, "$0: $i is known by MANIFEST but is marked as generated\n"
                if $state{generated}{$i};
        }
    }
    &$callback(@inconsistent);
    return \%state;
}

1;

# Local variables:
# cperl-indent-level: 4
# indent-tabs-mode: nil
# End:
#
# ex: set ts=8 sts=4 sw=4 et:
