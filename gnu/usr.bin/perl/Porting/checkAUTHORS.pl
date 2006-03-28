#!/usr/bin/perl -w
use strict;
use Text::Wrap;
$Text::Wrap::columns = 80;
my ($committer, $patch, $log);
use Getopt::Long;

my ($rank, $ta, @authors, %authors, %untraced, %patchers, %committers);
my $result = GetOptions ("rank" => \$rank,		    # rank authors
			 "thanks-applied" => \$ta,	    # ranks committers
			 "acknowledged=s"   => \@authors);  # authors files

if (!$result or (($rank||0) + ($ta||0) + (@authors ? 1 : 0) != 1) or !@ARGV) {
  die <<"EOS";
$0 --rank Changelogs                        # rank authors by patches
$0 --acknowledged <authors file> Changelogs # Display unacknowledged authors
$0 --thanks-applied Changelogs		    # ranks committers
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
		   merijn => "h.m.brand\100xs4all.nl",
		   mhx => "mhx-perl\100gmx.net",
		   nicholas => "nick\100unfortu.net",
		   nick => "nick\100ing-simmons.net",
		   pudge => "pudge\100pobox.com",
		   rgs => "rgarciasuarez\100free.fr",
		   sky => "sky\100nanisky.com", 
		   steveh => "steve.hay\100uk.radan.com",
		   stevep => "steve\100fisharerojo.org",
		   gisle => "gisle\100activestate.com",
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
		   "nospam-abuse\100bloodgate.com" => "tels\100bloodgate.com",
		   "jfriedl\100yahoo.com" => "jfriedl\100yahoo-inc.com",
		   "japhy\100pobox.com" => "japhy\100pobox.org",
		   "gellyfish\100gellyfish.com" => "jns\100gellyfish.com",
		   "jcromie\100divsol.com" => "jcromie\100cpan.org",
		   "demerphq\100gmail.com" => "demerphq\100hotmail.com",
		   "rick\100consumercontact.com" => "rick\100bort.ca",
		   "vkonovalov\100spb.lucent.com"
		   => "vkonovalov\100peterstar.ru",
		   "rjk\100linguist.dartmouth.edu"
		   => "rjk\100linguist.thayer.dartmouth.edu",
		   "domo\100computer.org" => "shouldbedomo\100mac.com",
		   "kane\100dwim.org" => "kane\100xs4all.net",
		   "allens\100cpan.org" => "easmith\100beatrice.rutgers.edu",
		   "spoon\100cpan.org" => "spoon\100dellah.org",
		   "ben_tilly\100operamail.com" => "btilly\100gmail.com",
		   "mbarbon\100dsi.unive.it" => "mattia.barbon\100libero.it",
		   "tassilo.parseval\100post.rwth-aachen.de" =>
		   "tassilo.von.parseval\100rwth-aachen.de",
		   "dcd\100tc.fluke.com" => "david.dyck\100fluke.com",
		   "kroepke\100dolphin-services.de"
		   => "kay\100dolphin-services.de",
		   "sebastien\100aperghis.net" => "maddingue\100free.fr",
		   "radu\100netsoft.ro" => "rgreab\100fx.ro",
		   "rick\100consumercontact.com"
		   => "rick.delaney\100rogers.com",
		   "p5-authors\100crystalflame.net"
		   => "perl\100crystalflame.net",
		   "stef\100mongueurs.net" => "stef\100payrard.net",
		   "kstar\100wolfetech.com" => "kstar\100cpan.org",
		   "7k8lrvf02\100sneakemail.com" =>
		   "kjx9zthh3001\100sneakemail.com",
		   "mgjv\100comdyn.com.au" => "mgjv\100tradingpost.com.au",
		   "thomas.dorner\100start.de" => "tdorner\100amadeus.net",
		   "ajohnson\100nvidia.com" => "ajohnson\100wischip.com",
		   "phil\100perkpartners.com" => "phil\100finchcomputer.com",
		   "tom.horsley\100mail.ccur.com" => "tom.horsley\100ccur.com",
		   "rootbeer\100teleport.com" => "rootbeer\100redcat.com",
		   "cp\100onsitetech.com" => "publiustemp-p5p\100yahoo.com",
		   "epeschko\100den-mdev1" => "esp5\100pge.com",
		   "pimlott\100idiomtech.com" => "andrew\100pimlott.net",
		   "fugazi\100zyx.net" => "larrysh\100cpan.org",
		   "merijnb\100iloquent.nl" => "merijnb\100iloquent.com",
		   "whatever\100davidnicol.com" => "davidnicol\100gmail.com",
		   "rmgiroux\100acm.org" => "rmgiroux\100hotmail.com",
		   "smcc\100mit.edu" => "smcc\100ocf.berkeley.edu",
		   "schubiger\100cpan.org" => "steven\100accognoscere.org",
		   "richard.foley\100ubsw.com"
		   => "richard.foley\100t-online.de",
		   "damian\100cs.monash.edu.au" => "damian\100conway.org",
		   "gp\100familiehaase.de" => "gerrit\100familiehaase.de",
		   "juerd\100cpan.org" => "juerd\100convolution.nl",
		   "paul.green\100stratus.com"
		   => "paul_greenvos\100vos.stratus.com",
		   "alian\100cpan.org" => "alian\100alianwebserver.com",
		   # Maybe we should special case this to get real names out?
		   "perlbug\100perl.org" => "perlbug-followup\100perl.org",
		  );

