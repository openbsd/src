#!/usr/bin/perl -w
use strict;
use Text::Wrap;
$Text::Wrap::columns = 80;
my ($committer, $patch, $log);
use Getopt::Long;

my ($rank, @authors, %authors, %untraced, %patchers);
my $result = GetOptions ("rank" => \$rank,			# rank authors
			 "acknowledged=s"   => \@authors);	# authors files

if (!$result or !($rank xor @authors) or !@ARGV) {
  die <<"EOS";
$0 --rank Changelogs                        # rank authors by patches
$0 --acknowledged <authors file> Changelogs # Display unacknowledged authors
Specify stdin as - if needs be. Remember that option names can be abbreviated.
EOS
}

my %map = reverse (
		   # "Correct" => "Alias"
		   adi => "enache\100rdslink.ro",
		   alanbur => "alan.burlison\100sun.com",
		   ams => "ams\100wiw.org",
		   chip => "chip\100pobox.com",
		   davem => "davem\100fdgroup.com",
		   doughera => " doughera\100lafayette.edu",
		   gbarr => "gbarr\100pobox.com",
		   gsar => "gsar\100activestate.com",
		   hv => "hv\100crypt.compulink.co.uk",
		   jhi => "jhi\100iki.fi",
		   merijn => "h.m.brand\100hccnet.nl",
		   mhx => "mhx-perl\100gmx.net",
		   nicholas => "nick\100unfortu.net",
		   nick => "nick\100ing-simmons.net",
		   pudge => "pudge\100pobox.com",
		   rgs => "rgarciasuarez\100free.fr",
		   sky => "sky\100nanisky.com", 
		   "abigail\100abigail.nl"=> "abigail\100foad.org",
		   "chromatic\100wgz.org" => "chromatic\100rmci.net",
		   "slaven\100rezic.de" => "slaven.rezic\100berlin.de",
		   "mjtg\100cam.ac.uk" => "mjtg\100cus.cam.ac.uk",
		   "robin.barker\100npl.co.uk" => "rmb1\100cise.npl.co.uk",
		   "paul.marquess\100btinternet.com"
		   => "paul_marquess\100yahoo.co.uk",
		   "wolfgang.laun\100chello.at" =>
		   "wolfgang.laun\100alcatel.at",
		   "t.jenness\100jach.hawaii.edu" => "timj\100jach.hawaii.edu",
		   "abe\100ztreet.demon.nl" => "abeltje\100cpan.org",
		   "perl_dummy\100bloodgate.com" => "tels\100bloodgate.com",
		   "jfriedl\100yahoo.com" => "jfriedl\100yahoo-inc.com",
		   "japhy\100pobox.com" => "japhy\100pobox.org",
		   "gellyfish\100gellyfish.com" => "jns\100gellyfish.com",
		  );

# Make sure these are all lower case.

$map{"alan.burlison\100uk.sun.com"} = "alanbur";
$map{"artur\100contiller.se"} = $map{"arthur\100contiller.se"} = "sky";
$map{"autrijus\100egb.elixus.org"} = $map{"autrijus\100geb.elixus.org"}
  = "autrijus\100autrijus.org";
$map{"craig.berry\100psinetcs.com"} = $map{"craig.berry\100metamorgs.com"}
  = $map{"craig.berry\100signaltreesolutions.com"} = "craigberry\100mac.com";
$map{"davem\100fdgroup.co.uk"} = "davem";
$map{"ilya\100math.ohio-state.edu"} = $map{"ilya\100math.berkeley.edu"}
  = $map{"ilya\100math.berkeley.edu"} = "nospam-abuse\100ilyaz.org";
$map{"jhi\100kosh.hut.fi"} = $map{"jhi\100cc.hut.fi"} = "jhi";
$map{"nick\100ccl4.org"} = $map{"nick\100talking.bollo.cx"}
  = $map{"nick\100plum.flirble.org"} = $map{"nick\100babyhippo.co.uk"}
  = $map{"nick\100bagpuss.unfortu.net"} = "nicholas";
