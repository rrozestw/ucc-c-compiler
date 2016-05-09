#!/bin/sh

if test $# -ne 1
then
	echo >&2 "Usage: $0 git-rev-list"
	echo >&2 "  set \$CONFIGURE_ARGS and \$MAKE_ARGS to influence builds"
	exit 2
fi

cp /dev/null test-between.log

log(){
	echo "$1: $2" >> test-between.log
}

git rev-list "$1" | tac | while read hash
do
	if ! git checkout "$hash"
	then exit 1
	fi
	if ! ./configure $CONFIGURE_ARGS >/dev/null 2>&1
	then
		log "$hash" "configure failure"
		continue
	fi
	if ! make -Csrc $MAKE_ARGS >/dev/null 2>&1
	then
		log "$hash" "make failure"
		continue
	fi
	if ! (cd test2; ./run_tests -i ignores .) >/dev/null 2>&1
	then
		log "$hash" "test failure"
		continue
	fi
	log "$hash" "success"
done