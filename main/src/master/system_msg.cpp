#include "master/system_msg.h"

#include "esp_log.h"
#include "esp_system.h"

namespace Master
{

void ProcessSystemMessage(HttpApi::Type::SystemMsg::Operation op)
{
	using Op = HttpApi::Type::SystemMsg::Operation;

	static const char * TAG = "HttpSysOp";

	if (op == Op::Restart) {
		ESP_LOGI(TAG, "Reset from HTTP");
		esp_restart();
	}
}
}  // namespace Master
