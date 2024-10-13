# Reporting

This is implemented in `reporting.{h,cc}`.

STG difference reporting is built on [comparison](comparison.md) and
[naming](naming.md). All that remains is to serialise the difference graph into
a readable or usable format.

Just as ABI graphs may contain cycles, so can difference graphs. STG currently
supports the following reporting options.

| **format** | **description**                                                 |
| ---------- | --------------------------------------------------------------- |
| plain      | simple recursion, avoiding repeated node visits - unreadable    |
:            : for deep graphs                                                 :
| flat       | graph is split into fragments each rooted at a node that "owns" |
:            : differences                                                     :
| small      | like flat, but empty sub graphs are omitted                     |
| short      | like small, but with lossy compression of repetitive            |
:            : differences                                                     :
| viz        | Graphviz - infeasible for rendering once the graphs become      |
:            : large                                                           :

The most useful option is `small`. Use `flat` if you need to see the impact on
the interface symbols and types. Use `short` if large reports should be
condensed.

The `flat` reporting and its derivatives will misbehave (go into an infinite
loop) in the current implementation, if the difference graph contains a cycle
where none of the nodes refers to a named type.
