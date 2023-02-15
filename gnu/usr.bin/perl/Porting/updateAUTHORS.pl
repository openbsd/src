#!/usr/bin/env perl
package Porting::updateAUTHORS;
use strict;
use warnings;
use Getopt::Long qw(GetOptions);
use Pod::Usage qw(pod2usage);
use Data::Dumper;
use Encode qw(encode_utf8 decode_utf8 decode);

# The style of this file is determined by:
#
# perltidy -w -ple -bbb -bbc -bbs -nolq -l=80 -noll -nola -nwls='=' \
#   -isbc -nolc -otr -kis -ci=4 -se -sot -sct -nsbl -pt=2 -fs  \
#   -fsb='#start-no-tidy' -fse='#end-no-tidy'

# Info and config for passing to git log.
#   %an: author name
#   %aN: author name (respecting .mailmap, see git-shortlog(1) or git-blame(1))
#   %ae: author email
#   %aE: author email (respecting .mailmap, see git-shortlog(1) or git-blame(1))
#   %cn: committer name
#   %cN: committer name (respecting .mailmap, see git-shortlog(1) or git-blame(1))
#   %ce: committer email
#   %cE: committer email (respecting .mailmap, see git-shortlog(1) or git-blame(1))
#   %H: commit hash
#   %h: abbreviated commit hash
#   %s: subject
#   %x00: print a byte from a hex code

my %field_spec= (
    "an" => "author_name",
    "aN" => "author_name_mm",
    "ae" => "author_email",
    "aE" => "author_email_mm",
    "cn" => "committer_name",
    "cN" => "committer_name_mm",
    "ce" => "committer_email",
    "cE" => "committer_email_mm",
    "H"  => "commit_hash",
    "h"  => "abbrev_hash",
    "s"  => "commit_subject",
);

my @field_codes= sort keys %field_spec;
my @field_names= map { $field_spec{$_} } @field_codes;
my $tformat= join "%x00", map { "%" . $_ } @field_codes;

sub _make_name_author_info {
    my ($author_info, $commit_info, $name_key)= @_;
    (my $email_key= $name_key) =~ s/name/email/;
    my $email= $commit_info->{$email_key};
    my $name= $commit_info->{$name_key};

    my $line= $author_info->{"email2line"}{$email}
        // $author_info->{"name2line"}{$name};

    $line //= sprintf "%-31s<%s>",
        $commit_info->{$name_key}, $commit_info->{$email_key};
    return $line;
}

sub _make_name_simple {
    my ($commit_info, $key)= @_;
    my $name_key= $key . "_name";
    my $email_key= $key . "_email";
    return sprintf "%s <%s>", $commit_info->{$name_key},
        lc($commit_info->{$email_key});
}

sub read_commit_log {
    my ($author_info, $mailmap_info)= @_;
    $author_info ||= {};
    open my $fh, qq(git log --pretty='tformat:$tformat' |);

    while (defined(my $line= <$fh>)) {
        chomp $line;
        $line= decode_utf8($line);
        my $commit_info= {};
        @{$commit_info}{@field_names}= split /\0/, $line, 0 + @field_names;

        my $author_name_mm= _make_name_author_info($author_info, $commit_info,
            "author_name_mm");

        my $committer_name_mm=
            _make_name_author_info($author_info, $commit_info,
            "committer_name_mm");

        my $author_name_real= _make_name_simple($commit_info, "author");

        my $committer_name_real= _make_name_simple($commit_info, "committer");

        _check_name_mailmap(
            $mailmap_info, $author_name_mm, $author_name_real,
            $commit_info,  "author name"
        );
        _check_name_mailmap($mailmap_info, $committer_name_mm,
            $committer_name_real, $commit_info, "committer name");

        $author_info->{"lines"}{$author_name_mm}++;
        $author_info->{"lines"}{$committer_name_mm}++;
    }
    return $author_info;
}

