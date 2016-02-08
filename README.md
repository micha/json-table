# json-table
Shell tool to transform nested JSON structures into regular tabular data.

## Overview

**Jt** transforms JSON data structures into tables of columns and rows for
processing in the shell. Extracting information from deeply nested JSON data
is difficult and unreliable with tools like **sed** and **awk**, and tools
that are specially designed for manipulating JSON are cumbersome to use in
the shell because they either return their results as JSON or introduce a
new turing complete scripting language that needs to be quoted and constructed
via string interpolation.

**Jt** provides only what is needed to extract data from nested JSON data
structures and organize the data into a table. Once the data is in tabular
format the standard shell tools like **cut**, **paste**, **join**, **sort**,
and **uniq** can be used to further manipulate the data to produce whatever
final format is required.

You can get an idea of what **jt** can do from this one-liner that produces
a table of Elastic Load Balancer names to EC2 instance IDs for all of your
ELBs:

```
$ aws elb describe-load-balancers \
-   | jt LoadBalancerDescriptions [ LoadBalancerName ] Instances [ InstanceId ]
```

## Installation

```
make &&  sudo make install
```

See the [man page](http://htmlpreview.github.io/?https://raw.githubusercontent.com/micha/json-table/master/jt.1.html).

## Getting Started

Consider the following JSON data:

```json
{
  "Region": "US",
  "Units": [
    {
      "Type": 1,
      "Parts": [
        {
          "Id": 100,
          "Price": 200.00
        },
        {
          "Id": 101,
          "Price": 400.00
        }
      ]
    },
    {
      "Type": 2,
      "Parts": [
        {
          "Id": 200,
          "Price": 17.95
        },
        {
          "Id": 201,
          "Price": 92.50
        },
        {
          "Id": 202,
          "Price": 760.49
        }
      ]
    }
  ]
}
```

One thing we might want to do is get a list of all the part IDs for a type 1
unit. If this were in tabular format it would be trivial:

```
Type Id
1    100
1    101
2    200
2    201
2    202
```

With the data in this pleasant format we could simply do

```
join -j 1 <(sort -k 1 units.txt) <(echo 1) | cut -f2
```
```
100
101
```

and the work is done. **Jt** is the tool that converts the JSON tree into the
table of tab-delimited rows that tools like **join** and **cut** are designed
to work with.

## Traveral

The first step to extract data from a large JSON blob is to drill down
incrementally, exploring as you go. **Jt** supports this with the `-k` or
`--keys` command. Using the JSON above as an example:

```
$ cat units.json | jt -k
```
```
Region
Units
```

The top-level object was inspected, and its keys were printed, one per line.
We can explore deeper using the `-d` or `--down` command to traverse the
`Units` property of the JSON object and get access to the objects nested more
deeply:

```
$ cat units.json | jt -d Units -k
```
```
Type
Parts
```

and so on:

```
$ cat units.json | jt -d Units -d Parts -k
```
```
Id
Price
```

Now that we know where the information we are interested in is located and we
know how to move around we can direct **jt** to extract the parts we want into
a table for further processing with the familiar shell tools.

## Stacks

The traversal operations make use of a stack to store the piece of the JSON
blob that is currently being worked on: **the JSON stack**. Commands operate
on the object at the top of this stack. The `-d` or `--down` command, for
example, extracts the value associated with some key from the object at the
top of the stack and pushes that value onto the stack, making it the new top.
The `-u` or `--up` command pops the JSON stack, "rewinding" the current focus
to the previous location in the JSON data.

There is also another stack where data is stored for extraction: **the OUTPUT
stack**. This stack holds the data for the row of the table that is currently
being processed. The `-p` or `--print` command, for example, copies the object
at the top of the JSON stack and pushes it onto the output stack, adding it to
the current row.

Finally, there is a third stack where "subroutines" may store a pointer into
the JSON stack: **the GOSUB stack**. This makes it possible to drill down into
one part of the JSON data, extract a value to the output stack, and then rewind
back to some earlier location so that another branch may be followed to find
the next value. The `-g` or `--gosub` command pushes the current JSON stack
pointer onto the gosub stack. The `-r` or `--return` command expands into a
`--print` command and a number of `--up` commands (enough to restore the JSON
stack to where the gosub pointer indicates), and it pops the gosub stack.

## Extraction

The `-p` or `--print` command pushes whatever is at the top of the JSON stack
onto the output stack, without modifying the JSON stack. So we can extract the
`Region` value from the example JSON like this:

```
$ cat units.json | jt -d Region -p
```
```
US
```

**Jt** iterates over arrays automatically, so to extract the `Region` ID plus
all of the unit `Type` IDs for that region we can use a combination of drilling
down and rewinding, and **jt** will perform an implicit join:

```
$ cat units.json | jt -d Region -p -u -d Units -d Type -p
```
```
US	1
US	2
```

And finally, suppose we want to extract the `Region`, `Type`, `Id`, and `Price`
for all of the `Parts` in the JSON data:

```
$ cat units.json | jt \
-   -d Region -p -u \
-   -d Units -d Type -p -u \
-   -d Parts -d Id -p -u -d Price -p
```
```
US	1	 100	00.0
US	1	 101	00.0
US	2	 200	7.95
US	2	 201	2.5
US	2	 202	60.49
```

Notice that each time we use the `-p` or `--print` command we create a new
column in the resulting table.

## Shortcuts

It can get tedious typing all those `-d`, `-u`, and `-p` commands, and it can
get confusing to see where you're at in the traversal sometimes. Since most
JSON APIs in the wild do not choose weird names for keys **jt** can internally
add those commands when necessary. The previous example can be simplified by
leaving out the `-d` commands:

```
$ cat test.json | jt Region -p -u Units Type -p -u Parts Id -p -u Price -p
```
```
US	1	 100	00.0
US	1	 101	00.0
US	2	 200	7.95
US	2	 201	2.5
US	2	 202	60.49
```

The `-g` or `--gosub` and `-r` or `--return` commands can shave some more
keystrokes sometimes:

```
$ cat test.json | jt -g Region -r Units -g Type -r Parts -g Id -r Price -p
```
```
US	1	 100	00.0
US	1	 101	00.0
US	2	 200	7.95
US	2	 201	2.5
US	2	 202	60.49
```

And finally, there are the `[` and `]` aliases for the `--gosub` and `--return`
commands. Note that there must be space between the `[` or `]` and the next
command:

```
$ cat test.json | jt [ Region ] Units [ Type ] Parts [ Id ] [ Price ]
```
```
US	1	 100	00.0
US	1	 101	00.0
US	2	 200	7.95
US	2	 201	2.5
US	2	 202	60.49
```

This notation nicely highlights the columns that will be included in the table
of results, in this case `Region`, `Type`, `Id`, and `Price`.

## Output Options

These options regulate how the results table is presented:

* `-F <CHAR>`, `--col-delim=<CHAR>`
  Sets the field delimiter character to `CHAR`. Each row in the results table
  consists of a number of fields separated by this character. The default is
  `"\t"`.

* `-R <CHAR>`, `--record-delim=<CHAR>`
  Sets the record delimiter character to `CHAR`. The results table consists
  of a number of records separated by this character. The default is `"\n"`.

For example:

```
$ cat test.json | jt -F \| [ Region ] Units [ Type ] Parts [ Id ] [ Price ]
```
```
US|1|100|00.0
US|1|101|00.0
US|2|200|7.95
US|2|201|2.5
US|2|202|60.49
```

## Commands

* `-k`, `--keys`:
  Prints the names of any keys the object at the top of the JSON stack may
  contain, one per line, and then exits.

* `-d <PROP>`, `--down=<PROP>`:
  Pushes the value of `x.PROP` onto the JSON stack, where `x` is the object
  at the top of the JSON stack.

* `-u`, `--up`:
  Pops the top value off of the JSON stack.

* `-p`, `--print`:
  Pushes the object at the top of the JSON stack onto the output stack. (No
  effect on the JSON stack.)

* `-q`, `--quote`:
  Pushes a JSON encoded string representation of the object at the top of the
  JSON stack onto the JSON stack.

* `-g`, `--gosub`, `[`:
  Pushes the current JSON stack pointer onto the gosub stack. Note that when
  using the `[` alias there must be at least one space before and after the
  `[` token. Otherwise it will be interpreted as being part of a JSON property
  name.

* `-r`, `--return`, `]`:
  Expands to a `--print` command and a number of `--up` commands, enough to
  restore the JSON stack to the place where the corresponding `--gosub` was
  called. Also pops the gosub stack, removing the corresponding `--gosub`
  from it.

## Copyright

Copyright © 2016 Micha Niskin. Distributed under the Eclipse Public License.
