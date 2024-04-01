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
	/// @brief Constructor.
	/// @param cfg configuration
	App(const AppConfig & cfg);
	~App();

	/// @brief Initialize.
	void Init();

private:
	/// @brief Master application implementation
	Impl::App * _impl;  // can't use unique_ptr; it's not implemented properly
};

}  // namespace Master
