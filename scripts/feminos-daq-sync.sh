#!/bin/bash

# input directory
input_dir=$1 # e.g. /home/daq/data
# remote host e.g. lobis@sultan.unizar.es
remote_host=$2
# output directory (in the remote host)
output_dir=$3 # e.g. /storage/iaxo/iaxo-lab/iaxo-d1/rawData

# show usage if bad arguments
if [ $# -ne 3 ]; then
    echo "Usage: $0 <input_dir> <remote_host> <output_dir_in_remote_host>"
    exit 1
fi

echo "Input directory: $input_dir"
echo "Remote host: $remote_host"
echo "Output directory: $output_dir"

# check if input directory exists
if [ ! -d $input_dir ]; then
    echo "Input directory does not exist"
    exit 1
fi

# check remote host is accessible
ssh -q $remote_host exit
if [ $? -ne 0 ]; then
    echo "Remote host is not accessible"
    exit 1
fi

# create remote directory if it does not exist
ssh -q $remote_host "[ -d $output_dir ] || mkdir -p $output_dir"

# check if output directory exists
ssh -q $remote_host "[ -d $output_dir ]"
if [ $? -ne 0 ]; then
    echo "Output directory does not exist"
    exit 1
fi

# loop forever
while true; do
    # count files in local directory, output directory. Size of files in both and print
    local_files=$(ls -1 $input_dir | wc -l)
    remote_files=$(ssh -q $remote_host "ls -1 $output_dir | wc -l")
    local_size=$(du -sh $input_dir | awk '{print $1}')
    remote_size=$(ssh -q $remote_host "du -sh $output_dir" | awk '{print $1}')

    echo "Number of files in $input_dir: $local_files, size on disk: $local_size"
    echo "Number of remote files in $output_dir: $remote_files, size on disk: $remote_size"

    rsync -av --human-readable --partial --progress --stats $input_dir/* $remote_host:$output_dir

    time_sleep=60
    echo "Sleeping for $time_sleep seconds"
    sleep $time_sleep
done
