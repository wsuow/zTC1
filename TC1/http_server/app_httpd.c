/**
 ******************************************************************************
 * @file    app_https.c
 * @author  QQ DING
 * @version V1.0.0
 * @date    1-September-2015
 * @brief   The main HTTPD server initialization and wsgi handle.
 ******************************************************************************
 *
 *  The MIT License
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is furnished
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 *  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************************
 */

#include <time.h>
#include <httpd.h>
#include <http_parse.h>
#include <http-strings.h>
#include "stdlib.h"

#include "mico.h"
#include "httpd_priv.h"
#include "app_httpd.h"
#include "user_gpio.h"
#include "user_wifi.h"
#include "user_power.h"
#include "main.h"
#include "web_data.c"
#include "web_log.h"
#include "timed_task/timed_task.h"
#include "ota_server/user_ota.h"
#include "mqtt_server/user_mqtt_client.h"

static bool is_http_init;
static bool is_handlers_registered;
const struct httpd_wsgi_call g_app_handlers[];
char power_info_json[2560] = {0};
char up_time[16] = "00:00:00";
#define OTA_BUFFER_SIZE 512  // 每次写入的缓存大小

/*
void GetPraFromUrl(char* url, char* pra, char* val)
{
    char* sub = strstr(url, pra);
    if (sub == NULL)
    {
        val[0] = 0;
        return;
    }
    sub = strstr(sub, "=");
    if (sub == NULL)
    {
        val[0] = 0;
        return;
    }
    int len = strlen(sub);
    int n = 0;
    for (int i = 0; i < len; i++)
    {
        if (sub[i] == '&' || i == len - 1)
        {
            n = len;
            break;
        }
    }
    if (n > 0)
    {
        strncpy(val, sub + 1, n - 1);
        val[n - 1] = 0;
        return;
    }
    val[0] = 0;
}
*/

static int HttpGetIndexPage(httpd_request_t *req) {
    OSStatus err = kNoErr;

    err = httpd_send_all_header(req, HTTP_RES_200, sizeof(web_index_html), HTTP_CONTENT_HTML_ZIP);
    require_noerr_action(err, exit, http_log("ERROR: Unable to send http index headers."));

    err = httpd_send_body(req->sock, web_index_html, sizeof(web_index_html));
    require_noerr_action(err, exit, http_log("ERROR: Unable to send http index body."));

    exit:
    return err;
}

static int HttpGetDemoPage(httpd_request_t *req) {
    OSStatus err = kNoErr;
    err = httpd_send_all_header(req, HTTP_RES_200, sizeof(web_index_html), HTTP_CONTENT_HTML_ZIP);
    require_noerr_action(err, exit, http_log("ERROR: Unable to send http demo headers."));

    err = httpd_send_body(req->sock, web_index_html, sizeof(web_index_html));
    require_noerr_action(err, exit, http_log("ERROR: Unable to send http demo body."));
    exit:
    return err;
}

static int HttpGetAssets(httpd_request_t *req) {
    OSStatus err = kNoErr;

    char *file_name = strstr(req->filename, "/assets/");
    if (!file_name) { http_log("HttpGetAssets url[%s] err", req->filename);
        return err;
    }
    //http_log("HttpGetAssets url[%s] file_name[%s]", req->filename, file_name);

    int total_sz = 0;
    const unsigned char *file_data = NULL;
    const char *content_type = HTTP_CONTENT_JS_ZIP;
    if (strcmp(file_name + 8, "js_pack.js") == 0) {
        total_sz = sizeof(js_pack);
        file_data = js_pack;
    } else if (strcmp(file_name + 8, "css_pack.css") == 0) {
        total_sz = sizeof(css_pack);
        file_data = css_pack;
        content_type = HTTP_CONTENT_CSS_ZIP;
    }

    if (total_sz == 0) return err;

    err = httpd_send_all_header(req, HTTP_RES_200, total_sz, content_type);
    require_noerr_action(err, exit, http_log("ERROR: Unable to send http assets headers."));

    err = httpd_send_body(req->sock, file_data, total_sz);
    require_noerr_action(err, exit, http_log("ERROR: Unable to send http assets body."));

    exit:
    return err;
}

