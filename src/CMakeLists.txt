# ESP-IDF "main" komponens: platformréteg (esp/), ISOBUS alkalmazás (app/), vezérlőmag (core/)
file(GLOB SEEDER_SRCS
    "${CMAKE_CURRENT_LIST_DIR}/esp/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/app/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/core/*.cpp")
idf_component_register(
    SRCS ${SEEDER_SRCS}
    INCLUDE_DIRS "." "app" "esp" "../include"
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_server app_update
             driver esp_timer esp_partition esp_system esp_rom mbedtls)
# ISO 11783-6 object pool (bináris) és a webes felület (szöveg, lezáró \0-val)
target_add_binary_data(${COMPONENT_TARGET} "../generated/object_pool.iop" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../web/index.html" TEXT)
