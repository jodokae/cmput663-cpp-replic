#!/bin/sed -f
# delete whitespaces between macro start "#" and macro-name (e.g. ifdef)
s:^\#[ \t]\+:\#:g
