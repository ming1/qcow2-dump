#!/bin/bash

IMG=$1

set -x
./qcow2.py $IMG dump-header
./qcow2.py $IMG dump-l1-table | grep ^# | wc
./qcow2.py $IMG dump-refcount-table
./qcow2.py $IMG dump-refcount-blk 0 | grep ^# | wc
ls -l $IMG
