#include "core/wrapper/interface/gatts_if.h"

#include "core/utility/mac.h"

#include <sstream>

std::string ToString(const Gatts::Type::Write & p)
{
	std::stringstream ss;
	// clang-format off
	ss << "{ "
		<< "\"conn_id\": " << p.conn_id
		<< ", \"trans_id\": " << p.trans_id
		<< ", \"bda\": " << ToString(Mac(p.bda))
		<< ", \"handle\": " << p.handle
		<< ", \"offset\": " << p.offset
		<< ", \"need_rsp\": " << p.need_rsp
		<< ", \"is_prep\": " << p.is_prep
		<< ", \"len\": " << p.len
		<< ", \"value\": \'";
	for (int i = 0; i < p.len; i++) {
		ss << (int)p.value[i] << ",";
	}
	ss << "\' }";
	// clang-format on
	return ss.str();
}
