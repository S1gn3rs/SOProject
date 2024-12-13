#!/bin/bash

# Number of random tests to generate
NUM_TESTS=5

# Command to run
COMMAND="./kvs temp/ 3 3"

# Function to generate random .job files
generate_random_jobs() {
    mkdir -p temp # Ensure the jobs directory exists
    for i in $(seq 1 $NUM_TESTS); do
        JOB_FILE="temp/test_$i.job"
        
        # Create a list of possible commands
        COMMANDS=(
            "WRITE [$(for j in $(seq $((RANDOM % 5 + 1))); do echo -n "(Key$j,Value$j)"; done)]"
            "READ [$(for j in $(seq $((RANDOM % 3 + 1))); do echo -n "Key$j,"; done | sed 's/,$//')]"
            "DELETE [$(for j in $(seq $((RANDOM % 3 + 1))); do echo -n "Key$j,"; done | sed 's/,$//')]"
            "WAIT $((RANDOM % 5000 + 500))"
            "SHOW"
            "BACKUP"
        )

        # Shuffle the commands
        SHUFFLED_COMMANDS=($(shuf -e "${COMMANDS[@]}"))

        # Write the shuffled commands to the .job file
        {
            for cmd in "${SHUFFLED_COMMANDS[@]}"; do
                echo "$cmd"
            done
        } > "$JOB_FILE"
    done
}

# Generate random job files
generate_random_jobs

# Execute each generated .job file
for i in $(seq 1 $NUM_TESTS); do
    JOB_FILE="temp/test_$i.job"
    echo "Running test: $JOB_FILE"
    
    # Run the main command
    OUTPUT=$($COMMAND 2>&1)
    EXIT_CODE=$?

    # Print stdout and stderr
    echo "--- OUTPUT for $JOB_FILE ---"
    echo "$OUTPUT"

    # Check for "ThreadSanitizer" in the output
    if echo "$OUTPUT" | grep -q "ThreadSanitizer"; then
        echo "WARNING: ThreadSanitizer detected in the program output for $JOB_FILE!"
    fi

    # Check exit code
    if [ $EXIT_CODE -ne 0 ]; then
        echo "Program exited with non-zero code: $EXIT_CODE for $JOB_FILE"
    fi
done
