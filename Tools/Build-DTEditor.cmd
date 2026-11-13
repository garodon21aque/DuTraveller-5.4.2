@echo off
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" DTEditor Win64 Development "C:\Personal Games\DuTraveller-5.4.2-source\DT.uproject" -waitmutex
echo BUILD_EXIT=%ERRORLEVEL%
