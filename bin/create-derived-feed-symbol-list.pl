#!/usr/bin/perl
# parse HiloAndStitch.xml file and produce a symbol list.
# badly assumes one line per FX pair.

$SUFFIX = "VTA";

while(<STDIN>) {
	if (m#<(pair|synthetic)\s+name="(.+?)".*>#) {
		$ric = "$2$SUFFIX";
		print "$ric\r\n";
	}
}
