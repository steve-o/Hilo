#!/usr/bin/perl

use Data::Dumper;

open(MOO, "ticks.txt") || die "error: $!\n";
my @columns = split /,/, <MOO>;
my %idx;
my $i = 0;
foreach  my $column (@columns) {
	$idx{$column} = $i++;
}

my $hi_bid = 0, $lo_bid = 999999, $hi_bid_time = "null", $lo_bid_time = "null";
my $hi_ask = 0, $lo_ask = 999999, $hi_ask_time = "null", $lo_ask_time = "null";
while(<MOO>) {
	chomp;
	@row = split /,/;
	my $bid = $row[$idx{'BidPrice'}], $ask = $row[$idx{'AskPrice'}], $time = $row[$idx{'Time'}];
	$time =~ s/(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})/\1-\2-\3 \4:\5:\6/;
#	print "$bid - $ask\n";
	if ($bid > $hi_bid) {
		$hi_bid = $bid;
		$hi_bid_time = $time;
	}
	if ($ask > $hi_ask) {
		$hi_ask = $ask;
		$hi_ask_time = $time;
	}
	if ($bid < $lo_bid) {
		$lo_bid = $bid;
		$lo_bid_time = $time;
	}
	if ($ask < $lo_ask) {
		$lo_ask = $ask;
		$lo_ask_time = $time;
	}
}

print<<MOO;

Summary
-------

Bid: ${lo_bid} (${lo_bid_time}) - ${hi_bid} (${hi_bid_time})
Ask: ${lo_ask} (${lo_ask_time}) - ${hi_ask} (${hi_ask_time})

MOO
