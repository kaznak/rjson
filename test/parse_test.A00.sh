#!/bin/bash

pname=$(basename $0)
stime=$(date +%Y%m%d%H%M%S)
tmp=/tmp/$pname.$stime.$$

trap 'rm -f $tmp-*' EXIT

cat <<EOF	> $tmp-json
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

yes "$(tr -d ' \n' < $tmp-json)"	|
head -n $((100*10000))	> $tmp-input

/usr/bin/time	dd < $tmp-input	> /dev/null
# /usr/bin/time	cut -d: -f2 < $tmp-input > /dev/null
/usr/bin/time rjson -w4 < $tmp-input	> /dev/null
