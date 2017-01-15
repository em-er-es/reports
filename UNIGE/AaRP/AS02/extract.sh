#!/bin/bash
CDATE="$(date +%d%m%y)";
CTIME="$(date +%H%M%S)";
echo "### $CDATE : $CTIME ###"
ARCHIVE=AS02.100216-174143.tar.gz;
tar xvf "$ARCHIVE";
