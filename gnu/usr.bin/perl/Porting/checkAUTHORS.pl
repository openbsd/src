#!/usr/bin/perl -w
package Porting::checkAUTHORS;
use strict;
use warnings;

use v5.026;

use utf8;
use Getopt::Long;
use Unicode::Collate;
use Text::Wrap;
$Text::Wrap::columns = 80;

my ($committer, $patch, $author);
my ($rank, $ta, $ack, $who, $tap, $update) = (0) x 6;
my ($percentage, $cumulative, $reverse);
my (%authors, %untraced, %patchers, %committers, %real_names);
my ( $from_commit, $to_commit );
my ( $map, $preferred_email_or_github );
my $AUTHORS_header;
my $author_file= './AUTHORS';

sub main {
    my $result = GetOptions (
                 # modes
                 "who"            => \$who,
                 "rank"           => \$rank,
                 "thanks-applied" => \$ta,
                 "missing"        => \$ack ,
                 "tap"            => \$tap,
                 "update"         => \$update,

                 # modifiers
                 "authors=s"      => \$author_file,
                 "percentage"     => \$percentage,      # show as %age
                 "cumulative"     => \$cumulative,
                 "reverse"        => \$reverse,
                 "from=s"         => \$from_commit,
                 "to=s"           => \$to_commit,

                );


    my $has_from_commit = defined $from_commit ? 1 : 0;

    if ( !$result # GetOptions failed
        or ( $rank + $ta + $who + $ack + $tap + $update != 1 ) # use one and one exactly 'mode'
        or !( scalar @ARGV + $has_from_commit )  # gitlog provided from --from or stdin
        ) {
        usage();
    }

    die "Can't locate '$author_file'. Specify it with '--authors <path>'."
      unless -f $author_file;

    ( $map, $preferred_email_or_github ) = generate_known_author_map();

    my $preserve_case = $update ? 1 : 0;
    my $AUTHORS_header = read_authors_file($author_file, $preserve_case);

    if ($rank) {
      parse_commits();
      display_ordered(\%patchers);
    } elsif ($ta) {
      parse_commits();
      display_ordered(\%committers);
    } elsif ($tap) {
      parse_commits_authors();
      display_test_output(\%patchers, \%authors, \%real_names);
    } elsif ($ack) {
      parse_commits();
      display_missing_authors(\%patchers, \%authors, \%real_names);
    } elsif ($who) {
      parse_commits();
      list_authors(\%patchers, \%authors);
    } elsif ( $update ) {
      update_authors_files( \%authors, $map, $preferred_email_or_github, $author_file );
    } else {
        die "unknown mode";
    }

    exit(0);
}

main() unless caller;

sub usage {

  die <<"EOS";
Usage: $0 [modes] [modifiers] <git-log-output-file>

Modes (use only one):
   --who                          # show list of unique authors by full name
   --rank                         # rank authors by patches
   --thanks-applied               # ranks committers of others' patches
   --missing                      # display authors not in AUTHORS
   --tap                          # show authors present/missing as TAP
   --update                       # update the AUTHORS file with missing

Modifiers:
   --authors <authors-file>       # path to authors file (default: ./AUTHORS)
   --percentage                   # show rankings as percentages
   --cumulative                   # show rankings cumulatively
   --reverse                      # show rankings in reverse
   --from                         # git commit ID used for 'git log' source (use file from STDIN when missing)
   --to[=HEAD]                    # git commit ID used for 'git log' destination, default to HEAD.

Sample Usages:

  \$ perl Porting/checkAUTHORS.pl --who --from=v5.31.6 --to=v5.31.7
  \$ perl Porting/checkAUTHORS.pl --rank --percentage --from=v5.31.6
  \$ perl Porting/checkAUTHORS.pl --thanks-applied --from=v5.31.6
  \$ perl Porting/checkAUTHORS.pl --missing --from=v5.31.6
  \$ perl Porting/checkAUTHORS.pl --tap --from=v5.31.6
  \$ perl Porting/checkAUTHORS.pl --update --from=v5.31.6

or the split int two and generate your own git log output

Generate git-log-output-file with git log --pretty=fuller rev1..rev2
(or pipe by specifying '-' for stdin).  For example:
  \$ git log --pretty=fuller v5.31.6..v5.31.7 > gitlog
  \$ perl Porting/checkAUTHORS.pl --rank --percentage gitlog


EOS
}

sub list_authors {
    my ($patchers, $authors) = @_;
    binmode(STDOUT, ":utf8");
    print wrap '', '', join(', ', Unicode::Collate->new(level => 1)->sort(
                      map { $authors->{$_} }
                      grep { length $_ > 1 } # skip the exception '!' and '?'
                      keys %$patchers)) . ".\n";
}

# use --from [and --to] if provided
# otherwise fallback to stdin for backward compatibility
sub _git_log {
    if ( length $from_commit ) {
        my ( $from, $to ) = ( $from_commit, $to_commit );
        $to //= 'HEAD';
        my $gitlog = [ qx{git log --pretty=fuller $from..$to} ];
        die "git log failed: $!" unless $? == 0;
        return $gitlog;
    }

    return [ <> ];
}

sub parse_commits {
    my ( $process ) = @_;

    $process //= \&process; # default processor

    my $git_log = _git_log();

    my @lines = split( /^commit\s*/sm, join( '', $git_log->@* ) );
    foreach (@lines) {
        next if m/^$/;
        next if m/^(\S*?)^Merge:/ism;    # skip merge commits
        if (m/^(.*?)^Author:\s*(.*?)^AuthorDate:\s*.*?^Commit:\s*(.*?)^(.*)$/gism) {

            # new patch
            ( $patch, $author, $committer ) = ( $1, $2, $3 );
            chomp($author);
            unless ($author) { die $_ }
            chomp($committer);
            unless ($committer) { die $_ }

            $process->( $committer, $patch, $author );
        } else {
            die "XXX $_ did not match";
        }
    }

    return;
}

# just grab authors. Quicker than parse_commits

sub parse_commits_authors {

    my $git_log = _git_log();

    foreach ($git_log->@*) {
        next unless /^Author:\s*(.*)$/;
        my $author = $1;
        $author = _raw_address($author);
        $patchers{$author}++;
    }

    return;
}

