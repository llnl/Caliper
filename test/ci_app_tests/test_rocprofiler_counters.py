# HIP tests

import io
import unittest

import caliperreader
import calipertest as cat

class CaliperRocmCounterTest(unittest.TestCase):
    """ Caliper test class for rocprofiler-sdk services w/ counters """

    def test_rocm_activity_profile_with_counters(self):
        target_cmd = [ './vectoradd', 'rocm-activity-profile,profile.roctx,rocm.counters=SQ_WAVES_sum,output=stdout' ]
        env = { 'HIP_LAUNCH_BLOCKING': '1' }

        out,_ = cat.run_test(target_cmd, env)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'rocm.activity',
                         'rocm.kernel.name',
                         'scale#sum#rocm.activity.duration',
                         'path',
                         'rocm.marker' }
        ))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'rocm.activity': 'KERNEL_DISPATCH_COMPLETE',
                         'sum#sum#rocm.activity.count': '1',
                         'path': ['main', 'vectoradd', 'hipLaunchKernel'] }
        ))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'rocm.activity': 'MEMORY_COPY_DEVICE_TO_HOST',
                         'sum#sum#rocm.activity.count': '1',
                         'sum#sum#rocm.bytes': '4194304',
                         'path': ['main', 'copy_d2h', 'hipMemcpy'] }
        ))

        rec = cat.get_snapshot_with_keys(snapshots, ['path', 'sum#sum#rocm.SQ_WAVES_sum'])
        self.assertIsNotNone(rec)
        self.assertEqual(int(rec['sum#sum#rocm.SQ_WAVES_sum']), 16384)
        self.assertEqual(rec['path'], ['main', 'vectoradd', 'hipLaunchKernel'])


if __name__ == "__main__":
    unittest.main()
