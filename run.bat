@echo off
setlocal
cd /d "%~dp0"

powershell.exe -NoProfile -Command "try { if ((Invoke-WebRequest -UseBasicParsing 'http://localhost:8080/api/trains' -TimeoutSec 2).StatusCode -eq 200) { exit 0 } } catch {}; exit 1" >nul 2>nul
if not errorlevel 1 (
  start "" "http://localhost:8080"
  exit /b 0
)

where.exe g++ >nul 2>nul
if errorlevel 1 (
  set "CXX=C:\msys64\ucrt64\bin\g++.exe"
) else (
  set "CXX=g++"
)

echo Building Railway Reservation System...
"%CXX%" -std=c++17 -Wall -Wextra -pedantic main.cpp -lws2_32 -lshell32 -o railway_server.exe
if errorlevel 1 (
  echo Build failed. Install MSYS2 UCRT64 g++ and add its bin folder to PATH.
  pause
  exit /b 1
)

railway_server.exe --open-browser
