# CpuInfo tests

import io
import unittest

import caliperreader
import calipertest as cat

class CaliperLinuxServicesTest(unittest.TestCase):
    """ Caliper test class for linux-specific services """

    def test_cpuinfo_service(self):
        target_cmd = [ './ci_test_basic' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,cpuinfo,trace,recorder',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'cpuinfo.cpu',
                         'cpuinfo.numa_node',
                         'myphase',
                         'iteration' }))

    def test_memstat_service(self):
        target_cmd = [ './ci_test_basic' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,memstat,trace,recorder',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'memstat.vmsize',
                         'memstat.data',
                         'myphase',
                         'iteration' }))

    def test_proc_status_service(self):
        target_cmd = [ './ci_test_basic' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,proc_status,trace,recorder',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'proc_status.vmpeak',
                         'proc_status.vmsize',
                         'proc_status.vmlck',
                         'proc_status.vmpin',
                         'proc_status.vmhwm',
                         'proc_status.vmrss',
                         'proc_status.rssanon',
                         'proc_status.rssfile',
                         'proc_status.rssshmem',
                         'proc_status.vmdata',
                         'proc_status.vmstk',
                         'proc_status.vmexe',
                         'proc_status.vmlib',
                         'proc_status.vmpte',
                         'proc_status.vmswap',
                         'proc_status.hugetlbpages',
                         'myphase',
                         'iteration' }))

    def test_smaps_service(self):
        target_cmd = [ './ci_test_basic' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,smaps,trace,recorder',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'smaps.rss',
                         'smaps.pss',
                         'smaps.shared_clean',
                         'smaps.shared_dirty',
                         'smaps.private_clean',
                         'smaps.private_dirty',
                         'myphase',
                         'iteration' }))

if __name__ == "__main__":
    unittest.main()
