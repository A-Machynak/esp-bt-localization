#include <esp_log.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

// clang-format off
#if defined(CONFIG_MASTER)
	#define MAIN_TAG "Master"
	#include "master/master.h"
	#include "cfg_master.h"
	static Master::App app(Cfg);
#elif defined(CONFIG_SCANNER)
	#define MAIN_TAG "Scanner"
	#include "scanner/scanner.h"
	#include "cfg_scanner.h"
	static Scanner::App app(Cfg);
#elif defined(CONFIG_TAG)
	#define MAIN_TAG "Tag"
	#include "tag/tag.h"
	#include "cfg_tag.h"
	static Tag::App app(Cfg);
#else
	#error "'MASTER', 'SCANNER' or 'TAG' isn't defined"
#endif

extern "C" void app_main(void)
{
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	app.Init();
}
// clang-format on
