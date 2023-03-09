#!/bin/bash
# #!/bin/sh

# pwd
# ls -la
# echo '\n'
# echo 'Hi! I am a shell script!\n'
# echo 'I am running in a container\n'

# # cd src/udp

# make clean-server
# make run-server


# Set the number of times to run the command
num_runs=5

# Set the starting port
start_port=8347

# Loop over the number of runs
for i in $(seq 1 $num_runs); do
  # Calculate the port to use for this run
  port=$((start_port + i - 1))

  # Run the command with the current port
  make clean-server && make run-server port3=$port &
done

# Wait for all background processes to finish
wait
