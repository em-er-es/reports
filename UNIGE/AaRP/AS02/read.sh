#!/bin/bash
LOGEXT="log";
LOGNAME="as02a";

for LOG in as02*.log; do
	less -R "$LOG"
done
