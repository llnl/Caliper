// Copyright (c) 2026, Triad National Security, LLC.
// See top-level LICENSE file for details.

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"

#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include <array>

using namespace cali;

namespace
{

inline std::array<uint64_t, 16> parse_proc_status(const char* buf)
{
    std::array<uint64_t, 16> numbers = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    const char* start = strstr(buf, "VmPeak:");
    sscanf(start, "VmPeak: %" SCNu64, &numbers[0]);

    start = strstr(start, "VmSize:");
    sscanf(start, "VmSize: %" SCNu64, &numbers[1]);

    start = strstr(start, "VmLck:");
    sscanf(start, "VmLck: %" SCNu64, &numbers[2]);

    start = strstr(start, "VmPin:");
    sscanf(start, "VmPin: %" SCNu64, &numbers[3]);

    start = strstr(start, "VmHWM:");
    sscanf(start, "VmHWM: %" SCNu64, &numbers[4]);

    start = strstr(start, "VmRSS:");
    sscanf(start, "VmRSS: %" SCNu64, &numbers[5]);

    start = strstr(start, "RssAnon:");
    sscanf(start, "RssAnon: %" SCNu64, &numbers[6]);

    start = strstr(start, "RssFile:");
    sscanf(start, "RssFile: %" SCNu64, &numbers[7]);

    start = strstr(start, "RssShmem:");
    sscanf(start, "RssShmem: %" SCNu64, &numbers[8]);

    start = strstr(start, "VmData:");
    sscanf(start, "VmData: %" SCNu64, &numbers[9]);

    start = strstr(start, "VmStk:");
    sscanf(start, "VmStk: %" SCNu64, &numbers[10]);

    start = strstr(start, "VmExe:");
    sscanf(start, "VmExe: %" SCNu64, &numbers[11]);

    start = strstr(start, "VmLib:");
    sscanf(start, "VmLib: %" SCNu64, &numbers[12]);

    start = strstr(start, "VmPTE:");
    sscanf(start, "VmPTE: %" SCNu64, &numbers[13]);

    start = strstr(start, "VmSwap:");
    sscanf(start, "VmSwap: %" SCNu64, &numbers[14]);

    start = strstr(start, "HugetlbPages:");
    sscanf(start, "HugetlbPages: %" SCNu64, &numbers[15]);

    return numbers;
}

class ProcStatusService
{
    Attribute m_vmpeak_attr;
    Attribute m_vmsize_attr;
    Attribute m_vmlck_attr;
    Attribute m_vmpin_attr;
    Attribute m_vmhwm_attr;
    Attribute m_vmrss_attr;
    Attribute m_rssanon_attr;
    Attribute m_rssfile_attr;
    Attribute m_rssshmem_attr;
    Attribute m_vmdata_attr;
    Attribute m_vmstk_attr;
    Attribute m_vmexe_attr;
    Attribute m_vmlib_attr;
    Attribute m_vmpte_attr;
    Attribute m_vmswap_attr;
    Attribute m_hugetlbpages_attr;

    int m_status_fd, m_clear_fd;

    unsigned m_status_failed, m_clear_failed;

    void snapshot_cb(Caliper*, SnapshotBuilder& rec)
    {
        char    buf[4096];
        ssize_t ret = pread(m_status_fd, buf, sizeof(buf), 0);

        if (ret < 0) {
            ++m_status_failed;
            return;
        }

        auto val = parse_proc_status(buf);

        rec.append(m_vmpeak_attr, cali_make_variant_from_uint(val[0]));
        rec.append(m_vmsize_attr, cali_make_variant_from_uint(val[1]));
        rec.append(m_vmlck_attr, cali_make_variant_from_uint(val[2]));
        rec.append(m_vmpin_attr, cali_make_variant_from_uint(val[3]));
        rec.append(m_vmhwm_attr, cali_make_variant_from_uint(val[4]));
        rec.append(m_vmrss_attr, cali_make_variant_from_uint(val[5]));
        rec.append(m_rssanon_attr, cali_make_variant_from_uint(val[6]));
        rec.append(m_rssfile_attr, cali_make_variant_from_uint(val[7]));
        rec.append(m_rssshmem_attr, cali_make_variant_from_uint(val[8]));
        rec.append(m_vmdata_attr, cali_make_variant_from_uint(val[9]));
        rec.append(m_vmstk_attr, cali_make_variant_from_uint(val[10]));
        rec.append(m_vmexe_attr, cali_make_variant_from_uint(val[11]));
        rec.append(m_vmlib_attr, cali_make_variant_from_uint(val[12]));
        rec.append(m_vmpte_attr, cali_make_variant_from_uint(val[13]));
        rec.append(m_vmswap_attr, cali_make_variant_from_uint(val[14]));
        rec.append(m_hugetlbpages_attr, cali_make_variant_from_uint(val[15]));

        // Reset accounting: write 5 to /proc/self/clear_refs
        ret = pwrite(m_clear_fd, "5", sizeof(char), 0);

        if (ret < 0) {
            ++m_clear_failed;
            return;
        }
    }

