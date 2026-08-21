// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// @file Log.cpp
/// Log class implementation

#include "caliper/common/Log.h"

#include <cstdlib>
#include <cstring>
#include <string>

using namespace cali;

struct LogImpl {
    // --- data

    static const char* s_prefix;

    static LogImpl* s_instance;

    enum class Stream { StdOut, StdErr, None, File };

    Stream        m_stream;
    std::ofstream m_ofstream;
    int           m_verbosity;
    std::string   m_prefix;

    // --- helpers

    void init_stream(const std::string& name)
    {
        if (name == "stderr")
            m_stream = Stream::StdErr;
        else if (name == "stdout")
            m_stream = Stream::StdOut;
        else if (name == "none")
            m_stream = Stream::None;
        else {
            m_stream = Stream::File;
            m_ofstream.open(name);

            if (!m_ofstream && m_verbosity > 0)
                std::cerr << s_prefix << "Could not open log file " << name << std::endl;
        }
    }

    // --- interface

    LogImpl() : m_verbosity { 0 }, m_prefix { s_prefix }
    {
        char* val = getenv("CALI_LOG_VERBOSITY");
        m_verbosity = val ? std::max(0, std::stoi(val)) : 0;

        val = getenv("CALI_LOG_LOGFILE");
        init_stream(val ? val : "stderr");
    }

    std::ostream& get_stream()
    {
        switch (m_stream) {
        case Stream::StdOut:
            return std::cout;
        case Stream::StdErr:
            return std::cerr;
        default:
            return m_ofstream;
        }
    }
};

const char* LogImpl::s_prefix = "== CALIPER: ";

LogImpl* LogImpl::s_instance = nullptr;

//
// --- Log public interface
//

std::ostream& Log::get_stream()
{
    return LogImpl::s_instance->get_stream() << LogImpl::s_instance->m_prefix;
}

std::ostream& Log::perror(int errnum, const char* msg)
{
    if (verbosity() < m_level)
        return m_nullstream;

#ifdef _GLIBCXX_HAVE_STRERROR_R
    char buf[120];
    return get_stream() << msg << strerror_r(errnum, buf, sizeof(buf));
#else
    return get_stream() << msg << strerror(errnum);
#endif
}

int Log::verbosity()
{
    if (LogImpl::s_instance == nullptr)
        return -1;

    return LogImpl::s_instance->m_verbosity;
}

void Log::set_verbosity(int v)
{
    LogImpl::s_instance->m_verbosity = v;
}

void Log::add_prefix(const std::string& prefix)
{
    LogImpl::s_instance->m_prefix += prefix;
}

void Log::init()
{
    LogImpl::s_instance = new LogImpl;
}

void Log::fini()
{
    delete LogImpl::s_instance;
    LogImpl::s_instance = nullptr;
}

bool Log::is_initialized()
{
    return LogImpl::s_instance != nullptr;
}
