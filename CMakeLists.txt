idf_component_register(SRCS "ota.c"
                    INCLUDE_DIRS include
                    REQUIRES esp_system app_update esp_http_client esp_https_ota
                    )