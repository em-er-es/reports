#!/bin/bash
FILTERS=("INIT" "MEDI" "PUB1" "PUB2" "SUB1" "SUB2" "SUB3");
LOGEXT="log";
LOGNAME="as02b";
LOGFILE="$LOGNAME.$LOGEXT";
TESTEXEC="test.sh";

if [[ ! -f "$LOGFILE" ]]; then sh "$TESTEXEC" > "$LOGFILE"; fi
#for FILTER in $FILTERS; do
for FILTER in $(seq 0 6); do
	echo "${FILTERS[$FILTER]}";
echo	awk '"/'${FILTERS[$FILTER]}'/"' "$LOGFILE" RED "$LOGNAME-${FILTERS[$FILTER]}.$LOGEXT";
	awk '"/'${FILTERS[$FILTER]}'/"' "$LOGFILE" > "$LOGNAME-${FILTERS[$FILTER]}.$LOGEXT";
#	grep '\[.*'${FILTERS[$FILTER]}'.*\]' > "$LOGNAME-${FILTERS[$FILTER]}.$LOGEXT";
done
