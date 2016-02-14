# json-table
Transform nested JSON data into tabular data in the shell.

## Overview

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
-   | jt LoadBalancerDescriptions [ LoadBalancerName % ] Instances [ InstanceId % ]
```
```
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

## Copyright

Copyright © 2016 Micha Niskin. Distributed under the Eclipse Public License.

[man]: http://htmlpreview.github.io/?https://raw.githubusercontent.com/micha/json-table/master/jt.1.html
