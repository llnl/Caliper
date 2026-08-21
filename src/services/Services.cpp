// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "Services.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"

#include <cassert>
#include <cctype>
#include <map>
#include <string>
#include <sstream>

using namespace cali;

// Get auto-generated add_default_service_specs() function from services.inc.cpp
#include "services.inc.cpp"

namespace
{

std::string get_name_from_spec(const char* name_or_spec)
{
    // try to parse the given string as spec, otherwise return it as-is
    bool ok   = false;
    auto dict = StringConverter(name_or_spec).rec_dict(&ok);

    if (!ok)
        return std::string { name_or_spec };

    auto it = dict.find("name");
    if (it != dict.end())
        return it->second.to_string();

    return std::string { name_or_spec };
}

class ServicesManager
{
    std::map<std::string, CaliperService> m_services;

public:

    std::vector<std::string> get_available_services() const
    {
        std::vector<std::string> ret;
        ret.reserve(m_services.size());

        for (const auto& p : m_services)
            ret.push_back(p.first);

        return ret;
    }

    bool register_service(const char* name, Caliper* c, Channel* channel) const
    {
        for (const auto& p : m_services)
            if (p.first == name && p.second.register_fn) {
                (*p.second.register_fn)(c, channel);
                return true;
            }

        return false;
    }

    void add_services(const CaliperService list[])
    {
        for (const CaliperService* s = list; s && s->name_or_spec && s->register_fn; ++s)
            m_services[get_name_from_spec(s->name_or_spec)] = *s;
    }

    std::string get_service_description(const std::string& name)
    {
        auto service_itr = m_services.find(name);
        if (service_itr == m_services.end())
            return std::string();
        auto dict     = StringConverter(service_itr->second.name_or_spec).rec_dict();
        auto spec_itr = dict.find("description");
        return spec_itr != dict.end() ? spec_itr->second.to_string() : std::string();
    }

    std::ostream& print_service_documentation(std::ostream& os, const std::string& name)
    {
        auto service_itr = m_services.find(name);
        if (service_itr == m_services.end())
            return os;

        auto dict     = StringConverter(service_itr->second.name_or_spec).rec_dict();
        auto spec_itr = dict.find("description");
        if (spec_itr != dict.end())
            os << ' ' << spec_itr->second.to_string() << '\n';
        else
            os << " (no description)\n";

        spec_itr = dict.find("config");
        if (spec_itr == dict.end())
            return os;

        // print description of config variables
        auto list = spec_itr->second.rec_list();
        for (const auto& s : list) {
            auto cfg_dict = s.rec_dict();
            auto it       = cfg_dict.find("name");
            if (it == cfg_dict.end())
                continue;

            std::string key = it->second.to_string();
            if (key.empty())
                continue;
            std::string str = std::string("CALI_").append(name).append("_").append(key);
            transform(str.begin(), str.end(), str.begin(), ::toupper);

            os << "  " << str;

            std::string val;
            it = cfg_dict.find("value");
            if (it != cfg_dict.end())
                val = it->second.to_string();
            if (!val.empty())
                os << '=' << val;

            it = cfg_dict.find("type");
            if (it != cfg_dict.end())
                os << " (" << it->second.to_string() << ")\n";

            it = cfg_dict.find("description");
            if (it != cfg_dict.end())
                os << "   " << it->second.to_string() << '\n';
            else
                os << "   (no description)\n";
        }

        return os;
    }

    static ServicesManager* instance()
    {
        static std::unique_ptr<ServicesManager> inst { new ServicesManager };
        return inst.get();
    }
};

} // namespace

namespace cali
{

namespace services
{

bool register_service(Caliper* c, Channel* channel, const char* name)
{
    return ServicesManager::instance()->register_service(name, c, channel);
}

void register_configured_services(Caliper* c, Channel* channel)
{
    std::vector<std::string> services = channel->config().get("services", "enable").to_stringlist(",:");

    ServicesManager* sM = ServicesManager::instance();

    for (const std::string& s : services) {
        if (!sM->register_service(s.c_str(), c, channel))
            Log(0).stream() << "Service \"" << s << "\" not found!" << std::endl;
    }
}

void add_service_specs(const CaliperService* services)
{
    ServicesManager::instance()->add_services(services);
}

std::vector<std::string> get_available_services()
{
    return ServicesManager::instance()->get_available_services();
}

std::ostream& print_service_documentation(std::ostream& os, const std::string& name)
{
    return ServicesManager::instance()->print_service_documentation(os, name);
}

std::string get_service_description(const std::string& name)
{
    return ServicesManager::instance()->get_service_description(name);
}

} // namespace services

} // namespace cali
