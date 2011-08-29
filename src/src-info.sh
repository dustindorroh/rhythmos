#!/bin/bash
echo "Syscall kernel macros"
grep -n ' SYSCALL_' *.c

SRC=$(dirname $0)
pushd $SRC > /dev/null

echo "------------------------------------------------------------"
echo "Syscall names"
grep SYSCALL_ *.c | awk '{print $3}' | tr -d ':' \
| tr '_' '\n' | grep -v SYSCALL | sort | uniq | tr [A-Z] [a-z]

echo "------------------------------------------------------------"
echo "Location of system call declarations:"
for i in $(grep SYSCALL_ *.c | awk '{print $3}'\
 | tr -d ':' | tr '_' '\n' | grep -v SYSCALL\
 | sort | uniq | tr [A-Z] [a-z] | tr '\n' ' '); 
 do  grep -n syscall_$i\( *.c| grep -v 'res =' | grep -v \; 
done

echo "------------------------------------------------------------"
echo "List of system headers used"
grep -n '#include <' *.c | awk '{ print $2 }' | sort | uniq | tr -d '<->'

echo "------------------------------------------------------------"
echo "System call uses"
for i in $(grep SYSCALL_ *.c | awk '{print $3}' \
 | tr -d ':' | tr '_' '\n' | grep -v SYSCALL | sort \
 | uniq | tr [A-Z] [a-z] | tr '\n' ' '); 
do grep -n ' '$i\( *.c| grep -v 'res =' | sort \
 | grep $i ; done
popd > /dev/null