sub generate_known_author_map {
    my %map;

    my %preferred_email_or_github;

    my $previous_name = "";
    my $previous_preferred_contact = "";
    while (<DATA>) {
        next if m{^\s*#};

        chomp;
        s/\\100/\@/g;

        $_ = lc;
        if ( my ( $name, $contact ) = /^\s*([^#\s]\S*)\s+(.*\S)/ ) {

            $name =~ s/^\\043/#/;
            # use the previous stored email if the line starts by a '+'
            if   ( $name eq '+' ) {
                $name             = $previous_name;
            }
            else {
                $previous_name              = $name;
                $previous_preferred_contact = $contact;
                if ( index($name, '@' ) > 0 ) {
                    # if name is an email, then this is our preferred email... legacy list
                    $previous_preferred_contact = $name;
                }
            }

            $map{$contact} = $name;

            if ( $contact ne $previous_preferred_contact ) {
                $preferred_email_or_github{$contact} = $previous_preferred_contact;
            }
            if ( $name ne '+' ) {
                $preferred_email_or_github{$name} = $previous_preferred_contact;
            }
        }
    }

    #
    # Email addresses for we do not have names.
    #
    $map{$_} = "?"
        for
        "bah\100longitude.com",
        "bbucklan\100jpl-devvax.jpl.nasa.gov",
        "bilbo\100ua.fm",
        "bob\100starlabs.net",
        "cygwin\100cygwin.com",
        "david\100dhaller.de", "erik\100cs.uni-jena.de", "info\100lingo.kiev.ua",    # Lingo Translation agency
        "jms\100mathras.comcast.net",
        "premchai21\100yahoo.com",
        "pxm\100nubz.org",
        "raf\100tradingpost.com.au",
        "smoketst\100hp46t243.cup.hp.com", "root\100chronos.fi.muni.cz",             # no clue - jrv 20090803
        "gomar\100md.media-web.de",    # no clue - jrv 20090803
        "data-drift\100so.uio.no",     # no data. originally private message from 199701282014.VAA12645@selters.uio.no
        "arbor\100al37al08.telecel.pt"
        ,    # reported perlbug ticket 5196 - no actual code contribution. no real name - jrv 20091006
        "oracle\100pcr8.pcr.com",    # Reported perlbug ticket 1015 - no patch - Probably Ed Eddington ed@pcr.com
        "snaury\100gmail.com",       # Reported cpan ticket 35943, with patch for fix
        ;

    #
    # Email addresses for people that don't have an email address in AUTHORS
    # Presumably deliberately?
    #

    $map{$_} = '!' for

        # Nick Ing-Simmons has passed away (2006-09-25).
        "nick\100ing-simmons.net",
        "nik\100tiuk.ti.com",
        "nick.ing-simmons\100elixent.com",
        "nick\100ni-s.u-net.com",
        "nick.ing-simmons\100tiuk.ti.com",

        # Iain Truskett has passed away (2003-12-29).
        "perl\100dellah.anu.edu.au", "spoon\100dellah.org", "spoon\100cpan.org",

        # Ton Hospel
        "me-02\100ton.iguana.be", "perl-5.8.0\100ton.iguana.be", "perl5-porters\100ton.iguana.be",

        # Beau Cox
        "beau\100beaucox.com",

        # Randy W. Sims
        "ml-perl\100thepierianspring.org",

        # Jason Hord
        "pravus\100cpan.org",

        # perl internal addresses
        "perl5-porters\100africa.nicoh.com",
        "perlbug\100perl.org",,
        "perl5-porters.nicoh.com",
        "perlbug-followup\100perl.org",
        "perlbug-comment\100perl.org",
        "bug-module-corelist\100rt.cpan.org",
        "bug-storable\100rt.cpan.org",
        "bugs-perl5\100bugs6.perl.org",
        "unknown",
        "unknown\100unknown",
        "unknown\100longtimeago",
        "unknown\100perl.org",
        "",
        "(none)",
        ;

    return ( \%map, \%preferred_email_or_github );
}

sub read_authors_file {
    my ( $filename, $preserve_case ) = @_;
    return unless defined $filename;

    my @headers;

    my (%count, %raw);
    {
        open my $fh, '<', $filename or die "Can't open $filename: $!";
        binmode $fh, ':encoding(UTF-8)';
        my $in_header = 1;
        while (<$fh>) {
            next if /^\#/;
            do { $in_header = 0; next } if /^-- /;
            if (/^([^<]+)<([^>]+)>/) {
                # Easy line.
                my ($name, $email) = ($1, $2);
                $name =~ s/\s*\z//;
                $raw{$email} = $name;
                $count{$email}++;
            } elsif ( /^([^@]+)\s+(\@\S+)\s*$/ ) {
                my ($name, $github) = ($1, $2);
                $name =~ s/\s*\z//;
                $raw{$github} = $name;
                $count{$github}++;
            } elsif (/^([- .'\w]+)[\t\n]/) {

                # Name only
                $untraced{$1}++;
            } elsif ( length $_ ) {
                chomp;
                warn "Can't parse line '$_'";
            } else {
                next;
            }
        }
        continue {
            push @headers, $_ if $in_header;
        }
    }
    foreach my $contact ( sort keys %raw ) {
        print "E-mail $contact occurs $count{$contact} times\n" if $count{$contact} > 1;
        my $lc = lc $contact;
        my $key = $preserve_case ? $contact : $lc;
        $authors{ $map->{$lc} || $key } = $raw{$contact};
    }
    $authors{$_} = $_ for qw(? !);

    push @headers, '-- ', "\n";

    return join( '', @headers );
}

sub update_authors_files {
    my ( $authors, $known_authors, $preferred_email_or_github, $author_file ) = @_;

    die qq[Cannot find AUTHORS file '$author_file'] unless -f $author_file;
    binmode(STDOUT, ":utf8");

    # add missing authors from the recent commits
    _detect_new_authors_from_recent_commit( $authors, $known_authors );

    my @author_names = sort { $a cmp $b } values %$authors;
    my $maxlen = length [ sort { length $b <=> length $a } @author_names ]->[0];

    my @list;
    foreach my $github_or_email ( sort keys %authors ) {

        next if length $github_or_email == 1;

        my $name = $authors{$github_or_email};
        $name =~ s{\s+$}{};

        #$github_or_email = $known_authors->{ $github_or_email } // $github_or_email;
        $github_or_email = $preferred_email_or_github->{ $github_or_email } // $github_or_email;

        if ( index( $github_or_email, '@' ) != 0 ) { # preserve '<>' for unicode consortium
            $github_or_email = '<' . $github_or_email . '>';
        }

        push @list, sprintf( "%-${maxlen}s %s\n", $name, $github_or_email);
    }

    # preserve the untraced authors :-) [without email or GitHub account]
    push @list, map { "$_\n" } keys %untraced;

    {
        open my $fh, '>', $author_file or die "Can't open $author_file: $!";
        binmode $fh, ':raw:encoding(UTF-8)';

        print {$fh} $AUTHORS_header;

        map { print {$fh} $_ } sort { lc $a cmp lc $b } @list;

        close $fh;

    }

    return;
}

# read all recent commits and check if the author email is known
#   if the email is unknown add the author's GitHub account if possible or his email
sub _detect_new_authors_from_recent_commit {
    my ( $authors, $known_authors ) = @_;

    my $check_if_email_known = sub {
        my ( $email ) = @_;

        my $preferred = $map->{$email} // $map->{lc $email}
            // $preferred_email_or_github->{$email}
            // $preferred_email_or_github->{lc $email}
            // $email;

        return $authors{$preferred} || $authors{ lc $preferred } ? 1 : 0;
    };

    my $already_checked = {};
    my $process = sub {
        my ( $committer, $patch, $author ) = @_;

        foreach my $person ( $author, $committer ) {
            next unless length $person;
            next if $already_checked->{$person};
            $already_checked->{$person} = 1;

            my $is_author = $person eq $author;

            if ( $person =~ m{^(.+)\s+<(.+)>$} ) {
                my ( $name, $email ) = ( $1, $2 );

                # skip unicode consortium and bad emails
                if ( index( $email, '@' ) <= 0 ) {
                    warn "# Skipping new author: $person - bad email";
                    next;
                }

                next if $check_if_email_known->( $email );

                # for new users we would prefer using the GitHub account
                my $github_or_email = _commit_to_github_id( $patch, $is_author ) // $email;

                next if $check_if_email_known->( $github_or_email );

                print "# Detected a new author: $name using email $email [ $github_or_email ]\n";
                $authors{$github_or_email} = $name; # add it to the list of authors
            } else {
                warn "Fail to parse author: $person";
            }
        }
    };

    parse_commits( $process );

    return;
}

sub _commit_to_github_id {
    my ( $commit, $is_author ) = @_;

    chomp $commit if defined $commit;
    return unless length $commit;

    eval { require HTTP::Tiny; 1 } or do {
        warn "HTTP::Tiny is missing, cannot detect GitHub account from commit id.";
        no warnings;
        *_commit_to_github_id = sub {};
        return;
    };

    my $github_url_for_commit = q[https://github.com/Perl/perl5/commit/] . $commit;
    my $response = HTTP::Tiny->new->get( $github_url_for_commit );

    if ( ! $response->{success} ) {
        warn "HTTP Request Failed: '$github_url_for_commit'";
        return;
    }

    my $content = $response->{content} // '';

    # poor man scrapping - probably have to be improved over time
    # try to parse something like: <a href="/Perl/perl5/commits?author=ThisIsMyGitHubID"
    my @github_ids; # up to two entries author and committer
    while ( $content =~ s{\Q<a href="/Perl/perl5/commits?author=\E(.+)"}{} ) {
        push @github_ids, '@' . $1;
    }

    warn "Found more than two github ids for $github_url_for_commit" if scalar @github_ids > 2;

    return $github_ids[0] if $is_author;
    if ( !$is_author && scalar @github_ids >= 2 ) {
        return $github_ids[1]; # committer is the second entry
    }

    return $github_ids[0];
}


sub display_test_output {
    my $patchers   = shift;
    my $authors    = shift;
    my $real_names = shift;
    my $count = 0;
    printf "1..%d\n", scalar keys %$patchers;

    foreach my $email ( sort keys %$patchers ) {
        $count++;
        if ($authors->{$email}) {
            print "ok $count - ".$real_names->{$email} ." $email\n";
        } else {
            print "not ok $count - Contributor not found in AUTHORS. ",
                  ($real_names->{$email} || '???' )." $email\n",
                  "# To fix run Porting/updateAUTHORS.pl and then review",
                  " and commit the result.\n";
            print STDERR "# ", ($real_names->{$email} || '???' ), " <$email>",
                  " not found in AUTHORS.\n",
                  "# To fix run Porting/updateAUTHORS.pl and then review",
                  " and commit the result.\n";
        }
    }

    return;
}

sub display_missing_authors {
    my $patchers   = shift;
    my $authors    = shift;
    my $real_names = shift;
    my %missing;
    foreach ( sort keys %$patchers ) {
        next if $authors->{$_};

        # Sort by number of patches, then name.
        $missing{ $patchers{$_} }->{$_}++;
    }
    foreach my $patches ( sort { $b <=> $a } keys %missing ) {
        print "\n\n=head1 $patches patch(es)\n\n";
        foreach my $author ( sort keys %{ $missing{$patches} } ) {
            my $xauthor = $author;
            $xauthor =~ s/@/\\100/g;    # xxx temp hack
            print "" . ( $real_names->{$author} || $author ) . "\t\t\t<" . $xauthor . ">\n";
        }
    }

    return;
}

sub display_ordered {
    my $what = shift;
    my @sorted;
    my $total;

    while ( my ( $name, $count ) = each %$what ) {
        push @{ $sorted[$count] }, $name;
        $total += $count;
    }

    my $i = @sorted;
    return unless @sorted;
    my $sum = 0;
    foreach my $i ( $reverse ? 0 .. $#sorted : reverse 0 .. $#sorted ) {
        next unless $sorted[$i];
        my $prefix;
        $sum += $i * @{ $sorted[$i] };

        # Value to display is either this one, or the cumulative sum.
        my $value = $cumulative ? $sum : $i;
        if ($percentage) {
            $prefix = sprintf "%6.2f:\t", 100 * $value / $total;
        } else {
            $prefix = "$value:\t";
        }
        print wrap ( $prefix, "\t", join( " ", sort @{ $sorted[$i] } ), "\n" );
    }

    return;
}

sub process {
    my ( $committer, $patch, $author ) = @_;
    return unless $author;
    return unless $committer;

    $author = _raw_address($author);
    $patchers{$author}++;

    $committer = _raw_address($committer);
    if ( $committer ne $author ) {

        # separate commit credit only if committing someone else's patch
        $committers{$committer}++;
    }

    return;
}

sub _raw_address {
    my $addr = shift;
    my $real_name;
    if ($addr =~ /(?:\\?")?\s*\(via RT\) <perlbug-followup\@perl\.org>$/p) {
        my $name = ${^PREMATCH};
        $addr = 'perlbug-followup@perl.org';
        #
        # Try to find the author
        #
        if (exists $map->{$name}) {
            $addr = $map->{$name};
            $real_name = $authors{$addr};
        }
        else {
            while (my ($email, $author_name) = each %authors) {
                if ($name eq $author_name) {
                    $addr = $email;
                    $real_name = $name;
                    last;
                }
            }
        }
    }
    elsif ( $addr =~ /<.*>/ ) {
        $addr =~ s/^\s*(.*)\s*<\s*(.*?)\s*>.*$/$2/;
        $real_name = $1;
    }
    $addr =~ s/\[mailto://;
    $addr =~ s/\]//;
    $addr = lc $addr;
    $addr = $map->{$addr} || $addr;
    $addr =~ s/\\100/@/g;    # Sometimes, there are encoded @ signs in the git log.

    if ($real_name) { $real_names{$addr} = $real_name }

    return $addr;
}

1; # make sure we return true in case we are required.
__DATA__

#
# List of mappings. First entry the "correct" email address or GitHub account,
# as appears in the AUTHORS file. Other lines are "alias" mapped to it.
#
# If the "correct" email address is a '+', the entry above it is reused;
# this for addresses with more than one alias.
#
# Note that all entries are in lowercase. Further, no '@' signs should
# appear; use \100 instead.
#
#
#  Committers.
#
adamh                                   \100BytesGuy
+                                       bytesguy\100users.noreply.github.com
+                                       git\100ahartley.com
adi                                     enache\100rdslink.ro
alanbur                                 alan.burlison\100sun.com
+                                       alan.burlison\100uk.sun.com
+                                       aburlison\100cix.compulink.co.uk
ams                                     ams\100toroid.org
+                                       ams\100wiw.org
atoomic                                 \100atoomic
+                                       atoomic\100cpan.org
+                                       cpan\100atoomic.org
+                                       nicolas\100atoomic.org
chip                                    chip\100pobox.com
+                                       chip\100perl.com
+                                       salzench\100nielsenmedia.com
+                                       chip\100atlantic.net
+                                       chip\100rio.atlantic.net
+                                       salzench\100dun.nielsen.com
+                                       chip\100ci005.sv2.upperbeyond.com
craigb                                  craigberry\100mac.com
+                                       craig.berry\100metamorgs.com
+                                       craig.berry\100signaltreesolutions.com
+                                       craig.berry\100psinetcs.com
+                                       craig.a.berry\100gmail.com
+                                       craig a. berry)
davem                                   davem\100iabyn.nospamdeletethisbit.com
+                                       davem\100fdgroup.com
+                                       davem\100iabyn.com
+                                       davem\100fdgroup.co.uk
+                                       davem\100fdisolutions.com
+                                       davem\100iabyn.com
demerphq                                demerphq\100gmail.com
+                                       yves.orton\100de.mci.com
+                                       yves.orton\100mciworldcom.de
+                                       yves.orton\100booking.com
+                                       demerphq\100dromedary.booking.com
+                                       demerphq\100gemini.(none)
+                                       demerphq\100camel.booking.com
+                                       demerphq\100hotmail.com
doughera                                doughera\100lafayette.edu
+                                       doughera\100lafcol.lafayette.edu
+                                       doughera\100fractal.phys.lafayette.edu
+                                       doughera.lafayette.edu
+                                       doughera\100newton.phys.lafayette.edu

gbarr                                   gbarr\100pobox.com
+                                       bodg\100tiuk.ti.com
+                                       gbarr\100ti.com
+                                       graham.barr\100tiuk.ti.com
+                                       gbarr\100monty.mutatus.co.uk
gisle                                   gisle\100aas.no
+                                       gisle\100activestate.com
+                                       aas\100aas.no
+                                       aas\100bergen.sn.no
gsar                                    gsar\100cpan.org
+                                       gsar\100activestate.com
+                                       gsar\100engin.umich.edu
hv                                      hv\100crypt.org
+                                       hv\100crypt.compulink.co.uk
+                                       hv\100iii.co.uk
jhi                                     jhi\100iki.fi
+                                       jhietaniemi\100gmail.com
+                                       jhi\100kosh.hut.fi
+                                       jhi\100alpha.hut.fi
+                                       jhi\100cc.hut.fi
+                                       jhi\100hut.fi
+                                       jarkko.hietaniemi\100nokia.com
+                                       jarkko.hietaniemi\100cc.hut.fi
+                                       jarkko.hietaniemi\100booking.com
jesse                                   jesse\100fsck.com
+                                       jesse\100bestpractical.com
+                                       jesse\100perl.org
merijn                                  h.m.brand\100xs4all.nl
+                                       h.m.brand\100procura.nl
+                                       merijn.brand\100procura.nl
+                                       h.m.brand\100hccnet.nl
+                                       merijn\100l1.procura.nl
+                                       merijn\100a5.(none)
+                                       perl5\100tux.freedom.nl
mhx                                     mhx-perl\100gmx.net
+                                       mhx\100r2d2.(none)
+                                       mhx\100cpan.org
mst                                     mst\100shadowcat.co.uk
+                                       matthewt\100hercule.scsys.co.uk
nicholas                                nick\100ccl4.org
+                                       nick\100unfortu.net
+                                       nick\100talking.bollo.cx
+                                       nick\100plum.flirble.org
+                                       nick\100babyhippo.co.uk
+                                       nick\100bagpuss.unfortu.net
+                                       nick\100babyhippo.com
+                                       nicholas\100dromedary.ams6.corp.booking.com
+                                       Nicholas Clark (sans From field in mail header)
pudge                                   pudge\100pobox.com
rgs                                     rgs@consttype.org
+                                       rgarciasuarez\100free.fr
+                                       rgarciasuarez\100mandrakesoft.com
+                                       rgarciasuarez\100mandriva.com
+                                       rgarciasuarez\100gmail.com
+                                       raphel.garcia-suarez\100hexaflux.com
sky                                     artur\100contiller.se
+                                       sky\100nanisky.com
+                                       arthur\100contiller.se
smueller                                smueller\100cpan.org
+                                       7k8lrvf02\100sneakemail.com
+                                       kjx9zthh3001\100sneakemail.com
+                                       dtr8sin02\100sneakemail.com
+                                       rt8363b02\100sneakemail.com
+                                       o6hhmk002\100sneakemail.com
+                                       l2ot9pa02\100sneakemail.com
+                                       wyp3rlx02\100sneakemail.com
+                                       0mgwtfbbq\100sneakemail.com
+                                       xyey9001\100sneakemail.com
steveh                                  steve.m.hay\100googlemail.com
+                                       stevehay\100planit.com
+                                       steve.hay\100uk.radan.com
stevep                                  steve\100fisharerojo.org
+                                       steve.peters\100gmail.com
+                                       root\100dixie.cscaper.com
timb                                    Tim.Bunce\100pobox.com
+                                       tim.bunce\100ig.co.uk
tonyc                                   tony\100develop-help.com
+                                       tony\100openbsd32.tony.develop-help.com
+                                       tony\100saturn.(none)

#
# Mere mortals.
#
\043####\100juerd.nl                    juerd\100cpan.org
+                                       juerd\100c3.convolution.nl
+                                       juerd\100convolution.nl
a.r.ferreira\100gmail.com               aferreira\100shopzilla.com
abe\100ztreet.demon.nl                  abeltje\100cpan.org
abela\100hsc.fr                         abela\100geneanet.org
abigail\100abigail.be                   abigail\100foad.org
+                                       abigail\100abigail.nl
+                                       abigail\100fnx.com
aburt\100isis.cs.du.edu                 isis!aburt
ach\100mpe.mpg.de                       ach\100rosat.mpe-garching.mpg.de
adavies\100ptc.com                      alex.davies\100talktalk.net
ajohnson\100nvidia.com                  ajohnson\100wischip.com
+                                       anders\100broadcom.com
alexm\100netli.com                      alexm\100w-m.ru
alex-p5p\100earth.li                    alex\100rcon.rog
alexmv\100mit.edu                       alex\100chmrr.net
alian\100cpan.org                       alian\100alianwebserver.com
allen\100grumman.com                    allen\100gateway.grumman.com
allen\100huarp.harvard.edu              nort\100bottesini.harvard.edu
+                                       nort\100qnx.com
allens\100cpan.org                      easmith\100beatrice.rutgers.edu
+                                       root\100dogberry.rutgers.edu
ambs\100cpan.org                        hashashin\100gmail.com
andrea                                  a.koenig@mind.de
+                                       andreas.koenig\100anima.de
+                                       andreas.koenig.gmwojprw\100franz.ak.mind.de
+                                       andreas.koenig.7os6vvqr\100franz.ak.mind.de
+                                       a.koenig\100mind.de
+                                       k\100anna.in-berlin.de
+                                       andk\100cpan.org
+                                       koenig\100anna.mind.de
+                                       k\100anna.mind.de
+                                       root\100ak-71.mind.de
+                                       root\100ak-75.mind.de
+                                       k\100sissy.in-berlin.de
+                                       a.koenig\100kulturbox.de
+                                       k\100sissy.in-berlin.de
+                                       root\100dubravka.in-berlin.de
anno4000\100lublin.zrz.tu-berlin.de     anno4000\100mailbox.tu-berlin.de
+                                       siegel\100zrz.tu-berlin.de
apocal@cpan.org                         perl\1000ne.us
arnold\100gnu.ai.mit.edu                arnold\100emoryu2.arpa
+                                       gatech!skeeve!arnold
arodland\100cpan.org                    andrew\100hbslabs.com
arussell\100cs.uml.edu                  adam\100adam-pc.(none)
ash\100cpan.org                         ash_cpan\100firemirror.com
avar                                    avar\100cpan.org
+                                       avarab\100gmail.com
bailey\100newman.upenn.edu              bailey\100hmivax.humgen.upenn.edu
+                                       bailey\100genetics.upenn.edu
+                                       bailey.charles\100gmail.com
bah\100ecnvantage.com                   bholzman\100longitude.com
barries\100slaysys.com                  root\100jester.slaysys.com
bkedryna\100home.com                    bart\100cg681574-a.adubn1.nj.home.com
bcarter\100gumdrop.flyinganvil.org      q.eibcartereio.=~m-b.{6}-cgimosx\100gumdrop.flyinganvil.org
ben_tilly\100operamail.com              btilly\100gmail.com
+                                       ben_tilly\100hotmail.com
ben\100morrow.me.uk                     mauzo\100csv.warwick.ac.uk
+                                       mauzo\100.(none)
bepi\100perl.it                         enrico.sorcinelli\100gmail.com
bert\100alum.mit.edu                    bert\100genscan.com
bigbang7\100gmail.com                   ddascalescu+github\100gmail.com
blgl\100stacken.kth.se                  blgl\100hagernas.com
+                                       2bfjdsla52kztwejndzdstsxl9athp\100gmail.com
b@os13.org                              brad+github\10013os.net
khw\100cpan.org                         khw\100karl.(none)
brian.d.foy\100gmail.com                bdfoy\100cpan.org
BQW10602\100nifty.com                   sadahiro\100cpan.org
bulk88\100hotmail.com                   bulk88

chad.granum\100dreamhost.com            exodist7\100gmail.com
choroba\100cpan.org                     choroba\100weed.(none)
+                                       choroba\100matfyz.cz
chromatic\100wgz.org                    chromatic\100rmci.net
ckuskie\100cadence.com                  colink\100perldreamer.com
claes\100surfar.nu                      claes\100versed.se
clintp\100geeksalad.org                 cpierce1\100ford.com
clkao\100clkao.org                      clkao\100bestpractical.com
corion\100corion.net                    corion\100cpan.org
+                                       github@corion.net
cp\100onsitetech.com                    publiustemp-p5p\100yahoo.com
+                                       publiustemp-p5p3\100yahoo.com
+                                       ovid\100cpan.org
cpan\100audreyt.org                     autrijus\100egb.elixus.org
+                                       autrijus\100geb.elixus.org
+                                       autrijus\100gmail.com
+                                       autrijus\100ossf.iis.sinica.edu.tw
+                                       autrijus\100autrijus.org
+                                       audreyt\100audreyt.org
cpan\100ton.iguana.be                   me-01\100ton.iguana.be
crt\100kiski.net                        perl\100ctweten.amsite.com
cp\100onsitetech.com                    ovid\100cpan.org
dairiki\100dairiki.org                  dairiki at dairiki.org
dagolden\100cpan.org                    xdaveg\100gmail.com
+                                       xdg\100xdg.me
damian\100conway.org                    damian\100cs.monash.edu.au
dan\100sidhe.org                        sugalsd\100lbcc.cc.or.us
+                                       sugalskd\100osshe.edu
daniel\100bitpusher.com                 daniel\100biz.bitpusher.com
dave\100mag-sol.com                     dave\100dave.org.uk
+                                       dave\100perlhacks.com
david.dyck\100fluke.com                 dcd\100tc.fluke.com
david\100justatheory.com                david\100wheeler.net
+                                       david\100kineticode.com
+                                       david\100wheeler.com
+                                       david\100wheeler.net
whatever\100davidnicol.com              davidnicol\100gmail.com
dennis\100booking.com                   dennis\100camel.ams6.corp.booking.com
+					dennis.kaarsemaker\100booking.com
+                                       dennis\100kaarsemaker.net
dev-perl\100pimb.org                    knew-p5p\100pimb.org
+                                       lists-p5p\100pimb.org
djberg86\100attbi.com                   djberg96\100attbi.com
dk\100tetsuo.karasik.eu.org             dmitry\100karasik.eu.org
dma+github@stripysock.com               dominichamon@users.noreply.github.com
dom\100earth.li                         dom\100semmle.com
domo\100computer.org                    shouldbedomo\100mac.com
+                                       domo\100slipper.ip.lu
+                                       domo\100tcp.ip.lu
dougm\100covalent.net                   dougm\100opengroup.org
+                                       dougm\100osf.org
dougw\100cpan.org                       doug_wilson\100intuit.com
dwegscheid\100qtm.net                   wegscd\100whirlpool.com
edwardp\100excitehome.net               epeschko\100den-mdev1
+                                       epeschko\100elmer.tci.com
+                                       esp5\100pge.com
egf7\100columbia.edu                    efifer\100sanwaint.com
eggert\100twinsun.com                   eggert\100sea.sm.unisys.com
etj\100cpan.org                         mohawk2\100users.noreply.github.com

fugazi\100zyx.net                       larrysh\100cpan.org
+                                       lshatzer\100islanddata.com

gbacon\100itsc.uah.edu                  gbacon\100adtrn-srv4.adtran.com
gerberb\100zenez.com                    root\100devsys0.zenez.com
gfuji\100cpan.org                       g.psy.va\100gmail.com
genesullivan50\100yahoo.com             gsullivan\100cpan.org
gerard\100ggoossen.net                  gerard\100tty.nl
gibreel\100pobox.com                    stephen.zander\100interlock.mckesson.com
+                                       srz\100loopback
gideon\100cpan.org                      gidisrael\100gmail.com
gnat\100frii.com                        gnat\100prometheus.frii.com
gp\100familiehaase.de                   gerrit\100familiehaase.de
grazz\100pobox.com                      grazz\100nyc.rr.com
gward\100ase.com                        greg\100bic.mni.mcgill.ca
haggai\100cpan.org                      alanhaggai\100alanhaggai.org
+                                       alanhaggai\100gmail.com
hansmu\100xs4all.nl                     hansm\100icgroup.nl
+                                       hansm\100icgned.nl
+                                       hans\100icgned.nl
+                                       hans\100icgroup.nl
+                                       hansm\100euronet.nl
+                                       hansm\100euro.net
hio\100ymir.co.jp                       hio\100hio.jp
hops\100sco.com                         hops\100scoot.pdev.sco.com

ian.goodacre\100xtra.co.nz              ian\100debian.lan
ingo_weinhold\100gmx.de                 bonefish\100cs.tu-berlin.de

james\100mastros.biz                    theorb\100desert-island.me.uk
jan\100jandubois.com                    jand\100activestate.com
+                                       jan.dubois\100ibm.net
japhy\100pobox.com                      japhy\100pobox.org
+                                       japhy\100perlmonk.org
+                                       japhy\100cpan.org
+                                       jeffp\100crusoe.net
jari.aalto\100poboxes.com               jari.aalto\100cante.net
jarausch\100numa1.igpm.rwth-aachen.de   helmutjarausch\100unknown
jasons\100cs.unm.edu                    jasons\100sandy-home.arc.unm.edu
jbuehler\100hekimian.com                jhpb\100hekimian.com
jcromie\100cpan.org                      jcromie\100100divsol.com
+                                       jim.cromie\100gmail.com
jd\100cpanel.net                        lightsey\100debian.org
+                                       john\10004755.net
+                                       john\100nixnuts.net
jdhedden\100cpan.org                    jerry\100hedden.us
+                                       jdhedden\1001979.usna.com
+                                       jdhedden\100gmail.com
+                                       jdhedden\100yahoo.com
+                                       jhedden\100pn100-02-2-356p.corp.bloomberg.com
+                                       jdhedden\100solydxk
jeremy\100zawodny.com                   jzawodn\100wcnet.org
jesse\100sig.bsh.com                    jesse\100ginger
jfriedl\100yahoo.com                    jfriedl\100yahoo-inc.com
jfs\100fluent.com                       jfs\100jfs.fluent.com
jhannah\100mutationgrid.com             jay\100jays.net
+                                       jhannah\100omnihotels.com
jidanni\100jidanni.org                  jidanni\100hoffa.dreamhost.com
jjore\100cpan.org                       twists\100gmail.com
jkeenan\100cpan.org                     jkeen\100verizon.net
+                                       jkeenan\100dromedary-001.ams6.corp.booking.com
jns\100integration-house.com            jns\100gellyfish.com
+                                       gellyfish\100gellyfish.com
john\100atlantech.com                   john\100titanic.atlantech.com
john\100johnwright.org                  john.wright\100hp.com
joseph\100cscaper.com                   joseph\1005sigma.com
joshua\100rodd.us                       jrodd\100pbs.org
jtobey\100john-edwin-tobey.org          jtobey\100user1.channel1.com
jpeacock\100messagesystems.com          john.peacock\100havurah-software.org
+                                       jpeacock\100havurah-software.org
+                                       jpeacock\100dsl092-147-156.wdc1.dsl.speakeasy.net
+                                       jpeacock\100jpeacock-hp.doesntexist.org
+                                       jpeacock\100cpan.org
+                                       jpeacock\100rowman.com
james.schneider\100db.com               jschneid\100netilla.com
jpl.jpl\100gmail.com                    jpl\100research.att.com
jql\100accessone.com                    jql\100jql.accessone.com
jsm28\100hermes.cam.ac.uk               jsm28\100cam.ac.uk

kane\100dwim.org                        kane\100xs4all.net
+                                       kane\100cpan.org
+                                       kane\100xs4all.nl
+                                       jos\100dwim.org
+                                       jib\100ripe.net
keith.s.thompson\100gmail.com           kst\100mib.org
ken\100mathforum.org                    kenahoo\100gmail.com
+                                       ken.williams\100thomsonreuters.com
kentfredric\100gmail.com                kentnl\100cpan.org
kmx\100volny.cz                         kmx\100volny.cz
+                                       kmx\100cpan.org
kroepke\100dolphin-services.de          kay\100dolphin-services.de
kst\100mib.org                          kst\100cts.com
+                                       kst\100SDSC.EDU
kstar\100wolfetech.com                  kstar\100cpan.org
+                                       kurt_starsinic\100ml.com
+                                       kstar\100www.chapin.edu
+                                       kstar\100chapin.edu
larry\100wall.org                       lwall\100jpl-devvax.jpl.nasa.gov
+                                       lwall\100netlabs.com
+                                       larry\100netlabs.com
+                                       lwall\100sems.com
+                                       lwall\100scalpel.netlabs.com
laszlo.molnar\100eth.ericsson.se        molnarl\100cdata.tvnet.hu
+                                       ml1050\100freemail.hu
lewart\100uiuc.edu                      lewart\100vadds.cvm.uiuc.edu
+                                       d-lewart\100uiuc.edu
lindblad@gmx.com                        52227507+apparluk\100users.noreply.github.com
lkundrak\100v3.sk                       lubo.rintel\100gooddata.com
lstein\100cshl.org                      lstein\100formaggio.cshl.org
+                                       lstein\100genome.wi.mit.edu
l.mai\100web.de                         plokinom\100gmail.com
lupe\100lupe-christoph.de               lupe\100alanya.m.isar.de
lutherh\100stratcom.com                 lutherh\100infinet.com
mab\100wdl.loral.com                    markb\100rdcf.sm.unisys.com
marcel\100codewerk.com                  gr\100univie.ac.at
+                                       hanekomu\100gmail.com
marcgreen\100cpan.org                   marcgreen\100wpi.edu
markleightonfisher\100gmail.com         fisherm\100tce.com
+                                       mark-fisher\100mindspring.com
mark.p.lutz\100boeing.com               tecmpl1\100triton.ca.boeing.com
marnix\100gmail.com                     pttesac!marnix!vanam
marty+p5p\100kasei.com                  marty\100martian.org
mats\100sm6sxl.net                      mats\100sm5sxl.net
mbarbon\100dsi.unive.it                 mattia.barbon\100libero.it
+                                       mattia\100barbon.org
mcmahon\100ibiblio.org                  mcmahon\100metalab.unc.edu
me\100davidglasser.net                  glasser\100tang-eleven-seventy-nine.mit.edu
merijnb\100iloquent.nl                  merijnb\100ms.com
+                                       merijnb\100iloquent.com
merlyn\100stonehenge.com                merlyn\100gadget.cscaper.com
mestre.smash\100gmail.com               smash\100cpan.org
mgjv\100comdyn.com.au                   mgjv\100tradingpost.com.au
mlh\100swl.msd.ray.com                  webtools\100uewrhp03.msd.ray.com
michael.schroeder\100informatik.uni-erlangen.de mls\100suse.de
mike\100stok.co.uk                      mike\100exegenix.com
61100689+mikefultondev\100users.noreply.github.com mikefultonpersonal\100gmail.com
miyagawa\100bulknews.net                    miyagawa\100edge.co.jp
mjtg\100cam.ac.uk                       mjtg\100cus.cam.ac.uk
mikedlr\100tardis.ed.ac.uk              mikedlr\100it.com.pl
moritz\100casella.verplant.org          moritz\100faui2k3.org
+                                       moritz lenz

neale\100VMA.TABNSW.COM.AU              neale\100pucc.princeton.edu
neeracher\100mac.com                    neeri\100iis.ee.ethz.ch
neilb\100neilb.org                      neilb\100cre.canon.co.uk
+                                       neil\100bowers.com

nospam-abuse\100bloodgate.com           tels\100bloodgate.com
+                                       perl_dummy\100bloodgate.com

ian.phillipps\100iname.com              ian_phillipps\100yahoo.co.uk
+                                       ian\100dial.pipex.com
ignasi.roca\100fujitsu-siemens.com      ignasi.roca\100fujitsu.siemens.es
ikegami\100adaelis.com                  eric\100fmdev10.(none)
ilmari\100ilmari.org                    ilmari\100vesla.ilmari.org
illpide\100telecel.pt                   arbor\100al37al08.telecel.pt
# see http://www.nntp.perl.org/group/perl.perl5.porters/2001/01/msg28925.html
#
ilya\100math.berkeley.edu               ilya\100math.ohio-state.edu
+                                       nospam-abuse\100ilyaz.org
+                                       [9]ilya\100math.ohio-state.edu
ilya\100martynov.org                    ilya\100juil.nonet

joshua\100paloalto.com                  joshua.pritikin\100db.com

litt\100acm.org                         tlhackque\100yahoo.com

meyering@asic.sc.ti.com                 jim\100meyering.net

okamoto\100corp.hp.com                  okamoto\100hpcc123.corp.hp.com
orwant\100oreilly.com                   orwant\100media.mit.edu

p5-authors\100crystalflame.net          perl\100crystalflame.net
+                                       rs\100crystalflame.net
+                                       coral\100eekeek.org
+                                       coral\100moonlight.crystalflame.net
+                                       rs\100oregonnet.com
+                                       rs\100topsy.com
paul.green\100stratus.com               paul_greenvos\100vos.stratus.com
+                                       pgreen\100seussnt.stratus.com
pmqs                                    pmqs\100cpan.org
+                                       paul.marquess\100btinternet.com
+                                       paul_marquess\100yahoo.co.uk
+                                       paul.marquess\100ntlworld.com
+                                       paul.marquess\100openwave.com
+                                       pmarquess\100bfsec.bt.co.uk
+                                       pmqs\100cpan.org
+                                       paul\100paul-desktop.(none)
Pavel.Zakouril\100mff.cuni.cz           root\100egg.karlov.mff.cuni.cz
pcg\100goof.com                         schmorp\100schmorp.de
perl\100cadop.com                       cdp\100hpescdp.fc.hp.com
perl\100greerga.m-l.org                 greerga\100m-l.org
perl\100profvince.com                   vince\100profvince.com
perl-rt\100wizbit.be                    p5p\100perl.wizbit.be
# Maybe we should special case this to get real names out?
Peter.Dintelmann\100Dresdner-Bank.com   peter.dintelmann\100dresdner-bank.com
# NOTE: There is an intentional trailing space in the line above
pfeifer\100wait.de                      pfeifer\100charly.informatik.uni-dortmund.de
+                                       upf\100de.uu.net
pjacklam\100online.no                   pjacklam\100gmail.com
ribasushi@cpan.org			rabbit\100rabbit.us
+					rabbit+bugs\100rabbit.us
arc\100cpan.org                         perl\100aaroncrane.co.uk
+                                       arc@users.noreply.github.com
phil\100perkpartners.com                phil\100finchcomputer.com
pimlott\100idiomtech.com                andrew\100pimlott.net
+                                       pimlott\100abel.math.harvard.edu
pixel\100mandriva.com                   pixel\100mandrakesoft.com
pne\100cpan.org                         philip.newton\100gmx.net
+                                       philip.newton\100datenrevision.de
+                                       pnewton\100gmx.de
pprymmer\100factset.com                 pvhp\100forte.com
khw\100cpan.org                         khw\100karl.(none)
+                                       public\100khwilliamson.com
+                                       khw\100khw-desktop.(none)

radu\100netsoft.ro                      rgreab\100fx.ro
raiph                                   \100raiph
+                                       raiph.mellor\100gmail.com
rajagopa\100pauline.schrodinger.com     rajagopa\100schrodinger.com
raphael.manfredi\100pobox.com           raphael_manfredi\100grenoble.hp.com
info\100perl-services.de                renee.baecker\100smart-websolutions.de
+                                       reneeb\100reneeb-desktop.(none)
+                                       github\100renee-baecker.de
+                                       otrs\100ubuntu.(none)
+                                       perl\100renee-baecker.de
+                                       reb\100perl-services.de
+                                       module\100renee-baecker.de
rich+perl\100hyphen-dash-hyphen.info    richardleach\100users.noreply.github.com
richard.foley\100rfi.net                richard.foley\100t-online.de
+                                       richard.foley\100ubs.com
+                                       richard.foley\100ubsw.com
rick\100consumercontact.com             rick\100bort.ca
+                                       rick.delaney\100rogers.com
+                                       rick\100bort.ca
+                                       rick.delaney\100home.com
rjbs\100cpan.org                        rjbs-perl-p5p\100lists.manxome.org
+                                       perl.p5p\100rjbs.manxome.org
+                                       rjbs\100semiotic.systems
+                                       rjbs\100users.noreply.github.com
rjk\100linguist.dartmouth.edu           rjk\100linguist.thayer.dartmouth.edu
+                                       rjk-perl-p5p\100tamias.net
+                                       rjk\100tamias.net
rjray\100redhat.com                     rjray\100uswest.com
rmgiroux\100acm.org                     rmgiroux\100hotmail.com
+                                       mgiroux\100bear.com
rmbarker\100cpan.org                    rmb1\100cise.npl.co.uk
+                                       robin.barker\100npl.co.uk
+                                       rmb\100cise.npl.co.uk
+                                       robin\100spade-ubuntu.(none)
+                                       r.m.barker\100btinternet.com
+                                       rmbarker.cpan\100btinternet.com
robertmay\100cpan.org                   rob\100themayfamily.me.uk
roberto\100keltia.freenix.fr            roberto\100eurocontrol.fr
robin\100cpan.org                       robin\100kitsite.com
roderick\100argon.org                   roderick\100gate.net
+                                       roderick\100ibcinc.com
argrath\100ub32.org                     root\100ub32.org
rootbeer\100teleport.com                rootbeer\100redcat.com
+                                       tomphoenix\100unknown
rra\100stanford.edu                     rra\100cpan.org
rurban\100cpan.org                      rurban\100x-ray.at
+                                       rurban\100cpanel.net
rvtol+news\100isolution.nl              rvtol\100isolution.nl
sartak\100gmail.com                     sartak\100bestpractical.com
+                                       code\100sartak.org
danny-cpan\100sadinoff.com              sadinoff\100olf.com
schubiger\100cpan.org                   steven\100accognoscere.org
+                                       sts\100accognoscere.org
+                                       schubiger\100gmail.com
+                                       stsc\100refcnt.org
schwern\100pobox.com                    schwern\100gmail.com
+                                       schwern\100athens.arena-i.com
+                                       schwern\100blackrider.aocn.com
+                                       schwern\100ool-18b93024.dyn.optonline.net
scop\100cs132170.pp.htv.fi              ville.skytta\100iki.fi
scotth\100sgi.com                       author scotth\100sgi.com 842220273 +0000
+                                       schotth\100sgi.com
schwab\100suse.de                       schwab\100issan.informatik.uni-dortmund.de
+                                       schwab\100ls5.informatik.uni-dortmund.de
sebastien\100aperghis.net               maddingue\100free.fr
+                                       saper\100cpan.org
shigeya\100wide.ad.jp                   shigeya\100foretune.co.jp
shlomif\100cpan.org                     shlomif\100vipe.technion.ac.il
+                                       shlomif\100iglu.org.il
+                                       shlomif+processed-by-perl\100gmail.com
+                                       shlomif\100shlomifish.org
simon\100netthink.co.uk                 simon\100simon-cozens.org
+                                       simon\100pembro4.pmb.ox.ac.uk
+                                       simon\100brecon.co.uk
+                                       simon\100othersideofthe.earth.li
+                                       simon\100cozens.net
+
sisyphus\100cpan.org                    sisyphus1\100optusnet.com.au
+                                       sisyphus359\100gmail.com
lannings\100who.int                     lannings\100gmail.com
+                                       slanning\100cpan.org
slaven\100rezic.de                      slaven.rezic\100berlin.de
+                                       srezic\100iconmobile.com
+                                       srezic\100cpan.org
+                                       eserte\100cs.tu-berlin.de
+                                       eserte\100vran.herceg.de
smcc\100mit.edu                         smcc\100ocf.berkeley.edu
+                                       smcc\100csua.berkeley.edu
+                                       alias\100mcs.com
+                                       smccam\100uclink4.berkeley.edu
spider\100orb.nashua.nh.us              spider\100web.zk3.dec.com
+                                       spider\100leggy.zk3.dec.com
+                                       spider-perl\100orb.nashua.nh.us
+                                       spider\100peano.zk3.dec.com
+                                       spider.boardman\100orb.nashua.nh.us>
+                                       spidb\100cpan.org
+                                       spider.boardman\100orb.nashua.nh.us
+                                       root\100peano.zk3.dec.com
s.denaxas\100gmail.com                  spiros\100lokku.com
spp\100ds.net                           spp\100psa.pencom.com
+                                       spp\100psasolar.colltech.com
+                                       spp\100spotter.yi.org
stef\100mongueurs.net                   stef\100payrard.net
+                                       s.payrard\100wanadoo.fr
+                                       properler\100freesurf.fr
+                                       stef\100francenet.fr
stevan\100cpan.org                      stevan.little\100gmail.com
+                                       stevan.little\100iinteractive.com
sthoenna\100efn.org                     ysth\100raven.shiftboard.com

tassilo.parseval\100post.rwth-aachen.de tassilo.von.parseval\100rwth-aachen.de
tchrist\100perl.com                     tchrist\100mox.perl.com
+                                       tchrist\100jhereg.perl.com
thomas.dorner\100start.de               tdorner\100amadeus.net
tjenness\100cpan.org                    t.jenness\100jach.hawaii.edu
+                                       timj\100jach.hawaii.edu
tobez\100tobez.org                      tobez\100plab.ku.dk
toddr\100cpan.org                       toddr\100cpanel.net
tom\100compton.nu                       thh\100cyberscience.com
tom.horsley\100mail.ccur.com            tom.horsley\100ccur.com
+                                       tom\100amber.ssd.hcsc.com

vkonovalov\100lucent.com                vkonovalov\100peterstar.ru
+                                       konovalo\100mail.wplus.net
+                                       vadim\100vkonovalov.ru
+                                       vkonovalov\100spb.lucent.com
+                                       vkonovalov\100alcatel-lucent.com
+                                       vadim.konovalov\100alcatel-lucent.com

whatever\100davidnicol.com              davidnicol\100gmail.com
wolfgang.laun\100alcatel.at             wolfgang.laun\100chello.at
+                                       wolfgang.laun\100thalesgroup.com
+                                       wolfgang.laun\100gmail.com
wolfsage\100gmail.com                   mhorsfall\100darmstadtium.(none)
yath\100yath.de                         yath-perlbug\100yath.de