    void finish_cb(Caliper*, Channel* channel)
    {
        if (m_status_failed > 0)
            Log(0).stream() << channel->name() << ": proc_status: failed to read /proc/self/status " << m_status_failed
                            << " times\n";
        if (m_clear_failed > 0)
            Log(0).stream() << channel->name() << ": proc_status: failed to write to /proc/self/clear_refs "
                            << m_clear_failed << " times\n";
    }

    ProcStatusService(Caliper* c, int status_fd, int clear_fd)
        : m_status_fd { status_fd }, m_clear_fd { clear_fd }, m_status_failed { 0 }, m_clear_failed { 0 }
    {
        m_vmpeak_attr = c->create_attribute(
            "proc_status.vmpeak",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmsize_attr = c->create_attribute(
            "proc_status.vmsize",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmlck_attr = c->create_attribute(
            "proc_status.vmlck",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmpin_attr = c->create_attribute(
            "proc_status.vmpin",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmhwm_attr = c->create_attribute(
            "proc_status.vmhwm",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmrss_attr = c->create_attribute(
            "proc_status.vmrss",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_rssanon_attr = c->create_attribute(
            "proc_status.rssanon",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_rssfile_attr = c->create_attribute(
            "proc_status.rssfile",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_rssshmem_attr = c->create_attribute(
            "proc_status.rssshmem",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmdata_attr = c->create_attribute(
            "proc_status.vmdata",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmstk_attr = c->create_attribute(
            "proc_status.vmstk",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmexe_attr = c->create_attribute(
            "proc_status.vmexe",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmlib_attr = c->create_attribute(
            "proc_status.vmlib",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmpte_attr = c->create_attribute(
            "proc_status.vmpte",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_vmswap_attr = c->create_attribute(
            "proc_status.vmswap",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_hugetlbpages_attr = c->create_attribute(
            "proc_status.hugetlbpages",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
    }

public:

    static void proc_status_register(Caliper* c, Channel* channel)
    {
        int status_fd = open("/proc/self/status", O_RDONLY | O_NONBLOCK);

        if (status_fd < 0) {
            Log(0).perror(errno, "open(\"/proc/self/status\")");
            return;
        }

        int clear_fd = open("/proc/self/clear_refs", O_WRONLY | O_NONBLOCK);

        if (clear_fd < 0) {
            Log(0).perror(errno, "open(\"/proc/self/clear_refs\")");
            return;
        }

        ProcStatusService* instance = new ProcStatusService(c, status_fd, clear_fd);

        channel->events().snapshot.connect([instance](Caliper* c, SnapshotView, SnapshotBuilder& rec) {
            instance->snapshot_cb(c, rec);
        });
        channel->events().finish_evt.connect([instance](Caliper* c, Channel* channel) {
            instance->finish_cb(c, channel);
            close(instance->m_status_fd);
            close(instance->m_clear_fd);
            delete instance;
        });

        Log(1).stream() << channel->name() << ": registered proc_status service\n";
    }
};

const char* proc_status_spec = R"json(
{
    "name"        : "proc_status",
    "description" : "Record process memory info from /proc/self/status"
}
)json";

} // namespace

namespace cali
{

CaliperService proc_status_service { ::proc_status_spec, ::ProcStatusService::proc_status_register };

}
