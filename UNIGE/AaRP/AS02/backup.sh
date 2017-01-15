#!/bin/bash
CDATE="$(date +%d%m%y)";
CTIME="$(date +%H%M%S)";
echo "### $CDATE : $CTIME ###"
tar rvf "AS02.snapshots.tar" *.tar.gz && rm *.tar.gz
#tar cvf "AS02.$CDATE-$CTIME.tar.gz" *.cpp* *.sh *.txt *.log *.tar AS02[AB]-*
tar cvf "AS02.$CDATE-$CTIME.tar.gz" *.cpp* *.sh *.txt *.log AS02[AB]-*
ln -sfv "AS02.$CDATE-$CTIME.tar.gz" ES4268738-AaRP-AS02.tar.gz

cat extract.txt > extract.sh;
echo "ARCHIVE=AS02.$CDATE-$CTIME.tar.gz;" >> extract.sh
echo 'tar xvf "$ARCHIVE";' >> extract.sh
