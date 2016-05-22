#!/usr/bin/env bash

FAIL="$(echo -e "\033[1;31mFAIL\033[0m")"
PASS="$(echo -e "\033[1;32mPASS\033[0m")"

fails=0

assert() {
  local x=$(diff -u <(echo "$2") <(echo "$3"))
  local z=$(printf 'L%-4d\t' $1)
  echo -en "\033[1m${z}\033[0m"
  if [[ -z "$x" ]]; then
    echo "$PASS"
  else
    echo "$FAIL"
    echo "$x"
  fi
}

JSON='{"foo":"a","bar":{"x":"b"},"baz":[{"y":"c"},{"y":"d","z":"e"}]}'

assert $LINENO \
  "$(echo "$JSON" | ./jt @)" \
  "$(cat <<'EOT'
foo
bar
baz
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | ./jt bar @)" \
  "$(cat <<'EOT'
x
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | ./jt baz @)" \
  "$(cat <<'EOT'
y
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | ./jt baz ^)" \
  "$(cat <<'EOT'
0
1
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | ./jt foo)" \
  "$(cat <<'EOT'
a
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | ./jt bar x)" \
  "$(cat <<'EOT'
b
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | ./jt [ foo ] bar x)" \
  "$(cat <<'EOT'
a	b
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | ./jt [ foo ] [ bar x ] baz y)" \
  "$(cat <<'EOT'
a	b	c
a	b	d
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | ./jt [ foo ] [ bar x ] baz y ^)" \
  "$(cat <<'EOT'
a	b	c	0
a	b	d	1
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | ./jt [ foo ] baz [ y ] z)" \
  "$(cat <<'EOT'
a	c	
a	d	e
EOT
)"

assert $LINENO \
  "$(echo "$JSON" | ./jt -j [ foo ] baz [ y ] z)" \
  "$(cat <<'EOT'
a	d	e
EOT
)"

[[ $fails -gt 0 ]] && exit 1
