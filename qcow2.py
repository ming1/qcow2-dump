#!/usr/bin/env python3
#
# Manipulations with qcow2 image
#
# Copyright (C) 2012 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import sys

from qcow2_format import (
    QcowHeader,
    QcowHeaderExtension,
    Qcow2State
)


is_json = False

def cmd_dump_l1_table(fd):
    qs = Qcow2State(fd)
    for l1_entry in qs.L1_entries():
        print(l1_entry)
    print()

def cmd_dump_refcount_table(fd):
    qs = Qcow2State(fd)
    for entry in qs.refcount_table_entries():
        if entry.is_allocated():
            print(entry)
    print()

def dump_l2_table(qs, seq):
    if seq < qs.header.l1_size:
        print("L2 table in seq ", seq)
        for l2_entry in qs.L2_entries(seq):
            if l2_entry.is_allocated():
                vm_addr = (seq * qs.nr_l2_entry + l2_entry.seq) << qs.header.cluster_bits
                print("addr 0x{:x} -> {} ".format(vm_addr, l2_entry))

def cmd_dump_l2_table(fd, seq):
    qs = Qcow2State(fd)
    seq = int(seq)
    
    if seq >= 0:
        dump_l2_table(qs, seq)
    else:
        for i in range(qs.header.l1_size):
            dump_l2_table(qs, i)
    print()

def dump_refcount_blk(qs, seq):
    #qs.dump_refcount_blk(seq)
    for refblk_entry in qs.refcount_blk_entries(seq):
        if refblk_entry.is_allocated():
            vm_addr = (seq * qs.nr_refcount_blk_entry + refblk_entry.seq) << qs.header.cluster_bits
            print("addr 0x{:x} -> {} ".format(vm_addr, refblk_entry))

def cmd_dump_refcount_blk(fd, seq):
    qs = Qcow2State(fd)
    seq = int(seq)
    
    if seq >= 0:
        dump_refcount_blk(qs, seq)
    else:
        for i in range(qs.max_refcount_table_entries):
            dump_refcount_blk(qs, i)
    print()

def cmd_translate_guest_addr(fd, guest_addr):
    qs = Qcow2State(fd)
    guest_addr = int(guest_addr, 16)
    l2_entry, offset = qs.translate_guest_addr(guest_addr)
    if l2_entry is None:
        print("0x{:x}: unallocated address".format(guest_addr))
    else:
        print("0x{:x} -> 0x{:x}, l2_entry {}, offset_in_cluster 0x{:x}".format(
            guest_addr, l2_entry.cluster_offset + offset, l2_entry, offset))

def cmd_get_guest_addr_refcount(fd, guest_addr):
    qs = Qcow2State(fd)
    guest_addr = int(guest_addr, 16)
    refcnt = qs.get_guest_addr_refcount(guest_addr)
    print("0x{:x}: {}".format(guest_addr, refcnt))

def cmd_get_free_cluster(fd):
    qs = Qcow2State(fd)
    cluster = qs.get_free_clusters()
    print("1st free cluster 0x{:x}".format(cluster))

def cmd_dump_header(fd):
    h = QcowHeader(fd)
    h.dump(is_json)
    print()
    h.dump_extensions(is_json)


def cmd_dump_header_exts(fd):
    h = QcowHeader(fd)
    h.dump_extensions(is_json)


def cmd_set_header(fd, name, value):
    try:
        value = int(value, 0)
    except ValueError:
        print("'%s' is not a valid number" % value)
        sys.exit(1)

    fields = (field[2] for field in QcowHeader.fields)
    if name not in fields:
        print("'%s' is not a known header field" % name)
        sys.exit(1)

    h = QcowHeader(fd)
    h.__dict__[name] = value
    h.update(fd)


