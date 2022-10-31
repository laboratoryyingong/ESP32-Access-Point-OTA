// void AccessPoint::Start()
// {
// IPAddress local_ip(192, 168, 4, 1);
// IPAddress gateway(192, 168, 4, 1);
// IPAddress subnet(255, 255, 255, 0);

// if (!WiFi.softAPConfig(local_ip, gateway, subnet))
// {
//     ESP_LOGE(TAG, "AP Config Failed");
// }
// else
// {
//     ESP_LOGI(TAG, "AP Config Success");
// }

// // ESP_ERROR_CHECK(esp_netif_init());
// // ESP_ERROR_CHECK(esp_event_loop_create_default());
// // esp_netif_create_default_wifi_ap();

// // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
// // ESP_ERROR_CHECK(esp_wifi_init(&cfg));

// // ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
// //                                                     ESP_EVENT_ANY_ID,
// //                                                     &AccessPoint::wifi_event_handler,
// //                                                     NULL,
// //                                                     NULL));

// String ESP_WIFI_MAC = WiFi.macAddress();
// ESP_WIFI_MAC.replace(":", "");
// String ESP_WIFI_SSID = "SplashMe_" + ESP_WIFI_MAC;
// String ESP_WIFI_PASS = "12345678";

// wifi_config_t wifi_config;

// // 1. setup ssid
// char c_ssid[32];
// ESP_WIFI_SSID.toCharArray(c_ssid, 31);
// for (int i = 0; i < 32; i++)
// {
//     wifi_config.ap.ssid[i] = uint8_t(c_ssid[i]);
// }

// // 2. setup password
// char c_password[64];
// ESP_WIFI_PASS.toCharArray(c_password, 63);
// for (int i = 0; i < 64; i++)
// {
//     wifi_config.ap.password[i] = uint8_t(c_password[i]);
// }

// ESP_LOGI(TAG, "Start AP mode with SSID: %s, password : %s", wifi_config.ap.ssid, wifi_config.ap.password);

// // 3. other settings
// wifi_config.ap.ssid_len = ESP_WIFI_SSID.length();
// wifi_config.ap.channel = 13;
// wifi_config.ap.max_connection = 4;
// wifi_config.ap.beacon_interval = 100 * 4;
// wifi_config.ap.ssid_hidden = 0;
// wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK; // change auth mode to modify
// wifi_config.ap.ftm_responder = false;
// wifi_config.ap.pairwise_cipher = WIFI_CIPHER_TYPE_NONE;

// if (ESP_WIFI_PASS.length() == 0)
// {
//     wifi_config.ap.authmode = WIFI_AUTH_OPEN;
// }

// // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
// ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
// ESP_ERROR_CHECK(esp_wifi_start());

// ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d", wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);

// server.begin();

// // AccessPoint::StartLocalWebServer();
// }

// void AccessPoint::Stop()
// {
//     ESP_LOGI(TAG, "%s Close AP mode ......", TAG);
//     WiFi.softAPdisconnect();
// }

// void AccessPoint::StartLocalWebServer()
// {
//     ESP_LOGI(TAG, "Start Local Web Server, the IP address: %s ", WiFi.softAPIP().toString());

//     server.onNotFound(AccessPoint::NotFound);

//     server.on("/welcome", HTTP_GET, [](AsyncWebServerRequest *request)
//               { request->send(200, "text/plain", "Hello, SplashMe!"); });

//     server.on("/api", HTTP_POST, APIHandler, APIFileHandler, APIDataHandler);

//     server.onNotFound(NotFound);

//     server.begin();
// }

// void AccessPoint::APIHandler(AsyncWebServerRequest *request)
// {
//     if (request->method() != HTTP_POST)
//     {
//         request->send(405); // 405 Method Not Allowed
//     }
// }

// void AccessPoint::APIFileHandler(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
// {
//     if (!filename.equals("firmware.bin"))
//         request->send(200, "text/plain", "The file is not firmware");

//     static time_t startTimer;

//     if (!index)
//     {
//         startTimer = millis();
//         const char *fileSizeHeader{"FileSize"};
//         ESP_LOGD(TAG, "%s UPLOAD: %s starting upload\n", TAG, filename.c_str());
//         if (request->hasHeader(fileSizeHeader))
//         {
//             ESP_LOGD(TAG, "%s UPLOAD: fileSize:%s\n", TAG, request->header(fileSizeHeader));
//             if (request->header(fileSizeHeader).toInt() > MAX_FILESIZE)
//             {
//                 char content[70];
//                 snprintf(content, sizeof(content), "File too large. (%s)<br>Max upload size is %s", AccessPoint::_HumanReadableSize(request->header(fileSizeHeader).toInt()), AccessPoint::_HumanReadableSize(MAX_FILESIZE));
//                 request->send(400, "text/plain", content);
//                 request->client()->close();
//                 ESP_LOGD(TAG, "%s UPLOAD: Aborted upload of '%s' because file too big.\n", TAG, filename.c_str());
//             }
//         }

