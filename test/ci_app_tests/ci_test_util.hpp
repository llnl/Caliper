#ifndef CALI_CI_TEST_UTIL_HPP
#define CALI_CI_TEST_UTIL_HPP

#include <map>
#include <string>
#include <vector>

/// \brief Find \a test_target (sub)dict contents in json records \a str. Returns number of targets NOT found.
int find_targets_in_json_output(const std::vector<std::map<std::string,std::string>>& test_targets, const std::string& str);

/// \brief Run CalQL query \a query on .cali input \a in and return result
std::string run_query(const std::string& in, const char* query);

#endif