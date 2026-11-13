@echo off
rem Runs PIE-GameplayAudit.py inside a live editor process (PIE-capable).
rem -run=pythonscript is a commandlet that exits before PIE; do not use it.
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Personal Games\DuTraveller-5.4.2-source\DT.uproject" /Game/Levels/Tutorial -unattended -nullrhi -nosound -NoAutoSave -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -ExecutePythonScript="C:\Personal Games\DuTraveller-5.4.2-source\Tools\PIE-GameplayAudit.py" -abslog="C:\Personal Games\DuTraveller-5.4.2-source\Saved\Logs\PIE-GameplayAudit.log"
echo SCRIPT_EXIT=%ERRORLEVEL%
