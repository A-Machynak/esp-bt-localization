
if( (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/certificates/ca.pem) AND
    (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/certificates/client.crt) AND
    (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/certificates/client.key))
    set(CERTIFICATE_FILES
        certificates/ca.pem
        certificates/client.crt
        certificates/client.key)
else()
    set(CERTIFICATE_FILES "")
endif()

idf_component_register(
    SRC_DIRS "src/"
        "src/core" "src/core/utility" "src/core/wrapper" "src/core/wrapper/interface"
        "src/math" "src/math/minimizer" "src/math/path_loss"
        "src/master" "src/master/memory" "src/master/http" "src/master/http/api"
        "src/scanner"
        "src/tag"
    INCLUDE_DIRS "include"
    REQUIRES bt nvs_flash esp_event esp_wifi esp_http_server wpa_supplicant
    EMBED_TXTFILES ${CERTIFICATE_FILES}
)
