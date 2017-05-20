#!/bin/sh

verbose=
error=0
prefix=
for arg in "$@"
do
	if [ "$arg" = -v ]
	then
		shift
		verbose=-v
	elif [ "$1" = -e ]
	then
		shift
		error=1
	elif echo "$arg" | grep '^--prefix=' >/dev/null
	then
		shift
		prefix="$arg"
	else
		break
	fi
done

usage(){
	echo "Usage: $0 [-e] cc-params..." >&2
	echo "-e: expect ucc to error" >&2
	exit 1
}

ucc=../ucc
e=/tmp/check.$$

trap "rm -f $e" EXIT

# ensure "$f" comes after other args, to allow for things like -x ...
f="$1"
shift

$ucc -o/dev/null -c "$@" "$f" 2>$e
r=$?

# check for abort
if [ $r -eq 134 ]
then
	echo >&2 "abort: ucc on $@"
	exit 1
fi

if [ $r -ne 0 ]
then r=1
fi

if [ $r -ne $error ]
then
	s=
	if [ $error -eq 0 ]
	then s="no "
	fi
	echo "${s}error expected"
	cat $e
	exit 1
fi >&2
./check.pl $prefix $verbose < $e "$f"
exit $?
