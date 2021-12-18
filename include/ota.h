#pragma once

#define OTA_DEBUG               //uncomment to enable debug logging
//#define ANTI_ROLLBACK       //uncomment to enable Secure ver check

#ifndef OTA_H
#define OTA_H

#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#ifdef ANTI_ROLLBACK
    #include "esp_efuse.h"
#endif
#ifdef OTA_DEBUG
    #include "esp_log.h"
#endif

typedef struct{
    char*           url;        //url where the fw image is located, e.g. https://bla.com/fw.bin
    const char*     CA_root;    //CA root cer, pem as string
    //uint8_t       auth_type;  //idk
    //char*         jwt;        //idk if this is needed
    uint32_t        timeout_ms; //rq timeout in ms    
} ota_config_t;



esp_err_t img_header_val(esp_app_desc_t *);
esp_err_t process_ota(ota_config_t*);
esp_err_t mark_image_valid();
#endif