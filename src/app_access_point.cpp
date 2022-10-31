#include <app_access_point.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_ota_ops.h>
#include <StringSplitter.h>
#include <ESPmDNS.h>

static const char *TAG = "AP";

// Command Response structure
struct CmdResponse
{
    boolean isHttp;
    boolean sendResponse;
    int statusCode;
    int responseCode;
    int cmdCode;
    String payload;
    int trackingId = -1;
    String cmd_str;
};

WiFiServer ap_server(AP_PORT);

esp_ota_handle_t ota_update_handle;
esp_err_t ota_update_err;
const esp_partition_t *update_partition = NULL;

const size_t MAX_FILESIZE = 1024 * 1024 * 5;

void AccessPoint::wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

void AccessPoint::Init()
{
    IPAddress local_ip(192, 168, 0, 1);
    IPAddress gateway(0, 0, 0, 0);
    IPAddress subnet(255, 255, 255, 240);

    if (!WiFi.softAPConfig(local_ip, gateway, subnet))
    {
        ESP_LOGE(TAG, "AP Config Failed");
    }
    else
    {
        ESP_LOGI(TAG, "AP Config Success");
    }

    String ESP_WIFI_MAC = WiFi.macAddress();
    ESP_WIFI_MAC.replace(":", "");
    String ESP_WIFI_SSID = "SplashMe_" + ESP_WIFI_MAC;
    String ESP_WIFI_PASS = "12345678";

    wifi_config_t wifi_config;

    // 1. setup ssid
    char c_ssid[32];
    ESP_WIFI_SSID.toCharArray(c_ssid, 31);
    for (int i = 0; i < 32; i++)
    {
        wifi_config.ap.ssid[i] = uint8_t(c_ssid[i]);
    }

    // 2. setup password
    char c_password[64];
    ESP_WIFI_PASS.toCharArray(c_password, 63);
    for (int i = 0; i < 64; i++)
    {
        wifi_config.ap.password[i] = uint8_t(c_password[i]);
    }

    ESP_LOGI(TAG, "Start AP mode with SSID: %s, password : %s", wifi_config.ap.ssid, wifi_config.ap.password);

    // 3. other settings
    wifi_config.ap.ssid_len = ESP_WIFI_SSID.length();
    wifi_config.ap.channel = 6;
    wifi_config.ap.max_connection = 8;
    wifi_config.ap.beacon_interval = 100;
    wifi_config.ap.ssid_hidden = 0;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK; // change auth mode to modify
    wifi_config.ap.ftm_responder = true;
    wifi_config.ap.pairwise_cipher = WIFI_CIPHER_TYPE_NONE;

    if (ESP_WIFI_PASS.length() == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    esp_err_t res = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP ERROR code: 0x%04x", res);
    }

    esp_err_t res_ps = esp_wifi_set_ps(WIFI_PS_NONE);
    if (res_ps != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP ERROR code: 0x%04x", res_ps);
    }

    esp_err_t res_start = esp_wifi_start();
    if (res_start != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP ERROR code: 0x%04x", res_ps);
    }

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d", wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);

    // if (!MDNS.begin("splashme"))
    // {
    //     ESP_LOGE(TAG, "DNS setup failed!");
    // }
    // else
    // {
    //     ESP_LOGI(TAG, "DNS setup successfully!");
    // }

    ap_server.begin();
}

