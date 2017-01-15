#!/bin/bash
EXECPATH="."
EXECFILE="AS02A-I";
echo "[SHELL] Start";
if [[ -z $1 ]]; then TIMED=4; else TIMED=$1; fi
if [[ -z $2 ]]; then LOG=""; else LOG=$2; echo "[SHELL] Log file: $LOG"; fi
if [[ -z $3 ]]; then COLOUR=1; else COLOUR=$3; fi
if [[ -z $4 ]]; then DELAY1=1; else DELAY1=$4; fi
if [[ -z $5 ]]; then DELAY2=2; else DELAY2=$5; fi
if [[ -z $6 ]]; then DELAY3=3; else DELAY3=$6; fi
if [[ -z $7 ]]; then DELAY4=0; else DELAY4=$7; fi
if [[ -z $8 ]]; then DELAY5=0; else DELAY5=$8; fi
if [[ -z $9 ]]; then DELAY6=0; else DELAY6=$9; fi
#./AS02A-I & (sleep 2; pkill -SIGUSR1 AS02A-P)
#./AS02A-I & (sleep 120; pkill -SIGUSR1 AS02A)
#PUB=\[\e\[38\;5\;87m\];
PUB=\[\\e\[38\;5\;87m\];
SUB='\[\e\[38;5;160m\]';
INIT='\[\e\[38;5;154m\]';
MEDI='\[\e\[38;5;220m\]';

#Subshell
(sleep $TIMED; 
echo "[SHELL] Sending signal"; 
sleep $DELAY1;
pkill -SIGUSR1 "${EXECFILE:0:-1}P";
sleep $DELAY2;
pkill -SIGUSR1 "${EXECFILE:0:-1}S"; 
sleep $DELAY3;
pkill -SIGUSR1 "${EXECFILE:0:-1}M"; 
echo "[SHELL]" Signal send;
ps aux | grep "${EXECFILE:0:-1}") &

if [[ -n "$LOG" ]]; then "$EXECPATH/$EXECFILE" | tee "$LOG"; else "$EXECPATH/$EXECFILE"; fi

#pkill -9 "${EXECFILE:0:-1}"; # Just to make sure it's dead
#if [[ -n "$LOG" ]] && [[ $COLOUR == 1 ]]; then sed -i 's/^\[PUB/'$PUB'\[PUB/g' "$LOG"; fi
echo "[SHELL] Exit";
