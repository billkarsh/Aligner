#!/bin/sh


if (($# == 1))
then
    last=$1
else
    last=$2
fi

for i in $(seq $1 $last)
do
	echo $i
	cd $i
	qsub -N lou-d-$i -cwd -V -b y -pe batch 8 make -f thumbs.down -j 8 EXTRA=''
	cd ..
done