static int HttpGetTc1Status(httpd_request_t *req) {
    char *sockets = GetSocketStatus();
    char *short_click_config = GetButtonClickConfig();
    char *tc1_status = malloc(2048);
    char *socket_names = malloc(512);
    sprintf(socket_names, "%s,%s,%s,%s,%s,%s",
            user_config->socket_names[0],
            user_config->socket_names[1],
            user_config->socket_names[2],
            user_config->socket_names[3],
            user_config->socket_names[4],
            user_config->socket_names[5]);
    sprintf(tc1_status, TC1_STATUS_JSON, sockets, ip_status.mode,
            sys_config->micoSystemConfig.ssid, sys_config->micoSystemConfig.user_key,
            user_config->ap_name, user_config->ap_key, MQTT_SERVER, MQTT_SERVER_PORT,
            MQTT_SERVER_USR, MQTT_SERVER_PWD,
            VERSION, ip_status.ip, ip_status.mask, ip_status.gateway, user_config->mqtt_report_freq,
            user_config->power_led_enabled, 0L, socket_names, childLockEnabled,
            sys_config->micoSystemConfig.name, short_click_config);

    OSStatus err = kNoErr;
    send_http(tc1_status, strlen(tc1_status), exit, &err);

    exit:
    if (socket_names) free(socket_names);
    if (tc1_status) free(tc1_status);
    return err;
}

static int HttpSetSocketStatus(httpd_request_t *req) {
    OSStatus err = kNoErr;

    int buf_size = 512;
    char *buf = malloc(buf_size);

    err = httpd_get_data(req, buf, buf_size);
    require_noerr(err, exit);

    SetSocketStatus(buf);

    send_http("OK", 2, exit, &err);

    exit:
    if (buf) free(buf);
    return err;
}

static int HttpSetSocketName(httpd_request_t *req) {
    OSStatus err = kNoErr;

    int buf_size = 70;
    char *buf = malloc(buf_size);

    err = httpd_get_data(req, buf, buf_size);
    require_noerr(err, exit);
    int index;
    char name[64];
    sscanf(buf, "%d %s", &index, name);
    strcpy(user_config->socket_names[index], name);
    mico_system_context_update(sys_config);
    registerMqttEvents();
    send_http("OK", 2, exit, &err);

    exit:
    if (buf) free(buf);
    return err;
}

static int HttpSetButtonEvent(httpd_request_t *req) {
    OSStatus err = kNoErr;

    int buf_size = 10;
    char *buf = malloc(buf_size);

    err = httpd_get_data(req, buf, buf_size);
    require_noerr(err, exit);
    int index;
    int func;
    int longPress;
    sscanf(buf, "%d %d %d", &index, &func, &longPress);
    if (longPress == 1) {
        set_key_map(user_config->user,index, get_short_func(user_config->user[index]), func == -1 ? NO_FUNCTION : func);
    } else {
        set_key_map(user_config->user,index, func == -1 ? NO_FUNCTION : func, get_long_func(user_config->user[index]));
    }
    key_log("WARNGIN:set KEY func %d %d %d", index,get_short_func(user_config->user[index]),get_long_func(user_config->user[index]));
    mico_system_context_update(sys_config);

    send_http("OK", 2, exit, &err);

    exit:
    if (buf) free(buf);
    return err;
}

