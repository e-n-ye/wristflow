import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from ota_boot_check import leaf_block, jump_unreferenced


class RamLeafChecks(unittest.TestCase):
    def block(self, operation='nop', *, start=0x20020000, tail='bx\tlr'):
        return (f'{start:08x} <probe>:\n'
                f' {start:x}:\tdsb\tsy\n'
                f' {start+4:x}:\tisb\tsy\n'
                f' {start+8:x}:\t{operation}\n'
                f' {start+12:x}:\t{tail}\n')

    def test_ram_leaf_and_return(self):
        self.assertEqual(leaf_block(self.block(), 'probe', byte_size=16)['instructions'], 4)

    def test_ram_jump_tail(self):
        self.assertEqual(leaf_block(self.block(tail='bx\tr3'), 'probe', final_jump=True, byte_size=16)['instructions'], 4)

    def test_rejects_flash_placement(self):
        with self.assertRaisesRegex(ValueError, 'outside boot RAM'):
            leaf_block(self.block(start=0x12020000), 'probe', byte_size=16)

    def test_rejects_calls_including_ram_calls(self):
        for operation in ('bl\t20020100 <helper>', 'blx\tr3', 'bl.w\t12021000 <helper>'):
            with self.subTest(operation=operation), self.assertRaisesRegex(ValueError, 'Call inside'):
                leaf_block(self.block(operation), 'probe', byte_size=16)

    def test_rejects_pc_literal_and_embedded_data(self):
        for operation in ('ldr\tr1, [pc, #32]', '.word\t0x12020000', 'tbb\t[r0, r1]'):
            with self.subTest(operation=operation), self.assertRaises(ValueError):
                leaf_block(self.block(operation), 'probe', byte_size=16)

    def test_rejects_external_branches(self):
        for operation in ('b.w\t12020000 <outside>', 'bne.n\t20020010 <after>', 'cbz\tr3, 20020100 <outside>'):
            with self.subTest(operation=operation), self.assertRaises(ValueError):
                leaf_block(self.block(operation), 'probe', byte_size=16)
        self.assertEqual(leaf_block(self.block('bne.n\t20020008 <probe+0x8>'), 'probe', byte_size=16)['instructions'], 4)

    def test_rejects_indirect_jump_and_missing_barrier(self):
        with self.assertRaises(ValueError):
            leaf_block(self.block(tail='bx\tr4'), 'probe', byte_size=16)
        with self.assertRaisesRegex(ValueError, 'Missing barriers'):
            leaf_block(self.block().replace('dsb', 'nop'), 'probe', byte_size=16)

    def test_rejects_wrong_jump_tail(self):
        with self.assertRaises(ValueError):
            leaf_block(self.block(), 'probe', final_jump=True, byte_size=16)

    def test_rejects_all_direct_jump_references_outside_body(self):
        for op in ('b', 'b.n', 'b.w', 'bl', 'bl.w', 'blx', 'ldr'):
            with self.subTest(op=op), self.assertRaises(ValueError):
                jump_unreferenced(f'20020000:\t{op}\t20020100 <wf_ota_ram_jump>\n', 0x20020100, 128)
        jump_unreferenced('20020110:\tbne.n\t20020108 <wf_ota_ram_jump+0x8>\n', 0x20020100, 128)


if __name__ == '__main__':
    unittest.main()
