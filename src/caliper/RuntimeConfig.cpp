// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// RuntimeConfig class implementation

#include "RuntimeConfig.h"

#include "../common/util/parse_util.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

using namespace cali;

namespace
{

std::string config_var_name(const std::string& name, const std::string& key)
{
    // make uppercase PREFIX_NAMESPACE_KEY string
    std::string str = std::string("CALI_") + name + std::string("_") + key;
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

typedef std::map<std::string, std::string> config_profile_t;

} // namespace

namespace cali
{

//
// --- ConfigSet implementation
//

struct ConfigSetImpl {
    // --- data

    std::unordered_map<std::string, StringConverter> m_dict;

    // --- interface

    StringConverter get(const char* key) const
    {
        auto it = m_dict.find(key);
        return (it == m_dict.end() ? StringConverter() : it->second);
    }
};

//
// --- RuntimeConfig implementation
//

struct RuntimeConfig::RuntimeConfigImpl {
    // --- data

    bool m_allow_read_env = true;

    // combined profile: initially receives settings made through "add" API,
    // then merges all selected profiles in here
    ::config_profile_t m_combined_profile;

    // top-priority profile: receives all settings made through "set" API
    // that overwrite other settings
    ::config_profile_t m_top_profile;

    // DB of initialized config sets
    std::map<std::string, std::shared_ptr<ConfigSetImpl>> m_database;

    // Caliper v1 style named config profiles
    std::map<std::string, ::config_profile_t> m_config_profiles;

    // --- helpers

    // Returns the CALI_[set]_[key] value for set and key
    std::string find_config_value(const std::string& set, const std::string& key, const std::string& in = {})
    {
        std::string varname = ::config_var_name(set, key);
        std::string val { in };

        // See if there is an entry in the top config profile
        auto top_itr = m_top_profile.find(varname);
        if (top_itr != m_top_profile.end()) {
            val = top_itr->second;
        } else {
            // See if there is an entry in the base config profile
            auto it = m_combined_profile.find(varname);
            if (it != m_combined_profile.end())
                val = it->second;

            if (m_allow_read_env) {
                // See if there is a environment variable set
                char* env_val = getenv(varname.c_str());
                if (env_val)
                    val = env_val;
            }
        }

        return val;
    }

    void read_config_profiles(std::istream& in)
    {
        //
        // Parse config file line-by-line
        // * '#' as the first character is a comment, or start of a new
        //   group if a string enclosed in square brackets ("[group]") exists
        // * Other lines are parsed as NAME=VALUE, or ignored if no '=' is found
        //

        ::config_profile_t current_profile;
        std::string        current_profile_name { "default" };

        for (std::string line; std::getline(in, line);) {
            if (line.length() < 1)
                continue;

            if (line[0] == '#') {
                // is it a new profile?
                std::string::size_type b = line.find_first_of('[');
                std::string::size_type e = line.find_first_of(']');

                if (b != std::string::npos && e != std::string::npos && b + 1 < e) {
                    if (current_profile.size() > 0)
                        m_config_profiles[current_profile_name].insert(current_profile.begin(), current_profile.end());

                    current_profile.clear();
                    current_profile_name = line.substr(b + 1, e - b - 1);
                } else {
                    continue;
                }
            }

            std::string::size_type s = line.find_first_of('=');

            if (s > 0 && s < line.size()) {
                std::istringstream is(line.substr(s + 1));
                current_profile[line.substr(0, s)] = util::read_word(is, "");
            }
        }

        if (current_profile.size() > 0)
            m_config_profiles[current_profile_name] = current_profile;
    }

    void read_config_files(const std::vector<std::string>& filenames)
    {
        for (const auto& s : filenames) {
            std::ifstream fs(s.c_str());
            if (fs)
                read_config_profiles(fs);
        }
    }

    // Read initial configuration from Caliper v1 style config files and/or environment
    void init_config_database()
    {
        // get config file name
        StringConverter cfg_file_names { find_config_value("config", "file", "caliper.config") };

        // read config files
        read_config_files(cfg_file_names.to_stringlist());

        // merge "default" profile into combined profile
        {
            for (auto& p : m_config_profiles["default"])
                m_combined_profile[p.first] = p.second;
        }

        // get profile name for Caliper v1 style config files
        StringConverter cfg_profile_names { find_config_value("config", "profile", "default") };

        // get the selected config profile names
        std::vector<std::string> profile_names = cfg_profile_names.to_stringlist();

        // merge all selected profiles
        for (const std::string& profile_name : profile_names) {
            auto it = m_config_profiles.find(profile_name);

            if (it == m_config_profiles.end()) {
                std::cerr << "caliper: error: config profile \"" << profile_name << "\" not defined." << std::endl;
                continue;
            }

            for (auto& p : it->second)
                m_combined_profile[p.first] = p.second;
        }

        // put the config settings in the database
        std::shared_ptr<ConfigSetImpl> config_cfg { new ConfigSetImpl };
        config_cfg->m_dict["file"] = cfg_file_names;
        config_cfg->m_dict["profile"] = cfg_profile_names;

        m_database.insert(std::make_pair("config", config_cfg));
    }

