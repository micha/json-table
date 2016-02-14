# json-table

**Jt** transforms JSON data structures into tables of columns and rows for
processing in the shell. Extracting information from deeply nested JSON data
is difficult and unreliable with tools like **sed** and **awk**, and tools
that are specially designed for manipulating JSON are cumbersome to use in
the shell because they either return their results as JSON or introduce a
new turing complete scripting language that needs to be quoted and constructed
via string interpolation.

**Jt** provides only what is needed to extract data from nested JSON data
structures and organize the data into a table. Tools like **cut**, **paste**,
**join**, **sort**, **uniq**, etc. can be used to efficiently reduce the
tabular data to produce the final result.


You can get an idea of what **jt** can do from this one-liner that produces
a table of ELB names to EC2 instance IDs:

```
$ aws elb describe-load-balancers \
-   | jt LoadBalancerDescriptions [ LoadBalancerName % ] Instances InstanceId %
elb-1	i-94a6f73a
elb-2	i-b910a256
```

## Installation

`Jt` doesn't have any dependencies.

```
make &&  sudo make install
```

## Documentation

See the [man page][man] or `man jt` in your terminal.

## Examples

We'll use the following JSON data for the examples:

    $ JSON='{"foo":"a","bar":{"x":"b"},"baz":[{"y":"c"},{"y":"d","z":"e"}]}'

Explore JSON data, print an object's keys:

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

Extract values from JSON data:

    $ echo "$JSON" | jt foo %
    a

Extract nested JSON data:

    $ echo "$JSON" | jt bar x %
    b

Extract multiple values by saving and restoring the data stack:

    $ echo "$JSON" | jt [ foo % ] bar x %
    a       b

Iterate over nested arrays, producing one row per iteration:

    $ echo "$JSON" | jt [ foo % ] [ bar x % ] baz y %
    a       b       c
    a       b       d

Include the array index as a column in the result:

    $ echo "$JSON" | jt [ foo % ] [ bar x % ] baz y ^ %
    a       b       0       c
    a       b       1       d

Notice the empty column &mdash; some objects don't have the `z` key:

    $ echo "$JSON" | jt [ foo % ] baz [ y % ] z %
    a       c
    a       d       e

Inner join mode will remove rows from the output when any key in the traversal
path doesn't exist:

    $ echo "$JSON" | jt -j [ foo % ] baz [ y % ] z %
    a       d       e

## Copyright

Copyright © 2016 Micha Niskin. Distributed under the Eclipse Public License.

[man]: http://htmlpreview.github.io/?https://raw.githubusercontent.com/micha/json-table/master/jt.1.html
