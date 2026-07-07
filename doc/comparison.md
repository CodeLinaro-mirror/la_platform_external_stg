# Comparison

This is implemented in `comparison.{h,cc}`.

Graph comparison is the basis of all STG difference reporting.

All the various STG node attributes (such as `size` or `name`) and edge
identifiers (whether explicit or not, such as function parameter index) can be
reduced to generic labels. This (over)simplification reduces the problem to
solve to:

*   given two rooted directed graphs, containing labelled nodes and edges
*   generate a graph that encapsulates all the differences found by following
    matching pairs of edges

Report generation from such a graph is the subject of [Reporting](reporting.md).

STG compares the graphs starting at the root nodes and recursing along edges
with matching labels. The recursion stops at incomparable nodes (such as a
`struct` and `int`). Otherwise a comparison specific to the kind of node is
performed.

Given that the graph will in general not be a tree and may contain cycles, STG

*   memoises comparisons with known results
*   detects comparison cycles and ensures correct termination and propagation of
    results

Overall, this ensures that any given pair of nodes (one from each graph) is
compared at most once. In the case of a small change to a typical ABI graph of
size *N*, *O(N)* node comparisons are performed.

## Ignoring certain kinds of differences

It has proved useful to add selective suppression of certain kinds of
differences, for distinct purposes.

| **ignore**               | **directionality** | **purpose**               |
| ------------------------ | ------------------ | ------------------------- |
| interface addition       | asymmetric         | compatibility checking    |
| type definition addition | asymmetric         | compatibility checking    |
| primitive type encoding  | symmetric          | cross comparison noise    |
:                          :                    : reduction                 :
| member size              | symmetric          | cross comparison noise    |
:                          :                    : reduction                 :
| enum underlying type     | symmetric          | cross comparison noise    |
:                          :                    : reduction                 :
| qualifier                | symmetric          | cross comparison noise    |
:                          :                    : reduction                 :
| symbol CRC               | symmetric          | cross comparison noise    |
:                          :                    : reduction                 :
| symbol type presence     | symmetric          | libabigail XML comparison |
:                          :                    : noise reduction           :
| type declaration status  | symmetric          | libabigail XML comparison |
:                          :                    : noise reduction           :

The first two options can be used to test whether one ABI is a subset of
another.

It can be useful to cross compare ABIs extracted in different ways to validate
the fidelity of one ABI source against another. Where the models or
implementations differ systematically, suppressing those differences will make
the remainder more obvious.

The libabigail versions used in Android's GKI project often generated ABIs with
spurious differences due to the disappearance (or reappearance) of type
definitions and (occasionally) symbol types. The corresponding ignore options
replicate the behaviour of libabigail's `abidiff`.

## Differences and difference graphs

When comparing a pair of nodes, each difference falls into one of the following
categories:

*   node label - a purely local difference
*   matching edge - a recursive difference found by following edges with
    matching labels
*   added or removed labelled edges, where edges are identified by label -
    modelled as a recursive difference with a node absent on one side

Each node in an STG difference graph is one of the following[^1]:

*   a node removal or addition, containing
    *   a reference to either a node in first graph or one in the second
*   a node change, containing
    *   a reference to two nodes, one in each of the two graphs
    *   a possibly-empty list of differences which can each be one of
        *   a node attribute difference in the form of some informative text
        *   an edge difference in the form of a link to a difference node

[^1]: STG models comparisons as pairs of nodes where either node can be absent.
    The absent-absent comparison is used to represent "no change". All such
    edges, except those representing the root of a "no change" graph comparison,
    are pruned during diff graph creation.

Note that STG's difference nodes are *unkinded*, there is only one kind of
difference node, unlike STG's data nodes where there is a separate kind of node
for each kind of C type etc.

## Matching and comparing collections of edges

While an algorithm based on generic label comparison will work, there are a
couple of issues:

*   collections of outgoing edges may not have an obvious label to assign
    (multiple anonymous members of a `struct`, for example)
*   edges are often ordered (parameters and members, for example) and we want to
    preserve this order when reporting differences

Comparing two pointer types is straightforward, just compare the pointed-to
types. However, symbol tables, function arguments and struct members all require
comparisons of multiple entities simultaneously.

