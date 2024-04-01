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
	/// @brief Constructor.
	/// @param cfg configuration
	App(const AppConfig & cfg);
	~App();

	/// @brief Initialize.
	void Init();

private:
	/// @brief Scanner application implementation
	Impl::App * _impl;  // can't use unique_ptr; it's not implemented properly
};

}  // namespace Scanner
