// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/reader/QuerySpec.h"

#include <sstream>

using namespace cali;

const cali::QuerySpec::FunctionSignature cali::QuerySpec::FunctionSignatureTerminator { -1, nullptr, -1, -1, nullptr };

namespace
{

std::ostream& format_condition(std::ostream& os, const QuerySpec::Condition& cond)
{
    switch (cond.op) {
    case QuerySpec::Condition::Op::Exist:
        os << cond.attr_name;
        break;
    case QuerySpec::Condition::Op::NotExist:
        os << " not " << cond.attr_name;
        break;
    case QuerySpec::Condition::Op::Equal:
        os << cond.attr_name << '=' << cond.value;
        break;
    case QuerySpec::Condition::Op::NotEqual:
        os << " not " << cond.attr_name << '=' << cond.value;
        break;
    case QuerySpec::Condition::Op::LessThan:
        os << cond.attr_name << '<' << cond.value;
        break;
    case QuerySpec::Condition::Op::GreaterThan:
        os << cond.attr_name << '>' << cond.value;
        break;
    case QuerySpec::Condition::Op::LessOrEqual:
        os << " not " << cond.attr_name << ">" << cond.value;
        break;
    case QuerySpec::Condition::Op::GreaterOrEqual:
        os << " not " << cond.attr_name << "<" << cond.value;
        break;
    default:
        break;
    }
    return os;
}

std::ostream& format_preprocess_op(std::ostream& os, const QuerySpec::PreprocessSpec& spec)
{
    os << spec.target << '=' << spec.op.op.name << '(';

    int count = 0;
    for (const std::string& arg : spec.op.args)
        os << (count++ == 0 ? "" : ", ") << arg;
    os << ')';

    if (spec.cond.op != QuerySpec::Condition::Op::None)
        format_condition(os << " if ", spec.cond);

    return os;
}

std::ostream& format_aggregation_op(std::ostream& os, const QuerySpec::AggregationOp& op)
{
    os << op.op.name << '(';
    int count = 0;
    for (const std::string& arg : op.args)
        os << (count == 0 ? "" : ", ") << arg;
    return os << ')';
}

} // namespace [anonymous]

namespace cali
{

std::ostream& operator<< (std::ostream& os, const QuerySpec& spec)
{
    if (!spec.preprocess_ops.empty()) {
        int count = 0;
        for (const QuerySpec::PreprocessSpec& op : spec.preprocess_ops)
            format_preprocess_op(os << (count++ == 0 ? "let " : ", "), op);
    }

    if (spec.aggregate.selection == QuerySpec::AggregationSelection::List) {
        int count = 0;
        for (const QuerySpec::AggregationOp& op : spec.aggregate.list)
            format_aggregation_op(os << (count++ == 0 ? " aggregate " : ", "), op);
    }

    if (spec.select.selection == QuerySpec::AttributeSelection::All) {
        os << " select *";
    } else if (spec.select.selection == QuerySpec::AttributeSelection::List) {
        int count = 0;
        if (spec.select.use_path) {
            os << " select path";
            ++count;
        }
        for (const std::string& s : spec.select.list) {
            os << (count++ == 0 ? " select " : ", ") << s;
            auto as_it = spec.aliases.find(s);
            if (as_it != spec.aliases.end())
                os << " as " << as_it->second;
            auto unit_it = spec.units.find(s);
            if (unit_it != spec.units.end())
                os << " unit " << unit_it->second;
        }
    }

    if (spec.groupby.selection == QuerySpec::AttributeSelection::List) {
        int count = 0;
        if (spec.groupby.use_path) {
            os << " group by path";
            ++count;
        }
        for (const std::string& s : spec.groupby.list)
            os << (count++ == 0 ? " group by " : ", ") << s;
    }

    if (spec.filter.selection == QuerySpec::FilterSelection::List) {
        int count = 0;
        for (const QuerySpec::Condition& cond : spec.filter.list)
            format_condition(os << (count++ == 0 ? " where " : ", "), cond);
    }

    if (spec.sort.selection == QuerySpec::SortSelection::List) {
        int count = 0;
        for (const QuerySpec::SortSpec& s : spec.sort.list) {
            os << (count++ == 0 ? " order by " : ", ") << s.attribute << (s.order == QuerySpec::SortSpec::Descending ? " DESC" : " ASC");
        }
    }

    if (spec.format.opt == QuerySpec::FormatSpec::User) {
        os << " format " << spec.format.formatter.name;
        if (!spec.format.kwargs.empty()) {
            int count = 0;
            for (const auto &p : spec.format.kwargs)
                os << (count++ == 0 ? "(" : ", ") << p.first << '=' << p.second;
            os << ')';
        }
    }

    return os;
}

} // namespace cali