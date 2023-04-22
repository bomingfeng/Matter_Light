/* LwIP SNTP example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "sntp_task.h"
#include "lwip/apps/sntp.h"
#include "esp_sntp.h"

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

static void obtain_time(void);
static void initialize_sntp(void);

static uint8_t startHour = 21;
static uint8_t endHour = 7;
static struct tm starttimeinfo;
static struct tm endtimeinfo;

static bool isInTime(uint8_t now)
{
	// 过夜
	if(endHour < startHour) 
	{  
		if(now >= endHour && now < startHour) 
		{  
			return false;  //不在时间段内;
		} 
		else 
		{  
			return true;  //在时间段内;
		}  
	}   
	// 当天
	else 
	{  
		if(now >= startHour && now < endHour) 
		{  
			return true;  //在时间段内;
		} 
		else 
		{  
			return false;  //不在时间段内;
		}  
	}  	
}

static bool isInTmTime(struct tm timeinfo)
{
	// 过夜
	if((endtimeinfo.tm_sec < starttimeinfo.tm_sec) && (endtimeinfo.tm_min < starttimeinfo.tm_min) && (endtimeinfo.tm_hour < starttimeinfo.tm_hour)) 
	{  
		if((timeinfo.tm_sec >= endtimeinfo.tm_sec) && (timeinfo.tm_min >= endtimeinfo.tm_min) && (timeinfo.tm_hour >= endtimeinfo.tm_hour) &&   \
            (timeinfo.tm_sec < starttimeinfo.tm_sec) && (timeinfo.tm_min < starttimeinfo.tm_min) && (timeinfo.tm_hour < starttimeinfo.tm_hour)) 
		{  
			return false;  //不在时间段内;
		} 
		else 
		{  
			return true;  //在时间段内;
		} 
	}   
	// 当天
	else 
	{  
		if((timeinfo.tm_sec >= starttimeinfo.tm_sec) && (timeinfo.tm_min >= starttimeinfo.tm_min) && (timeinfo.tm_hour >= starttimeinfo.tm_hour) && \
            (timeinfo.tm_sec < endtimeinfo.tm_sec) && (timeinfo.tm_min < endtimeinfo.tm_min) && (timeinfo.tm_hour < endtimeinfo.tm_hour)) 
		{  
			return true;  //在时间段内;
		} 
		else 
		{  
			return false;  //不在时间段内;
		}  
	}  	
}

void sntp_task(void *pvParam)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    xEventGroupSetBits(APP_event_group,APP_event_Entrance_Guard_Enable_BIT);
    xEventGroupWaitBits(APP_event_group, \
                APP_event_WIFI_STA_CONNECTED_BIT, \
                pdFALSE,                               \
                pdFALSE,                               \
                portMAX_DELAY);

    obtain_time();

    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) {
        ESP_LOGI("sntp","Waiting for system time to be set...\r\n");
        vTaskDelay(5000 / portTICK_PERIOD_MS);

    }

    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    xEventGroupSetBits(APP_event_group,APP_event_SNTP_ok_flags_BIT);

    //xEventGroupClearBits(APP_event_group,APP_event_Entrance_Guard_Enable_BIT);

    while(1)
    {
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI("sntp_task", "The current date/time in Shanghai is: %s.\r\n", strftime_buf);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        isInTime(8);
        isInTmTime(timeinfo);
    }

}

static void obtain_time(void)
{
    /**
     * NTP server address could be aquired via DHCP,
     * see following menuconfig options:
     * 'LWIP_DHCP_GET_NTP_SRV' - enable STNP over DHCP
     * 'LWIP_SNTP_DEBUG' - enable debugging messages
     *
     * NOTE: This call should be made BEFORE esp aquires IP address from DHCP,
     * otherwise NTP option would be rejected by default.
     */
#ifdef LWIP_DHCP_GET_NTP_SRV
    sntp_servermode_dhcp(1);      // accept NTP offers from DHCP server, if any
#endif

    initialize_sntp();
}

static void initialize_sntp(void)
{
    ESP_LOGI("sntp", "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
/*
 * If 'NTP over DHCP' is enabled, we set dynamic pool address
 * as a 'secondary' server. It will act as a fallback server in case that address
 * provided via NTP over DHCP is not accessible
 */
/*
#if LWIP_DHCP_GET_NTP_SRV && SNTP_MAX_SERVERS > 1
    sntp_setservername(0, "cn.ntp.org.cn");

#if LWIP_IPV6 && SNTP_MAX_SERVERS > 2          // statically assigned IPv6 address is also possible
    ip_addr_t ip6;
    if (ipaddr_aton("2a01:3f7::1", &ip6)) {    // ipv6 ntp source "ntp.netnod.se"
        sntp_setserver(2, &ip6);
    }
#endif  // LWIP_IPV6 

//#else   // LWIP_DHCP_GET_NTP_SRV && (SNTP_MAX_SERVERS > 1) 
    // otherwise, use DNS address from a pool */
    sntp_setservername(0, "time.windows.com");
    sntp_setservername(1, "cn.ntp.org.cn");
    sntp_setservername(2, "edu.ntp.org.cn");
//#endif

#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif
    sntp_init();

    ESP_LOGI("sntp", "List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i){
        if (sntp_getservername(i)){
            ESP_LOGI("sntp", "server %d: %s", i, sntp_getservername(i));
        } else {
            // we have either IPv4 or IPv6 address, let's print it
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = sntp_getserver(i);
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
                ESP_LOGI("sntp", "server %d: %s", i, buff);
        }
    }
}