STG compares edge aggregates as follows:

*   arrays (like function arguments): by index, comparing recursively items with
    the same index, reporting removals and additions of the remaining items
*   maps (like the symbol table): by key, comparing recursively items with
    matching keys, reporting removals and additions of unmatched items
*   otherwise synthesise a key for comparison, compare by key, report
    differences in the original order of items being compared (favouring the
    second list's order over the first's); the reordering is an *O(n)* operation

## Implementation Details

Comparison is mostly done pair-wise recursively with a DFS, by the function
object `CompareWorker` and with the help of the [SCC finder](scc.md).

The algorithm divides responsibility between `operator()(Id, Id)` and various
`operator()(Node, Node)` methods. There are also various helpers in and outside
`CompareWorker`.

The `Result` type encapsulates the difference between two nodes being compared.
It contains both a list (`Diff`) of differences (`DiffDetail`) and a boolean
equality outcome. The latter is used to propagate inequality information in the
presence of cycles in the diff comparison graph.

### `operator()(Node, Node)`

Specialised for each `Node` type, these methods have the job of computing local
differences, matching edges and obtaining edge differences from recursive calls
to `operator()(Id, Id)` (or `Removed` and `Added`, if edge labels are
unmatched).

Local differences can easily be rendered as text, but edge differences need
recursive calls, the results of which are merged into the local differences
`Result` with helper methods.

These methods form the bulk of the comparison code, so in general we want them
to be as small as possible, containing no boilerplate and simply mirroring the
node data. The helper functions were therefore chosen for power, laziness and
concision.

### `Added` and `Removed`

These take care of comparisons where one side is absent.

There are several reasons for not folding this functionality into `operator(Id,
Id)` itself:

*   it would result in unnecessary extra work as its callers would need to pack
    and the function would need to unpack `optional<Id>` arguments
*   added and removed nodes have none of the other interesting features that it
    handles
*   `Added` and `Removed` don't need to decorate their return values with any
    difference information

### `Defined`

This takes care of comparisons of user-defined types which may be forward
declarations or full definitions.

### `Nodes`

These take care of comparisons of sequences of arbitrary nodes (or enumerators).

STG uses "matching keys" to reduce the problem of comparing sequences to
comparing maps. It attempts to preserve original sequence order as described in
`order.h`.

### `operator(Id, Id)`

This controls recursion and handles some special cases before delegating to some
`operator()(Node, Node)` in the "normal" case.

It takes care of the following:

*   revisited, completed comparisons
*   revisited, in-progress comparison
*   qualified types
*   typedefs
*   incomparable and comparable nodes
*   comparison cycles

Note that the non-trivial special cases relating to typedefs and qualified types
(and their current concrete representations) require non-parallel traversals of
the graphs being compared; this is the only place where the comparison is not
purely structural.

#### Revisited Nodes and Recursive Comparison

STG comparison relies on `SCC` to control recursion and behaviour in the face of
graphs containing arbitrary cycles. Any difference found affecting one node in a
comparison cycle affects all nodes in that cycle.

Excluding the two special cases documented in the following sections, the
comparison steps are approximately:

1.  if the comparison already has a known result then return this
2.  if the comparison already is in progress then return a potential difference
3.  start node visit, register the node with the SCC finder
    1.  (special handling of qualified types and typedefs)
    2.  incomparable nodes (such as a `struct` and an `int`) go to `Mismatch`
        which returns a difference; there is no further recursion
    3.  otherwise delegate node comparison to a node-specific function; with
        possible recursion
    4.  the comparison result here is tentative, due to potential cycles
4.  finish node visit, informing the SCC finder
5.  if an SCC was closed, we've just finished its root comparison
    1.  root compared equal? discard unwanted potential differences
    2.  difference found? record confirmed differences
    3.  record all its comparison results as final
6.  return result (which will be final if an SCC was closed)

#### Typedefs

This special handling is subject to change.

*   `typedef` foo bar ⇔ foo

Typedefs are named type aliases which cannot refer to themselves or later
defined types. The referred-to type is exactly identical to the typedef. So for
difference *finding*, typedefs should just be resolved and skipped over.
However, for *reporting*, it may be still useful to say where a difference came
from. This requires extra handling to collect the typedef names on each side of
the comparison, when there is something to report.

