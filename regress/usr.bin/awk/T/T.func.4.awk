BEGIN { unireghf() }

function unireghf(hfeed) {
	hfeed[1]=0
	rcell("foo",hfeed)
	hfeed[1]=0
	rcell("bar",hfeed)
}

function rcell(cellname,hfeed) {
	print cellname
}
