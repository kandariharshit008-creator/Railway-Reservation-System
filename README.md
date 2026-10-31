# Railway Reservation System

A small browser-based railway reservation project with a C++17 backend. The backend serves the UI and provides the reservation API using Windows Winsock. Booking data is stored in `reservations.txt` beside the executable.

## Requirements

- Windows
- `g++` available on `PATH` (the current environment already has MSYS2 UCRT64 `g++`)
- No third-party libraries or package downloads

## Run

Double-click `run.bat`, or build and start it from this folder:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -lws2_32 -lshell32 -o railway_server.exe
.\railway_server.exe --open-browser
```

Double-click `run.bat` to build the backend, start it, and open the browser. Keep the server window open while using the app; press Ctrl+C in that window to stop it. If port 8080 is already in use, stop the other service before starting this project.

## Features

- Eight sample trains with live remaining-seat counts
- Passenger bookings for one or more seats
- PNR confirmation and reservation list
- Cancellation by PNR
- Local persistence across server restarts
