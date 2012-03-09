#!/bin/bash
#
#       program-tool.sh
#       
#       Copyright 2012 Dustin Dorroh <dustindorroh@gmail.com>
#

usage() {	
printf "\
Usage:	%s [OPTIONS]... [PROGRAM NAME]
       
Description:
	Adds/removes all the necessary configuration code to
	install/uninstall a program into the kernel. This is done
	by specifically adding/removing the program name to the
	correct make target.

Options:
	-i,	install program. (cannot be used with -u)
	-u,	uninstall program. (cannot be used with -i)
	-l,	list installed programs. (can be used by with -i or -u)
	-h,	print help (i.e. this menu).
	
Examples:
	%s -l           # list installed programs.
	%s -i foo       # install foo.
	%s -u foo       # uninstall foo.
	%s -l -i foo    # install foo then list the installed programs.
	%s -l -u foo    # uninstall foo then list the installed programs.
" $(basename $0) $(basename $0) $(basename $0) $(basename $0) $(basename $0) $(basename $0)
}

help () {
	printf "Try \`%s -h' for more information.\n" $(basename $0)
}

# Initialize flags.
UNINSTALL=0
INSTALL=0
LIST=0

# Process command line options.
while getopts ":iulh" flag
do
	case $flag in
		i)	# Install the program.
			INSTALL=1
			;;
		u)	# Uninstall the program
			UNINSTALL=1
			;;
		l)	# List installed programs
			LIST=1
			;;
		h)	# Print usage and exit successfully.
			usage
			exit 0
			;;
		:)	# Handle missing option argument. Failure condition.
			printf "%s: option '$*' requires an argument\n" $(basename $0)
			help
			exit 1
			;;
		*)	# Handle illegal option. Failure condition.
			printf "%s: illegal option '$*'\n" $(basename $0)
			help
			exit 1
		;;
	esac
done

shift $((OPTIND-1))

# Check that the minimum number of arguments were used.
# Handle missing file operands. Failure condition.
if [[ $LIST -eq 0 && $UNINSTALL -eq 0 && $INSTALL -eq 0 ]]; then
	# No option selected
	printf "%s: option '$*' requires an argument\n" $(basename $0)
	help
	exit 1
elif [[ $UNINSTALL -eq 1 && $INSTALL -eq 1 ]]; then
	# Conflicting options selected
	printf "%s: Argument list too long.\n" $(basename $0)
	help
	exit 1
elif [[ $LIST -eq 0 && $# -lt 1 ]]; then
	# No name given
	printf "%s: missing name operand\n" $(basename $0)
	help
	exit 1
else
	PROGRAM_NAME=$@
fi

DIRNAME=$(dirname $0)
MAKEFILE_AM="$DIRNAME/Makefile.am"
GITIGNORE="$DIRNAME/.gitignore"
COREUTILS_LINE_NUM=$(grep -nr "COREUTILS =" $MAKEFILE_AM | awk -F ':' '{print $1}')

# Install of Uninstall program
# 
if [[ $INSTALL -eq 1  ]]; then
	# Handle program install
	sed -i '/^COREUTILS /s|$| '$PROGRAM_NAME'|' $MAKEFILE_AM
	echo $PROGRAM_NAME >> $GITIGNORE
	echo "$PROGRAM_NAME installed."
elif  [[ $UNINSTALL -eq 1  ]]; then
	# Handle program uninstall
	sed -i ''$COREUTILS_LINE_NUM' s/'$PROGRAM_NAME'//g' $MAKEFILE_AM
	sed -i '/'$PROGRAM_NAME'/d' $GITIGNORE
	echo "$PROGRAM_NAME uninstalled."
fi

# List installed programs
# 
if [[ $LIST -eq 1 ]]; then
	# Handle listing programs
	echo "Installed Programs:"
	grep "COREUTILS =" $MAKEFILE_AM | sed 's/COREUTILS = //g' |tr ' ' '\n' | tr -s '\n'
fi