# Make sure these are all lower case.

$map{"autrijus\100egb.elixus.org"} = $map{"autrijus\100geb.elixus.org"}
  = $map{"autrijus\100gmail.com"} = $map{"autrijus\100ossf.iis.sinica.edu.tw"}
  = "autrijus\100autrijus.org";
$map{"ilya\100math.ohio-state.edu"} = $map{"ilya\100math.berkeley.edu"}
  = $map{"ilya\100math.berkeley.edu"} = "nospam-abuse\100ilyaz.org";
$map{"philip.newton\100gmx.net"} = $map{"philip.newton\100datenrevision.de"}
  = $map{"pnewton\100gmx.de"} = "pne\100cpan.org",
$map{"simon\100pembro4.pmb.ox.ac.uk"} = $map{"simon\100brecon.co.uk"}
  = $map{"simon\100othersideofthe.earth.li"} = $map{"simon\100cozens.net"}
  = $map{"simon\100netthink.co.uk"} = "simon\100simon-cozens.org";
$map{"spider\100web.zk3.dec.com"} = $map{"spider\100leggy.zk3.dec.com"}
  = $map{"spider-perl\100orb.nashua.nh.us"}
  = $map{"spider\100peano.zk3.dec.com"}
  = "spider\100orb.nashua.nh.us";
$map{"andreas.koenig.gmwojprw\100franz.ak.mind.de"}
  = $map{"a.koenig\100mind.de"} =  "andreas.koenig\100anima.de";
$map{"japhy\100perlmonk.org"} = $map{"japhy\100cpan.org"}
  = "japhy\100pobox.com";
$map{"rmbarker\100cpan.org"} = "robin.barker\100npl.co.uk";
$map{"yves.orton\100de.mci.com"} = $map{"yves.orton\100mciworldcom.de"}
  = "demerphq\100gmail.com";
$map{"jim.cromie\100gmail.com"} = "jcromie\100divsol.com";
$map{"perl_dummy\100bloodgate.com"} = "nospam-abuse\100bloodgate.com";
$map{"paul.marquess\100ntlworld.com"} = "paul.marquess\100btinternet.com";
$map{"konovalo\100mail.wplus.net"} = $map{"vadim\100vkonovalov.ru"}
  = "vkonovalov\100spb.lucent.com";
$map{"kane\100cpan.org"} = "kane\100dwim.org";
$map{"rs\100crystalflame.net"} = "p5-authors\100crystalflame.net";
$map{"(srezic\100iconmobile.com)"} = "slaven\100rezic.de";
$map{"perl\100dellah.anu.edu.au"} = "spoon\100cpan.org";
$map{"rjk-perl-p5p\100tamias.net"} = "rjk\100linguist.dartmouth.edu";
$map{"sts\100accognoscere.org"} = "schubiger\100cpan.org";
$map{"s.payrard\100wanadoo.fr"} = "stef\100mongueurs.net";
$map{"richard.foley\100ubs.com"} = "richard.foley\100ubsw.com";
# I assume that Ton Hopsel's lack of e-mail address in AUTHORS is deliberate
$map{"me-02\100ton.iguana.be"} = $map{"perl-5.8.0\100ton.iguana.be"}
  = $map{"perl5-porters\100ton.iguana.be"} = "!";
