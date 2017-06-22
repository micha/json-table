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

You can get an idea of what **jt** can do from this one-liner that produces
a table of ELB names to EC2 instance IDs from the complex JSON returned by the
`aws elb` tool:

```bash
aws elb describe-load-balancers \
  |jt LoadBalancerDescriptions [ LoadBalancerName % ] Instances InstanceId %
```
```
elb-1	i-94a6f73a
elb-2	i-b910a256
...
```

or filter JSON log files:

```bash
cat <<EOT | jt [ account % ] % | awk -F\\t '$1 == 123 {print $2}'
{"account":123,"balance":100}
{"account":789,"balance":200}
{"account":123,"balance":300}
{"account":456,"balance":400}
EOT
```
```
{"account":123,"balance":100}
{"account":123,"balance":300}
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

### Options

- **-h** &mdash; Print usage info and exit.
- **-V** &mdash; Print version and license info and exit.
- **-a** &mdash; Enable explicit iteration mode.
- **-j** &mdash; Enable inner join mode.
- **-s** &mdash; No-op (included for compatibility with earlier versions).
- **-u** *<string>* &mdash; Unescape JSON *<string>*, print it, and exit.

### Commands

Non-option arguments are commands in a limited stack-based programming language.
There are a number of stacks provided by the **jt** runtime:

- The **data stack** contains JSON objects from the input stream. Commands
  operate on the top of this stack to traverse the JSON tree and print values.

- The **gosub stack** contains pointers into the data stack. Commands push data
  stack pointers onto this stack to save the state of the data stack and pop
  them off to restore the data stack to a saved state.

- The **index stack** contains the indexes of nested arrays or properties of
  nested objects as **jt** iterates over them. Commands can print the value at
  the top of this stack.

**Jt** provides the following commands:

|:--:|----|
| **[** | Save the state of the data stack. The current data stack pointer is pushed onto the gosub stack. |
| **]** | Restore the data stack to a saved state: pop the gosub stack and restore the data stack pointer to that state. |
| **%** | Print the value at the top of the data stack. If the item at the top of the data stack is not a string or primitive type its JSON representation is printed. See the **Output Format** section below for details. |
| **^** | Print the array index or object key at the top of the index stack. Note that the index stack will always have at least one item &mdash; the index of the current JSON object read from stdin. |
| **@** | Print the keys of the object at the top of the data stack. If the top of the data stack is not an object then its type is printed in brackets, eg.  `[array]`, `[string]`, `[number]`, `[boolean]`, or `[null]`.  If there is no value then `[none]` is printed. |
| **+** | Parse nested JSON: if the item at the top of the data stack is a string, parse it and push the result onto the data stack.<br><br>If the item at the top of the data stack is not a string or if there is an error parsing the JSON then nothing is done (the operation is a no-op). |
| **.** | Iterate over the values of the object at the top of the data stack. The current value will be pushed onto the data stack and the current key will be pushed onto the index stack. |
| **[***KEY***]** | Drill down: get the value of the *KEY* property of the object at the top of the data stack and push that value onto the data stack.<br><br>If the item at the top of the data stack is not an object or if the object has no *KEY* property a blank field is printed, unless the `-j` option was specified in which case the entire row is removed from the output.<br><br>If the *KEY* property of the object is an array subsequent commands will operate on one of the items in the array, chosen automatically by **jt**.  The array index will be available to subsequent commands via the index stack. |
| *KEY* | See **[***KEY***]** above &mdash; the **[** and **]** may be omitted if the property name *KEY* does not conflict with any **jt** command. |

### Operation

When **jt** starts, the root JSON object is pushed onto the data stack. Then
commands are evaluated, from left to right. When a JSON array is pushed onto
the data stack a corresponding index (starting at zero) is pushed onto the
index stack, and the associated object in the array at that index is
automatically pushed onto the data stack.

When all commands have been evaluated, each item in the output stack is
printed, separated by tabs, the data stack is restored to its initial state
(containing only the root JSON object), and evaluation begins again, evaluating
the commands from left to right.

Each iteration will increment indices stored in the index stack such that all
iterations follow a different path through the nested JSON tree, each traversing
a different permuation of array indices. You can think of this process as an
implicit join, if that helps.

### Output Format

The format of printed output from **Jt** (see `%` in **Commands**, above)
depends on the type of thing being printed.

JSON primitives (i.e. `null`, `true`, and `false`) and numbers are printed
verbatim. **Jt** does not process them in any way. Numbers can be of arbitrary
precision, as long as they conform to the JSON parsing grammar.

If special formatting is required the `printf` program is your friend:

```bash
printf %.0f 2.99792458e9
```
```
2997924580
```

Strings are printed verbatim, minus the enclosing double quotes. No unescaping
is performed because tabs or newlines in JSON strings would break the tabular
output format.

If unescaped values are desired the `-u` option can be used:

```bash
jt -u 'i love music \u266A'
```
```
i love music ♪
```

Objects and arrays are printed as JSON, with insignificant whitespace omitted.

## EXAMPLES

Below are a number of examples demonstrating how to use **jt** commands to do
some simple exploration and extraction of data from JSON and JSON streams.

### Exploration

The `@` command prints information about the item at the top of the data stack.
When the item is an object `@` prints its keys:

```bash
cat <<EOT | jt @
{
  "foo": 100,
  "bar": 200,
  "baz": 300
}
EOT
```
```
foo
bar
baz
```

When the top item is an array `@` prints information about the first item in
the array:

```bash
cat <<EOT | jt @
[{"foo":100,"bar":200,"baz":300}]
EOT
```
```
foo
bar
baz
```

Otherwise, `@` prints the type of the item:

```bash
cat <<EOT | jt @
"hello world"
EOT
```
```
[string]
```

### Drill Down

Property names are also commands. Use `foo` here as a command to drill down
into the `foo` property and then use `@` to print its keys:

```bash
cat <<EOT | jt foo @
{
  "foo": {
    "bar": 100,
    "baz": 200
  }
}
EOT
```
```
bar
baz
```

When a property name conflicts with a **jt** command you must wrap the property
name with square brackets to drill down:

```bash
cat <<EOT | jt [@] @
{
  "@": {
    "bar": 100,
    "baz": 200
  }
}
EOT
```
```
bar
baz
```

It's okay if the property name itself has square brackets. Just wrap with more
brackets:

```bash
cat <<EOT | jt [[@]] @
{
  "[@]": {
    "bar": 100,
    "baz": 200
  }
}
EOT
```
```
bar
baz
```

### Extract

The `%` command prints the item at the top of the data stack. Note that when
the top item is a collection it is printed as JSON (insiginificant whitespace
removed):

```bash
cat <<EOT | jt %
{
  "foo": 100,
  "bar": 200
}
EOT
```
```
{"foo":100,"bar":200}
```

Drill down and print:

```bash
cat <<EOT | jt foo bar %
{
  "foo": {
    "bar": 100
  }
}
EOT
```
```
100
```

The `%` command can be used multiple times. The printed values will be delimited
by tabs:

```bash
cat <<EOT | jt % foo % bar %
{
  "foo": {
    "bar": 100
  }
}
EOT
```
```
{"foo":{"bar":100}}     {"bar":100}     100
```

### Save / Restore

The `[` and `]` commands provide a sort of `GOSUB` facility &mdash; the data
stack is saved by `[` and restored by `]`. This can be used to extract values
from different paths in the JSON as a single record:

```bash
cat <<EOT | jt [ foo % ] [ bar % ]
{
  "foo": 100,
  "bar": 200
}
EOT
```
```
100     200
```

### Iteration (Arrays)

**Jt** automatically iterates over arrays, running the program once for each
item in the array. This produces one tab-delimited record for each iteration,
separated by newlines:

```bash
cat <<EOT | jt [ foo % ] [ bar baz % ]
{
  "foo": 100,
  "bar": [
    {"baz": 200},
    {"baz": 300},
    {"baz": 400}
  ]
}
EOT
```
```
100     200
100     300
100     400
```

The `^` command includes the array index as a column in the result:

```bash
cat <<EOT | jt [ foo % ] [ bar ^ baz % ]
{
  "foo": 100,
  "bar": [
    {"baz": 200},
    {"baz": 300},
    {"baz": 400}
  ]
}
EOT
```
```
100     0       200
100     1       300
100     2       400
```

Note that `^` is scoped &mdash; it prints the index of the innermost enclosing
loop:

```bash
cat <<EOT | jt foo ^ bar ^ %
{
  "foo": [
    {"bar": [100, 200]},
    {"bar": [300, 400]}
  ]
}
EOT
```
```
0       0       100
0       1       200
1       0       300
1       1       400
```

### Iteration (Objects)

The `.` command iterates over the values of an object:

```bash
cat <<EOT | jt . %
{
  "foo": 100,
  "bar": 200,
  "baz": 300
}
EOT
```
```
100
200
300
```

When iterating over an object the `^` command prints the name of the current
property:

```bash
cat <<EOT | jt [ foo % ] [ bar . ^ % ]
{
  "foo": 100,
  "bar": {
    "baz": 200,
    "baf": 300,
    "qux": 400
  }
}
EOT
```
```
100     baz     200
100     baf     300
100     qux     400
```

The scope of `^` is similar when iterating over objects:

```bash
cat <<EOT | jt . ^ . ^ %
{
  "foo": {
    "bar": 100,
    "baz": 200
  }
}
EOT
```
```
foo     bar     100
foo     baz     200
```

### JSON Streams

**Jt** automatically iterates over all entities in a [JSON stream][json-stream]
(a stream of JSON entities delimited by optional insignificant whitespace):

```bash
cat <<EOT | jt [ foo % ] [ bar % ]
{"foo": 100, "bar": 200}
{"foo": 200, "bar": 300}
{"foo": 300, "bar": 400}
EOT
```
```
100     200
200     300
300     400
```

Insignificant whitespace is ignored:

```bash
cat <<EOT | jt [ foo % ] [ bar % ]
{
  "foo": 100,
  "bar": 200
}{"foo":200,"bar":300}
    {
      "foo": 300,
      "bar": 400
    }
