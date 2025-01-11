#!/bin/bash

COMMAND="./kvs jobs/ 4 4"

while true; do
    # Run the command and capture stdout and stderr
    OUTPUT=$( $COMMAND 2>&1 )
    EXIT_CODE=$?

    # Print stdout and stderr
    echo "--- OUTPUT ---"
    echo "$OUTPUT"

    # Check exit code
    if [ $EXIT_CODE -ne 0 ]; then
        echo "Program exited with non-zero code: $EXIT_CODE"
        break
    fi
done

