#!/usr/bin/perl
# parse HiloAndStitch.xml file and produce a feedlog Tcl script.
# badly assumes one line per FX pair.
# badly assumes no custom FIDs.

use POSIX qw(strftime);

$FEEDLOG_PATH = 'D:/Vhayu/Feeds/Hilo_';
$INTERVAL = 900;

$yesterday = time - (1 * 24 * 60 * 60);
$timestamp_a = strftime ("%Y%m%d", gmtime $yesterday);
$timestamp_b = strftime ("%Y-%m-%d", gmtime $yesterday);

print<<HEADER;
# I am not a comment

set feed_log "${FEEDLOG_PATH}${timestamp_a}000000.log"

set FX [list]
HEADER

while(<STDIN>) {
# <pair name="USDDKK=" src="DKK="/>
	if (m#<pair\s+name="(.+)"\s+src="(.+)"\s*/>#) {
		$name = $1;
		$ric  = $2;
		print<<MOO;
lappend FX [list "${name},${ric}"]
MOO
	}
# <pair name="EURGBP=">
	elsif (m#<pair\s+name="(.+)"\s*/>#) {
		$ric = $1;
		print<<MOO;
lappend FX [list "${fx},${fx}"]
MOO
	}
# <synthetic name="TWDJPY="><divide/><leg>JPY=X</leg><leg>TWD=X</leg></synthetic>
	elsif (m#<synthetic\s+name="(.+)"\s*>\s*<(times|divide)\s*/>\s*<leg.*>(.+?)</leg>\s*<leg.*>(.+?)</leg>\s*</synthetic>#) {
		$name   = $1;
		$op     = (0 == ($2 cmp "times")) ? "MUL" : "DIV";
		$first  = $3;
		$second = $4;
		print<<MOO;
lappend FX [list "${name},${op},${first},${second}"]
MOO
	}
}

print<<FOOTER;

set interval ${INTERVAL}

set from [clock scan "${timestamp_b} 00:00:00"]
set till [clock scan "${timestamp_b} 23:59:59"]

set now [clock clicks -milliseconds]
set results [hilo_feedlog \$feed_log \$FX \$interval \$from \$till]
set elapsed [expr [clock clicks -milliseconds] - \$now]

set stats [list]
lappend stats "hilo_feedlog \${elapsed}ms 0"
join "\$results \$stats" \\n
FOOTER

# eof
