#!/usr/bin/perl

use strict;
use warnings 'all';

use LWP::Simple qw /$ua getstore/;

my %urls;

my @dummy = qw(
	   http://something.here
	   http://www.pvhp.com
	      );
my %dummy;

@dummy{@dummy} = ();

foreach my $file (<*/*.pod */*/*.pod */*/*/*.pod README README.* INSTALL>) {
    open my $fh => $file or die "Failed to open $file: $!\n";
    while (<$fh>) {
        if (m{(?:http|ftp)://(?:(?!\w<)[-\w~?@=.])+} && !exists $dummy{$&}) {
            my $url = $&;
            $url =~ s/\.$//;
            $urls {$url} ||= { };
            $urls {$url} {$file} = 1;
        }
    }
    close $fh;
}

sub fisher_yates_shuffle {
    my $deck = shift;  # $deck is a reference to an array
    my $i = @$deck;
    while (--$i) {
	my $j = int rand ($i+1);
	@$deck[$i,$j] = @$deck[$j,$i];
    }
}

my @urls = keys %urls;

fisher_yates_shuffle(\@urls);

sub todo {
    warn "(", scalar @urls, " URLs)\n";
}

my $MAXPROC = 40;
my $MAXURL  = 10;
my $MAXFORK = $MAXPROC < $MAXURL ? 1 : $MAXPROC / $MAXURL;

select(STDERR); $| = 1;
select(STDOUT); $| = 1;

while (@urls) {
    my @list;
    my $pid;
    my $i;

    todo();

    for ($i = 0; $i < $MAXFORK; $i++) {
	$list[$i] = [ splice @urls, 0, $MAXURL ];
	$pid = fork;
	die "Failed to fork: $!\n" unless defined $pid;
	last unless $pid; # Child.
    }

    if ($pid) {
        # Parent.
	warn "(waiting)\n";
	1 until -1 == wait; # Reap.
    } else {
        # Child.
        foreach my $url (@{$list[$i]}) {
            my $code = getstore $url, "/dev/null";
            next if $code == 200;
            my $f = join ", " => keys %{$urls {$url}};
            printf "%03d  %s: %s\n" => $code, $url, $f;
        }

        exit;
    }
}

__END__
