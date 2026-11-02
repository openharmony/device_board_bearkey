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

#include "location_vendor_lib.h"
#include "location_log.h"
#include "uart_io_stream.h"
#include "minmea.h"

#include <pthread.h>

typedef OHOS::HDI::Location::GnssVendorInterface GnssVendorInterfaceType;

namespace OHOS {
namespace HDI {
namespace Location {
namespace VendorAdapter {

typedef struct {
    GnssLocation location;
    GnssSatelliteStatus ss;
}TGnssInfo;

class VendorGnssAdapter {
public:
    VendorGnssAdapter(const VendorGnssAdapter&) = delete;
    VendorGnssAdapter();
    pthread_t m_tid;

    static VendorGnssAdapter& GetInstance()
    {
        static VendorGnssAdapter _instance;
        return _instance;
    }
    ~VendorGnssAdapter();

    void SetGnssCtrlCallback(GnssCallbackStruct *callbacks);
    void SetAgnssCtrlCallback(AgnssCallbackIfaces* callbacks);
    const GnssVendorInterfaceType *GetGnssVendorIface();
    const AgnssModuleInterface *GetAGnssModuleIface();

    /* set thread state, if false would be exit thread */
    inline void SetGnssThreadState(bool state);
    inline bool IsGnssEnable();

    /* Serial operation */
    int32_t GnssCommIfaceOpenPort();
    int32_t GnssCommIfaceClosePort();
    int32_t GnssCommIfaceWriteData(uint8_t *data, int data_len);
    int32_t GnssCommIfaceReadData(uint8_t *data, int data_len);
    bool GnssCommIsOpen();

    /* set gnss module work status */
    void SetGnssBeginNavigating();
    void SetGnssStoppedNavigating();
    void SetGnssSuspend();
    void SetGnssPowerOff();

    /* parse and handle the nmea data form serial */
    void HandleNmeaBuffer(std::string &buf);

protected:
    static int  GnssEnable(GnssCallbackStruct *callbacks);
    static int  GnssDisable(void);
    static int  GnssStart(uint32_t type);
    static int  GnssStop(uint32_t type);
    static int  GnssInjectsReferenceInformation(int type, GnssReferenceInfo *info);
    static int  GnssSetConfigPara(GnssConfigParameter *para);
    static int  GnssInjectExtendedEphemeris(char *data, int length);
    static int  GnssGetCachedLocationsSize();

    static void AgnssStatusChange(const AgnssDataConnectionRequest *status);
    static void AgnssgetSetid(uint16_t type);
    static void AgnssgetRefLoc(uint32_t type);
    static bool AgnssSetCallback(AgnssCallbackIfaces *callbacks);
    static bool AgnssSetRefLocation(const AgnssReferenceInfo *refLoc);
    static bool AgnssSetSetid(uint16_t type, const char *setid, size_t len);
    static bool AgnssSetAgnssServer(uint16_t type, const char *server, size_t len, int32_t port);

    static void GnssFlushCachedLocations();
    static void GnssRemoveAuxiliaryData(uint16_t flags);
    static const void *GetGnssModuleIface(int iface);

    /* Methods of nema sentence analysis */
    int32_t ParseNmeaDataBuff(const char *str, TGnssInfo &gnssinfo);
    int32_t HandleNemaSentenceGGA(const char *str, GnssLocation *location);
    int32_t HandleNemaSentenceGSA(const char *str, GnssLocation *location);
    int32_t HandleNemaSentenceGSV(const char *str, GnssSatelliteStatus *ss);
    int32_t HandleNemaSentenceRMC(const char *str, GnssLocation *location);
    int32_t HandleNemaSentenceZDA(const char *str, GnssLocation *location);

private:
    time_t GetCurrentTimeMsec();
    int32_t SaveSatellitesInfo(int constellation, struct minmea_sat_info *sat_info, GnssSatelliteStatus *ss);

private:
    UartIoStream *m_uart;
    uint16_t m_gnss_work_state;
    bool is_gnss_enable;

    GnssVendorInterfaceType m_gnss_interface;
    AgnssModuleInterface m_agnss_interface;

    GnssCallbackStruct *m_gnss_callback;
    AgnssCallbackIfaces *m_agnss_callback;
};

}
}
}
}