static int HttpSetOTAFile(httpd_request_t *req) {
    OSStatus err = kNoErr;
    uint32_t total = 0, ota_offset = 0;
    char *buffer = malloc(OTA_BUFFER_SIZE);
    if (!buffer) return kGeneralErr;

//    mico_logic_partition_t* ota_partition = MicoFlashGetInfo(MICO_PARTITION_OTA_TEMP);
//    MicoFlashErase(MICO_PARTITION_OTA_TEMP, 0x0, ota_partition->partition_length);

    CRC16_Context crc_context;
    CRC16_Init(&crc_context);

    tc1_log("開始接收 OTA 數據...");
    struct timeval timeout = {60, 0}; // 60秒
    setsockopt(req->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
int remaining = -1;
    while (remaining!=0) {
    int readSize = remaining>OTA_BUFFER_SIZE?OTA_BUFFER_SIZE:(remaining>0?remaining:OTA_BUFFER_SIZE);
        remaining = httpd_get_data(req, buffer, readSize);
        if (remaining < 0) {
            err = kConnectionErr;
            tc1_log("httpd_get_data 失敗");
            goto exit;
            break;
        }

//        CRC16_Update(&crc_context, buffer, readSize);

//        err = MicoFlashWrite(MICO_PARTITION_OTA_TEMP, &crc_context->offset, (uint8_t *)buffer, readSize);
//        require_noerr_quiet(err, exit);
        total+=readSize;
        mico_thread_msleep(10);
    }

    uint16_t crc16;
//    CRC16_Final(&crc_context, &crc16);
    char response[64];

    snprintf(response, sizeof(response), "OK, total: %ld bytes, CRC: 0x%04X", total, crc16);
    send_http(response, strlen(response), exit, &err);
    return 0;

    err = mico_ota_switch_to_new_fw(ota_offset, crc16);
    require_noerr(err, exit);

    tc1_log("OTA 完成，重啟系統");
    mico_system_power_perform(mico_system_context_get(), eState_Software_Reset);

exit:
    if (req->sock >= 0) {
        close(req->sock);
        req->sock = -1;
    }
    if (buffer) free(buffer);
    tc1_log("OTA 結束，狀態: %d", err);
    return err;
}

static int HttpSetDeviceName(httpd_request_t *req) {
    OSStatus err = kNoErr;

    int buf_size = 70;
    char *buf = malloc(buf_size);

    err = httpd_get_data(req, buf, buf_size);
    require_noerr(err, exit);
    char name[64];
    sscanf(buf, "%s", name);
    strcpy(sys_config->micoSystemConfig.name, name);
    mico_system_context_update(sys_config);
    registerMqttEvents();
    send_http("OK", 2, exit, &err);

    exit:
    if (buf) free(buf);
    return err;
}

static int HttpSetChildLock(httpd_request_t *req) {
    OSStatus err = kNoErr;

    int buf_size = 32;
    char *buf = malloc(buf_size);

    err = httpd_get_data(req, buf, buf_size);
    require_noerr(err, exit);
    int enableLock;
    sscanf(buf, "%d", &enableLock);
    user_config->user[0] = enableLock;
    childLockEnabled = enableLock;
    mico_system_context_update(sys_config);
    UserMqttSendChildLockState();
    send_http("OK", 2, exit, &err);

    exit:
    if (buf) free(buf);
    return err;
}

static int HttpGetPowerInfo(httpd_request_t *req) {
    OSStatus err = kNoErr;
    char buf[16];
    err = httpd_get_data(req, buf, 16);
    require_noerr(err, exit);

    int idx = 0;
    sscanf(buf, "%d", &idx);

    //计算系统运行时间
    mico_time_t past_ms = 0;
    mico_time_get_time(&past_ms);
    int past = past_ms / 1000;
    int d = past / 3600 / 24;
    int h = past / 3600 % 24;
    int m = past / 60 % 60;
    int s = past % 60;
    sprintf(up_time, "%d - %02d:%02d:%02d", d, h, m, s);

    char *powers = GetPowerRecord(idx);
    char *sockets = GetSocketStatus();
    char *short_click_config = GetButtonClickConfig();
    char *socket_names = malloc(512);
    sprintf(socket_names, "%s,%s,%s,%s,%s,%s",
            user_config->socket_names[0],
            user_config->socket_names[1],
            user_config->socket_names[2],
            user_config->socket_names[3],
            user_config->socket_names[4],
            user_config->socket_names[5]);
    sprintf(power_info_json, POWER_INFO_JSON, sockets, power_record.idx, PW_NUM, p_count, powers,
            up_time, user_config->power_led_enabled, RelayOut() ? 1 : 0, socket_names,
            user_config->p_count_1_day_ago, user_config->p_count_2_days_ago, childLockEnabled,
            sys_config->micoSystemConfig.name, short_click_config);
    send_http(power_info_json, strlen(power_info_json), exit, &err);
    if (socket_names) free(socket_names);
    exit:
    return err;
}

static int HttpGetWifiConfig(httpd_request_t *req) {
    OSStatus err = kNoErr;
    char *status = "test";
    send_http(status, strlen(status), exit, &err);
    exit:
    return err;
}

static int HttpSetWifiConfig(httpd_request_t *req) {
    OSStatus err = kNoErr;

    int buf_size = 97;
    char *buf = malloc(buf_size);
    int mode = -1;
    char *wifi_ssid = malloc(32);
    char *wifi_key = malloc(32);

    err = httpd_get_data(req, buf, buf_size);
    require_noerr(err, exit);

    sscanf(buf, "%d %s %s", &mode, wifi_ssid, wifi_key);
    if (mode == 1) {
        WifiConnect(wifi_ssid, wifi_key);
    } else {
        ApConfig(wifi_ssid, wifi_key);
    }

    send_http("OK", 2, exit, &err);

    exit:
    if (buf) free(buf);
    if (wifi_ssid) free(wifi_ssid);
    if (wifi_key) free(wifi_key);
    return err;
}

static int HttpGetWifiScan(httpd_request_t *req) {
    OSStatus err = kNoErr;
    if (scaned) {
        scaned = false;
        send_http(wifi_ret, strlen(wifi_ret), exit, &err);
        free(wifi_ret);
    } else {
        send_http("NO", 2, exit, &err);
    }

    exit:
    return err;
}

static int HttpSetWifiScan(httpd_request_t *req) {
    micoWlanStartScanAdv();
    OSStatus err = kNoErr;
    send_http("OK", 2, exit, &err);
    exit:
    return err;
}

static int HttpSetRebootSystem(httpd_request_t *req) {
    OSStatus err = kNoErr;

    send_http("OK", 2, exit, &err);

    MicoSystemReboot();

    exit:
    return err;
}

static int HttpSetMqttConfig(httpd_request_t *req) {
    OSStatus err = kNoErr;

    int buf_size = 97;
    char *buf = malloc(buf_size);

    err = httpd_get_data(req, buf, buf_size);
    require_noerr(err, exit);

    sscanf(buf, "%s %d %s %s", MQTT_SERVER, &MQTT_SERVER_PORT, MQTT_SERVER_USR, MQTT_SERVER_PWD);
    mico_system_context_update(sys_config);

    send_http("OK", 2, exit, &err);

    exit:
    if (buf) free(buf);
    return err;
}


static int HttpSetMqttReportFreq(httpd_request_t *req) {
    OSStatus err = kNoErr;

    int buf_size = 97;
    char *buf = malloc(buf_size);

    err = httpd_get_data(req, buf, buf_size);
    require_noerr(err, exit);

    sscanf(buf, "%d", &MQTT_REPORT_FREQ);
    mico_system_context_update(sys_config);

    send_http("OK", 2, exit, &err);

    exit:
    if (buf) free(buf);
    return err;
}

static int HttpGetMqttReportFreq(httpd_request_t *req) {
    OSStatus err = kNoErr;
    int buf_size = 97;
    char *freq = malloc(buf_size);
    sprintf(freq, "%d", MQTT_REPORT_FREQ);

    send_http(freq, strlen(freq), exit, &err);

    exit:
    if (freq) free(freq);
    return err;
}

static int HttpGetLog(httpd_request_t *req) {
    OSStatus err = kNoErr;
    char *logs = GetLogRecord();
    send_http(logs, strlen(logs), exit, &err);

    exit:
    return err;
}

static int HttpGetTasks(httpd_request_t *req) {
    OSStatus err = kNoErr;
    char *tasks_str = GetTaskStr();
    send_http(tasks_str, strlen(tasks_str), exit, &err);

    exit:
    if (tasks_str) free(tasks_str);
    return err;
}

static int HttpGetButtonEvents(httpd_request_t *req) {
    OSStatus err = kNoErr;
    char *clicks = GetButtonClickConfig();
    send_http(clicks, strlen(clicks), exit, &err);

    exit:
    return err;
}

static int HttpAddTask(httpd_request_t *req) {
    OSStatus err = kNoErr;

    //1577369623 4 0
    char buf[20] = {0};
    err = httpd_get_data(req, buf, 20);
    require_noerr(err, exit);

    pTimedTask task = NewTask();
    if (task == NULL) { http_log("NewTask() error, max task num = %d!", MAX_TASK_NUM);
        char *mess = "NO SPACE";
        send_http(mess, strlen(mess), exit, &err);
        return err;
    }
    int re = sscanf(buf, "%ld %d %d %d", &task->prs_time, &task->operation, &task->on,
                    &task->weekday);http_log("AddTask buf[%s] re[%d] (%ld %d %d %d)",
                                             buf, re, task->prs_time, task->operation, task->on,
                                             task->weekday);
    if (task->prs_time < 1577428136 || task->prs_time > 9577428136
        || task->operation < 0 || task->operation > 11) { http_log("AddTask Error!");
        re = 0;
    }

    char *mess = (re == 4 && AddTask(task)) ? "OK" : "NO";

    send_http(mess, strlen(mess), exit, &err);

    exit:
    return err;
}

static int HttpDelTask(httpd_request_t *req) {
    OSStatus err = kNoErr;

    char *time_str = strstr(req->filename, "/task/");
    if (!time_str) { http_log("HttpDelTask url[%s] err", req->filename);
        return err;
    }http_log("HttpDelTask url[%s] time_str[%s][%s]", req->filename, time_str, time_str + 6);

    int time1;
    sscanf(time_str + 6, "%d", &time1);

    char *mess = DelTask(time1) ? "OK" : "NO";

    send_http(mess, strlen(mess), exit, &err);
    exit:
    return err;
}

static int LedStatus(httpd_request_t *req) {
    OSStatus err = kNoErr;
    int buf_size = 97;
    char *led = malloc(buf_size);
    sprintf(led, "%d", MQTT_LED_ENABLED);

    send_http(led, strlen(led), exit, &err);

    exit:
    if (led) free(led);
    return err;
}

static int LedSetEnabled(httpd_request_t *req) {
    OSStatus err = kNoErr;

    int buf_size = 97;
    char *buf = malloc(buf_size);

    err = httpd_get_data(req, buf, buf_size);
    require_noerr(err, exit);

    sscanf(buf, "%d", &MQTT_LED_ENABLED);
    if (RelayOut() && MQTT_LED_ENABLED) {
        UserLedSet(1);
    } else {
        UserLedSet(0);
    }
    UserMqttSendLedState();
    mico_system_context_update(sys_config);

    send_http("OK", 2, exit, &err);

    exit:
    if (buf) free(buf);
    return err;
}

static int TotalSocketSetEnabled(httpd_request_t *req) {
    OSStatus err = kNoErr;

    int buf_size = 97;
    int on;
    char *buf = malloc(buf_size);

    err = httpd_get_data(req, buf, buf_size);
    require_noerr(err, exit);

    sscanf(buf, "%d", &on);
    UserRelaySetAll(on);
    int i = 0;
    for (; i < SOCKET_NUM; i++) {
        UserMqttSendSocketState(i);
    }
    UserMqttSendTotalSocketState();
    send_http("OK", 2, exit, &err);

    exit:
    if (buf) free(buf);
    return err;
}

static int Otastatus(httpd_request_t *req) {
    OSStatus err = kNoErr;
    char buf[16] = {0};
    sprintf(buf, "%.2f", ota_progress);
    send_http(buf, strlen(buf), exit, &err);
    exit:
    return err;
}

static int OtaStart(httpd_request_t *req) {
    OSStatus err = kNoErr;
    char buf[64] = {0};
    err = httpd_get_data(req, buf, 64);
    require_noerr(err, exit);

    http_log("OtaStart ota_url[%s]", buf);
    UserOtaStart(buf, NULL);

    send_http("OK", 2, exit, &err);
    exit:
    return err;
}

const struct httpd_wsgi_call g_app_handlers[] = {
        {"/",                 HTTPD_HDR_DEFORT, 0,                             HttpGetIndexPage, NULL,                       NULL, NULL},
        {"/demo",             HTTPD_HDR_DEFORT, 0,                             HttpGetDemoPage,  NULL,                       NULL, NULL},
        {"/assets", HTTPD_HDR_ADD_SERVER |
                    HTTPD_HDR_ADD_CONN_CLOSE,   APP_HTTP_FLAGS_NO_EXACT_MATCH, HttpGetAssets,    NULL,                       NULL, NULL},
        {"/socket",           HTTPD_HDR_DEFORT, 0, NULL,                                              HttpSetSocketStatus,   NULL, NULL},
        {"/status",           HTTPD_HDR_DEFORT, 0,                             HttpGetTc1Status, NULL,                       NULL, NULL},
        {"/power",            HTTPD_HDR_DEFORT, 0,                             HttpGetPowerInfo,      HttpGetPowerInfo,      NULL, NULL},
        {"/wifi/config",      HTTPD_HDR_DEFORT, 0,                             HttpGetWifiConfig,     HttpSetWifiConfig,     NULL, NULL},
        {"/wifi/scan",        HTTPD_HDR_DEFORT, 0,                             HttpGetWifiScan,       HttpSetWifiScan,       NULL, NULL},
        {"/mqtt/config",      HTTPD_HDR_DEFORT, 0, NULL,                                              HttpSetMqttConfig,     NULL, NULL},
        {"/reboot",           HTTPD_HDR_DEFORT, 0, NULL,                                              HttpSetRebootSystem,   NULL, NULL},
        {"/mqtt/report/freq", HTTPD_HDR_DEFORT, 0,                             HttpGetMqttReportFreq, HttpSetMqttReportFreq, NULL, NULL},
        {"/log",              HTTPD_HDR_DEFORT, 0,                             HttpGetLog,       NULL,                       NULL, NULL},
        {"/task",             HTTPD_HDR_DEFORT, APP_HTTP_FLAGS_NO_EXACT_MATCH, HttpGetTasks,          HttpAddTask,           NULL, HttpDelTask},
        {"/ota",              HTTPD_HDR_DEFORT, 0,                             Otastatus,             OtaStart,              NULL, NULL},
        {"/led",              HTTPD_HDR_DEFORT, 0,                             LedStatus,             LedSetEnabled,         NULL, NULL},
        {"/socketAll",        HTTPD_HDR_DEFORT, 0, NULL,                                              TotalSocketSetEnabled, NULL, NULL},
        {"/socketNames",      HTTPD_HDR_DEFORT, 0, NULL,                                              HttpSetSocketName,     NULL, NULL},
        {"/childLock",        HTTPD_HDR_DEFORT, 0, NULL,                                              HttpSetChildLock,      NULL, NULL},
        {"/deviceName",       HTTPD_HDR_DEFORT, 0, NULL,                                              HttpSetDeviceName,     NULL, NULL},
        {"/buttonEvents",     HTTPD_HDR_DEFORT, 0,                             HttpGetButtonEvents,   HttpSetButtonEvent,    NULL, NULL},
        {"/ota/fileUpload",     HTTPD_HDR_DEFORT, 0,                             NULL,   HttpSetOTAFile,    NULL, NULL},
};

static int g_app_handlers_no = sizeof(g_app_handlers) / sizeof(struct httpd_wsgi_call);

static void AppHttpRegisterHandlers() {
    int rc;
    rc = httpd_register_wsgi_handlers((struct httpd_wsgi_call *) g_app_handlers, g_app_handlers_no);
    if (rc) { http_log("failed to register test web handler");
    }
}

static int _AppHttpdStart() {
    OSStatus err = kNoErr;http_log("initializing web-services");

    /*Initialize HTTPD*/
    if (is_http_init == false) {
        err = httpd_init();
        require_noerr_action(err, exit, http_log("failed to initialize httpd"));
        is_http_init = true;
    }

    /*Start http thread*/
    err = httpd_start();
    if (err != kNoErr) { http_log("failed to start httpd thread");
        httpd_shutdown();
    }
    exit:
    return err;
}

int AppHttpdStart(void) {
    OSStatus err = kNoErr;

    err = _AppHttpdStart();
    require_noerr(err, exit);

    if (is_handlers_registered == false) {
        AppHttpRegisterHandlers();
        is_handlers_registered = true;
    }

    exit:
    return err;
}

int AppHttpdStop() {
    OSStatus err = kNoErr;

    /* HTTPD and services */
    http_log("stopping down httpd");
    err = httpd_stop();
    require_noerr_action(err, exit, http_log("failed to halt httpd"));

    exit:
    return err;
}