sub read_authors {
    my ($authors_file)= @_;
    $authors_file ||= "AUTHORS";

    my @authors_preamble;
    open my $in_fh, "<", $authors_file
        or die "Failed to open for read '$authors_file': $!";
    while (defined(my $line= <$in_fh>)) {
        chomp $line;
        push @authors_preamble, $line;
        if ($line =~ /^--/) {
            last;
        }
    }
    my %author_info;
    while (defined(my $line= <$in_fh>)) {
        chomp $line;
        $line= decode_utf8($line);
        my ($name, $email);
        my $copy= $line;
        $copy =~ s/\s+\z//;
        if ($copy =~ s/<([^<>]*)>//) {
            $email= $1;
        }
        elsif ($copy =~ s/\s+(\@\w+)\z//) {
            $email= $1;
        }
        $copy =~ s/\s+\z//;
        $name= $copy;
        $email //= "unknown";
        $email= lc($email);

        $author_info{"lines"}{$line}++;
        $author_info{"email2line"}{$email}= $line
            if $email and $email ne "unknown";
        $author_info{"name2line"}{$name}= $line
            if $name and $name ne "unknown";
        $author_info{"email2name"}{ lc($email) }= $name
            if $email
            and $name
            and $email ne "unknown";
        $author_info{"name2email"}{$name}= $email
            if $name and $name ne "unknown";
    }
    close $in_fh
        or die "Failed to close '$authors_file': $!";
    return (\%author_info, \@authors_preamble);
}

sub update_authors {
    my ($author_info, $authors_preamble, $authors_file)= @_;
    $authors_file ||= "AUTHORS";
    my $authors_file_new= $authors_file . ".new";
    open my $out_fh, ">", $authors_file_new
        or die "Failed to open for write '$authors_file_new': $!";
    binmode $out_fh;
    foreach my $line (@$authors_preamble) {
        print $out_fh encode_utf8($line), "\n"
            or die "Failed to print to '$authors_file_new': $!";
    }
    foreach my $author (_sorted_hash_keys($author_info->{"lines"})) {
        next if $author =~ /^unknown/;
        if ($author =~ s/\s*<unknown>\z//) {
            next if $author =~ /^\w+$/;
        }
        print $out_fh encode_utf8($author), "\n"
            or die "Failed to print to '$authors_file_new': $!";
    }
    close $out_fh
        or die "Failed to close '$authors_file_new': $!";
    rename $authors_file_new, $authors_file
        or die "Failed to rename '$authors_file_new' to '$authors_file':$!";
    return 1;    # ok
}

sub read_mailmap {
    my ($mailmap_file)= @_;
    $mailmap_file ||= ".mailmap";

    open my $in, "<", $mailmap_file
        or die "Failed to read '$mailmap_file': $!";
    my %mailmap_hash;
    my @mailmap_preamble;
    my $line_num= 0;
    while (defined(my $line= <$in>)) {
        ++$line_num;
        next unless $line =~ /\S/;
        chomp($line);
        $line= decode_utf8($line);
        if ($line =~ /^#/) {
            if (!keys %mailmap_hash) {
                push @mailmap_preamble, $line;
            }
            else {
                die encode_utf8 "Not expecting comments after header ",
                    "finished at line $line_num!\nLine: $line\n";
            }
        }
        else {
            $mailmap_hash{$line}= $line_num;
        }
    }
    close $in;
    return \%mailmap_hash, \@mailmap_preamble;
}

# this can be used to extract data from the checkAUTHORS data
sub merge_mailmap_with_AUTHORS_and_checkAUTHORS_data {
    my ($mailmap_hash, $author_info)= @_;
    require 'Porting/checkAUTHORS.pl' or die "No authors?";
    my ($map, $preferred_email_or_github)=
        Porting::checkAUTHORS::generate_known_author_map();

    foreach my $old (sort keys %$preferred_email_or_github) {
        my $new= $preferred_email_or_github->{$old};
        next if $old !~ /\@/ or $new !~ /\@/ or $new eq $old;
        my $name= $author_info->{"email2name"}{$new};
        if ($name) {
            my $line= "$name <$new> <$old>";
            $mailmap_hash->{$line}++;
        }
    }
    return 1;    # ok
}

sub _sorted_hash_keys {
    my ($hash)= @_;
    my @sorted= sort { lc($a) cmp lc($b) || $a cmp $b } keys %$hash;
    return @sorted;
}

sub update_mailmap {
    my ($mailmap_hash, $mailmap_preamble, $mailmap_file)= @_;
    $mailmap_file ||= ".mailmap";

    my $mailmap_file_new= $mailmap_file . "_new";
    open my $out, ">", $mailmap_file_new
        or die "Failed to write '$mailmap_file_new':$!";
    binmode $out;
    foreach my $line (@$mailmap_preamble, _sorted_hash_keys($mailmap_hash),) {
        print $out encode_utf8($line), "\n"
            or die "Failed to print to '$mailmap_file': $!";
    }
    close $out;
    rename $mailmap_file_new, $mailmap_file
        or die "Failed to rename '$mailmap_file_new' to '$mailmap_file':$!";
    return 1;    # ok
}

