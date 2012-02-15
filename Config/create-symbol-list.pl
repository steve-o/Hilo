#!/usr/bin/perl
# parse HiloAndStitch.xml file and produce a symbol list.
# badly assumes one line per FX pair.

while(<STDIN>) {
# <pair name="USDDKK=" src="DKK="/>
	if (m#<pair.+src="(.+)".*>#) {
		print "$1\r\n";
	}
# <pair name="EURGBP=">
	elsif (m#<pair.+name="(.+)".*>#) {
		print "$1\r\n";
	}
# <synthetic name="TWDJPY="><divide/><leg>JPY=X</leg><leg>TWD=X</leg></synthetic>
	elsif (m#<leg.*>(.+?)</leg>\s*<leg.*>(.+?)</leg>#) {
		print "$1\r\n$2\r\n";
	}
}
