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

inline std::array<uint64_t, 6> parse_smaps_rollup(const char* buf)
{
    std::array<uint64_t, 6> numbers = { 0, 0, 0, 0, 0, 0 };

    const char* start = strstr(buf, "Rss:");
    sscanf(start, "Rss: %" SCNu64, &numbers[0]);

    start = strstr(start, "Pss:");
    sscanf(start, "Pss: %" SCNu64, &numbers[1]);

    start = strstr(start, "Shared_Clean:");
    sscanf(start, "Shared_Clean: %" SCNu64, &numbers[2]);

    start = strstr(start, "Shared_Dirty:");
    sscanf(start, "Shared_Dirty: %" SCNu64, &numbers[3]);

    start = strstr(start, "Private_Clean:");
    sscanf(start, "Private_Clean: %" SCNu64, &numbers[4]);

    start = strstr(start, "Private_Dirty:");
    sscanf(start, "Private_Dirty: %" SCNu64, &numbers[5]);

    return numbers;
}

class SmapsService
{
    Attribute m_rss_attr;
    Attribute m_pss_attr;
    Attribute m_shared_clean_attr;
    Attribute m_shared_dirty_attr;
    Attribute m_private_clean_attr;
    Attribute m_private_dirty_attr;

    int m_fd;

    unsigned m_failed;

    void snapshot_cb(Caliper*, SnapshotBuilder& rec)
    {
        char    buf[1024];
        ssize_t ret = pread(m_fd, buf, sizeof(buf), 0);

        if (ret < 0) {
            ++m_failed;
            return;
        }

        auto val = parse_smaps_rollup(buf);

        rec.append(m_rss_attr, cali_make_variant_from_uint(val[0]));
        rec.append(m_pss_attr, cali_make_variant_from_uint(val[1]));
        rec.append(m_shared_clean_attr, cali_make_variant_from_uint(val[2]));
        rec.append(m_shared_dirty_attr, cali_make_variant_from_uint(val[3]));
        rec.append(m_private_clean_attr, cali_make_variant_from_uint(val[4]));
        rec.append(m_private_dirty_attr, cali_make_variant_from_uint(val[5]));
    }

    void finish_cb(Caliper*, Channel* channel)
    {
        if (m_failed > 0)
            Log(0).stream() << channel->name() << ": smaps: failed to read /proc/self/smaps_rollup " << m_failed
                            << " times\n";
    }

    SmapsService(Caliper* c, int fd) : m_fd { fd }, m_failed { 0 }
    {
        m_rss_attr = c->create_attribute(
            "smaps.rss",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_pss_attr = c->create_attribute(
            "smaps.pss",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_shared_clean_attr = c->create_attribute(
            "smaps.shared_clean",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_shared_dirty_attr = c->create_attribute(
            "smaps.shared_dirty",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_private_clean_attr = c->create_attribute(
            "smaps.private_clean",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
        m_private_dirty_attr = c->create_attribute(
            "smaps.private_dirty",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE
        );
    }

public:

    static void smaps_register(Caliper* c, Channel* channel)
    {
        int fd = open("/proc/self/smaps_rollup", O_RDONLY | O_NONBLOCK);

        if (fd < 0) {
            Log(0).perror(errno, "open(\"/proc/self/smaps_rollup\")");
            return;
        }

        SmapsService* instance = new SmapsService(c, fd);

        channel->events().snapshot.connect([instance](Caliper* c, SnapshotView, SnapshotBuilder& rec) {
            instance->snapshot_cb(c, rec);
        });
        channel->events().finish_evt.connect([instance](Caliper* c, Channel* channel) {
            instance->finish_cb(c, channel);
            close(instance->m_fd);
            delete instance;
        });

        Log(1).stream() << channel->name() << ": registered smaps service\n";
    }
};

const char* smaps_spec = R"json(
{
    "name"        : "smaps",
    "description" : "Record process memory info from /proc/self/smaps_rollup"
}
)json";

} // namespace

namespace cali
{

CaliperService smaps_service { ::smaps_spec, ::SmapsService::smaps_register };

}
