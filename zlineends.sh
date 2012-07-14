#!/bin/sh


# > ./zlineends -> List all files with <cr><lf> ends
# > ./zlineends -f  dir -> Make Unix ends in dir


if [ "$1" = "-f" ]
then
	for ifile in `grep -rl $'\r' $2*`
	do
		sed -i 's|\r||g' $ifile
	done
else
	grep -rl $'\r' *
fi

