#pragma once

#include "master/http/api/post_data.h"

namespace Master
{

/// @brief Process a "system message" from HTTP POST request.
/// @param op operation
void ProcessSystemMessage(HttpApi::Type::SystemMsg::Operation op);

}  // namespace Master
