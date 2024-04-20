
idf_component_register(
    SRC_DIRS "src/"
        "src/core" "src/core/utility" "src/core/wrapper" "src/core/wrapper/interface"
        "src/math" "src/math/minimizer" "src/math/path_loss"
        "src/master" "src/master/http" "src/master/http/api"
        "src/scanner"
        "src/tag"
    INCLUDE_DIRS "include"
    REQUIRES bt nvs_flash esp_event esp_wifi esp_http_server
)
