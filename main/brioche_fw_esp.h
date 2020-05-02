//
//  brioche_fw_esp.h
//  
//
//  Created by Nicolas Machado on 2/8/19.
//

#ifndef brioche_fw_esp_h
#define brioche_fw_esp_h

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "mdns.h"

#define xstr(s) str(s)
#define str(s) #s
#define TGT_TEAM 3309

 #define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


#endif /* brioche_fw_esp_h */