If `operator()(Id, Id)` sees a comparison involving a typedef, it resolves
typedef chains on both sides and keeps track of the names. Then it calls itself
recursively. If the result is no-diff, it returns no-diff, otherwise, it reports
the differences between the types at the end of the typedef chains.

An alternative would be to genuinely just follow the epsilons. Store the
typedefs in the diff tree but record the comparison of what they resolve to. The
presentation layer can decorate the comparison text with resolution chains.

Note that qualified typedefs present extra complications.

#### Qualified Types

This special handling is subject to change.

*   `const` → `volatile` → foo ⇔ `volatile` → `const` → foo

STG currently represents type qualifiers as separate, individual nodes. They are
relevant for finding differences but there may be no guarantee of the order in
which they will appear. For diff reporting, STG currently reports added and
removed qualifiers but also compares the underlying types.

This implies that when faced with a comparison involving a qualifier,
`operator()(Id, Id)` should collect and compare all qualifiers on both sides and
treat the types as compound objects consisting of their qualifiers and the
underlying types, either or both of which may have differences to report.
Comparing the underlying types requires recursive calls.

Note that qualified typedefs present extra complications.

#### Qualified typedefs

STG does not currently do anything special for qualified typedefs which can have
subtle and surprising behaviours. For example:

Before:

```c++
const int quux;
```

After 1:

```c++
typedef int foo;
const foo quux;
```

After 2:

```c++
typedef const int foo;
foo quux;
```

After 3:

```c++
typedef const int foo;
const foo quux;
```

In all cases above, the type of `quux` is unchanged. These examples strongly
suggest that a better model of C types would involve tracking qualification as a
decoration present on every type node, including typedefs.

Note that this behaviour implies C's type system is not purely constructive as
there is machinery to discard duplicate qualifiers which would be illegal
elsewhere.

For the moment, we can pretend that outer qualifications are always significant,
even though they may be absorbed by inner ones, and risk occasional false
positives.

A worse case is:

Before:

```c++
const int quux[];
```

After 1:

```c++
typedef int foo[];
const foo quux;
```

After 2:

```c++
typedef const int foo[];
foo quux;
```

After 3:

```c++
typedef const int foo[];
const foo quux;
```

All the `quux` are identically typed. There is an additional wart that what
would normally be illegal qualifiers on an array type instead decorate its
element type.

Finally, worst is:

Before:

```c++
const int quux();
```

After 1:

```c++
typedef int foo();
const foo quux;
```

After 2:

```c++
typedef const int foo();
foo quux;
```

After 3:

```c++
typedef const int foo();
const foo quux;
```

The two `const foo quux` cases invoke undefined behaviour. The consistently
crazy behaviour would have been to decorate the return type instead.

The "worstest" case is GCC allowing the specification of typedef alignment
different to the defining type. This abomination should not exist and should
never be used. Alignment is not currently modelled by STG.

### Diff helpers

These are mainly used by the `CompareWorker::operator()(Node, Node)` methods.

*   `MarkIncomparable` - nodes are just different
*   `AddNodeDiff` - add node difference, unconditionally
*   `AddEdgeDiff` - add edge difference (addition or removal), unconditionally
*   `MaybeAddNodeDiff` - add node difference (label change), conditionally
*   `MaybeAddEdgeDiff` - add matching edge recursive difference, conditionally

Variants are possible where text is generated lazily on a recursive diff being
found, as are ones where labels are compared and serialised only if different.

## Diff Presentation

In general, there are two problems to solve:

*   generating suitable text, for
    *   nodes and edges
    *   node and edge differences
*   building a report with some meaningful structure

Node and edge description and report structure are the responsibility of the
*reporting* code. See [Naming](naming.md) for more detailed notes on node
description, mainly C type name syntax.

Several report formats are supported and the simplest is (omitting various
complications) a rendering of a difference graph as a difference *tree* where
revisiting nodes is avoided by reporting 2 additional artificial kinds of
difference:

1.  already reported - to handle diff sharing
2.  being compared - to handle diff cycles

The various formats are not documented further here.

Finally, node and edge difference description is currently the responsibility of
the *comparison* code. This may change in the future, but might require a typed
difference graph.
