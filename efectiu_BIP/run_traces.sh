#!/bin/bash
FILE="benchmarks.txt"
rm results.txt
let c=1
while IFS= read line
do
	echo "################## BENCHMARK NUMBER $c ##############################"
	# Running LRU Policy
	export DAN_POLICY=0; ./efectiu ~/tracesWorking/"$line.trace.gz" > output.txt
        # Now extract IPC from output.txt
	last_line=$(awk '/./{line=$0} END{print line}' output.txt)
	arr=($last_line)
	lru_ipc=${arr[2]}
	echo "LRU IPC for $line = $lru_ipc"

	# Running CONTESTANT Policy
	export DAN_POLICY=2; ./efectiu ~/tracesWorking/"$line.trace.gz" > output.txt
        # Now extract IPC from output.txt
	last_line=$(awk '/./{line=$0} END{print line}' output.txt)
	arr=($last_line)
	my_ipc=${arr[2]}
	echo "CONTESTANT IPC for $line = $my_ipc"
	echo "$lru_ipc $my_ipc" >> results.txt
	
	c=$((c + 1));
done < "$FILE"
