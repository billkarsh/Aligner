#!/bin/sh

for i in $(seq $1 $2)
do
	echo $i
	cd $i
	if (($i == $1))
	then
		make -f make.pts pts.same
		make -f make.pts pts.up
	elif (($i == $2))
	then
		make -f make.pts pts.same
		make -f make.pts pts.down
	else
		make -f make.pts
	fi
	cd ..
done
