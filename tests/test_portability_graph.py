"""Asset-free checks that an incomplete static graph cannot hide missing work."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from index_portability import closure


class PortabilityGraph(unittest.TestCase):
    def test_recursive_call_cycles_terminate_and_keep_missing_dependencies(self):
        graph={'a':{'callees':['b','missing']},'b':{'callees':['a','b']}}
        reached,missing=closure(graph,{'a'})
        self.assertEqual(reached,{'a','b','missing'})
        self.assertEqual(missing,{'missing'})

    def test_uncatalogued_roots_are_explicit_and_disconnected_code_stays_out(self):
        graph={'leaf':{'callees':[]},'unrelated':{'callees':[]}}
        self.assertEqual(closure(graph,{'leaf','unknown'}),({'leaf','unknown'},{'unknown'}))


if __name__=='__main__':
    unittest.main()
