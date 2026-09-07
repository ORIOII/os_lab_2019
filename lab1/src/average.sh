#!/bin/bash

COUNT=$#
SUM=0

if [ $COUNT == 0 ]; then
echo "No arguments"
exit 1
fi

for arg in "$@"
do
SUM=$((SUM + arg))
done

AVERAGE=$((SUM / COUNT)

echo "Arguments count: $COUNT"
echo "Average: $AVERAGE"
