
idf_component_register(
    SRC_DIRS "src/"
        "src/core" "src/core/utility" "src/core/wrapper" "src/core/wrapper/interface"
        "src/master" "src/master/http" "src/master/http/api"
        "src/math" "src/math/minimizer" "src/math/path_loss"
        "src/scanner"
    INCLUDE_DIRS "include"
    REQUIRES bt nvs_flash esp_event esp_wifi esp_http_server
)