$map{"philip.newton\100gmx.net"} = $map{"philip.newton\100datenrevision.de"}
  = "pnewton\100gmx.de",
$map{"raphel.garcia-suarez\100hexaflux.com"} = "rgs";
$map{"simon\100pembro4.pmb.ox.ac.uk"} = $map{"simon\100brecon.co.uk"}
  = $map{"simon\100othersideofthe.earth.li"} = $map{"simon\100cozens.net"}
  = $map{"simon\100netthink.co.uk"} = "simon\100simon-cozens.org";
$map{"spider\100web.zk3.dec.com"} = $map{"spider\100leggy.zk3.dec.com"}
  = $map{"spider-perl\100orb.nashua.nh.us"}
  = $map{"spider\100peano.zk3.dec.com"}
  = "spider\100orb.nashua.nh.us";
$map{"nik\100tiuk.ti.com"} = "nick";

$map{"a.koenig\100mind.de"} = "andreas.koenig\100anima.de";
$map{"japhy\100perlmonk.org"} = $map{"japhy\100cpan.org"}
  = "japhy\100pobox.com";
$map{"rmbarker\100cpan.org"} = "robin.barker\100npl.co.uk";

if (@authors) {
  my %raw;
  foreach my $filename (@authors) {
    open FH, "<$filename" or die "Can't open $filename: $!";
    while (<FH>) {
      next if /^\#/;
      next if /^-- /;
      if (/<([^>]+)>/) {
	# Easy line.
	$raw{$1}++;
      } elsif (/^([-A-Za-z0-9 .\'À-ÖØöø-ÿ]+)[\t\n]/) {
	# Name only
	$untraced{$1}++;
      } else {
	chomp;
	warn "Can't parse line '$_'";
      }
    }
  }
  foreach (keys %raw) {
    print "E-mail $_ occurs $raw{$_} times\n" if $raw{$_} > 1;
    $_ = lc $_;
    $authors{$map{$_} || $_}++;
  }
}

while (<>) {
  next if /^-+/;
  if (m!^\[\s+(\d+)\]\s+By:\s+(\S+)\s+on!) {
    # new patch
    my @new = ($1, $2);
    &process ($committer, $patch, $log);
    ($patch, $committer) = @new;
    undef $log;
  } elsif (s/^(\s+Log: )//) {
    die "Duplicate Log:" if $log;
    $log = $_;
    my $prefix = " " x length $1;
    LOG: while (<>) {
      if (s/^$prefix//) {
	$log .= $_;
      } elsif (/^\s+Branch:/) {
	last LOG;
      } else {
	die "Malformed log end with $_";
      }
    }
  }
}

&process ($committer, $patch, $log);

if ($rank) {
  &display_ordered;
} elsif (%authors) {
  my %missing;
  foreach (sort keys %patchers) {
    next if $authors{$_};
    # Sort by number of patches, then name.
    $missing{$patchers{$_}}->{$_}++;
  }
  foreach my $patches (sort {$b <=> $a} keys %missing) {
    print "$patches patch(es)\n";
    foreach my $author (sort keys %{$missing{$patches}}) {
      print "  $author\n";
    }
  }
}

sub display_ordered {
  my @sorted;
  while (my ($name, $count) = each %patchers) {
    push @{$sorted[$count]}, $name;
  }

  my $i = @sorted;
  while (--$i) {
    next unless $sorted[$i];
    print wrap ("$i:\t", "\t", join (" ", sort @{$sorted[$i]}), "\n");
  }
}

sub process {
  my ($committer, $patch, $log) = @_;
  return unless $committer;
  my @authors = $log =~ /From:.+\s+([^\@ \t\n]+\@[^\@ \t\n]+)/gm;

  if (@authors) {
    foreach (@authors) {
      s/^<//;
      s/>$//;
      $_ = lc $_;
      $patchers{$map{$_} || $_}++;
    }
    # print "$patch: @authors\n";
  } else {
    # print "$patch: $committer\n";
    # Not entirely fair as this means that the maint pumpking scores for
    # everything intergrated that wasn't a third party patch in blead
    $patchers{$committer}++;
  }
}