    // --- interface

    StringConverter get(const std::string& set, const std::string& key)
    {
        auto db_it = m_database.find(set);
        if (db_it != m_database.end()) {
            auto entry_it = db_it->second->m_dict.find(key);
            if (entry_it != db_it->second->m_dict.end())
                return entry_it->second;
        }

        if (m_database.empty())
            init_config_database();

        StringConverter ret(find_config_value(set, key));

        if (db_it != m_database.end())
            db_it->second->m_dict[key] = ret;
        else {
            std::shared_ptr<ConfigSetImpl> sptr { new ConfigSetImpl };
            sptr->m_dict[key] = ret;
            m_database[set] = sptr;
        }

        m_database[set]->m_dict[key] = ret;
        return ret;
    }

    std::shared_ptr<ConfigSetImpl> from_spec(const char* json_spec, const char* set_name_p)
    {
        if (m_database.empty())
            init_config_database();

        std::shared_ptr<ConfigSetImpl> ret { new ConfigSetImpl };

        auto dict = StringConverter(json_spec).rec_dict();

        std::string set_name;
        if (set_name_p)
            set_name = set_name_p;
        else {
            auto spec_itr = dict.find("name");
            if (spec_itr != dict.end())
                set_name = spec_itr->second.to_string();
        }

        assert(!set_name.empty() && "RuntimeConfig::from_spec(): config set name missing");

        auto spec_itr = dict.find("config");
        if (spec_itr != dict.end()) {
            for (const auto& e : spec_itr->second.rec_list()) {
                auto cfg_dict = e.rec_dict();

                std::string key, val;
                auto itr = cfg_dict.find("name");
                assert(itr != cfg_dict.end() && "RuntimeConfig::from_spec(): config entry name missing");
                key = itr->second.to_string();
                itr = cfg_dict.find("value");
                if (itr != cfg_dict.end())
                    val = itr->second.to_string();

                ret->m_dict.emplace(key, StringConverter(find_config_value(set_name, key, val)));
            }
        }

        m_database[set_name] = ret;

        return ret;
    }

    void print(std::ostream& os) const
    {
        for (auto set : m_database)
            for (auto entry : set.second->m_dict)
                os << ::config_var_name(set.first, entry.first) << '=' << entry.second.to_string() << std::endl;
    }
};

} // namespace cali

//
// --- ConfigSet public interface
//

StringConverter ConfigSet::get(const char* key) const
{
    if (!mP)
        return StringConverter();

    return mP->get(key);
}

//
// --- RuntimeConfig public interface
//

RuntimeConfig::RuntimeConfig() : mP(new RuntimeConfigImpl)
{}

StringConverter RuntimeConfig::get(const char* set, const char* key)
{
    return mP->get(set, key);
}

ConfigSet RuntimeConfig::from_spec(const char* json_spec, const char* set_name)
{
    return ConfigSet(mP->from_spec(json_spec, set_name));
}

void RuntimeConfig::preset(const char* key, const std::string& value)
{
    mP->m_combined_profile[key] = value;
}

void RuntimeConfig::set(const char* key, const std::string& value)
{
    mP->m_top_profile[key] = value;
}

void RuntimeConfig::import(const std::map<std::string, std::string>& values)
{
    for (auto& p : values)
        mP->m_top_profile[p.first] = p.second;
}

void RuntimeConfig::print(std::ostream& os)
{
    mP->print(os);
}

bool RuntimeConfig::allow_read_env() const
{
    return mP->m_allow_read_env;
}

bool RuntimeConfig::allow_read_env(bool allow)
{
    mP->m_allow_read_env = allow;
    return mP->m_allow_read_env;
}

//
// static interface
//

RuntimeConfig RuntimeConfig::get_default_config()
{
    static RuntimeConfig s_default_config;
    return s_default_config;
}
