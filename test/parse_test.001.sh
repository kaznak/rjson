#!/bin/bash

pname=$(basename $0)
stime=$(date +%Y%m%d%H%M%S)
tmp=/tmp/$pname.$stime.$$

trap 'rm -f $tmp-*' EXIT


cat	<<EOF	> $tmp-expected
$.firstName "John"
$.lastName "Smith"
$.isAlive true
$.age 25
$.address.streetAddress "21 2nd Street"
$.address.city "New York"
$.address.state "NY"
$.address.postalCode "10021-3100"
$.phoneNumbers[0000].type "home"
$.phoneNumbers[0000].number "212 555-1234"
$.phoneNumbers[0001].type "office"
$.phoneNumbers[0001].number "646 555-4567"
$.phoneNumbers[0002].type "mobile"
$.phoneNumbers[0002].number "123 456-7890"
$.children[0000] []
$.spouse null
EOF

rjson -w 4 <<EOF	> $tmp-output
{
  "firstName": "John",
  "lastName": "Smith",
  "isAlive": true,
  "age": 25,
  "address": {
    "streetAddress": "21 2nd Street",
    "city": "New York",
    "state": "NY",
    "postalCode": "10021-3100"
  },
  "phoneNumbers": [
    {
      "type": "home",
      "number": "212 555-1234"
    },
    {
      "type": "office",
      "number": "646 555-4567"
    },
    {
      "type": "mobile",
      "number": "123 456-7890"
    }
  ],
  "children": [],
  "spouse": null
}
EOF

if diff $tmp-expected $tmp-output 2> /dev/null ; then
    echo "$pname $stime OK"
    exit 0
else
    echo "$pname $stime NG"
    exit 1
fi
