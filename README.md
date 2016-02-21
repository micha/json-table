# json-table &mdash; (╯°□°)╯︵ ┻━┻

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
* **CSV output mode** &mdash; transform JSON input to CSV for spreadsheet analysis.

#### Example

You can get an idea of what **jt** can do from this one-liner that produces
a table of ELB names to EC2 instance IDs from the complex JSON returned by the
`aws elb` tool:

```
$ aws elb describe-load-balancers \
-   | jt LoadBalancerDescriptions [ LoadBalancerName % ] Instances InstanceId %
elb-1	i-94a6f73a
elb-2	i-b910a256
...
```

There are more examples in the [EXAMPLES](#examples) section below.

## INSTALL

Linux users can install prebuilt binaries from the [release tarball][tgz]:

```
sudo bash -c "cd /usr/local && wget -O - https://github.com/micha/json-table/releases/download/1.1.0/jt-1.1.0.tar.gz | tar xzvf -"
```

Otherwise, to build from source:

```
git checkout 1.1.0 && make && sudo make install
```

## DOCUMENTATION

See the [man page][man] or `man jt` in your terminal.

## EXAMPLES

We'll use the following JSON data for the examples:

    $ JSON='{"foo":"a","bar":{"x":"b"},"baz":[{"y":"c"},{"y":"d","z":"e"}]}'

#### Explore JSON Structure

Print an object's keys:

    $ echo "$JSON" | jt ?
    foo
    bar
    baz

Print a nested object's keys:

    $ echo "$JSON" | jt bar ?
    x

Print the keys of the first object in a nested array:

    $ echo "$JSON" | jt baz ?
    y

Print the indexes in a nested array:

    $ echo "$JSON" | jt baz ^
    0
    1

#### Extract Values From JSON Data

Get the value associated with the `foo` property of the JSON object:

    $ echo "$JSON" | jt foo %
    a

Get the value from a nested JSON object:

    $ echo "$JSON" | jt bar x %
    b

#### Save / Restore Stack To Backtrack

Drill down to get the `foo` value, then backtrack to get the `bar` value of the
same JSON object:

    $ echo "$JSON" | jt [ foo % ] bar x %
    a       b

#### Iterate Over Arrays

**Jt** will automatically iterate over nested arrays. It will run commands from
left to right, once for each nested object in the array. Stacks are reset
between runs, and each run produces one row of output:

    $ echo "$JSON" | jt [ foo % ] [ bar x % ] baz y %
    a       b       c
    a       b       d

Use the `^` command to include the array index as a column in the result:

    $ echo "$JSON" | jt [ foo % ] [ bar x % ] baz y ^ %
    a       b       0       c
    a       b       1       d

#### Left Join Vs. Inner Join

Notice the empty column &mdash; some objects don't have a `z` property:

    $ echo "$JSON" | jt [ foo % ] baz [ y % ] z %
    a       c
    a       d       e

Inner join mode (the `-j` option) will remove rows from the output when any
key in the traversal path doesn't exist:

    $ echo "$JSON" | jt -j [ foo % ] baz [ y % ] z %
    a       d       e

## COPYRIGHT

Copyright © 2016 Micha Niskin. Distributed under the Eclipse Public License.

[man]: http://htmlpreview.github.io/?https://raw.githubusercontent.com/micha/json-table/master/jt.1.html
[tgz]: https://github.com/micha/json-table/releases/download/1.1.0/jt-1.1.0.tar.gz