//         ESP_LOGI(TAG, "%s ------ Starting OTA Update ------", TAG);
//         const esp_partition_t *configured = esp_ota_get_boot_partition();
//         const esp_partition_t *running = esp_ota_get_running_partition();

//         if (configured != running)
//         {
//             ESP_LOGW(TAG, "%s Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x", TAG,
//                      configured->address, running->address);
//             ESP_LOGW(TAG, "%s (This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)", TAG);
//         }
//         ESP_LOGI(TAG, "%s Running partition type %d subtype %d (offset 0x%08x)", TAG,
//                  running->type, running->subtype, running->address);

//         update_partition = esp_ota_get_next_update_partition(NULL);
//         ESP_LOGI(TAG, "%s Running partition type %d subtype %d (offset 0x%08x)", TAG,
//                  update_partition->type, update_partition->subtype, update_partition->address);

//         ESP_LOGI(TAG, "%s esp_ota_begin", TAG);
//         err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
//         if (err != ESP_OK)
//         {
//             ESP_LOGE(TAG, "%s esp_ota_begin failed (%s)", TAG, esp_err_to_name(err));
//             esp_ota_abort(update_handle);
//             request->send(200, "text/plain", esp_err_to_name(err));
//         }

//         ESP_LOGI(TAG, "%s esp_ota_begin succeeded", TAG);
//     }

//     // ESP_LOGD(TAG, "%s file: '%s' received %i bytes\ttotal: %i\n", TAG, filename.c_str(), len, index + len);
//     err = esp_ota_write(update_handle, data, len);

//     if (err != ESP_OK)
//     {
//         ESP_LOGE(TAG, "%s Error: esp_ota_write failed (%s)!", TAG, esp_err_to_name(err));
//         esp_ota_abort(update_handle);
//         request->send(200, "text/plain", esp_err_to_name(err));
//     }

//     if (final)
//     {
//         ESP_LOGD(TAG, "%s UPLOAD: %s Done. Received %i bytes in %.2fs which is %.2f kB/s.", TAG, filename.c_str(),
//                  index + len,
//                  (millis() - startTimer) / 1000.0,
//                  1.0 * (index + len) / (millis() - startTimer));

//         err = esp_ota_end(update_handle);

//         if (err != ESP_OK)
//         {
//             if (err == ESP_ERR_OTA_VALIDATE_FAILED)
//             {
//                 ESP_LOGE(TAG, "%s Image validation failed, image is corrupted", TAG);
//                 request->send(200, "text/plain", "Image validation failed, image is corrupted");
//             }
//             else
//             {
//                 ESP_LOGE(TAG, "%s esp_ota_end failed (%s)!", TAG, esp_err_to_name(err));
//                 request->send(200, "text/plain", esp_err_to_name(err));
//             }
//         }

//         err = esp_ota_set_boot_partition(update_partition);
//         if (err != ESP_OK)
//         {
//             ESP_LOGE(TAG, "%s esp_ota_set_boot_partition failed (%s)!", TAG, esp_err_to_name(err));
//             request->send(200, "text/plain", esp_err_to_name(err));
//         }
//         ESP_LOGI(TAG, "%s Prepare to restart system!", TAG);
//         ESP_LOGI(TAG, "%S Update successfull! Ready to reboot after 1 seconds!", TAG);

//         request->send(200, "text/plain", "Update Successfully!");
//         delay(2000);
//         esp_restart();
//     }
// }

// void AccessPoint::APIDataHandler(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
// {
//     StringSplitter *splitter = new StringSplitter(request->url(), '/', 8); // new StringSplitter(string_to_split, delimiter, limit)

//     String item = splitter->getItemAtIndex(3); // catch the fourth item
//     String cmd = splitter->getItemAtIndex(2);

//     ESP_LOGI(TAG, "request url : %s ", request->url().c_str());

//     if (cmd == "cmd")
//     {
//         // get command code from dict
//         uint8_t cmdCode = GetCommandCodeFromDict(item.c_str());
//         ESP_LOGI(TAG, "%s new method retrieve command Code is : %d ", TAG, cmdCode);

//         // Send command and data to process
//         CmdResponse cmdResponse;
//         cmdResponse.isHttp = true;
//         cmdResponse.sendResponse = true;
//         cmdResponse.cmdCode = cmdCode;
//         cmdResponse.responseCode = -1;

//         SplashMeComms::ProcessCommand(cmdResponse, data, len, request);

//         if (cmdResponse.sendResponse)
//         {
//             ESP_LOGI(TAG, "Payload is %s", cmdResponse.payload.c_str());
//             request->send(200, "application/json", cmdResponse.payload);
//         }
//     }
//     else
//     {
//         request->send(404);
//     }

//     delete splitter; // Important: remove from heap
// }