sub parse_mailmap_hash {
    my ($mailmap_hash)= @_;
    my @recs;
    foreach my $line (sort keys %$mailmap_hash) {
        my $line_num= $mailmap_hash->{$line};
        $line =~ /^ \s* (?: ( [^<>]*? ) \s+ )? <([^<>]*)>
                (?: \s+ (?: ( [^<>]*? ) \s+ )? <([^<>]*)> )? \s* \z /x
            or die encode_utf8 "Failed to parse line num $line_num: '$line'";
        if (!$1 or !$2) {
            die encode_utf8 "Both preferred name and email are mandatory ",
                "in line num $line_num: '$line'";
        }

        # [ preferred_name, preferred_email, other_name, other_email ]
        push @recs, [ $1, $2, $3, $4, $line_num ];
    }
    return \@recs;
}

sub _safe_set_key {
    my ($hash, $root_key, $key, $val, $pretty_name)= @_;
    $hash->{$root_key}{$key} //= $val;
    my $prev= $hash->{$root_key}{$key};
    if ($prev ne $val) {
        die encode_utf8 "Collision on mapping $root_key: "
            . " '$key' maps to '$prev' and '$val'\n";
    }
}

my $O2P= "other2preferred";
my $O2PN= "other2preferred_name";
my $O2PE= "other2preferred_email";
my $P2O= "preferred2other";
my $N2P= "name2preferred";
my $E2P= "email2preferred";

my $blurb= "";    # FIXME - replace with a nice message

sub _check_name_mailmap {
    my ($mailmap_info, $auth_name, $raw_name, $commit_info, $descr)= @_;
    my $name= $auth_name;
    $name =~ s/<([^<>]+)>/<\L$1\E>/
        or $name =~ s/(\s)(\@\w+)\z/$1<\L$2\E>/
        or $name .= " <unknown>";

    $name =~ s/\s+/ /g;

    if (!$mailmap_info->{$P2O}{$name}) {
        warn encode_utf8 sprintf "Unknown %s '%s' in commit %s '%s'\n%s",
            $descr,
            $name, $commit_info->{"abbrev_hash"},
            $commit_info->{"commit_subject"},
            $blurb;
        $mailmap_info->{add}{"$name $raw_name"}++;
        return 0;
    }
    elsif (!$mailmap_info->{$P2O}{$name}{$raw_name}) {
        $mailmap_info->{add}{"$name $raw_name"}++;
    }
    return 1;
}

