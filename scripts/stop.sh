#!/bin/bash

# Stop service script for NovaIIM server (Linux)
# This script stops the NovaIIM server

echo "Stopping NovaIIM server..."

PID_FILE="server.pid"
if [ ! -f "$PID_FILE" ]; then
    echo "PID file not found. Server may not be running."
    # Try to kill any running server
    pkill -f "server"
    if [ $? -eq 0 ]; then
        echo "Server stopped."
    else
        echo "No running server found."
    fi
    exit 0
fi

PID=$(cat "$PID_FILE")
if [ -z "$PID" ]; then
    echo "Invalid PID in file."
    rm -f "$PID_FILE"
    exit 1
fi

if kill -TERM "$PID" 2>/dev/null; then
    echo "Server stopped (PID: $PID)"
    # Wait a bit for graceful shutdown
    sleep 2
    # Force kill if still running
    if kill -0 "$PID" 2>/dev/null; then
        kill -KILL "$PID" 2>/dev/null
        echo "Server force stopped."
    fi
else
    echo "Failed to stop server with PID $PID"
fi

# Clean up PID file
rm -f "$PID_FILE"