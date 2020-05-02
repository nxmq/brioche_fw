//
//  brioche_fw_esp_main.c
//
//
//  Created by Nicolas Machado on 2/8/19.
//
#include "dscommpacket.h"

DataStore store;

extern void RobotInit();
extern void TestInit();
extern void TeleopInit();
extern void DisabledInit();
extern void AutonomousInit();
extern void TestPeriodic();
extern void TeleopPeriodic();
extern void DisabledPeriodic();
extern void AutonomousPeriodic();
static void udp_server_task(void *pvParameters);
static void tcp_server_task(void *pvParameters);
static void user_code_task(void *pvParameters);

static const char* TAG = "brioche-fw";

void app_main()
{
    store.m_frameSize = SIZE_MAX;
    store.dsConnected = false;
    ESP_LOGI(TAG,"starting up a fake roboRIO for team %d!",TGT_TEAM);
    //init nonvolatile flash
    ESP_ERROR_CHECK( nvs_flash_init() );
    //init tcpip stack
    ESP_ERROR_CHECK( esp_netif_init() );
    //init event loop
    ESP_ERROR_CHECK( esp_event_loop_create_default() );

    esp_netif_t * ap = esp_netif_create_default_wifi_ap();

    //initialize wifi hw to default state
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //store wifi hw config to ram
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    
    //tell hw to assume AP mode
    esp_wifi_set_mode(WIFI_MODE_AP);
    
    wifi_config_t ap_config = {
        .ap = {
            .ssid = xstr(TGT_TEAM),
            .channel = 0,
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 0,
            .max_connection = 1,
            .beacon_interval = 100
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    esp_netif_ip_info_t ipinfo;
    ipinfo.ip.addr = (10 << 0) + ((TGT_TEAM / 100) << 8) + ((TGT_TEAM % 100) << 16) + (2 << 24);
    ipinfo.netmask.addr = 0x00FFFFFF;
    ipinfo.gw.addr = ipinfo.ip.addr;

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap));
    dhcps_lease_t lease;
    lease.enable = true;
    lease.start_ip.addr = ipinfo.ip.addr + (1 << 24);
    lease.end_ip.addr = ipinfo.ip.addr + (9 << 24);

    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap, &ipinfo));

    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap,\
        (esp_netif_dhcp_option_mode_t)ESP_NETIF_OP_SET,\
        (esp_netif_dhcp_option_id_t)ESP_NETIF_REQUESTED_IP_ADDRESS,\
        (void*)&lease, sizeof(dhcps_lease_t)));

        //start dhcp server....
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap));

    ESP_ERROR_CHECK( mdns_init() );

    //set hostname
    mdns_hostname_set("roboRIO-"xstr(TGT_TEAM)"-frc");
    //set default instance
    mdns_instance_name_set("brioche Robot");

    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
    xTaskCreate(user_code_task, "user_code", 4096, NULL, 5, NULL);
}


static void user_code_task(void *pvParameters)
{
    while(1) {
        if(store.dsConnected == false) {

        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

static void tcp_server_task(void *pvParameters)
{
    uint8_t rx_buffer[128];

    
    while (1) {
        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(1740);

        int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (listen_sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");
        
        int err = bind(listen_sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket bound");
        
        err = listen(listen_sock, 1);
        if (err != 0) {
            ESP_LOGE(TAG, "Error occured during listen: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket listening");
        
        struct sockaddr_in sourceAddr;
        uint addrLen = sizeof(sourceAddr);
        int sock = accept(listen_sock, (struct sockaddr *)&sourceAddr, &addrLen);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket accepted");
        
        while (1) {
            int len;
next_iter:
            len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occured during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            }
            // Connection closed
            else if (len == 0) {
                ESP_LOGI(TAG, "Connection closed");
                break;
            }
            // Data received
            else {
                uint8_t* rx_data = rx_buffer;
                while (len > 0) {
                    if (store.m_frameSize == SIZE_MAX) {
                        if (store.m_frameLen < 2u) {
                            size_t toCopy = MIN(2u - store.m_frameLen, len);
                            memcpy(store.m_frameData+store.m_frameLen-1,rx_data,toCopy);
                            store.m_frameLen += toCopy;
                            rx_data += toCopy;
                            len -= toCopy;
                            if (store.m_frameLen < 2u) goto next_iter;  // need more data
                        }
                        store.m_frameSize = (((uint16_t)store.m_frameData[0]) << 8) | ((uint16_t)store.m_frameData[1]);
                    }
                    if (store.m_frameSize != SIZE_MAX) {
                        size_t need = store.m_frameSize - (store.m_frameLen - 2);
                        size_t toCopy = MIN(need, len);
                        memcpy(store.m_frameData+store.m_frameLen-1,rx_data,toCopy);
                        store.m_frameLen += toCopy;
                        rx_data += toCopy;
                        len -= toCopy;
                        need -= toCopy;
                        if (need == 0) {
                            DSCommPacket_DecodeTCP(&(store.dsPacket),store.m_frameData,128);
                            store.m_frameLen = 0;
                            memset(store.m_frameData,0x00,128);
                            store.m_frameSize = SIZE_MAX;
                        }
                    }
                }
            }
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
            store.dsConnected = false;
        }
    }
    vTaskDelete(NULL);
}

static void udp_server_task(void *pvParameters)
{
    uint8_t rx_buffer[128];
    uint8_t tx_buffer[8];
    char addr_str[128];

    while (1) {
        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(1110);

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        int err = bind(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(TAG, "Socket bound");
        store.dsConnected = true;
        while (1) {

            struct sockaddr_in sourceAddr;
            socklen_t socklen = sizeof(sourceAddr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&sourceAddr, &socklen);

            // Error occured during receiving
            if (len < 0)     {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                inet_ntoa_r(sourceAddr.sin_addr, addr_str, sizeof(addr_str) - 1);
                DSCommPacket_DecodeUDP(&(store.dsPacket),rx_buffer,len);
                struct sockaddr_in outAddr;
                memcpy(&outAddr, &sourceAddr, sizeof(outAddr)); 
                outAddr.sin_family = AF_INET;
                outAddr.sin_port = htons(1150);
                int respSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
                if (sock < 0) {
                    ESP_LOGE(TAG, "Unable to create udp response socket: errno %d", errno);
                    break;
                }
                DSCommPacket_SetupSendBuffer(&(store.dsPacket),tx_buffer);
                int err = sendto(respSock, tx_buffer, sizeof(tx_buffer), 0, (struct sockaddr *)&outAddr, sizeof(outAddr));
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
                    break;
                }
                shutdown(respSock, 0);
                close(respSock);
            }
         vTaskDelay(20 / portTICK_PERIOD_MS);
        }
        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
            store.dsConnected = false;
            store.dsPacket.m_control_word.enabled = 0;

        }
    }
    vTaskDelete(NULL);
}