sub check_fix_mailmap_hash {
    my ($mailmap_hash, $authors_info)= @_;
    my $parsed= parse_mailmap_hash($mailmap_hash);
    my @fixed;
    my %seen_map;
    my %pref_groups;

    # first pass through the data, do any conversions, eg, LC
    # the email address, decode any MIME-Header style email addresses.
    # We also correct any preferred name entries so they match what
    # we already have in AUTHORS, and check that there aren't collisions
    # or other issues in the data.
    foreach my $rec (@$parsed) {
        my ($pname, $pemail, $oname, $oemail, $line_num)= @$rec;
        $pemail= lc($pemail);
        $oemail= lc($oemail) if defined $oemail;
        if ($pname =~ /=\?UTF-8\?/) {
            $pname= decode("MIME-Header", $pname);
        }
        my $auth_email= $authors_info->{"name2email"}{$pname};
        if ($auth_email) {
            ## this name exists in authors, so use its email data for pemail
            $pemail= $auth_email;
        }
        my $auth_name= $authors_info->{"email2name"}{$pemail};
        if ($auth_name) {
            ## this email exists in authors, so use its name data for pname
            $pname= $auth_name;
        }

        # neither name nor email exist in authors.
        if ($pname ne "unknown") {
            if (my $email= $seen_map{"name"}{$pname}) {
                ## we have seen this pname before, check the pemail
                ## is consistent
                if ($email ne $pemail) {
                    warn encode_utf8 "Inconsistent emails for name '$pname'"
                        . " at line num $line_num: keeping '$email',"
                        . " ignoring '$pemail'\n";
                    $pemail= $email;
                }
            }
            else {
                $seen_map{"name"}{$pname}= $pemail;
            }
        }
        if ($pemail ne "unknown") {
            if (my $name= $seen_map{"email"}{$pemail}) {
                ## we have seen this preferred_email before, check the preferred_name
                ## is consistent
                if ($name ne $pname) {
                    warn encode_utf8 "Inconsistent name for email '$pemail'"
                        . " at line num $line_num: keeping '$name', ignoring"
                        . " '$pname'\n";
                    $pname= $name;
                }
            }
            else {
                $seen_map{"email"}{$pemail}= $pname;
            }
        }

        # Build an index of "preferred name/email" to other-email, other name
        # we use this later to remove redundant entries missing a name.
        $pref_groups{"$pname $pemail"}{$oemail}{ $oname || "" }=
            [ $pname, $pemail, $oname, $oemail, $line_num ];
    }

    # this removes entries like
    # Joe <blogs> <whatever>
    # where there is a corresponding
    # Joe <blogs> Joe X <blogs>
    foreach my $pref (_sorted_hash_keys(\%pref_groups)) {
        my $entries= $pref_groups{$pref};
        foreach my $email (_sorted_hash_keys($entries)) {
            my @names= _sorted_hash_keys($entries->{$email});
            if ($names[0] eq "" and @names > 1) {
                shift @names;
            }
            foreach my $name (@names) {
                push @fixed, $entries->{$email}{$name};
            }
        }
    }

    # final pass through the dataset, build up a database
    # we will use later for checks and updates, and reconstruct
    # the canonical entries.
    my $new_mailmap_hash= {};
    my $mailmap_info=     {};
    foreach my $rec (@fixed) {
        my ($pname, $pemail, $oname, $oemail, $line_num)= @$rec;
        my $preferred= "$pname <$pemail>";
        my $other;
        if (defined $oemail) {
            $other= $oname ? "$oname <$oemail>" : "<$oemail>";
        }
        if ($other and $other ne "<unknown>") {
            _safe_set_key($mailmap_info, $O2P,  $other, $preferred);
            _safe_set_key($mailmap_info, $O2PN, $other, $pname);
            _safe_set_key($mailmap_info, $O2PE, $other, $pemail);
        }
        $mailmap_info->{$P2O}{$preferred}{$other}++;
        if ($pname ne "unknown") {
            _safe_set_key($mailmap_info, $N2P, $pname, $preferred);
        }
        if ($pemail ne "unknown") {
            _safe_set_key($mailmap_info, $E2P, $pemail, $preferred);
        }
        my $line= $preferred;
        $line .= " $other" if $other;
        $new_mailmap_hash->{$line}= $line_num;
    }
    return ($new_mailmap_hash, $mailmap_info);
}

sub add_new_mailmap_entries {
    my ($mailmap_hash, $mailmap_info, $mailmap_file)= @_;

    my $mailmap_add= $mailmap_info->{add}
        or return 0;

    my $num= 0;
    for my $new (sort keys %$mailmap_add) {
        !$mailmap_hash->{$new}++ or next;
        warn encode_utf8 "Updating '$mailmap_file' with: $new\n";
        $num++;
    }
    return $num;
}

sub read_and_update {
    my ($authors_file, $mailmap_file)= @_;

    # read the authors file and extract the info it contains
    my ($author_info, $authors_preamble)= read_authors($authors_file);

    # read the mailmap file.
    my ($orig_mailmap_hash, $mailmap_preamble)= read_mailmap($mailmap_file);

    # check and possibly fix the mailmap data, and build a set of precomputed
    # datasets to work with it.
    my ($mailmap_hash, $mailmap_info)=
        check_fix_mailmap_hash($orig_mailmap_hash, $author_info);

    # update the mailmap based on any check or fixes we just did,
    # we always write even if we did not do any changes.
    update_mailmap($mailmap_hash, $mailmap_preamble, $mailmap_file);

    # read the commits names using git log, and compares and checks
    # them against the data we have in authors.
    read_commit_log($author_info, $mailmap_info);

    # update the authors file with any changes, we always write,
    # but we may not change anything
    update_authors($author_info, $authors_preamble, $authors_file);

    # check if we discovered new email data from the commits that
    # we need to write back to disk.
    add_new_mailmap_entries($mailmap_hash, $mailmap_info, $mailmap_file)
        and update_mailmap($mailmap_hash, $mailmap_preamble,
        $mailmap_file, $mailmap_info);

    return undef;
}

