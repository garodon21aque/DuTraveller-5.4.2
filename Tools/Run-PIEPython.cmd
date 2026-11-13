@echo off
rem Usage: Run-PIEPython.cmd <relative-script-path> [map]
rem Runs a Python script inside a live editor process (PIE-capable).
set SCRIPT=%~1
set MAP=%~2
if "%MAP%"=="" set MAP=/Game/Levels/MainMenu
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Personal Games\DuTraveller-5.4.2-source\DT.uproject" %MAP% -unattended -nullrhi -nosound -NoAutoSave "-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities" "-ExecutePythonScript=%SCRIPT%" "-abslog=C:\Personal Games\DuTraveller-5.4.2-source\Saved\Logs\%~n1.log"
echo SCRIPT_EXIT=%ERRORLEVEL%
