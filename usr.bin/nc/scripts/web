#! /bin/sh
## The web sucks.  It is a mighty dismal kludge built out of a thousand
## tiny dismal kludges all band-aided together, and now these bottom-line
## clueless pinheads who never heard of "TCP handshake" want to run
## *commerce* over the damn thing.  Ye godz.  Welcome to TV of the next
## century -- six million channels of worthless shit to choose from, and
## about as much security as today's cable industry!
##
## Having grown mightily tired of pain in the ass browsers, I decided
## to build the minimalist client.  It doesn't handle POST, just GETs, but
## the majority of cgi forms handlers apparently ignore the method anyway.
## A distinct advantage is that it *doesn't* pass on any other information
## to the server, like Referer: or info about your local machine such as
## Netscum tries to!
##
## Since the first version, this has become the *almost*-minimalist client,
## but it saves a lot of typing now.  And with netcat as its backend, it's
## totally the balls.  Don't have netcat?  Get it here in /src/hacks!
## _H* 950824, updated 951009 et seq.
##
## args: hostname [port].  You feed it the filename-parts of URLs.
## In the loop, HOST, PORT, and SAVE do the right things; a null line
## gets the previous spec again [useful for initial timeouts]; EOF to exit.
## Relative URLs behave like a "cd" to wherever the last slash appears, or
## just use the last component with the saved preceding "directory" part.
## "\" clears the "filename" part and asks for just the "directory", and
## ".." goes up one "directory" level while retaining the "filename" part.
## Play around; you'll get used to it.

if test "$1" = "" ; then
  echo Needs hostname arg.
  exit 1
fi
umask 022

# optional PATH fixup
# PATH=${HOME}:${PATH} ; export PATH

test "${PAGER}" || PAGER=more
BACKEND="nc -v -w 15"
TMPAGE=/tmp/web$$
host="$1"
port="80"
if test "$2" != "" ; then
  port="$2"
fi

spec="/"
specD="/"
specF=''
saving=''

# be vaguely smart about temp file usage.  Use your own homedir if you're
# paranoid about someone symlink-racing your shell script, jeez.
rm -f ${TMPAGE}
test -f ${TMPAGE} && echo "Can't use ${TMPAGE}" && exit 1

# get loopy.  Yes, I know "echo -n" aint portable.  Everything echoed would
# need "\c" tacked onto the end in an SV universe, which you can fix yourself.
while echo -n "${specD}${specF} " && read spec ; do
  case $spec in
  HOST)
    echo -n 'New host: '
    read host
    continue
  ;;
  PORT)
    echo -n 'New port: '
    read port
    continue
  ;;
  SAVE)
    echo -n 'Save file: '
    read saving
# if we've already got a page, save it
    test "${saving}" && test -f ${TMPAGE} &&
      echo "=== ${host}:${specD}${specF} ===" >> $saving &&
      cat ${TMPAGE} >> $saving && echo '' >> $saving
    continue
  ;;
# changing the logic a bit here.  Keep a state-concept of "current dir"
# and "current file".  Dir is /foo/bar/ ; file is "baz" or null.
# leading slash: create whole new state.
  /*)
    specF=`echo "${spec}" | sed 's|.*/||'`
    specD=`echo "${spec}" | sed 's|\(.*/\).*|\1|'`
    spec="${specD}${specF}"
  ;;
# embedded slash: adding to the path.  "file" part can be blank, too
  */*)
    specF=`echo "${spec}" | sed 's|.*/||'`
    specD=`echo "${specD}${spec}" | sed 's|\(.*/\).*|\1|'`
  ;;
# dotdot: jump "up" one level and just reprompt [confirms what it did...]
  ..)
    specD=`echo "${specD}" | sed 's|\(.*/\)..*/|\1|'`
    continue
  ;;
# blank line: do nothing, which will re-get the current one
  '')
  ;;
# hack-quoted blank line: "\" means just zero out "file" part
  '\')
    specF=''
  ;;
# sigh
  '?')
    echo Help yourself.  Read the script fer krissake.
    continue
  ;;
# anything else is taken as a "file" part
  *)
    specF=${spec}
  ;;
  esac

# now put it together and stuff it down a connection.  Some lame non-unix
# http servers assume they'll never get simple-query format, and wait till
# an extra newline arrives.  If you're up against one of these, change
# below to (echo GET "$spec" ; echo '') | $BACKEND ...
  spec="${specD}${specF}"
    echo GET "${spec}" | $BACKEND $host $port > ${TMPAGE}
  ${PAGER} ${TMPAGE}

# save in a format that still shows the URLs we hit after a de-html run
  if test "${saving}" ; then
    echo "=== ${host}:${spec} ===" >> $saving
    cat ${TMPAGE} >> $saving
    echo '' >> $saving
  fi
done
rm -f ${TMPAGE}
exit 0

#######
# Encoding notes, finally from RFC 1738:
# %XX -- hex-encode of special chars
# allowed alphas in a URL: $_-.+!*'(),
# relative names *not* described, but obviously used all over the place
# transport://user:pass@host:port/path/name?query-string
# wais: port 210, //host:port/database?search or /database/type/file?
# cgi-bin/script?arg1=foo&arg2=bar&...  scripts have to parse xxx&yyy&zzz
# ISMAP imagemap stuff: /bin/foobar.map?xxx,yyy -- have to guess at coords!
# local access-ctl files: ncsa: .htaccess ; cern: .www_acl
#######
# SEARCH ENGINES: fortunately, all are GET forms or at least work that way...
# multi-word args for most cases: foo+bar
# See 'websearch' for concise results of this research...