sub main {
    local $Data::Dumper::Sortkeys= 1;
    my $authors_file= "AUTHORS";
    my $mailmap_file= ".mailmap";
    my $show_man= 0;
    my $show_help= 0;

    ## Parse options and print usage if there is a syntax error,
    ## or if usage was explicitly requested.
    GetOptions(
        'help|?'                      => \$show_help,
        'man'                         => \$show_man,
        'authors_file|authors-file=s' => \$authors_file,
        'mailmap_file|mailmap-file=s' => \$mailmap_file,
    ) or pod2usage(2);
    pod2usage(1)             if $show_help;
    pod2usage(-verbose => 2) if $show_man;

    read_and_update($authors_file, $mailmap_file);
    return 0;    # 0 for no error - intended for exit();
}

exit(main()) unless caller;

1;
__END__

=head1 NAME

Porting/updateAUTHORS.pl - Automatically update AUTHORS and .mailmap
based on commit data.

=head1 SYNOPSIS

Porting/updateAUTHORS.pl

 Options:
   --help               brief help message
   --man                full documentation
   --authors-file=FILE  override default location of AUTHORS
   --mailmap-file=FILE  override default location of .mailmap

=head1 OPTIONS

=over 4

=item --help

Print a brief help message and exits.

=item --man

Prints the manual page and exits.

=item --authors-file=FILE

=item --authors_file=FILE

Override the default location of the authors file, which is "AUTHORS" in
the current directory.

=item --mailmap-file=FILE

=item --mailmap_file=FILE

Override the default location of the mailmap file, which is ".mailmap"
in the current directory.

=back

=head1 DESCRIPTION

This program will automatically manage updates to the AUTHORS file and
.mailmap file based on the data in our commits and the data in the files
themselves. It uses no other sources of data. Expects to be run from
the root a git repo of perl.

In simple, execute the script and it will either die with a helpful
message or it will update the files as necessary, possibly not at all if
there is no need to do so. Note it will actually rewrite the files at
least once, but it may not actually make any changes to their content.
Thus to use the script is currently required that the files are
modifiable.

Review the changes it makes to make sure they are sane. If they are
commit. If they are not then update the AUTHORS or .mailmap files as is
appropriate and run the tool again. Typically you shouldn't need to do
either unless you are changing the default name or email for a user. For
instance if a person currently listed in the AUTHORS file whishes to
change their preferred name or email then change it in the AUTHORS file
and run the script again. I am not sure when you might need to directly
modify .mailmap, usually modifying the AUTHORS file should suffice.

=head1 FUNCTIONS

Note that the file can also be used as a package. If you require the
file then you can access the functions located within the package
C<Porting::updateAUTHORS>. These are as follows:

=over 4

=item add_new_mailmap_entries($mailmap_hash, $mailmap_info, $mailmap_file)

If any additions were identified while reading the commits this will
inject them into the mailmap_hash so they can be written out. Returns a
count of additions found.

=item check_fix_mailmap_hash($mailmap_hash, $authors_info)

Analyzes the data contained the in the .mailmap file and applies any
automated fixes which are required and which it can automatically
perform. Returns a hash of adjusted entries and a hash with additional
metadata about the mailmap entries.

=item main()

This implements the command line version of this module, handle command
line options, etc.

=item merge_mailmap_with_AUTHORS_and_checkAUTHORS_data

This is a utility function that combines data from this tool with data
contained in F<Porting/checkAUTHORS.pl> it is not used directly, but was
used to cleanup and generate the current version of the .mailmap file.

=item parse_mailmap_hash($mailmap_hash)

Takes a mailmap_hash and parses it and returns it as an array of array
records with the contents:

    [ $preferred_name, $preferred_email,
      $other_name, $other_email,
      $line_num ]

=item read_and_update($authors_file, $mailmap_file)

Wraps the other functions in this library and implements the logic and
intent of this tool. Takes two arguments, the authors file name, and the
mailmap file name. Returns nothing but may modify the AUTHORS file
or the .mailmap file. Requires that both files are editable.

=item read_commit_log($authors_info, $mailmap_info)

Read the commit log and find any new names it contains.

=item read_authors($authors_file)

Read the AUTHORS file and return data about it.

=item read_mailmap($mailmap_file)

Read the .mailmap file and return data about it.

=item update_authors($authors_info, $authors_preamble, $authors_file)

Write out an updated AUTHORS file. This is done atomically
using a rename, we will not leave a half modified file in
the repo.

=item update_mailmap($mm_hash, $mm_preamble, $mailmap_file, $mm_info)

Write out an updated .mailmap file. This is done atomically
using a rename, we will not leave a half modified file in
the repo.

=back

=head1 TODO

More documentation and testing.

=head1 SEE ALSO

F<Porting/checkAUTHORS.pl>

=head1 AUTHOR

Yves Orton <demerphq@gmail.com>

=cut
