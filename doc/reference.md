# STG

STG stands for Symbol-Type Graph.

# Overview

STG models Application Binary Interfaces. It supports extraction of ABIs from
DWARF and ingestion of BTF and libabigail XML into its model. Its primary
purpose is monitoring an ABI for changes over time and reporting such changes in
a comprehensible fashion.

STG captures symbol information, the size and layout of structs, function
argument and return types and much more, in a graph representation. Difference
reporting happens via a graph comparison.

Currently, STG functionality is exposed as two command-line tools, `stg` (for
ABI extraction) and `stgdiff` (for ABI comparison), and a native file format.

## Model

STG's model is an *abstraction* which does not and cannot capture every possible
interface property, invariant or behaviour. Conversely, the model includes
distinctions which are API significant but not ABI significant.

Concretely, STG's model is a rooted, connected, directed graph where each kind
of node corresponds to a meaningful ABI entity such as a symbol, function type
or struct member.

Nodes have specific attributes, such as name or size. Outgoing edges specify
things like return type. STG's model does not impose any constraints on which
nodes may be joined by edges.

Each node has an identity. However, for the purpose of comparison, nodes are
considered equal if they are of the same kind, have the same attributes and
matching outgoing edges and all nodes reachable via a pair of matching edges are
(recursively) equal. Renumbering nodes, (de)duplicating nodes and
adding/removing unreachable nodes do not affect this relationship.

### Symbols

As modelled by STG, symbols correspond closely to ELF symbols as seen in
`.dynsym` for shared object files or in `.symtab` for object files. In the case
of the Linux kernel, the `.symtab` is enriched with metadata and the effective
"ksymtab" is actually a subset of the ELF symbols together with CRC and
namespace information.

STG links symbols to their source-level types where these are known. Symbols
defined purely in assembly language will not have type information.

The symbol table is contained in the root node of the graph, which is an
*Interface* node.

### Types

STG models the C, C++ and (to a limited extent) Rust type systems.

For example, C++ template value parameters are poorly modelled for the simple
reason that this would require modelling C++ *values* as well as types,
something that DWARF itself doesn't do to the full extent permitted by C++20.

As type definitions are in general mutually recursive, an STG ABI is in general
a cyclic graph.

The root node of the graph can also contain a list of interface types, which may
not necessarily be reachable from the interface symbols.

## Supported Input Formats, Parsers and Limitations

STG can read its own native format for processing or comparison. It can also
process libabigail XML and BTF (`.BTF` ELF sections), with some limitations due
to model, design and implementation differences including missing features.

### Kinds of Node

STG has the following kinds of node.

*   **Special** - used for `void` and `...`
*   **Pointer / Reference** - `*`, `&` and `&&`
*   **Pointer to Member** - `foo::*`
*   **Typedef** - `typedef` and `using ... = ...`
*   **Qualified** - `const` and friends
*   **Primitive** - concrete types such as `int` and friends
*   **Array** - `foo[N]` - there is no distinction between zero and
    indeterminate length in the model
*   **Base Class** - inheritance metadata
*   **Method** - (only) virtual function
*   **Member** - data member
*   **Variant Member** - discriminated member
*   **Struct / Union** - `struct foo` etc., Rust tuples too
*   **Enumeration** - including the underlying value type - only values that are
    within the range of signed 64-bit integer are correctly modelled
*   **Variant** - for Rust enums holding data
*   **Function** - multiple argument, single return type
*   **ELF Symbol** - name, version, ELF metadata, Linux kernel metadata
*   **Interface** - top-level collection of symbols and types

An STG ABI consists of a rooted, connected graph of such nodes, and *nothing
else*. STG is blind to anything that cannot be represented by its model.

### Native Format

STG's native file format is a protocol buffer text format. It is suitable for
revision control, rather than human consumption. It is effectively described by
[`stg.proto`](../stg.proto).

In this textual serialisation of ABI graphs, external node identifiers and node
order are chosen to minimise file changes when a small subset of the graph
changes.

As an example, this is the definition of the **Typedef** node kind:

```proto
message Typedef {
  fixed32 id = 1;
  string name = 2;
  fixed32 referred_type_id = 3;
}
```

### Abigail (a.k.a. libabigail XML)

[libabigail](https://sourceware.org/libabigail/) is another project for ABI
monitoring. It uses a format that can be parsed as XML.

This command will transform Abigail into STG:

```shell
stg --abi library.xml --output library.stg
```

The main features modelled in Abigail but not STG are:

*   source file, line and column information
*   C++ access specifiers (public, protected, private)

The Abigail reader has these distinct phases of operation:

1.  text parsed into an XML tree
2.  XML cleaning - whitespace and unused attributes are stripped
3.  XML tidying - issues like duplicate nodes are resolved, if possible
4.  XML parsed into a graph with symbol information held separately
5.  symbols and root node added to the graph
6.  useless type qualifiers are stripped in post-processing

### BTF

[BTF](https://docs.kernel.org/bpf/btf.html) is typically used for the Linux
kernel where it is generated by `pahole -J` from ELF and DWARF information. It
can also be generated natively instead of DWARF using `gcc -gbtf` and by Clang,
but only for eBPF targets.

This command will transform BTF into STG:

```shell
stg --btf vmlinux --output vmlinux.stg
```

STG has primarily been tested against the `pahole` (libbtf) dialect of BTF and
support is not complete.

*   split BTF is not supported at all
*   any `.BTF.ext` section is just ignored
*   some kinds of BTF node are not handled:
    *   `BTF_KIND_DATASEC` - skip
    *   `BTF_KIND_DECL_TAG` - abort
    *   `BTF_KIND_TYPE_TAG` - abort

The BTF reader has these distinct phases of operation:

1.  file is opened as ELF and `.BTF` section data found
2.  BTF header processed
3.  BTF nodes parsed into a graph with symbol information held separately
4.  symbols and root node added to the graph

### DWARF

The ELF / DWARF reader operates similarly to the other readers at a high level,
but much more work has to be done to turn ELF symbols and DWARF DIEs into STG
nodes.

1.  the ELF file is checked for DWARF - missing DWARF results in a warning
2.  the ELF symbols are read (from `.dynsym` in the case of shared object file)
3.  the DWARF information is parsed into a partial STG graph
4.  the ELF and DWARF information are stitched together, adding symbols and a
    root node to the graph
5.  useless type qualifiers are stripped in post-processing

## Output preprocessing

Before `stg` outputs a serialised graph, it performs:

1.  a type normalisation step that unifies overlapping type definitions
2.  a final deduplication step to eliminate other redundant nodes