EOT
```
```
100     200
200     300
300     400
```

Within a JSON stream the `^` command prints the current stream index:

```bash
cat <<EOT | jt ^ [ foo % ] [ bar % ]
{"foo": 100, "bar": 200}
{"foo": 200, "bar": 300}
{"foo": 300, "bar": 400}
EOT
```
```
0       100     200
1       200     300
2       300     400
```

Note that one entity in the stream may result in more than one output record
when iteration is involved:

```bash
cat <<EOT | jt [ foo % ] [ bar % ]
{"foo":10,"bar":[100,200]}
{"foo":20,"bar":[300,400]}
EOT
```
```
10      100
10      200
20      300
20      400
```

### Nested JSON

The `+` command to parses JSON embedded in strings:

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

Note that `+` pushes the resulting JSON entity onto the data stack &mdash; it
does not mutate the original JSON:

```bash
cat <<EOT | jt [ foo + bar % ] %
{"foo":"{\"bar\":100}","baz":200}
{"foo":"{\"bar\":200}","baz":300}
{"foo":"{\"bar\":300}","baz":400}
EOT
```
```
100     {"foo":"{\"bar\":100}","baz":200}
200     {"foo":"{\"bar\":200}","baz":300}
300     {"foo":"{\"bar\":300}","baz":400}
```

### Joins

Notice the empty column &mdash; some objects don't have the `bar` key:

```bash
cat <<EOT | jt [ foo % ] [ bar % ]
{"foo":100,"bar":1000}
{"foo":200}
{"foo":300,"bar":3000}
EOT
```
```
100     1000
200
300     3000
```

Enable inner join mode with the `-j` flag. This removes output rows when a key
in the traversal path doesn't exist:

```bash
cat <<EOT | jt -j [ foo % ] [ bar % ]
{"foo":100,"bar":1000}
{"foo":200}
{"foo":300,"bar":3000}
EOT
```
```
100     1000
300     3000
```

Note that this does not remove rows when the key exists and the value is empty:

```bash
cat <<EOT | jt -j [ foo % ] [ bar % ]
{"foo":100,"bar":1000}
{"foo":200,"bar":""}
{"foo":300,"bar":3000}
EOT
```
```
100     1000
200
300     3000
```

### Explicit Iteration

Sometimes the implicit iteration over arrays is awkward:

```bash
cat <<EOT | jt . ^ . ^ %
{
  "foo": [
    {"bar":100},
    {"bar":200}
  ]
}
EOT
```
```
0       bar     100
1       bar     200
```

Should the first `^` be printing the array index (which it does, in this case)
or the object key (i.e. `foo`)? Explicit iteration with the `-a` flag eliminates
the ambiguity:

```bash
cat <<EOT | jt -a . . ^ . ^ %
{
  "foo": [
    {"bar":100},
    {"bar":200}
  ]
}
EOT
```
```
0       bar     100
1       bar     200
```

prints the array index and:

```bash
cat <<EOT | jt -a . ^ . . ^ %
{
  "foo": [
    {"bar":100},
    {"bar":200}
  ]
}
EOT
```
```
foo     bar     100
foo     bar     200
```

prints the object key, and

```bash
cat <<EOT | jt -a . ^ . ^ . ^ %
{
  "foo": [
    {"bar":100},
    {"bar":200}
  ]
}
EOT
```
```
foo     0       bar     100
foo     1       bar     200
```

prints both.

## COPYRIGHT

Copyright © 2017 Micha Niskin. Distributed under the Eclipse Public License.

[man]: http://htmlpreview.github.io/?https://raw.githubusercontent.com/micha/json-table/master/jt.1.html
[json-stream]: https://en.wikipedia.org/wiki/JSON_Streaming#Concatenated_JSON
