#!/usr/bin/env bash

jt=$1

PASSFAIL="$(echo -e "\033[1;31mFAIL\033[0m")"
PASSPASS="$(echo -e "\033[1;32mPASS\033[0m")"
FAILFAIL="$(echo -e "\033[1;32mFAIL\033[0m")"
FAILPASS="$(echo -e "\033[1;31mPASS\033[0m")"
PASSFAILIGNORE="$(echo -e "\033[1;30mFAIL\033[0m")"
FAILPASSIGNORE="$(echo -e "\033[1;30mPASS\033[0m")"

fails=0

ignore=$(cat <<EOT
test/fail10.json
test/fail1.json
EOT
)

contains() {
  ( echo "$1" ; echo "$2" ) |sort |uniq -d |grep -q .
}

assert() {
  local linenumber=$(printf 'L%-4d\t' $1)
  local inputfile=$2
  local expected=$3
  local result
  local ign

  if contains "$ignore" "$inputfile"; then
    ign=IGNORE
    echo -en "\033[1;30m$(printf "%-40s  " "$inputfile -- ignored")\033[0m"
  else
    echo -n "$(printf "%-40s  " $inputfile)"
  fi

  if cat "$inputfile" |$jt % > /dev/null 2>&1; then
    result=PASS
  else
    result=FAIL
  fi

  echo "$(eval "echo \$${expected}${result}${ign}")"

  if [[ $result != $expected ]] && [[ $ign != IGNORE ]]; then
    fails=$((fails + 1))
    echo -e "\033[31m$(cat "$inputfile")\033[0m"
  fi
}

echo -e "\033[1;30mThe following should\033[0m ${PASSPASS}:"
echo -e "\033[1;30m----------------------------------------------\033[0m"

for i in test/pass*.json; do
  assert $LINENO "$i" PASS
done

echo
echo -e "\033[1;30mThe following should\033[0m ${FAILFAIL}:"
echo -e "\033[1;30m----------------------------------------------\033[0m"

for i in test/fail*.json; do
  assert $LINENO "$i" FAIL
done

[[ $fails == 0 ]] || exit 1
