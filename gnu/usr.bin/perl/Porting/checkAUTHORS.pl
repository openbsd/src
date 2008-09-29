#!/usr/bin/perl -w
use strict;
use Text::Wrap;
$Text::Wrap::columns = 80;
my ($committer, $patch, $log);
use Getopt::Long;

my ($rank, $percentage, $cumulative, $reverse, $ta, @authors, %authors,
    %untraced, %patchers, %committers);
my $result = GetOptions ("rank" => \$rank,		    # rank authors
			 "thanks-applied" => \$ta,	    # ranks committers
			 "acknowledged=s"   => \@authors ,  # authors files
			 "percentage" => \$percentage,      # show as %age
			 "cumulative" => \$cumulative,
			 "reverse" => \$reverse,
			);

if (!$result or (($rank||0) + ($ta||0) + (@authors ? 1 : 0) != 1) or !@ARGV) {
  die <<"EOS";
$0 --rank Changelogs                        # rank authors by patches
$0 --acknowledged <authors file> Changelogs # Display unacknowledged authors
$0 --thanks-applied Changelogs		    # ranks committers
$0 --percentage ...                         # show rankings as percentages
$0 --cumulative ...                         # show rankings cumulatively
$0 --reverse ...                            # show rankings in reverse
Specify stdin as - if needs be. Remember that option names can be abbreviated.
EOS
}


my $prev = "";
my %map;

