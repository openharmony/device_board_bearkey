/*
 * Copyright 2023 Shenzhen Kaihong DID Co., Ltd..
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdio>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <cstdint>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>

#include "securec.h"
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include "osal_time.h"
#include "gnss_common.h"
#include "vendor_gnss_adapter.h"

using namespace OHOS::HDI::Location;

#define HDF_LOG_TAG HDF_GNSS_ADAPTER
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#endif
#define LOG_DOMAIN 0xD002300

#define MAX_UART_BUFF 1024

bool g_isThreadRunning = false;

/* ohos callback function */
const GnssVendorInterfaceType *get_gnss_interface()
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    return OHOS::HDI::Location::VendorAdapter::VendorGnssAdapter::GetInstance().GetGnssVendorIface();
}

const AgnssModuleInterface *get_agnss_interface()
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    return OHOS::HDI::Location::VendorAdapter::VendorGnssAdapter::GetInstance().GetAGnssModuleIface();
}

/* so vendorinface entry */
GnssVendorDevice GnssVendorInterface = {
    sizeof(GnssVendorDevice),
    get_gnss_interface
};


namespace OHOS {
namespace HDI {
namespace Location {
namespace VendorAdapter {

VendorGnssAdapter::VendorGnssAdapter()
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    is_gnss_enable = false;
    m_gnss_work_state = static_cast<uint16_t>(GnssWorkingStatus::GNSS_WORKING_STATUS_NULL);
    m_uart = new UartIoStream;
    
    m_gnss_callback = nullptr;
    m_agnss_callback = nullptr;
    m_gnss_interface = {
        sizeof(GnssVendorInterfaceType),
        GnssEnable,
        GnssDisable,
        GnssStart,
        GnssStop,
        GnssInjectsReferenceInformation,
        GnssSetConfigPara,
        GnssRemoveAuxiliaryData,
        GnssInjectExtendedEphemeris,
        GnssGetCachedLocationsSize,
        GnssFlushCachedLocations,
        GetGnssModuleIface
    };

