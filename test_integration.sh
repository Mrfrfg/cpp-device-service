#!/usr/bin/env bash

set -euo pipefail

server=$1
client=$2
port=$3
server_log=$(mktemp)
server_pid=""

cleanup()
{
    if [[ -n "$server_pid" ]] &&
       kill -0 "$server_pid" 2>/dev/null; then
        kill -INT "$server_pid"
        wait "$server_pid" || true
    fi

    rm -f "$server_log"
}

trap cleanup EXIT

"$server" "$port" >"$server_log" 2>&1 &
server_pid=$!

ready=false

for attempt in {1..50}
do
    if "$client" --port "$port" "GET readiness" >/dev/null 2>&1; then
        ready=true
        break
    fi

    if ! kill -0 "$server_pid" 2>/dev/null; then
        echo "Server exited during startup"
        cat "$server_log"
        exit 1
    fi

    sleep 0.1
done

if [[ "$ready" != true ]]; then
    echo "Server did not become ready"
    cat "$server_log"
    exit 1
fi

actual=$(
    "$client" \
    --port "$port" \
        "SET integration-01 42.5" \
        "GET integration-01"
)

expected=$'OK\nVALUE integration-01 42.5\nBYE'

if [[ "$actual" != "$expected" ]]; then
    echo "Unexpected success-path response"
    echo "Expected:"
    printf '%s\n' "$expected"
    echo "Actual:"
    printf '%s\n' "$actual"
    exit 1
fi

actual=$(
    "$client" \
    --port "$port" \
        "GET missing" \
        "SET"
)

expected=$'NOT_FOUND\nERROR usage: SET <device-id> <temperature>\nBYE'

if [[ "$actual" != "$expected" ]]; then
    echo "Unexpected error-path response"
    echo "Expected:"
    printf '%s\n' "$expected"
    echo "Actual:"
    printf '%s\n' "$actual"
    exit 1
fi

echo "Network integration test passed"
