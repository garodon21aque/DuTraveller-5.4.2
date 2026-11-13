@echo off
rem Usage: Run-PythonScript.cmd <relative-script-path>
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Personal Games\DuTraveller-5.4.2-source\DT.uproject" -run=pythonscript -script="%~1" -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput
echo SCRIPT_EXIT=%ERRORLEVEL%
