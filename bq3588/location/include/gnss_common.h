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

#ifndef _GNSS_COMMON_H_
#define _GNSS_COMMON_H_

#include "hdf_log.h"
#include "hdf_base.h"

#define GNSS_FUNC_LOGD(fmt, arg...) do { \
            HDF_LOGD("bqGnss [%{public}s][line:%{public}d]: " fmt, __func__, __LINE__, ##arg); \
        } while (0)

#define GNSS_FUNC_LOGI(fmt, arg...) do { \
            HDF_LOGI("bqGnss [%{public}s][line:%{public}d]: " fmt, __func__, __LINE__, ##arg); \
        } while (0)

#define GNSS_FUNC_LOGW(fmt, arg...) do { \
            HDF_LOGW("bqGnss [%{public}s][line:%{public}d]: " fmt, __func__, __LINE__, ##arg); \
        } while (0)
    
#define GNSS_FUNC_LOGE(fmt, arg...) do { \
            HDF_LOGE("bqGnss [%{public}s][line:%{public}d]: " fmt, __func__, __LINE__, ##arg); \
        } while (0)


#endif