#include "ota.h"

#ifdef OTA_DEBUG
    static const char *DESC= "OTA update";
#endif

/* Validating incoming fw image
 * args esp_app_desc_t of the incoming FW
 * returns esp_err_t
 * 
 * Fails if new FW version is the same as current FW OR
 * if secure version verification fails, that is, 
 * if new FW secure_version is lower than programmed eFuse
 */
esp_err_t img_header_val(esp_app_desc_t *new_fw){       
    if(new_fw == NULL)  //invalid image
        return ESP_ERR_INVALID_ARG;

    const esp_partition_t *run_part = esp_ota_get_running_partition();              //currently used partition
    esp_app_desc_t run_fw;
    if(esp_ota_get_partition_description(run_part,&run_fw) == ESP_OK){              //Load description header - current fw
        #ifdef OTA_DEBUG
            ESP_LOGI(DESC, "Current fw version: %s",run_fw.version);                //debug output
        #endif
    } else {
        #ifdef OTA_DEBUG
            ESP_LOGE(DESC, "FW version not available");                //debug output
        #endif
        return ESP_ERR_NOT_SUPPORTED;
    }

    if(memcmp(new_fw->version, run_fw.version, sizeof(new_fw->version)) == 0) {     //if we've received same fw, do nothing
        #ifdef OTA_DEBUG
            ESP_LOGW(DESC, "Current fw version == new fw version. Update not needed!");
        #endif
        return ESP_ERR_NOT_SUPPORTED;
    }

    #ifdef ANTI_ROLLBACK
        const uint32_t hw_sec_ver = esp_efuse_read_secure_version();
        if(new_fw->secure_version < hw_sec_ver) {                                   //Secure check failed
            #ifdef OTA_DEBUG
                ESP_LOGW(DESC,"New fw security version is less than efuse, %d < %d", new_fw->secure_version,hw_sec_ver);
                return ESP_FAIL;
            #endif
        }
    #endif

    return ESP_OK;
}


/* HTTP request config
 *
 *
esp_err_t http_client_init(esp_http_client_handle_t client,char* _jwt){
    esp_err_t err = ESP_OK;
    char *buffer = (char*)malloc(sizeof(_jwt)+10);
    sprintf(buffer,"Bearer %s",_jwt);
    err= esp_http_client_set_header(client, "Authorization",buffer);
    free(buffer);
    return err;
}*/

static esp_err_t setHdr(esp_http_client_handle_t h){
	return esp_http_client_set_header(h, "PRIVATE-TOKEN", "glpat-wqa82dLYn3iRJMN-TsFZ");
}

/* Main function for incoming OTA update handling
 * ARGS - ota_config_t struct - url and auth parameters
 * RETURNS esp_err_t
 */
esp_err_t process_ota(ota_config_t* param){
    #ifdef OTA_DEBUG
        ESP_LOGI(DESC, "OTA processing started");
    #endif


    esp_err_t finish_err = ESP_OK;

    esp_http_client_config_t http_conf = {
        .url = param->url,                      //URL where the firmware is located
        .cert_pem = param->CA_root,             //CA root cert for TLS/SSL verification
        .timeout_ms = param->timeout_ms,        //basic timeout
    };

    esp_https_ota_config_t ota_conf = {
        .http_config = &http_conf,          //pass parameters
		.http_client_init_cb = setHdr,
    };

    esp_https_ota_handle_t https_ota_h = NULL;      //initialize handle for ota
    
    esp_err_t err = esp_https_ota_begin(&ota_conf, &https_ota_h);
    if(err != ESP_OK){                              //establish HTTPS connection to server - blocking
        #ifdef OTA_DEBUG
            ESP_LOGE(DESC, "establishing HTTPS connection failed.");
        #endif
        return err;                                 //task failed - self destruct
    }

    esp_app_desc_t new_fw_desc;                     //fw image decription
    err = esp_https_ota_get_img_desc(https_ota_h, &new_fw_desc);    //populate description struct
    if(err != ESP_OK){    //load image description
        #ifdef OTA_DEBUG
            ESP_LOGE(DESC, "Reading new FW description failed.");
        #endif
        goto ota_end;                               //task failed - cleanup
    }


    err = img_header_val(&new_fw_desc);
    if(err != ESP_OK){  
        if(err!=ESP_ERR_NOT_SUPPORTED)                            //FW version/rollback protect. validation
        {
        #ifdef OTA_DEBUG
            ESP_LOGE(DESC, "FW verification failed");
        #endif
        }
        goto ota_end;                               //task failed - cleanup
    }

    while(1){
        err = esp_https_ota_perform(https_ota_h);
        if(err != ESP_ERR_HTTPS_OTA_IN_PROGRESS){    //receive new fw from stream and write it to free OTA partition
            break;
        }
        #ifdef OTA_DEBUG
            ESP_LOGD(DESC, "Bytes read: %d", esp_https_ota_get_image_len_read(https_ota_h));
        #endif
    }

    if(!esp_https_ota_is_complete_data_received(https_ota_h)){    //if receiving was interrupted - cleanup
        #ifdef OTA_DEBUG
            ESP_LOGE(DESC,"Received data wasnt complete.");
        #endif
    } else {
        finish_err = esp_https_ota_finish(https_ota_h);         //Close HTTPS, clean-up and mark new image to be booted after restart
        if((err==ESP_OK) && (finish_err == ESP_OK)){
            #ifdef OTA_DEBUG
                ESP_LOGI(DESC,"OTA successful. Rebooting");
            #endif
            sleep(1);                                          //wait 1s
            esp_restart();
        } else {
            if(finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {     //if the image is corrupted, clean-up and selfdestruct
                #ifdef OTA_DEBUG
                    ESP_LOGE(DESC, "Image validation failed, maybe corrupted?");
                #endif
            }
            #ifdef OTA_DEBUG
                ESP_LOGE(DESC, "OTA update failed 0x%x", finish_err);   //defined error
            #endif
            return finish_err;
        }
    }

ota_end:
    //esp_https_ota_abort(https_ota_h);
    #ifdef OTA_DEBUG
        ESP_LOGW(DESC, "OTA upgrade aborted");                           //undefined error
    #endif
    return ESP_FAIL;
}


/** esp_err_ t mark_image_valid()
 *  U should check if connection is working before calling this,
 *  to ensure that device can receive OTA updates in the future
 *  If there are critical errors before this function, you can call
 *  esp_ota_mark_app_invalid_rollback_and_reboot()
 *  which marks new update as invalid, and rolls back to old firmware.
 * 
 *  Also unexpected reset or brownout marks new firmware as invalid 
 *  and it will roll back to old fw.
 */
esp_err_t mark_image_valid() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {                          //if the fw is new, and we need to run selftests
            if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {           //no selftest are run here, were just marking it as OK, but u can add some
                #ifdef OTA_DEBUG
                    ESP_LOGI(DESC, "App is valid, rollback cancelled successfully");
                #endif
                return ESP_OK;
            } else {
                #ifdef OTA_DEBUG
                    ESP_LOGE(DESC, "Failed to cancel rollback");
                #endif
                return ESP_FAIL;
            }
        }
    }
    return ESP_FAIL;
}