    m_agnss_interface = {
        sizeof(AgnssModuleInterface),
        AgnssSetCallback,
        AgnssSetRefLocation,
        AgnssSetSetid,
        AgnssSetAgnssServer
    };
}

VendorGnssAdapter::~VendorGnssAdapter()
{
    g_isThreadRunning = false;
}

const GnssVendorInterfaceType *VendorGnssAdapter::GetGnssVendorIface()
{
    return static_cast<const GnssVendorInterface *>(&m_gnss_interface);
}

const AgnssModuleInterface *VendorGnssAdapter::GetAGnssModuleIface()
{
    return static_cast<const AgnssModuleInterface *>(&m_agnss_interface);
}

void VendorGnssAdapter::SetGnssCtrlCallback(GnssCallbackStruct *callbacks)
{
    m_gnss_callback = callbacks;
}

void VendorGnssAdapter::SetAgnssCtrlCallback(AgnssCallbackIfaces* callbacks)
{
    m_agnss_callback = callbacks;
}

const void *VendorGnssAdapter::GetGnssModuleIface(int iface)
{
    switch (iface) {
        case static_cast<int>(GnssModuleIfaceCategory::AGNSS_MODULE_INTERFACE):
            return get_agnss_interface();
        case static_cast<int>(GnssModuleIfaceCategory::GNSS_GEOFENCING_MODULE_INTERFACE):
            GNSS_FUNC_LOGE("Error: unimplemented GeofenceModuleInterface");
            break;
        case static_cast<int>(GnssModuleIfaceCategory::GNSS_NET_INITIATED_MODULE_INTERFACE):
            return get_agnss_interface();
        default:
            break;
    }
    return nullptr;
}

inline void VendorGnssAdapter::SetGnssThreadState(bool state)
{
    is_gnss_enable = state;
}

inline bool VendorGnssAdapter::IsGnssEnable()
{
    return is_gnss_enable;
}

time_t VendorGnssAdapter::GetCurrentTimeMsec()
{
    auto time = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    time_t timestamp = time.time_since_epoch().count();
    return timestamp;
}

int32_t VendorGnssAdapter::SaveSatellitesInfo(int constellation,
                                              struct minmea_sat_info *sat_info,
                                              GnssSatelliteStatus *ss)
{
    if (sat_info == nullptr) {
        return HDF_FAILURE;
    }
    if (ss == nullptr) {
        return HDF_FAILURE;
    }
    
    for (int i = 0; i < SATELLITE_NUM_MAXIMUM; ++i) {
        if (ss->satellitesList[i].satelliteId == 0) {
            ss->satellitesList[i].constellationCategory = static_cast<uint8_t>(constellation);
            ss->satellitesList[i].satelliteId = static_cast<int16_t>(sat_info->nr);
            ss->satellitesList[i].cn0 = static_cast<float>(sat_info->snr);
            ss->satellitesList[i].elevation =static_cast<float>(sat_info->elevation);
            ss->satellitesList[i].azimuth =static_cast<float>(sat_info->azimuth);
            ss->satellitesNum++;
            GNSS_FUNC_LOGD("$xxGSV:satellitesNum = %{public}d, sat nr %{public}d,\
                           elevation: %{public}d, azimuth: %{public}d, snr: %{public}d dbm",
                           ss->satellitesNum, sat_info->nr, sat_info->elevation, sat_info->azimuth, sat_info->snr);
            break;
        }
    }
    return HDF_SUCCESS;
}

int32_t VendorGnssAdapter::HandleNemaSentenceGGA(const char *str, GnssLocation *location)
{
    struct minmea_sentence_gga frame;

    if (minmea_parse_gga(&frame, str)) {
        location->latitude = minmea_tocoord(&frame.latitude);
        location->longitude = minmea_tocoord(&frame.longitude);
        location->altitude = minmea_tofloat(&frame.altitude);
        location->horizontalAccuracy = minmea_tofloat(&frame.hdop);
    } else {
        GNSS_FUNC_LOGE("$xxGGA sentence parse error!");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t VendorGnssAdapter::HandleNemaSentenceGSA(const char *str, GnssLocation *location)
{
    struct minmea_sentence_gsa frame;

    if (minmea_parse_gsa(&frame, str)) {
        location->horizontalAccuracy = minmea_tofloat(&frame.hdop);
        location->verticalAccuracy = minmea_tofloat(&frame.vdop);
        location->bearingAccuracy = minmea_tofloat(&frame.pdop);
    } else {
        GNSS_FUNC_LOGE("$xxGSA sentence parse error!");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t VendorGnssAdapter::HandleNemaSentenceGSV(const char *str, GnssSatelliteStatus *ss)
{
    int dataLen = 4;
    struct minmea_sentence_gsv frame;

    if (minmea_parse_gsv(&frame, str)) {
        GNSS_FUNC_LOGD("$xxGSV: %{public}d of %{public}d, constellation: %{public}d, satellites in view: %{public}d",
                       frame.msg_nr, frame.total_msgs, frame.constellation, frame.total_sats);
        for (int i = 0; i < dataLen; i++) {
            if (frame.sats[i].nr) {
                SaveSatellitesInfo(frame.constellation, &frame.sats[i], ss);
            }
        }
    } else {
        GNSS_FUNC_LOGE("$xxGSV sentence parse error!");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t VendorGnssAdapter::HandleNemaSentenceRMC(const char *str, GnssLocation *location)
{
    struct minmea_sentence_rmc frame;
    if (minmea_parse_rmc(&frame, str)) {
        location->speed = minmea_tofloat(&frame.speed);
        location->bearing = minmea_tofloat(&frame.course);
        location->latitude = minmea_tocoord(&frame.latitude);
        location->longitude = minmea_tocoord(&frame.longitude);
        location->timeForFix = location->timeSinceBoot = minmea_timestamp(&frame.date, &frame.time);
    } else {
        GNSS_FUNC_LOGE("$xxRMC sentence parse error!");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t VendorGnssAdapter::HandleNemaSentenceZDA(const char *str, GnssLocation *location)
{
    struct minmea_sentence_zda frame;
    
    if (minmea_parse_zda(&frame, str)) {
        location->timeForFix = location->timeSinceBoot = minmea_timestamp(&frame.date, &frame.time);
        time_t ts = GetCurrentTimeMsec();
    } else {
        GNSS_FUNC_LOGE("$xxZDA sentence parse error!");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t VendorGnssAdapter::ParseNmeaDataBuff(const char *str, TGnssInfo &gnssinfo)
{
    if (str == nullptr) {
        return HDF_FAILURE;
    }

    int ret = HDF_SUCCESS;
    GnssLocation *location = &gnssinfo.location;
    GnssSatelliteStatus *ss = &gnssinfo.ss;

    switch (minmea_sentence_id(str, true)) {
        case MINMEA_SENTENCE_GGA:
            ret = HandleNemaSentenceGGA(str, location);
            break;
        case MINMEA_SENTENCE_GSA:
            ret = HandleNemaSentenceGSA(str, location);
            break;
        case MINMEA_SENTENCE_GSV:
            ret = HandleNemaSentenceGSV(str, ss);
            break;
        case MINMEA_SENTENCE_RMC:
            ret = HandleNemaSentenceRMC(str, location);
            break;
        case MINMEA_SENTENCE_ZDA:
            ret = HandleNemaSentenceZDA(str, location);
            break;
        case MINMEA_INVALID:
            ret = HDF_FAILURE;
            break;
        default:
            break;
    }

    return ret;
}

void VendorGnssAdapter::HandleNmeaBuffer(std::string &buf)
{
    int ret;
    int nLine = 0;
    std::istringstream iss(buf);
    std::string token;
    time_t ts = GetCurrentTimeMsec();
    TGnssInfo gnssinfo;

    (void)memset_s(&gnssinfo, sizeof(TGnssInfo), 0, sizeof(TGnssInfo));
    gnssinfo.location.size = sizeof(GnssLocation);
    gnssinfo.ss.size = sizeof(GnssSatelliteStatus);
    gnssinfo.ss.satellitesNum = 0;
    m_gnss_callback->gnssCallback.nmeaUpdate((int64_t)ts, buf.c_str(), buf.length());

    while (std::getline(iss, token)) {
        if (!token.empty()) {
            GNSS_FUNC_LOGD("HandleNmeaBuffer line %02d:%{public}s", ++nLine, token.c_str());
            ret = ParseNmeaDataBuff(token.c_str(), gnssinfo);
            if (ret != HDF_SUCCESS) {
                GNSS_FUNC_LOGE("Error occurs: incorrect data(%{public}s)", token.c_str());
            }
        }
    }

    if (ret == HDF_SUCCESS) {
        m_gnss_callback->gnssCallback.locationUpdate(&gnssinfo.location);
        m_gnss_callback->gnssCallback.satelliteStatusUpdate(&gnssinfo.ss);
    }
}

static void *GnssThreadRun(void *arg)
{
    int ret;
    int sleepUs = 10000;
    int readNum = 0;
    unsigned int readCount = 0;
    unsigned int minLen = 4;
    unsigned int lastword = 1;

    char nmeaData[MAX_UART_BUFF] = {0};
    char *tmp = nullptr;
    std::string cache;
    VendorGnssAdapter *gnss_adapter = (VendorGnssAdapter *)arg;
    g_isThreadRunning = true;

    GNSS_FUNC_LOGI("Gnss thread start run");
    while (g_isThreadRunning) {
        if (!gnss_adapter->IsGnssEnable()) {
            usleep(sleepUs);
        }
        readNum = gnss_adapter->GnssCommIfaceReadData((uint8_t*)nmeaData + readCount, 1);
        if (readNum <= 0) {
            continue;
        }
        readCount += readNum;
        // 读到换行时，处理读到的数据
        if (nmeaData[readCount - lastword] == '\n' ||
            nmeaData[readCount - lastword] == '\r') {
            // '\r'替换为'\n'
            nmeaData[readCount - lastword] = '\n';
            if (readCount < minLen) {
                continue;
            }
            if (readCount > minLen && nmeaData[readCount - minLen] == '*') {
                GNSS_FUNC_LOGI("nmeaData:%s", nmeaData);
            }
            tmp = strstr(nmeaData, "$GN");
            // 处理从$GN开头,*XX结尾的一行数据
            if (readCount > minLen && nmeaData[readCount - minLen] == '*' && tmp != nullptr) {
                cache.append(static_cast<const char *>(tmp), readCount - (tmp - nmeaData));
            }
            if (strstr(nmeaData, "$GNGGA") || readCount >= MAX_UART_BUFF) {
                gnss_adapter->HandleNmeaBuffer(cache);
                cache.clear();
            }
            (void)memset_s(nmeaData, MAX_UART_BUFF, 0, readCount);
            readCount = 0;
        }
    }
    GNSS_FUNC_LOGI("Gnss thread exit");

    return nullptr;
}

void VendorGnssAdapter::SetGnssBeginNavigating()
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    m_gnss_work_state = static_cast<uint16_t>(GnssWorkingStatus::GNSS_WORKING_STATUS_NAVIGATING_BEGIN);
    m_gnss_callback->gnssCallback.gnssWorkingStatusUpdate(&m_gnss_work_state);
}

void VendorGnssAdapter::SetGnssStoppedNavigating()
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    m_gnss_work_state = static_cast<uint16_t>(GnssWorkingStatus::GNSS_WORKING_STATUS_NAVIGATING_END);
    m_gnss_callback->gnssCallback.gnssWorkingStatusUpdate(&m_gnss_work_state);
}

void VendorGnssAdapter::SetGnssSuspend()
{
    GNSS_FUNC_LOGI("end function %{public}s", __func__);
}

void VendorGnssAdapter::SetGnssPowerOff()
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    m_gnss_work_state = static_cast<uint16_t>(GnssWorkingStatus::GNSS_WORKING_STATUS_POWER_OFF);
    m_gnss_callback->gnssCallback.gnssWorkingStatusUpdate(&m_gnss_work_state);
}

int32_t VendorGnssAdapter::GnssCommIfaceOpenPort()
{
    GNSS_FUNC_LOGE("GnssCommIfaceOpenPort enter");
#define MAX_CMD_LENGTH 256
    char assert_node[MAX_CMD_LENGTH] = {0};
    DIR *dir;
    struct dirent *dir_info;
    char *device = nullptr;
    int eid = 1;
    int baud = 115200;

    if (m_uart == nullptr) {
        m_uart = new UartIoStream;
    }

    int ret = m_uart->Start();
    if (ret != HDF_SUCCESS) {
        GNSS_FUNC_LOGE("Enable tau gnss failed!");
        return HDF_FAILURE;
    }

    dir = opendir("/dev/");
    if (dir) {
        while ((dir_info = readdir(dir)) != nullptr) {
            if (strcmp(dir_info->d_name, ".") == 0 ||
                strcmp(dir_info->d_name, "..") == 0) {
                continue;
            }
            if (dir_info->d_type == DT_CHR && strncmp(dir_info->d_name, "ttyS9", strlen("ttyS9")) == 0) {
                device = dir_info->d_name;
                break;
            }
        }
    }

    if (device == nullptr) {
        GNSS_FUNC_LOGE("Can not find usb device node");
        return HDF_FAILURE;
    }

    ret = sprintf_s(assert_node, MAX_CMD_LENGTH, "/dev/%s", device);
    GNSS_FUNC_LOGI("Open USB to uart: %{public}s", assert_node);
    ret = m_uart->Open(assert_node, eid, baud);
    if (ret != HDF_SUCCESS) {
        GNSS_FUNC_LOGE("open uart failed: %{public}s", m_uart->GetErrorMsg());
        return HDF_ERR_IO;
    }
    
    return HDF_SUCCESS;
}

int32_t VendorGnssAdapter::GnssCommIfaceClosePort()
{
    GNSS_FUNC_LOGI("%{public}s enter.", __func__);
    m_uart->Drain();
    m_uart->Stop();

    return HDF_SUCCESS;
}

int32_t VendorGnssAdapter::GnssCommIfaceWriteData(uint8_t *data, int data_len)
{
    return m_uart->PutData(data, data_len);
}

int32_t VendorGnssAdapter::GnssCommIfaceReadData(uint8_t *data, int data_len)
{
    return m_uart->GetData(data, data_len);
}

bool VendorGnssAdapter::GnssCommIsOpen()
{
    return m_uart->IsOpen();
}

int VendorGnssAdapter::GnssEnable(GnssCallbackStruct *callbacks)
{
    GNSS_FUNC_LOGI("%{public}s.", __func__);
    if (callbacks == nullptr) {
        return HDF_FAILURE;
    }
    VendorGnssAdapter& gnss = VendorGnssAdapter::GetInstance();

    gnss.SetGnssCtrlCallback(callbacks);

    return HDF_SUCCESS;
}

int VendorGnssAdapter::GnssDisable(void)
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    VendorGnssAdapter& gnss = VendorGnssAdapter::GetInstance();

    gnss.SetGnssThreadState(false);
    gnss.GnssCommIfaceClosePort();

    return HDF_SUCCESS;
}

int VendorGnssAdapter::GnssStart(uint32_t type)
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    int ret;
    VendorGnssAdapter& gnss = VendorGnssAdapter::GetInstance();

    if (gnss.GnssCommIsOpen()) {
        GNSS_FUNC_LOGI("%{public}s uart is opened", __func__);
        gnss.SetGnssBeginNavigating();
        gnss.SetGnssThreadState(true);
        return HDF_SUCCESS;
    }

    if (gnss.GnssCommIfaceOpenPort() != HDF_SUCCESS) {
        GNSS_FUNC_LOGE("Enable tau GNSS failed!");
        return HDF_FAILURE;
    }
    GNSS_FUNC_LOGI("GNSS open port success");

    gnss.SetGnssThreadState(true);
    gnss.SetGnssBeginNavigating();
    ret = pthread_create(&gnss.m_tid, nullptr, GnssThreadRun, (void*)&gnss);
    if (ret != 0) {
        GNSS_FUNC_LOGE("Create gnss thread failed");
        return HDF_ERR_THREAD_CREATE_FAIL;
    }
    ret = pthread_detach(gnss.m_tid);
    if (ret != 0) {
        GNSS_FUNC_LOGE("GNSS thread detach failed");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int VendorGnssAdapter::GnssStop(uint32_t type)
{
    VendorGnssAdapter& gnss = VendorGnssAdapter::GetInstance();
    GNSS_FUNC_LOGI("Enter function %{public}s", __func__);

    gnss.SetGnssThreadState(false);

    return HDF_SUCCESS;
}

int VendorGnssAdapter::GnssInjectsReferenceInformation(int type, GnssReferenceInfo *info)
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    return HDF_SUCCESS;
}

int VendorGnssAdapter::GnssSetConfigPara(GnssConfigParameter *para)
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    return HDF_SUCCESS;
}

void VendorGnssAdapter::GnssRemoveAuxiliaryData(uint16_t flags)
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
}

int VendorGnssAdapter::GnssInjectExtendedEphemeris(char *data, int length)
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    return HDF_SUCCESS;
}

int VendorGnssAdapter::GnssGetCachedLocationsSize()
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    return HDF_SUCCESS;
}

void VendorGnssAdapter::GnssFlushCachedLocations()
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
}

void VendorGnssAdapter::AgnssStatusChange(const AgnssDataConnectionRequest *status)
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
}

void VendorGnssAdapter::AgnssgetSetid(uint16_t type)
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
}

void VendorGnssAdapter::AgnssgetRefLoc(uint32_t type)
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
}

bool VendorGnssAdapter::AgnssSetCallback(AgnssCallbackIfaces *callbacks)
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    VendorGnssAdapter& gnss = VendorGnssAdapter::GetInstance();
    gnss.SetAgnssCtrlCallback(callbacks);
    
    return true;
}

bool VendorGnssAdapter::AgnssSetRefLocation(const AgnssReferenceInfo *refLoc)
{
    GNSS_FUNC_LOGI("not support referecne location");
    return false;
}

bool VendorGnssAdapter::AgnssSetSetid(uint16_t type, const char *setid, size_t len)
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    return true;
}

bool VendorGnssAdapter::AgnssSetAgnssServer(uint16_t type, const char *server, size_t len, int32_t port)
{
    GNSS_FUNC_LOGI("enter function %{public}s", __func__);
    return true;
}

}
}
}
}
