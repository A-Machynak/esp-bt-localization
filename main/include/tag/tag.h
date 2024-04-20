#pragma once

#include "tag/tag_cfg.h"

namespace Tag::Impl
{
class App;
}  // namespace Tag::Impl

namespace Tag
{
/// @brief Tag application
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
	Impl::App * _impl;
};

}  // namespace Tag