while (<DATA>) {
    chomp;
    s/\\100/\@/g;
    $_ = lc;
    if (my ($correct, $alias) = /^\s*([^#\s]\S*)\s+(.*\S)/) {
        if ($correct eq '+') {$correct = $prev} else {$prev = $correct}
        $map {$alias} = $correct;
    }
}

#
# Email addresses for we do not have names.
#
$map {$_} = "?" for 
    "agrow\100thegotonerd.com",
    "alexander_bluhm\100genua.de",
    "alexander_gernler\100genua.de",
    "ammon\100rhythm.com",
    "bah\100longitude.com",
    "bbucklan\100jpl-devvax.jpl.nasa.gov",
    "bilbo\100ua.fm",
    "bob\100starlabs.net",
    "bonefish\100cs.tu-berlin.de",
    "bstrand\100switchmanagement.com",
    "cygwin\100cygwin.com",
    "david\100dhaller.de",
    "dformosa\100dformosa.zeta.org.au",
    "dgay\100acm.org",
    "erik\100cs.uni-jena.de",
    "glasser\100tang-eleven-seventy-nine.mit.edu",
    "gml4410\100ggr.co.uk",
    "grommel\100sears.com",
    "ilya\100juil.nonet",
    "info\100lingo.kiev.ua",
    "jms\100mathras.comcast.net",
    "kan\100dcit.cz",
    "kaminsky\100math.huji.ac.il",
    "knew-p5p\100pimb.org",
    "kvr\100centrum.cz",
    "mauzo\100csv.warwick.ac.uk",
    "merijnb\100ms.com",
    "mlelstv\100serpens.de",
    "p.boven\100sara.nl",
    "padre\100elte.hu",
    "perlbug\100veggiechinese.net",
    "pm\100capmon.dk",
    "premchai21\100yahoo.com",
    "pxm\100nubz.org",
    "raf\100tradingpost.com.au",
    "scott\100perlcode.org",
    "smoketst\100hp46t243.cup.hp.com",
    "yath-perlbug\100yath.de",
;

#
# Email addresses for people that don't have an email address in AUTHORS
# Presumably deliberately?
# 

$map {$_} = '!' for
     # Nick Ing-Simmons has passed away (2006-09-25).
     "nick\100ing-simmons.net",
     "nik\100tiuk.ti.com",
     "nick.ing-simmons\100elixent.com",

     # Iain Truskett has passed away (2003-12-29).
     "perl\100dellah.anu.edu.au",
     "spoon\100dellah.org",
     "spoon\100cpan.org",

     # Ton Hospel
     "me-02\100ton.iguana.be",
     "perl-5.8.0\100ton.iguana.be",
     "perl5-porters\100ton.iguana.be",

     # Beau Cox
     "beau\100beaucox.com",

     # Randy W. Sims 
     "ml-perl\100thepierianspring.org",

     # Yuval Kogman
     "nothingmuch\100woobling.org",

;


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
      s/^\t/        /;
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
  my $total;
  while (my ($name, $count) = each %$what) {
    push @{$sorted[$count]}, $name;
    $total += $count;
  }

  my $i = @sorted;
  return unless @sorted;
  my $sum = 0;
  foreach my $i ($reverse ? 0 .. $#sorted : reverse 0 .. $#sorted) {
    next unless $sorted[$i];
    my $prefix;
    $sum += $i * @{$sorted[$i]};
    # Value to display is either this one, or the cumulative sum.
    my $value = $cumulative ? $sum : $i;
    if ($percentage) {
	$prefix = sprintf "%6.2f:\t", 100 * $value / $total;
    } else {
	$prefix = "$value:\t";
    }
    print wrap ($prefix, "\t", join (" ", sort @{$sorted[$i]}), "\n");
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


__DATA__

#
# List of mappings. First entry the "correct" email address, as appears
# in the AUTHORS file. Second is any "alias" mapped to it.
#
# If the "correct" email address is a '+', the entry above is reused; 
# this for addresses with more than one alias.
#
# Note that all entries are in lowercase. Further, no '@' signs should
# appear; use \100 instead.
#
#
#  Committers.
#
adi                                     enache\100rdslink.ro
alanbur                                 alan.burlison\100sun.com
+                                       alan.burlison\100uk.sun.com
ams                                     ams\100wiw.org
chip                                    chip\100pobox.com
craigb                                  craig.berry\100psinetcs.com
+                                       craig.berry\100metamorgs.com
+                                       craig.berry\100signaltreesolutions.com
+                                       craigberry\100mac.com
davem                                   davem\100fdgroup.com
+                                       davem\100iabyn.nospamdeletethisbit.com
+                                       davem\100fdgroup.co.uk
+                                       davem\100fdisolutions.com
+                                       davem\100iabyn.com
demerphq                                demerphq\100gmail.com
+                                       yves.orton\100de.mci.com
+                                       yves.orton\100mciworldcom.de
doughera                                doughera\100lafayette.edu
gbarr                                   gbarr\100pobox.com
gisle                                   gisle\100activestate.com
+                                       gisle\100aas.no
gsar                                    gsar\100activestate.com
+                                       gsar\100cpan.org
hv                                      hv\100crypt.compulink.co.uk
+                                       hv\100crypt.org
jhi                                     jhi\100iki.fi
+                                       jhietaniemi\100gmail.com
+                                       jhi\100kosh.hut.fi
+                                       jhi\100cc.hut.fi
+                                       jarkko.hietaniemi\100nokia.com
merijn                                  h.m.brand\100xs4all.nl
+                                       h.m.brand\100hccnet.nl
+                                       merijn\100l1.procura.nl
mhx                                     mhx-perl\100gmx.net
nicholas                                nick\100unfortu.net
+                                       nick\100ccl4.org
+                                       nick\100talking.bollo.cx
+                                       nick\100plum.flirble.org
+                                       nick\100babyhippo.co.uk
+                                       nick\100bagpuss.unfortu.net
pudge                                   pudge\100pobox.com
rgs                                     rgarciasuarez\100free.fr
+                                       rgarciasuarez\100mandrakesoft.com
+                                       rgarciasuarez\100mandriva.com
+                                       rgarciasuarez\100gmail.com
+                                       raphel.garcia-suarez\100hexaflux.com
sky                                     sky\100nanisky.com
+                                       artur\100contiller.se
+                                       arthur\100contiller.se
steveh                                  stevehay\100planit.com
+                                       steve.hay\100uk.radan.com
stevep                                  steve\100fisharerojo.org
+                                       steve.peters\100gmail.com

#
# Mere mortals.
#
7k8lrvf02\100sneakemail.com             kjx9zthh3001\100sneakemail.com
+                                       dtr8sin02\100sneakemail.com
+                                       rt8363b02\100sneakemail.com
+                                       o6hhmk002\100sneakemail.com

abe\100ztreet.demon.nl                  abeltje\100cpan.org
abigail\100abigail.be                   abigail\100foad.org
+                                       abigail\100abigail.nl
ajohnson\100nvidia.com                  ajohnson\100wischip.com
alexm\100netli.com                      alexm\100w-m.ru
alian\100cpan.org                       alian\100alianwebserver.com
allens\100cpan.org                      easmith\100beatrice.rutgers.edu
andreas.koenig\100anima.de        andreas.koenig.gmwojprw\100franz.ak.mind.de
+                                 andreas.koenig.7os6vvqr\100franz.ak.mind.de
+                                       a.koenig\100mind.de
anno4000\100lublin.zrz.tu-berlin.de     anno4000\100mailbox.tu-berlin.de
+                                       siegel\100zrz.tu-berlin.de
ash\100cpan.org                         ash_cpan\100firemirror.com
avarab\100gmail.com                     avar\100cpan.org

bah\100ecnvantage.com                   bholzman\100longitude.com
bcarter@gumdrop.flyinganvil.org         q.eibcartereio.=~m-b.{6}-cgimosx@gumdrop.flyinganvil.org
ben_tilly\100operamail.com              btilly\100gmail.com

chromatic\100wgz.org                    chromatic\100rmci.net
clkao\100clkao.org                      clkao\100bestpractical.com
cp\100onsitetech.com                    publiustemp-p5p\100yahoo.com
+                                       publiustemp-p5p3\100yahoo.com
cpan\100audreyt.org                     autrijus\100egb.elixus.org
+                                       autrijus\100geb.elixus.org
+                                       autrijus\100gmail.com
+                                       autrijus\100ossf.iis.sinica.edu.tw
+                                       autrijus\100autrijus.org
+                                       audreyt\100audreyt.org

damian\100cs.monash.edu.au              damian\100conway.org
david.dyck\100fluke.com                 dcd\100tc.fluke.com
demerphq\100gmail.com                   demerphq\100hotmail.com
domo\100computer.org                    shouldbedomo\100mac.com

epeschko\100den-mdev1                   esp5\100pge.com

fugazi\100zyx.net                       larrysh\100cpan.org

gellyfish\100gellyfish.com              jns\100gellyfish.com
gp\100familiehaase.de                   gerrit\100familiehaase.de
grazz\100pobox.com                      grazz\100nyc.rr.com

hio\100ymir.co.jp                       hio\100hio.jp

japhy\100pobox.com                      japhy\100pobox.org
+                                       japhy\100perlmonk.org
+                                       japhy\100cpan.org
jari.aalto\100poboxes.com               jari.aalto\100cante.net
jcromie\100divsol.com                   jcromie\100cpan.org
+                                       jim.cromie\100gmail.com
jdhedden\100cpan.org                    jerry\100hedden.us
+                                       jdhedden\1001979.usna.com
+                                       jdhedden\100gmail.com
+                                       jdhedden\100yahoo.com
jfriedl\100yahoo.com                    jfriedl\100yahoo-inc.com
jjore\100cpan.org                       twists\100gmail.com
juerd\100cpan.org                       juerd\100convolution.nl

kane\100dwim.org                        kane\100xs4all.net
+                                       kane\100cpan.org
+                                       kane\100xs4all.nl
+                                       jos\100dwim.org
+                                       jib\100ripe.net
kroepke\100dolphin-services.de          kay\100dolphin-services.de
kstar\100wolfetech.com                  kstar\100cpan.org

mats\100sm6sxl.net                      mats\100sm5sxl.net
mbarbon\100dsi.unive.it                 mattia.barbon\100libero.it
mcmahon\100ibiblio.org                  mcmahon\100metalab.unc.edu
merijnb\100iloquent.nl                  merijnb\100iloquent.com
mgjv\100comdyn.com.au                   mgjv\100tradingpost.com.au
michael.schroeder\100informatik.uni-erlangen.de mls\100suse.de
mike\100stok.co.uk                      mike\100exegenix.com
mjtg\100cam.ac.uk                       mjtg\100cus.cam.ac.uk

nospam-abuse\100bloodgate.com           tels\100bloodgate.com
+                                       perl_dummy\100bloodgate.com
nospam-abuse\100ilyaz.org               ilya\100math.ohio-state.edu
+                                       ilya\100math.berkeley.edu
+                                       ilya\100math.berkeley.edu

p5-authors\100crystalflame.net          perl\100crystalflame.net
+                                       rs\100crystalflame.net
paul.green\100stratus.com               paul_greenvos\100vos.stratus.com
paul.marquess\100btinternet.com         paul_marquess\100yahoo.co.uk
+                                       paul.marquess\100ntlworld.com
+                                       paul.marquess\100openwave.com
pcg\100goof.com                         schmorp\100schmorp.de
# Maybe we should special case this to get real names out?
perlbug\100perl.org                     perlbug-followup\100perl.org
+                                       bugs-perl5\100bugs6.perl.org
phil\100perkpartners.com                phil\100finchcomputer.com
pimlott\100idiomtech.com                andrew\100pimlott.net
pne\100cpan.org                         philip.newton\100gmx.net
+                                       philip.newton\100datenrevision.de
+                                       pnewton\100gmx.de

radu\100netsoft.ro                      rgreab\100fx.ro
richard.foley\100ubsw.com               richard.foley\100t-online.de
+                                       richard.foley\100ubs.com
+                                       richard.foley\100rfi.net
rick\100consumercontact.com             rick\100bort.ca
+                                       rick.delaney\100rogers.com
rjbs\100cpan.org                        rjbs-perl-p5p\100lists.manxome.org
rjk\100linguist.dartmouth.edu           rjk\100linguist.thayer.dartmouth.edu
+                                       rjk-perl-p5p\100tamias.net
rmgiroux\100acm.org                     rmgiroux\100hotmail.com
robin.barker\100npl.co.uk               rmb1\100cise.npl.co.uk
+                                       rmbarker\100cpan.org
rootbeer\100teleport.com                rootbeer\100redcat.com

schubiger\100cpan.org                   steven\100accognoscere.org
+                                       sts\100accognoscere.org
schwern\100pobox.com                    schwern\100gmail.com
sebastien\100aperghis.net               maddingue\100free.fr
+                                       saper\100cpan.org
simon\100simon-cozens.org               simon\100pembro4.pmb.ox.ac.uk
+                                       simon\100brecon.co.uk
+                                       simon\100othersideofthe.earth.li
+                                       simon\100cozens.net
+                                       simon\100netthink.co.uk
slaven\100rezic.de                      slaven.rezic\100berlin.de
+                                       (srezic\100iconmobile.com)
smcc\100mit.edu                         smcc\100ocf.berkeley.edu
+                                       smcc\100csua.berkeley.edu
spider\100orb.nashua.nh.us              spider\100web.zk3.dec.com
+                                       spider\100leggy.zk3.dec.com
+                                       spider-perl\100orb.nashua.nh.us
+                                       spider\100peano.zk3.dec.com
stef\100mongueurs.net                   stef\100payrard.net
+                                       s.payrard\100wanadoo.fr

tassilo.parseval\100post.rwth-aachen.de tassilo.von.parseval\100rwth-aachen.de
thomas.dorner\100start.de               tdorner\100amadeus.net
tjenness\100cpan.org                    t.jenness\100jach.hawaii.edu
+                                       timj\100jach.hawaii.edu
tom.horsley\100mail.ccur.com            tom.horsley\100ccur.com

vkonovalov\100lucent.com                vkonovalov\100peterstar.ru
+                                       konovalo\100mail.wplus.net
+                                       vadim\100vkonovalov.ru
+                                       vkonovalov\100spb.lucent.com
+                                       vkonovalov\100alcatel-lucent.com

whatever\100davidnicol.com              davidnicol\100gmail.com
wolfgang.laun\100alcatel.at             wolfgang.laun\100chello.at
+                                       wolfgang.laun\100thalesgroup.com
+                                       wolfgang.laun\100gmail.com
