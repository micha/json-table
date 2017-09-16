#!/usr/bin/env bash

jt=$1

FAIL="$(echo -e "\033[1;31mFAIL\033[0m")"
PASS="$(echo -e "\033[1;32mPASS\033[0m")"

fails=0

assert() {
  local x=$(diff -u <(echo "$2") <(echo "$3"))
  local z=$(printf "%-40s  " $(printf 'L%-4d' $1))
  echo -en "\033[1m${z}\033[0m"
  if [[ -z "$x" ]]; then
    echo "$PASS"
  else
    fails=$((fails + 1))
    echo "$FAIL"
    echo "$x"
  fi
}

echo -e "\033[1;30mThe following should\033[0m ${PASS}:"
echo -e "\033[1;30m----------------------------------------------\033[0m"

JSON='{"foo":"a","bar":{"x":"b"},"baz":[{"y":"c"},{"y":"d","z":"e"}]}'

assert $LINENO \
  "$(echo "$JSON" | $jt @)" \
  "$(cat <<'EOT'
foo
bar
baz
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | $jt bar @)" \
  "$(cat <<'EOT'
x
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | $jt baz @)" \
  "$(cat <<'EOT'
y
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | $jt baz ^)" \
  "$(cat <<'EOT'
0
1
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | $jt foo %)" \
  "$(cat <<'EOT'
a
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | $jt bar x %)" \
  "$(cat <<'EOT'
b
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | $jt [ foo % ] bar x %)" \
  "$(cat <<'EOT'
a	b
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | $jt [ foo % ] [ bar x % ] baz y %)" \
  "$(cat <<'EOT'
a	b	c
a	b	d
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | $jt [ foo % ] [ bar x % ] baz y % ^)" \
  "$(cat <<'EOT'
a	b	c	0
a	b	d	1
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | $jt [ foo % ] [ bar x % ] baz y ^ %)" \
  "$(cat <<'EOT'
a	b	0	c
a	b	1	d
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | $jt [ foo % ] baz [ y % ] z %)" \
  "$(cat <<'EOT'
a	c	
a	d	e
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | $jt -j [ foo % ] baz [ y % ] z %)" \
  "$(cat <<'EOT'
a	d	e
EOT
)"

JSON='{"foo":"a","bar":{"x":"b"},"baz":[{"y":{"b":"c"}},{"y":"d","z":"e"}]}'

assert $LINENO \
  "$(echo "$JSON" | $jt [ foo % ] baz [ y % ] z %)" \
  "$(cat <<'EOT'
a	{"b":"c"}	
a	d	e
EOT
)"

JSON=$(cat <<'EOT'
{"a":100, "b":[{"foo":1},{"foo":10}],"c":[1,2,3]}
{"a":101, "b":[{"foo":2},{"foo":20}],"c":[4,5,6]}
{"a":102, "b":[{"foo":3},{"foo":30}],"c":[7,8,9]}
{"a":103, "b":[{"foo":4},{"foo":40}],"c":[6,6,6]}
EOT
)

assert $LINENO \
  "$(echo "$JSON" | $jt ^ [ a % ] [ b foo % ] c %)" \
  "$(cat <<'EOT'
0	100	1	1
0	100	1	2
0	100	1	3
0	100	10	1
0	100	10	2
0	100	10	3
1	101	2	4
1	101	2	5
1	101	2	6
1	101	20	4
1	101	20	5
1	101	20	6
2	102	3	7
2	102	3	8
2	102	3	9
2	102	30	7
2	102	30	8
2	102	30	9
3	103	4	6
3	103	4	6
3	103	4	6
3	103	40	6
3	103	40	6
3	103	40	6
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | $jt ^=i [ a %=a ] [ b foo %=foo ] c %=c)" \
  "$(cat <<'EOT'
i	a	foo	c
0	100	1	1
0	100	1	2
0	100	1	3
0	100	10	1
0	100	10	2
0	100	10	3
1	101	2	4
1	101	2	5
1	101	2	6
1	101	20	4
1	101	20	5
1	101	20	6
2	102	3	7
2	102	3	8
2	102	3	9
2	102	30	7
2	102	30	8
2	102	30	9
3	103	4	6
3	103	4	6
3	103	4	6
3	103	40	6
3	103	40	6
3	103	40	6
EOT
)"

JSON=$(cat <<'EOT'
{"foo":"{\"bar\":100}","baz":200}
{"foo":"{\"bar\":200}","baz":300}
{"foo":"{\"bar\":300}","baz":400}
EOT
)

assert $LINENO \
  "$(echo "$JSON" | $jt [ foo + bar % ] [ baz % ])" \
  "$(cat <<'EOT'
100	200
200	300
300	400
EOT
)"

assert $LINENO \
  "$(echo '{}' |$jt . % && echo OK)" \
  OK

[[ $fails == 0 ]] || exit 1
