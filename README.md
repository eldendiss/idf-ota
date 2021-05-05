# idf-ota
OTA handling for ESP32

## Misc

HTTPS OTA WRAPPER

## Functions and parameters

*esp_err_t img_header_val(esp_app_desc_t *)* - this function validates incoming FW image header and compares it to the current one.  
If the versions are same OR secure_version of new FW is lower than efuse, function returns ESP_FAIL. 
(note: if the incoming version is lower, update will continue).  
Otherwise returns ESP_OK.  
**Pseudo private function, should not be called by user.**

*ota_config_t* - struct. Members are:  
- *url* - URL from where should be the new image pulled.  
- *CA_root* - root cert pem as string, for server verification.  
- *timeout_ms* - request timeout in ms.  
note: this needs to be configured before calling process_ota();


*esp_err_t process_ota(ota_config_t*)* - this function will perform OTA update, based on parameters configured in ota_config_t struct. After successfull update, ESP will be rebooted. New image should be marked as valid after self tests using:

*esp_err_t mark_image_valid()* - Marks new image as bootable and valid.  
U should check if connection is working during self tests, to avoid "bricking" the device.  
If there are critical errors before this function, you can call
*esp_ota_mark_app_invalid_rollback_and_reboot()*
which marks new update as invalid, and rolls back to old firmware.  
Unexpected reset or brownout also marks new FW as invalid, so u should make the selftest as soon as the device is booted.  
Returns ESP_OK if image was marked as valid sucessfully, or ESP_FAIL if there was any error.  
