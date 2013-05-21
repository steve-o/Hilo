sudo apt-get install snmp libsnmp-dev

sudo apt-get install snmp-mibs-downloader 

sudo bash
cat > /etc/snmp/snmp.conf
mibs +ALL
^D

MIBS=+STITCH-MIB snmptranslate  -IR -On stitchTclQueryReceived 
.1.3.6.1.4.153736.1.1.2

MIBS=+STITCH-MIB snmptranslate -Tp -IR stitchMIB
+--stitchMIB(153736)
   |
   +--stitchPluginTable(1)
      |
      +--stitchPluginEntry(1)
         |  Index: stitchPluginId
         |
         +-- ---- Unsigned  stitchPluginId(1)
         +-- -R-- Counter   stitchTclQueryReceived(2)
         +-- -R-- Counter   stitchTimerQueryReceived(3)
         +-- -R-- Counter   stitchLastActivity(4)
         +-- -R-- Counter   stitchTclSvcTimeMin(5)
         +-- -R-- Counter   stitchTclSvcTimeMean(6)
         +-- -R-- Counter   stitchTclSvcTimeMax(7)
         +-- -R-- Counter   stitchTimerSvcTimeMin(8)
         +-- -R-- Counter   stitchTimerSvcTimeMean(9)
         +-- -R-- Counter   stitchTimerSvcTimeMax(10)
         +-- -R-- Counter   stitchRfaMsgsSent(11)
         +-- -R-- Counter   stitchRfaEventsReceived(12)
         +-- -R-- Counter   stitchRfaEventsDiscarded(13)
         +-- -R-- Counter   stitchOmmItemEventsReceived(14)
         +-- -R-- Counter   stitchOmmItemEventsDiscarded(15)
         +-- -R-- Counter   stitchResponseMsgsReceived(16)
         +-- -R-- Counter   stitchResponseMsgsDiscarded(17)
         +-- -R-- Counter   stitchMmtLoginResponseReceived(18)
         +-- -R-- Counter   stitchMmtLoginResponseDiscarded(19)
         +-- -R-- Counter   stitchMmtLoginSuccessReceived(20)
         +-- -R-- Counter   stitchMmtLoginSuspectReceived(21)
         +-- -R-- Counter   stitchMmtLoginClosedReceived(22)
         +-- -R-- Counter   stitchOmmCmdErrorsDiscarded(23)
         +-- -R-- Counter   stitchMmtLoginsValidated(24)
         +-- -R-- Counter   stitchMmtLoginsMalformed(25)
         +-- -R-- Counter   stitchMmtLoginsSent(26)
         +-- -R-- Counter   stitchMmtDirectorysValidated(27)
         +-- -R-- Counter   stitchMmtDirectorysMalformed(28)
         +-- -R-- Counter   stitchMmtDirectorysSent(29)
         +-- -R-- Counter   stitchTokensGenerated(30)

env MIBS="+ALL" mib2c -c /etc/snmp/mib2c.iterate.conf stitchMIB

