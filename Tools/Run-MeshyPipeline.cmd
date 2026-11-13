@echo off
rem Run-MeshyPipeline.cmd [script-name] [MESHY_MESH] [MESHY_NAME] [MESHY_FBX] [MESHY_ROUTES]
set SCRIPT=%~1
if "%SCRIPT%"=="" set SCRIPT=C:\Personal Games\DuTraveller-5.4.2-source\Tools\Meshy-Pipeline.py
if not "%~2"=="" set MESHY_MESH=%~2
if not "%~3"=="" set MESHY_NAME=%~3
if not "%~4"=="" set MESHY_FBX=%~4
if not "%~5"=="" set MESHY_ROUTES=%~5
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Personal Games\DuTraveller-5.4.2-source\DT.uproject" -run=pythonscript -script="%SCRIPT%" -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput
echo SCRIPT_EXIT=%ERRORLEVEL%
