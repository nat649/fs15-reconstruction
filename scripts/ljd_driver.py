"""Project adapter for the pinned LJD checkout; preserve observed bytecode intent.

LJD's slot substitution can reuse the same literal object in sibling arguments.
Its identity-based writer then skips later occurrences, producing `f(nil, , )`.
Copying leaf literals at substitution keeps the values and their occurrences.
Empty TNEW allocations also must not absorb later field assignments: doing so
can move calls before a table is published globally and change their behavior.
The third-party checkout is not modified or redistributed by this adapter.
"""
from copy import copy
from pathlib import Path
import runpy
import sys


def main():
    if len(sys.argv) < 2:
        raise SystemExit("Usage: ljd_driver.py PATH/TO/LJD/main.py [LJD arguments]")
    entry = Path(sys.argv[1]).absolute()
    sys.path.insert(0, str(entry.parent))
    import ljd.ast.nodes as nodes
    import ljd.ast.slotworks as slotworks
    import ljd.ast.builder as builder
    import ljd.ast.mutator as mutator
    import ljd.bytecode.instructions as instructions

    def distinct_literals(replace):
        def wrapped(holder, destination, source):
            if isinstance(source, (nodes.Primitive, nodes.Constant)):
                source = copy(source)
            return replace(holder, destination, source)
        return wrapped

    slotworks._replace_node_in_list = distinct_literals(slotworks._replace_node_in_list)
    slotworks._replace_node = distinct_literals(slotworks._replace_node)

    build_assignment = builder._build_var_assignment

    def preserve_empty_allocation(state, address, instruction):
        result = build_assignment(state, address, instruction)
        # LuaJIT emits TNEW with a zero allocation hint for an empty literal.
        # TDUP templates and sized TNEW constructors keep their normal handling.
        if instruction.opcode == instructions.TNEW.opcode and instruction.CD == 0:
            result[0].expressions.contents[0]._fs15_empty_allocation = True
        return result

    builder._build_var_assignment = preserve_empty_allocation
    fill_refs = slotworks._fill_simple_refs

    def preserve_table_references(info, data, ignore_ambiguous, safe_mode=True):
        source = info.assignment.expressions.contents[0]
        if getattr(source, '_fs15_empty_allocation', False):
            # Keep a referenced table slot while its element reads/writes exist.
            # Merely filtering data.tables after collection would let the final
            # read inline {}, orphaning earlier writes to the allocated table.
            for ref in info.references[1:]:
                holder = slotworks._get_holder(ref.path)
                if isinstance(holder, nodes.TableElement) and holder.table == ref.identifier:
                    return
        return fill_refs(info, data, ignore_ambiguous, safe_mode)

    slotworks._fill_simple_refs = preserve_table_references
    fill_constructor = mutator.MutatorVisitor._fill_constructor

    def preserve_later_stores(table, constructor, statements):
        if getattr(constructor, '_fs15_empty_allocation', False):
            return 0
        return fill_constructor(table, constructor, statements)

    mutator.MutatorVisitor._fill_constructor = staticmethod(preserve_later_stores)
    sys.argv = [str(entry)] + sys.argv[2:]
    runpy.run_path(str(entry), run_name="__main__")


if __name__ == "__main__":
    main()
