"""Project adapter for the pinned LJD checkout; retain distinct literal AST nodes.

LJD's slot substitution can reuse the same literal object in sibling arguments.
Its identity-based writer then skips later occurrences, producing `f(nil, , )`.
Copying leaf literals at substitution keeps the values and their occurrences.
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

    def distinct_literals(replace):
        def wrapped(holder, destination, source):
            if isinstance(source, (nodes.Primitive, nodes.Constant)):
                source = copy(source)
            return replace(holder, destination, source)
        return wrapped

    slotworks._replace_node_in_list = distinct_literals(slotworks._replace_node_in_list)
    slotworks._replace_node = distinct_literals(slotworks._replace_node)
    sys.argv = [str(entry)] + sys.argv[2:]
    runpy.run_path(str(entry), run_name="__main__")


if __name__ == "__main__":
    main()