def cmd_add_header_ext(fd, magic, data):
    try:
        magic = int(magic, 0)
    except ValueError:
        print("'%s' is not a valid magic number" % magic)
        sys.exit(1)

    h = QcowHeader(fd)
    h.extensions.append(QcowHeaderExtension.create(magic,
                                                   data.encode('ascii')))
    h.update(fd)


def cmd_add_header_ext_stdio(fd, magic):
    data = sys.stdin.read()
    cmd_add_header_ext(fd, magic, data)


def cmd_del_header_ext(fd, magic):
    try:
        magic = int(magic, 0)
    except ValueError:
        print("'%s' is not a valid magic number" % magic)
        sys.exit(1)

    h = QcowHeader(fd)
    found = False

    for ex in h.extensions:
        if ex.magic == magic:
            found = True
            h.extensions.remove(ex)

    if not found:
        print("No such header extension")
        return

    h.update(fd)


def cmd_set_feature_bit(fd, group, bit):
    try:
        bit = int(bit, 0)
        if bit < 0 or bit >= 64:
            raise ValueError
    except ValueError:
        print("'%s' is not a valid bit number in range [0, 64)" % bit)
        sys.exit(1)

    h = QcowHeader(fd)
    if group == 'incompatible':
        h.incompatible_features |= 1 << bit
    elif group == 'compatible':
        h.compatible_features |= 1 << bit
    elif group == 'autoclear':
        h.autoclear_features |= 1 << bit
    else:
        print("'%s' is not a valid group, try "
              "'incompatible', 'compatible', or 'autoclear'" % group)
        sys.exit(1)

    h.update(fd)


cmds = [
    ['dump-header', cmd_dump_header, 0,
     'Dump image header and header extensions'],
    ['dump-header-exts', cmd_dump_header_exts, 0,
     'Dump image header extensions'],
    ['set-header', cmd_set_header, 2, 'Set a field in the header'],
    ['add-header-ext', cmd_add_header_ext, 2, 'Add a header extension'],
    ['add-header-ext-stdio', cmd_add_header_ext_stdio, 1,
     'Add a header extension, data from stdin'],
    ['del-header-ext', cmd_del_header_ext, 1, 'Delete a header extension'],
    ['set-feature-bit', cmd_set_feature_bit, 2, 'Set a feature bit'],
    ['dump-l1-table', cmd_dump_l1_table, 0, 'Dump L1 table'],
    ['dump-l2-table', cmd_dump_l2_table, 1, 'Dump L2 table'],
    ['dump-refcount-table', cmd_dump_refcount_table, 0, 'Dump refcount table'],
    ['dump-refcount-blk', cmd_dump_refcount_blk, 1, 'Dump refcount block'],
    ['translate-guest-addr', cmd_translate_guest_addr, 1, 'translate guest addr'],
    ['refcnt-guest-addr', cmd_get_guest_addr_refcount, 1, 'get refcnt of cluster for guest addr'],
    ['get-free-cluster', cmd_get_free_cluster, 0, 'get 1st free cluster'],
]

def main(filename, cmd, args):
    fd = open(filename, "r+b")
    try:
        for name, handler, num_args, desc in cmds:
            if name != cmd:
                continue
            elif len(args) != num_args:
                usage()
                return
            else:
                handler(fd, *args)
                return
        print("Unknown command '%s'" % cmd)
    finally:
        fd.close()


def usage():
    print("Usage: %s <file> <cmd> [<arg>, ...] [<key>, ...]" % sys.argv[0])
    print("")
    print("Supported commands:")
    for name, handler, num_args, desc in cmds:
        print("    %-20s - %s" % (name, desc))
    print("")
    print("Supported keys:")
    print("    %-20s - %s" % ('-j', 'Dump in JSON format'))


if __name__ == '__main__':
    if len(sys.argv) < 3:
        usage()
        sys.exit(1)

    is_json = '-j' in sys.argv
    if is_json:
        sys.argv.remove('-j')

    main(sys.argv[1], sys.argv[2], sys.argv[3:])
