#! /bin/sh
## special handler for altavista, since they only hand out chunks of 10 at
## a time.  Tries to isolate out results without the leading/trailing trash.
## multiword arguments are foo+bar, as usual.
## Second optional arg switches the "what" field, to e.g. "news"

test "${1}" = "" && echo 'Needs an argument to search for!' && exit 1
WHAT="web"
test "${2}" && WHAT="${2}"

# convert multiple args
PLUSARG="`echo $* | sed 's/ /+/g'`"

# Plug in arg.  only doing simple-q for now; pg=aq for advanced-query
# embedded quotes define phrases; otherwise it goes wild on multi-words
QB="GET /cgi-bin/query?pg=q&what=${WHAT}&fmt=c&q=\"${PLUSARG}\""

# ping 'em once, to get the routing warm
nc -z -w 8 www.altavista.digital.com 24015 2> /dev/null
echo "=== Altavista ==="

for xx in 0 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160 170 180 \
  190 200 210 220 230 240 250 260 270 280 290 300 310 320 330 340 350 ; do
  echo "${QB}&stq=${xx}" | nc -w 15 www.altavista.digital.com 80 | \
  egrep '^<a href="http://'
done

exit 0

# old filter stuff
  sed -e '/Documents .* matching .* query /,/query?.*stq=.* Document/p' \
  -e d

