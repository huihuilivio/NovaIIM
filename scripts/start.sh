#!/bin/bash

# Start service script for NovaIIM server (Linux)
# This script starts the NovaIIM server

CONFIG_FILE=${1:-"./configs/server.yaml"}
LOG_FILE=${2:-"./logs/server.log"}

echo "Starting NovaIIM server..."

# Check if server is already running
if pgrep -f "server" > /dev/null; then
    echo "Server is already running."
    exit 0
fi

# Create logs directory if it doesn't exist
mkdir -p "$(dirname "$LOG_FILE")"

# Start the server
SERVER_PATH="./output/bin/server"
if [ ! -f "$SERVER_PATH" ]; then
    echo "Server executable not found at $SERVER_PATH"
    echo "Please build the project first."
    exit 1
fi

# Start in background
"$SERVER_PATH" --config "$CONFIG_FILE" >> "$LOG_FILE" 2>&1 &
SERVER_PID=$!

# Save PID for stop script
echo $SERVER_PID > server.pid

echo "Server started successfully (PID: $SERVER_PID)"
echo "Logs: $LOG_FILE"