void AccessPoint::Loop()
{
    WiFiClient client = ap_server.available();

    if (client)
    {
        // If a new client connects,
        bool content_start = false;
        bool ota_start = false;
        long content_length = 0;
        String content = "";
        uint16_t content_counter = 0;
        String cmd = "";
        uint8_t buff[2048] = {0};
        uint32_t len = 0;
        time_t startTimer;

        while (client.connected())
        {
            size_t size = client.available();

            if (size)
            {
                if (content_start)
                {
                    if (cmd == "frm")
                    {
                        if (!ota_start)
                        {
                            ESP_LOGI(TAG, "------ Starting Receive Firmware ------");
                            startTimer = millis();
                            const esp_partition_t *configured = esp_ota_get_boot_partition();
                            const esp_partition_t *running = esp_ota_get_running_partition();

                            if (configured != running)
                            {
                                ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x", configured->address, running->address);
                                ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
                            }
                            ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)", running->type, running->subtype, running->address);

                            update_partition = esp_ota_get_next_update_partition(NULL);
                            ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)", update_partition->type, update_partition->subtype, update_partition->address);

                            ESP_LOGI(TAG, "------ esp_ota_begin ------");
                            ota_update_err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_update_handle);
                            if (ota_update_err != ESP_OK)
                            {
                                ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(ota_update_err));
                                esp_ota_abort(ota_update_handle);
                                client.println("HTTP/1.1 200 OK");
                                client.println("Content-type:text/plain");
                                client.println("Connection: close");
                                client.println();
                                client.println(esp_err_to_name(ota_update_err));
                                client.println();
                                break;
                            }

                            ESP_LOGI(TAG, "------ esp_ota_begin succeeded ------");
                            ota_start = true;
                        }

                        int c = client.readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                        ota_update_err = esp_ota_write(ota_update_handle, buff, c);

                        if (ota_update_err != ESP_OK)
                        {
                            ESP_LOGE(TAG, "[%s] Error: esp_ota_write failed (%s)!", TAG, esp_err_to_name(ota_update_err));
                            esp_ota_abort(ota_update_handle);
                            client.println("HTTP/1.1 200 OK");
                            client.println("Content-type:text/plain");
                            client.println("Connection: close");
                            client.println();
                            client.println(esp_err_to_name(ota_update_err));
                            client.println();
                            break;
                        }

                        if (len > 0)
                        {
                            len -= c;
                            float precent = (len / (double)content_length) * 100;
                            ESP_LOGI(TAG, "Update progress %.2f", precent);
                        }

                        delay(1);

                        if (len == 0)
                        {
                            ESP_LOGI(TAG, "OTA Update Done. Received %i bytes in %.2fs which is %.2f kB/s.",
                                     content_length, (millis() - startTimer) / 1000.0,
                                     1.0 * (content_length) / (millis() - startTimer));

                            ota_update_err = esp_ota_end(ota_update_handle);

                            if (ota_update_err != ESP_OK)
                            {
                                if (ota_update_err == ESP_ERR_OTA_VALIDATE_FAILED)
                                {
                                    ESP_LOGE(TAG, "Image validation failed, image is corrupted");
                                    client.println("HTTP/1.1 200 OK");
                                    client.println("Content-type:text/plain");
                                    client.println("Connection: close");
                                    client.println();
                                    client.println("Image validation failed, image is corrupted");
                                    client.println();
                                    break;
                                }
                                else
                                {
                                    ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(ota_update_err));
                                    client.println("HTTP/1.1 200 OK");
                                    client.println("Content-type:text/plain");
                                    client.println("Connection: close");
                                    client.println();
                                    client.println(esp_err_to_name(ota_update_err));
                                    client.println();
                                    break;
                                }
                            }

                            ota_update_err = esp_ota_set_boot_partition(update_partition);
                            if (ota_update_err != ESP_OK)
                            {
                                ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(ota_update_err));
                                client.println("HTTP/1.1 200 OK");
                                client.println("Content-type:text/plain");
                                client.println("Connection: close");
                                client.println();
                                client.println(esp_err_to_name(ota_update_err));
                                client.println();
                                break;
                            }

                            ESP_LOGI(TAG, "Prepare to restart system!");
                            ESP_LOGI(TAG, "Update successfull! Ready to reboot after 1 seconds!");

                            client.println("HTTP/1.1 200 OK");
                            client.println("Content-type:application/json");
                            client.println("Connection: close");
                            client.println();
                            client.println("Success!");
                            client.println();

                            delay(3000);
                            ESP_LOGI(TAG, "Rebooting...");
                            esp_restart();
                        }
                    }
                    else
                    {
                        //  Read content
                        char c = client.read(); // read a byte, then
                        content += c;
                        content_counter++;

                        if (content_counter == content_length)
                        {
                            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                            // and a content-type so the client knows what's coming, then a blank line:
                            client.println("HTTP/1.1 200 OK");
                            client.println("Content-type:application/json");
                            client.println("Connection: close");
                            client.println();
                            client.println("Success!");
                            client.println();

                            // Break out of the while loop
                            break;
                        }
                    }
                }
                else
                {
                    // Read header
                    String incoming_line = client.readStringUntil('\n');

                    ESP_LOGI(TAG, "%s", incoming_line.c_str());

                    if (incoming_line == "\r")
                        content_start = true;

                    if (incoming_line.startsWith("POST"))
                    {
                        // Get URL
                        StringSplitter *s = new StringSplitter(incoming_line, ' ', 3); // new StringSplitter(string_to_split, delimiter, limit)
                        String url = s->getItemAtIndex(1);

                        StringSplitter *s_url = new StringSplitter(url, '/', 8); // new StringSplitter(string_to_split, delimiter, limit)

                        cmd = s_url->getItemAtIndex(3);

                        delete s_url;
                        delete s;

                        ESP_LOGI(TAG, "url : (%s) cmd : (%s)", url.c_str(), cmd.c_str());
                    }

                    if (incoming_line.startsWith("Content-Length"))
                    {
                        StringSplitter *s = new StringSplitter(incoming_line, ' ', 2);
                        content_length = s->getItemAtIndex(1).toInt();
                        delete s;

                        ESP_LOGI(TAG, "content_length : (%i)", content_length);
                        len = content_length;
                    }
                }
            }
        }

        // Close the connection
        client.stop();
    }
}

String AccessPoint::_HumanReadableSize(const size_t bytes)
{
    if (bytes < 1024)
        return String(bytes) + " B";
    else if (bytes < (1024 * 1024))
        return String(bytes / 1024.0) + " KB";
    else if (bytes < (1024 * 1024 * 1024))
        return String(bytes / 1024.0 / 1024.0) + " MB";
    else
        return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}
