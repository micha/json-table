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
* **Streaming mode** &mdash; reads JSON objects one-per-line e.g., from log files.

#### Example

You can get an idea of what **jt** can do from this one-liner that produces
a table of ELB names to EC2 instance IDs from the complex JSON returned by the
`aws elb` tool:

```
$ aws elb describe-load-balancers \
  |jt LoadBalancerDescriptions [ LoadBalancerName % ] Instances InstanceId %
elb-1	i-94a6f73a
elb-2	i-b910a256
...
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

We will use the following JSON input for the examples:

```bash
JSON='{"foo":"a","bar":{"x":"b"},"baz":[{"y":"c"},{"y":"d","z":"e"}]}'
```

We pretty-print it here for reference:

```json
{
    "foo": "a",
    "bar": {
        "x": "b"
    },
    "baz": [
        {
            "y": "c"
        },
        {
            "y": "d",
            "z": "e"
        }
    ]
}
```

### Explore

Explore JSON data, print an object's keys:

```bash
echo "$JSON" | jt @
```
```
foo
bar
baz
```

Print a nested object's keys:

```bash
echo "$JSON" | jt bar @
```
```
x
```

Same as above, with fuzzy property name matching:

```bash
echo "$JSON" | jt ^b @
```
```
x
```

Print the keys of the first object in a nested array:

```bash
echo "$JSON" | jt baz @
```
```
y
```

Print the indexes in a nested array:

```bash
echo "$JSON" | jt baz ^
```
```
0
1
```

### Extract

Extract values from JSON data:

```bash
echo "$JSON" | jt foo %
```
```
a
```

Extract nested JSON data:

```bash
echo "$JSON" | jt bar x %
```
```
b
```

### Save / Restore

Extract multiple values by saving and restoring the data stack:

```bash
echo "$JSON" | jt [ foo % ] bar x %
```
```
a       b
```

### Arrays

Iterate over nested arrays, producing one row per iteration:

```bash
echo "$JSON" | jt [ foo % ] [ bar x % ] baz y %
```
```
a       b       c
a       b       d
```

Include the array index as a column in the result:

```bash
echo "$JSON" | jt [ foo % ] [ bar x % ] baz y % ^
```
```
a       b       c       0
a       b       d       1
```

### Objects

Iterate over the values of an object without specifying intermediate keys:

```bash
echo $JSON | jt baz . %
```
```
c
d
e
```

Iterate over the keys and values of an object without specifying intermediate keys:

```bash
echo $JSON | jt baz . ^ %
```
```
y       c
y       d
z       e
```

### Joins

Notice the empty column &mdash; some objects don't have the <z> key:

```bash
echo "$JSON" | jt [ foo % ] baz [ y % ] z %
```
```
a       c
a       d       e
```

Inner join mode will remove rows from the output when any key in the traversal
path doesn't exist:

```bash
echo "$JSON" | jt -j [ foo % ] baz [ y % ] z %
```
```
a       d       e
```

Multiple JSON objects in the input stream, separated by whitespace:

```bash
cat <<EOT | jt [ foo % ] [ bar % ]
{"foo":100,"bar":200}
{"foo":200,"bar":300}
{"foo":300,"bar":400}
EOT
```
```
100     200
200     300
300     400
```

### Nested JSON

Use the `+` command to parse nested JSON:

```bash
cat <<EOT | jt [ foo + bar % ] [ baz % ]
{"foo":"{\"bar\":100}","baz":200}
{"foo":"{\"bar\":200}","baz":300}
{"foo":"{\"bar\":300}","baz":400}
EOT
```
```
100     200
200     300
300     400
```


## COPYRIGHT

Copyright © 2016 Micha Niskin. Distributed under the Eclipse Public License.

[man]: http://htmlpreview.github.io/?https://raw.githubusercontent.com/micha/json-table/master/jt.1.html
