#!/bin/bash
EXECPATH="."
EXECFILE="AS02B";
echo "[SHELL] Start";
if [[ -z $1 ]]; then TIMED=4; else TIMED=$1; fi
if [[ -z $2 ]]; then LOG=""; else LOG=$2; echo "[SHELL] Log file: $LOG"; fi
if [[ -z $3 ]]; then COLOUR=1; else COLOUR=$3; fi
if [[ -z $4 ]]; then DELAY1=1; else DELAY1=$4; fi

#Subshell
(sleep $TIMED; 
echo "[SHELL] Sending signal"; 
sleep $DELAY1;
pkill -SIGUSR1 "${EXECFILE}";
echo "[SHELL]" Signal send;
ps aux | grep "${EXECFILE}") &

if [[ -n "$LOG" ]]; then "$EXECPATH/$EXECFILE" | tee "$LOG"; else "$EXECPATH/$EXECFILE"; fi

#pkill -9 "${EXECFILE:0:-1}"; # Just to make sure it's dead
#if [[ -n "$LOG" ]] && [[ $COLOUR == 1 ]]; then sed -i 's/^\[PUB/'$PUB'\[PUB/g' "$LOG"; fi
echo "[SHELL] Exit";
