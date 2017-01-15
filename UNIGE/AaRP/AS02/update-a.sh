#!/bin/bash
for SOURCEFILE in *.cpp; do
	echo "### $SOURCEFILE ###";
	echo "g++ -o $(echo \"$SOURCEFILE\" | sed -e 's/\.cpp$//' -e 's/ES4268738-AaRP-//g') -lpthread \"$SOURCEFILE\";"
#	g++ -o $(echo "$SOURCEFILE" | sed -e 's/\.cpp$//' -e 's/ES4268738-AaRP-//g') -lpthread "$SOURCEFILE";
#	if [[ $? != 0 ]]; then exit 1; fi
	g++ -o $(echo "$SOURCEFILE" | sed -e 's/\.cpp$//' -e 's/ES4268738-AaRP-//g') -lpthread "$SOURCEFILE" || exit 1;
done
