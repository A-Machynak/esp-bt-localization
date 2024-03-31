#pragma once

#include "master/master_cfg.h"
#include <memory>

namespace Master::Impl
{
class App;
}  // namespace Master::Impl

namespace Master
{
/// @brief Master application
class App
{
public:
	App();
	~App();

	/// @brief Initialize with config.
	/// @param cfg config
	void Init(const AppConfig & cfg);

private:
	/// @brief Master application implementation
	Impl::App * _impl;  // can't use unique_ptr; it's not implemented properly
};

}  // namespace Master
