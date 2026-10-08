#include "ci_test_util.hpp"

#include <caliper/common/OutputStream.h>

#include <caliper/reader/CaliperMetadataDB.h>
#include <caliper/reader/CaliReader.h>
#include <caliper/reader/CalQLParser.h>
#include <caliper/reader/QueryProcessor.h>

#include "../../src/common/StringConverter.h"

#include <iostream>
#include <sstream>

using namespace cali;

int find_targets_in_json_output(const std::vector<std::map<std::string,std::string>>& test_targets, const std::string& str)
{
    std::vector<std::map<std::string, StringConverter>> records;

    auto array_of_dict_strings = StringConverter(str).rec_list();
    for (const auto &str : array_of_dict_strings)
        records.emplace_back(str.rec_dict());

    int ret = test_targets.size();

    for (auto &target : test_targets) {
        bool found = false;
        for (const auto &rec : records) {
            //   Each test target is a string:string map. Check if all elements of the test target
            // are in this record.
            auto t_it = target.begin();
            for ( ; t_it != target.end(); ++t_it) {
                auto rec_it = rec.find(t_it->first);
                if (rec_it == rec.end())
                    break;
                if (t_it->second == ">0") {
                    // convert record value to double and check if it is > 0
                    if (!(rec_it->second.to_double() > 0))
                        break;
                } else if (!t_it->second.empty() && t_it->second != rec_it->second.to_string()) {
                    break;
                }
            }

            if (t_it == target.end()) {
                found = true;
                break;
            }
        }

        if (found) {
            --ret;
        } else {
            std::cerr << "\n  ERROR: Test target { ";
            int count = 0;
            for (const auto &kv : target)
                std::cerr << (count++ > 0 ? ", \"" : "\"") << kv.first << "\":\"" << kv.second << "\"";
            std::cerr << " } not found" << std::endl;
        }
    }

    return ret;
}

void node_proc_noop(CaliperMetadataAccessInterface&,const Node*) {}

std::string run_query(const std::string& in, const char* query)
{
    CalQLParser parser(query);
    if (parser.error()) {
        std::cerr << "ci_test_util: run_query(): parse error: " << parser.error_msg() << std::endl;
        return "";
    }

    std::istringstream is(in);
    std::ostringstream os;

    OutputStream cali_str;
    cali_str.set_stream(&os);

    QueryProcessor proc(parser.spec(), cali_str);

    CaliperMetadataDB db;
    CaliReader reader;
    reader.read(is, db, node_proc_noop, proc);

    return os.str();
}
