#! /bin/sh
#
# compile.sh
# Copyright (C) 2017 ybs <ybs@ybs-CW35S>
#
# Distributed under terms of the MIT license.
#


gcc main.c -g -o main -L../src -I../src -lini
