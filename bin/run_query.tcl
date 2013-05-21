# I am not a comment

set FX [list]
lappend FX [list "JPYKRW=,DIV,KRW=,BidPrice,AskPrice,JPY=EBS,BidPrice,AskPrice"]
lappend FX [list "GBPHKD=,MUL,GBP=D2,BidPrice,AskPrice,HKD=D2,BidPrice,AskPrice"]
lappend FX [list "USDCAD=,EQ,CAD=D2,BidPrice,AskPrice"]
lappend FX [list "USDCAD=TOB,EQ,CAD=D2,GeneralValue1,GeneralValue3"]

set from [clock scan "2011-12-21 00:00:00"]
set till [clock scan "2011-12-21 23:59:59"]

set now [clock clicks -milliseconds]
set results [hilo_query $FX $from $till]
set elapsed [expr [clock clicks -milliseconds] - $now]

set stats [list]
lappend stats "hilo_query ${elapsed}ms 0"
join "$results $stats" \n

# Sample output:
#
# Symbol         High           Low
# ---------------------------------------------
# JPYKRW=        14.7619352772  14.79551857
# GBPHKD=        12.2247438     12.1660432
# USDCAD=         1.0308         1.0208
# USDCAD=TOB      1.0308         1.0208
# ---------------------------------------------
# hilo_query    234ms            0
