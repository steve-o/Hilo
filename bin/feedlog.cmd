@echo off
For /F "Tokens=1" %%I in ('type %VHAYU_HOME%\Engine\Scripts\IP.txt') Do Set VHAYU_IP=%%I
set VHAYU_ANALYTICSENGINE_PORT=7071
"%VHAYU_HOME%\Plugins\Hilo\Scripts\create-feedlog-tcl.pl" < "%VHAYU_HOME%\Plugins\Hilo\Config\HiloAndStitch.xml" > "%VHAYU_HOME%\Temp\feedlog.tcl"
"%VHAYU_HOME%\Engine\bin64\TCLClient" -i %VHAYU_IP% -p %VHAYU_ANALYTICSENGINE_PORT% -f"%VHAYU_HOME%\Temp\feedlog.tcl"
