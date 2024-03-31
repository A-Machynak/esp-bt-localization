#pragma once

#include "scanner/scanner_cfg.h"
#include <memory>

namespace Scanner::Impl
{
class App;
}  // namespace Scanner::Impl

namespace Scanner
{
/// @brief Scanner application
class App
{
public:
	App();
	~App();

	/// @brief Initialize with config.
	/// @param cfg config
	void Init(const AppConfig & cfg);

private:
	/// @brief Scanner application implementation
	Impl::App * _impl;  // can't use unique_ptr; it's not implemented properly
};

}  // namespace Scanner
