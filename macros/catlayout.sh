#!/bin/sh

dir=/groups/bock/bocklab/eric/forbill/cutout1

for i in $(seq 0 60)
do
	f=$(printf "%s/layout-%04d.txt" $dir $i)
	if [ -f "$f" ]
	then
		cat $f >> layout.txt
	fi
done