# No real name for these address
$map{$_} = "?" foreach ("grommel\100sears.com", "pxm\100nubz.org",
			"padre\100elte.hu", "jdhedden\100" . "1979.usna.com",
			"nothingmuch\100woobling.org", "bob\100starlabs.net",
			"bbucklan\100jpl-devvax.jpl.nasa.gov",
			"bilbo\100ua.fm", "mats\100sm5sxl.net",
			"chris\100ex-parrot.com", 
			"kaminsky\100math.huji.ac.il",
			"bonefish\100cs.tu-berlin.de",
			"bstrand\100switchmanagement.com",
			"glasser\100tang-eleven-seventy-nine.mit.edu",
			"raf\100tradingpost.com.au", "erik\100cs.uni-jena.de",
			"jms\100mathras.comcast.net", "kvr\100centrum.cz",
			"perlbug\100veggiechinese.net",
			"scott\100perlcode.org",
		       );
# We don't have an e-mail address for Beau Cox
$map{"beau\100beaucox.com"} = "?";

$map{"rgarciasuarez\100mandrakesoft.com"}
  = $map{"rgarciasuarez\100mandriva.com"}
  = $map{"raphel.garcia-suarez\100hexaflux.com"} = "rgs";
$map{"jhietaniemi\100gmail.com"} = $map{"jhi\100kosh.hut.fi"}
  = $map{"jhi\100cc.hut.fi"} = $map{"jarkko.hietaniemi\100nokia.com"} = "jhi";
$map{"nick\100ccl4.org"} = $map{"nick\100talking.bollo.cx"}
  = $map{"nick\100plum.flirble.org"} = $map{"nick\100babyhippo.co.uk"}
  = $map{"nick\100bagpuss.unfortu.net"} = "nicholas";
$map{"craig.berry\100psinetcs.com"} = $map{"craig.berry\100metamorgs.com"}
  = $map{"craig.berry\100signaltreesolutions.com"}
  = $map{"craigberry\100mac.com"} = "craigb";
$map{"davem\100iabyn.nospamdeletethisbit.com" }
  = $map{"davem\100fdgroup.co.uk"} = $map{"davem\100fdisolutions.com"}
 = "davem";
$map{"alan.burlison\100uk.sun.com"} = "alanbur";
$map{"artur\100contiller.se"} = $map{"arthur\100contiller.se"} = "sky";
$map{"h.m.brand\100hccnet.nl"} = $map{"merijn\100l1.procura.nl"} = "merijn";
$map{"nik\100tiuk.ti.com"} = $map{"nick.ing-simmons\100elixent.com"} = "nick";
$map{"hv\100crypt.org"} = "hv";
$map{"gisle\100aas.no"} = "gisle";

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
  ++$authors{'!'};
  ++$authors{'?'};
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
      next if /^$/;
      if (s/^$prefix//) {
	$log .= $_;
      } elsif (/^\s+Branch:/) {
	last LOG;
      } else {
	chomp;
	die "Malformed log end with '$_'";
      }
    }
  }
}

&process ($committer, $patch, $log);

if ($rank) {
  &display_ordered(\%patchers);
} elsif ($ta) {
  &display_ordered(\%committers);
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
  my $what = shift;
  my @sorted;
  while (my ($name, $count) = each %$what) {
    push @{$sorted[$count]}, $name;
  }

  my $i = @sorted;
  return unless $i;
  while (--$i) {
    next unless $sorted[$i];
    print wrap ("$i:\t", "\t", join (" ", sort @{$sorted[$i]}), "\n");
  }
}

sub process {
  my ($committer, $patch, $log) = @_;
  return unless $committer;
  my @authors = $log =~ /From:\s+.*?([^"\@ \t\n<>]+\@[^"\@ \t\n<>]+)/gm;

  if (@authors) {
    foreach (@authors) {
      s/^<//;
      s/>$//;
      $_ = lc $_;
      $patchers{$map{$_} || $_}++;
    }
    # print "$patch: @authors\n";
    ++$committers{$committer};
  } else {
    # print "$patch: $committer\n";
    # Not entirely fair as this means that the maint pumpking scores for
    # everything intergrated that wasn't a third party patch in blead
    $patchers{$committer}++;
  }
}


