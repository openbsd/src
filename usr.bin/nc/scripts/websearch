#! /bin/sh
## Hit the major search engines.  Hose the [large] output to a file!
## autoconverts multiple arguments into the right format for given servers --
## usually worda+wordb, with certain lame exceptions like dejanews.
## Extracting and post-sorting the URLs is highly recommended...
##
## Altavista currently handled by a separate script; may merge at some point.
##
## _H* original 950824, updated 951218 and 960209

test "${1}" = "" && echo 'Needs argument[s] to search for!' && exit 1
PLUSARG="`echo $* | sed 's/ /+/g'`"
PIPEARG="`echo ${PLUSARG} | sed 's/+/|/g'`"
IFILE=/tmp/.webq.$$

# Don't have "nc"?  Get "netcat" from avian.org and add it to your toolkit.
doquery () {
  echo GET "$1" | nc -v -i 1 -w 30 "$2" "$3"
}

# changed since original: now supplying port numbers and separator lines...

echo "=== Yahoo ==="
doquery "/bin/search?p=${PLUSARG}&n=300&w=w&s=a" search.yahoo.com 80

echo '' ; echo "=== Webcrawler ==="
doquery "/cgi-bin/WebQuery?searchText=${PLUSARG}&maxHits=300" webcrawler.com 80

# the infoseek lamers want "registration" before they do a real search, but...
echo '' ; echo "=== Infoseek ==="
echo "  is broken."
# doquery "WW/IS/Titles?qt=${PLUSARG}" www2.infoseek.com 80
# ... which doesn't work cuz their lame server wants the extra newlines, WITH
# CRLF pairs ferkrissake.  Fuck 'em for now, they're hopelessly broken.  If
# you want to play, the basic idea and query formats follow.
# echo "GET /WW/IS/Titles?qt=${PLUSARG}" > $IFILE
# echo "" >> $IFILE
# nc -v -w 30 guide-p.infoseek.com 80 < $IFILE

# this is kinda flakey; might have to do twice??
echo '' ; echo "=== Opentext ==="
doquery "/omw/simplesearch?SearchFor=${PLUSARG}&mode=phrase" \
  search.opentext.com 80

# looks like inktomi will only take hits=100, or defaults back to 30
# we try to suppress all the stupid rating dots here, too
echo '' ; echo "=== Inktomi ==="
doquery "/query/?query=${PLUSARG}&hits=100" ink3.cs.berkeley.edu 1234 | \
  sed '/^<IMG ALT.*inktomi.*\.gif">$/d'

#djnews lame shit limits hits to 120 and has nonstandard format
echo '' ; echo "=== Dejanews ==="
doquery "/cgi-bin/nph-dnquery?query=${PIPEARG}+maxhits=110+format=terse+defaultOp=AND" \
  smithers.dejanews.com 80

# OLD lycos: used to work until they fucking BROKE it...
# doquery "/cgi-bin/pursuit?query=${PLUSARG}&maxhits=300&terse=1" \
#   query5.lycos.cs.cmu.edu 80
# NEW lycos: wants the User-agent field present in query or it returns nothing
# 960206: webmaster@lycos duly bitched at
# 960208: reply received; here's how we will now handle it:
echo \
"GET /cgi-bin/pursuit?query=${PLUSARG}&maxhits=300&terse=terse&matchmode=and&minscore=.5 HTTP/1.x" \
  > $IFILE
echo "User-agent: *FUCK OFF*" >> $IFILE
echo "Why: go ask todd@pointcom.com (Todd Whitney)" >> $IFILE
echo '' >> $IFILE
echo '' ; echo "=== Lycos ==="
nc -v -i 1 -w 30 twelve.srv.lycos.com 80 < $IFILE

rm -f $IFILE
exit 0

# CURRENTLY BROKEN [?]
# infoseek

# some args need to be redone to ensure whatever "and" mode applies
