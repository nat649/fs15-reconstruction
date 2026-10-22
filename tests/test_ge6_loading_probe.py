"""Authored memory fixtures validate diagnostics, not original game execution."""
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from ge6_vm_loading_probe import TASK_QUEUE,snapshot


class LoadingProbeTests(unittest.TestCase):
    def fixture(self,index=4,count=10):
        header=struct.pack('<5Q',0x1000,0x1010,0x1080,0,0x2000)
        group=bytearray(0x38)
        struct.pack_into('<3Q',group,8,0x4000,0x4000+count*8,0x4100)
        struct.pack_into('<2I',group,0x28,index,count)
        group[0x30]=1
        memory={TASK_QUEUE:header,0x2000:struct.pack('<Q',0x3000),0x3000:bytes(group)}
        return lambda address,size:memory[address][:size]

    def test_partial_and_consumed_groups_remain_only_scheduler_observations(self):
        for index in (4,10):
            result=snapshot(self.fixture(index))
            self.assertEqual(result['queued_records'],2)
            self.assertEqual(result['task_index'],index)
            self.assertTrue(result['producer_finished'])
            self.assertNotIn('playable_game_verified',result)

    def test_invalid_and_changing_headers_cannot_report_progress(self):
        with self.assertRaises(ValueError):
            snapshot(self.fixture(index=11))
        fixture=self.fixture()
        calls=0
        def changing(address,size):
            nonlocal calls
            calls+=1
            return bytes(size) if calls==4 else fixture(address,size)
        with self.assertRaisesRegex(ValueError,'changed'):
            snapshot(changing)

    def test_empty_queue_does_not_prove_a_finished_world(self):
        result=snapshot(lambda address,size:bytes(size))
        self.assertEqual(result['queued_records'],0)
        self.assertNotIn('task_index',result)
        self.assertNotIn('playable_game_verified',result)
