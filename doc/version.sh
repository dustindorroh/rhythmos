#!/bin/bash
cp version.texi version.texi.bak
VERSION="$(cat version.texi | grep VERSION|awk '{print $3}' | tr '\n' ' ';)"
EDITION="$(cat version.texi | grep EDITION|awk '{print $3}' | tr '\n' ' ';)"
UPDATEDMONTH="$(cat version.texi | grep UPDATED-MONTH|awk '{print $3" "$4}' | tr '\n' ' ';)" # actual variable name is UPDATED-MONTH
UPDATED="$(cat version.texi | grep UPDATED| grep -v MONTH | awk '{print $3" "$4" "$5}' | tr '\n' ' ';)"

#echo VERSION=$VERSION
#echo EDITION=$EDITION
#echo UPDATED-MONTH=$UPDATEDMONTH
#echo UPDATED=$UPDATED

updated=$(date +"%d %B %Y")
updatedmonth=$(date +"%B %Y")

echo '@set UPDATED '$updated > version.texi
echo '@set UPDATED-MONTH '$updatedmonth >> version.texi
echo '@set EDITION '$EDITION >> version.texi
echo '@set VERSION '$VERSION >> version.texi
diff version.texi.bak version.texi > /dev/null
diff_return=$?
[[ 1 -eq $diff_return ]] && echo "version.texi updated"
[[ 0 -eq $diff_return ]] && echo "version.texi is already up to date"
exit 0
