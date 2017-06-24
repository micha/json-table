# json-table [![Build Status](https://travis-ci.org/micha/json-table.svg?branch=master)](https://travis-ci.org/micha/json-table)

**Jt** transforms JSON data structures into tables of columns and rows for
processing in the shell.

Extracting information from deeply nested JSON data is difficult and unreliable
with tools like **sed** and **awk**, and tools that are specially designed for
manipulating JSON are cumbersome to use in the shell because they either return
their results as JSON or introduce a new turing complete scripting language
that needs to be quoted and constructed via string interpolation.

**Jt** provides only what is needed to extract data from nested JSON data
structures and organize the data into a table. Tools like **cut**, **paste**,
**join**, **sort**, **uniq**, etc. can be used to efficiently reduce the
tabular data to produce the final result.

#### Features

* **Self contained** &mdash; statically linked, has no build or runtime dependencies.
* **Fast, small memory footprint** &mdash; efficiently process **large** JSON input.
* **Correct** &mdash; parser does not accept invalid JSON (see tests for details).

#### Example

Sum the amounts for account 123:

```bash
cat <<+++ |
{"account":123,"amount":1.00}
{"account":789,"amount":2.00}
{"account":123,"amount":3.00}
{"account":123,"amount":4.00}
{"account":456,"amount":5.00}
+++
jt [ account % ] amount % | awk -F\\t '$1 == 123 {print $2}' | paste -sd+ |bc
```
```
8.00
```

## INSTALL

Linux users can install prebuilt binaries from the release tarball:

```
sudo bash -c "cd /usr/local && wget -O - https://github.com/micha/json-table/raw/master/jt.tar.gz | tar xzvf -"
```

Otherwise, build from source:

```
make && make test && sudo make install
```

## DOCUMENTATION

See the [man page][man] or `man jt` in your terminal.

## EXAMPLES

The [man page][man] has many examples.

## COPYRIGHT

Copyright © 2017 Micha Niskin. Distributed under the Eclipse Public License.

[man]: http://htmlpreview.github.io/?https://raw.githubusercontent.com/micha/json-table/master/jt.1.html
