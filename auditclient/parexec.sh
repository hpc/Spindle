#!/bin/bash


for command in $*
do
    echo $command
    $command &
done
wait