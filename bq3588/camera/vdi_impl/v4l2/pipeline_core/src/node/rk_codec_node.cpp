/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "rk_codec_node.h"
#include "rk_node_utils.h"
#include <securec.h>
#include "camera_dump.h"

extern "C" {
#include <jpeglib.h>
#include <transupp.h>
}

namespace OHOS::Camera {
const unsigned long long TIME_CONVERSION_NS_S = 1000000000ULL; /* ns to s */

RKCodecNode::RKCodecNode(const std::string& name, const std::string& type, const std::string &cameraId)
    : NodeBase(name, type, cameraId)
{
    CAMERA_LOGV("%{public}s enter, type(%{public}s)\n", name_.c_str(), type_.c_str());
    jpegRotation_ = static_cast<uint32_t>(JXFORM_ROT_270);
    jpegQuality_ = 100; // 100:jpeg quality
    mppStatus_ = 0;
}

RKCodecNode::~RKCodecNode()
{
    CAMERA_LOGI("~RKCodecNode Node exit.");
}

RetCode RKCodecNode::Start(const int32_t streamId)
{
    CAMERA_LOGI("RKCodecNode::Start streamId = %{public}d\n", streamId);
    return RC_OK;
}

RetCode RKCodecNode::Stop(const int32_t streamId)
{
    CAMERA_LOGI("RKCodecNode::Stop streamId = %{public}d\n", streamId);
    std::unique_lock<std::mutex> l(halMpp);

    if (halCtx_ != nullptr) {
        CAMERA_LOGI("RKCodecNode::Stop HalMppCtxDelete\n");
        HalMppCtxDelete(halCtx_);
        halCtx_ = nullptr;
        mppStatus_ = 0;
    }

    return RC_OK;
}

RetCode RKCodecNode::Flush(const int32_t streamId)
{
    CAMERA_LOGI("RKCodecNode::Flush streamId = %{public}d\n", streamId);
    return RC_OK;
}

static void RotJpegImg(
    const unsigned char *inputImg, size_t inputSize, unsigned char **outImg, size_t *outSize, JXFORM_CODE rotDegrees)
{
    struct jpeg_decompress_struct inputInfo;
    struct jpeg_error_mgr jerrIn;
    struct jpeg_compress_struct outInfo;
    struct jpeg_error_mgr jerrOut;
    jvirt_barray_ptr *src_coef_arrays;
    jvirt_barray_ptr *dst_coef_arrays;

    CAMERA_LOGI("RotJpegImg enter");
    inputInfo.err = jpeg_std_error(&jerrIn);
    jpeg_create_decompress(&inputInfo);
    outInfo.err = jpeg_std_error(&jerrOut);
    jpeg_create_compress(&outInfo);
    jpeg_mem_src(&inputInfo, inputImg, inputSize);
    jpeg_mem_dest(&outInfo, outImg, (unsigned long *)outSize);

    JCOPY_OPTION copyoption;
    jpeg_transform_info transformoption;
    transformoption.transform = rotDegrees;
    transformoption.perfect = TRUE;
    transformoption.trim = FALSE;
    transformoption.force_grayscale = FALSE;
    transformoption.crop = FALSE;

    jcopy_markers_setup(&inputInfo, copyoption);
    (void)jpeg_read_header(&inputInfo, TRUE);

    if (!jtransform_request_workspace(&inputInfo, &transformoption)) {
        CAMERA_LOGE("%s: transformation is not perfect", __func__);
        return;
    }

    src_coef_arrays = jpeg_read_coefficients(&inputInfo);
    jpeg_copy_critical_parameters(&inputInfo, &outInfo);
    dst_coef_arrays = jtransform_adjust_parameters(&inputInfo, &outInfo, src_coef_arrays, &transformoption);
    jpeg_write_coefficients(&outInfo, dst_coef_arrays);
    jcopy_markers_execute(&inputInfo, &outInfo, copyoption);
    jtransform_execute_transformation(&inputInfo, &outInfo, src_coef_arrays, &transformoption);

    jpeg_finish_compress(&outInfo);
    jpeg_destroy_compress(&outInfo);
    (void)jpeg_finish_decompress(&inputInfo);
    jpeg_destroy_decompress(&inputInfo);
}

RetCode RKCodecNode::ConfigJpegOrientation(common_metadata_header_t* data)
{
    camera_metadata_item_t entry;
    int ret = FindCameraMetadataItem(data, OHOS_JPEG_ORIENTATION, &entry);
    if (ret != 0 || entry.data.i32 == nullptr) {
        CAMERA_LOGI("tag OHOS_JPEG_ORIENTATION not found");
        return RC_OK;
    }

    JXFORM_CODE jxRotation = JXFORM_ROT_270;
    int32_t ohosRotation = *entry.data.i32;
    if (ohosRotation == OHOS_CAMERA_JPEG_ROTATION_0) {
        jxRotation = JXFORM_NONE;
    } else if (ohosRotation == OHOS_CAMERA_JPEG_ROTATION_90) {
        jxRotation = JXFORM_ROT_90;
    } else if (ohosRotation == OHOS_CAMERA_JPEG_ROTATION_180) {
        jxRotation = JXFORM_ROT_180;
    } else {
        jxRotation = JXFORM_ROT_270;
    }
    jpegRotation_ = static_cast<uint32_t>(jxRotation);
    return RC_OK;
}

RetCode RKCodecNode::ConfigJpegQuality(common_metadata_header_t* data)
{
    camera_metadata_item_t entry;
    int ret = FindCameraMetadataItem(data, OHOS_JPEG_QUALITY, &entry);
    if (ret != 0) {
        CAMERA_LOGI("tag OHOS_JPEG_QUALITY not found");
        return RC_OK;
    }

    const int highQualityJpeg = 100;
    const int middleQualityJpeg = 95;
    const int lowQualityJpeg = 85;

    CAMERA_LOGI("OHOS_JPEG_QUALITY is = %{public}d", static_cast<int>(entry.data.u8[0]));
    if (*entry.data.i32 == OHOS_CAMERA_JPEG_LEVEL_LOW) {
        jpegQuality_ = lowQualityJpeg;
    } else if (*entry.data.i32 == OHOS_CAMERA_JPEG_LEVEL_MIDDLE) {
        jpegQuality_ = middleQualityJpeg;
    } else if (*entry.data.i32 == OHOS_CAMERA_JPEG_LEVEL_HIGH) {
        jpegQuality_ = highQualityJpeg;
    } else {
        jpegQuality_ = highQualityJpeg;
    }
    return RC_OK;
}

RetCode RKCodecNode::Config(const int32_t streamId, const CaptureMeta& meta)
{
    if (meta == nullptr) {
        CAMERA_LOGE("meta is nullptr");
        return RC_ERROR;
    }

    common_metadata_header_t* data = meta->get();
    if (data == nullptr) {
        CAMERA_LOGE("data is nullptr");
        return RC_ERROR;
    }

    RetCode rc = ConfigJpegOrientation(data);

    rc = ConfigJpegQuality(data);
    return rc;
}

void RKCodecNode::EncodeJpegToMemory(unsigned char* image, int width, int height,
                                     unsigned long* jpegSize, unsigned char** jpegBuf)
{
    struct jpeg_compress_struct cInfo;
    struct jpeg_error_mgr jErr;
    JSAMPROW row_pointer[1];
    int rowStride = 0;
    constexpr uint32_t colorMap = 3;
    constexpr uint32_t pixelsThick = 3;

    CAMERA_LOGI("EncodeJpegToMemory enter");
    cInfo.err = jpeg_std_error(&jErr);

    jpeg_create_compress(&cInfo);
    cInfo.image_width = width;
    cInfo.image_height = height;
    cInfo.input_components = colorMap;
    cInfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cInfo);
    CAMERA_LOGE("RKCodecNode::EncodeJpegToMemory jpegQuality_ is = %{public}d", jpegQuality_);
    jpeg_set_quality(&cInfo, jpegQuality_, TRUE);
    jpeg_mem_dest(&cInfo, jpegBuf, jpegSize);
    jpeg_start_compress(&cInfo, TRUE);

    rowStride = width;
    while (cInfo.next_scanline < cInfo.image_height) {
        row_pointer[0] = &image[cInfo.next_scanline * rowStride * pixelsThick];
        jpeg_write_scanlines(&cInfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cInfo);
    jpeg_destroy_compress(&cInfo);

    CAMERA_LOGI("EncodeJpegToMemory jpegRotation_=%d", jpegRotation_);
    size_t rotJpgSize = 0;
    unsigned char* rotJpgBuf = nullptr;
    /* rotate image */
    RotJpegImg(*jpegBuf, *jpegSize, &rotJpgBuf, &rotJpgSize, static_cast<JXFORM_CODE>(jpegRotation_));
    if (rotJpgBuf != nullptr && rotJpgSize != 0) {
        free(*jpegBuf);
        *jpegBuf = rotJpgBuf;
        *jpegSize = rotJpgSize;
    }
}

static int FindStartCode(unsigned char *data, size_t dataSz)
{
    constexpr uint32_t dataSize = 4;
    constexpr uint32_t dataBit2 = 2;
    constexpr uint32_t dataBit3 = 3;

    if (data == nullptr) {
        CAMERA_LOGI("RKCodecNode::FindStartCode paramater == nullptr");
        return -1;
    }

    if ((dataSz > dataSize) && (data[0] == 0) && (data[1] == 0) && \
        (data[dataBit2] == 0) && (data[dataBit3] == 1)) {
        return 4; // 4:start node
    }

    return -1;
}

static void SerchIFps(unsigned char* buf, size_t bufSize, std::shared_ptr<IBuffer>& buffer)
{
    size_t nalType = 0;
    size_t idx = 0;
    size_t size = bufSize;
    static constexpr uint32_t nalBit = 0x1F;
    constexpr uint32_t nalTypeValue = 0x05;

    if (buffer == nullptr || buf == nullptr) {
        CAMERA_LOGI("RKCodecNode::SerchIFps paramater == nullptr");
        return;
    }

    for (int i = 0; i < bufSize; i++) {
        int ret = FindStartCode(buf + idx, size);
        if (ret == -1) {
            idx += 1;
            size -= 1;
        } else {
            nalType = ((buf[idx + ret]) & nalBit);
            CAMERA_LOGI("RKCodecNode::SerchIFps nalu == 0x%{public}x buf == 0x%{public}x \n", nalType, buf[idx + ret]);
            if (nalType == nalTypeValue) {
                buffer->SetEsKeyFrame(1);
                CAMERA_LOGI("RKCodecNode::SerchIFps SetEsKeyFrame == 1 nalu == 0x%{public}x\n", nalType);
                break;
            } else {
                idx += ret;
                size -= ret;
            }
        }

        if (idx >= bufSize) {
            break;
        }
    }

    if (idx >= bufSize) {
        buffer->SetEsKeyFrame(0);
        CAMERA_LOGI("RKCodecNode::SerchIFps SetEsKeyFrame == 0 nalu == 0x%{public}x idx = %{public}d\n",
            nalType, idx);
    }
}

static void BufferFormatTransform(std::shared_ptr<IBuffer>& buffer, uint32_t format)
{
    auto oldFmt = buffer->GetFormat();
    buffer->SetFormat(format);
    RkNodeUtils::BufferScaleFormatTransform(buffer, false);
    buffer->SetFormat(oldFmt);
}

void RKCodecNode::Yuv420ToJpeg(std::shared_ptr<IBuffer>& buffer)
{
    int32_t ret = 0;
    CAMERA_LOGD("RKCodecNode::Yuv422ToJpeg begin");

    BufferFormatTransform(buffer, CAMERA_FORMAT_RGB_888);

    unsigned char* jBuf = nullptr;
    unsigned long jpegSize = 0;
    unsigned char *image = (unsigned char *)buffer->GetVirAddress();
    EncodeJpegToMemory(image, buffer->GetWidth(), buffer->GetHeight(), &jpegSize, &jBuf);
    ret = memcpy_s((unsigned char*)buffer->GetSuffaceBufferAddr(), buffer->GetSuffaceBufferSize(), jBuf, jpegSize);
    if (ret == 0) {
        buffer->SetIsValidDataInSurfaceBuffer(true);
        buffer->SetEsFrameSize(jpegSize);
    } else {
        CAMERA_LOGE("RKCodecNode::Yuv422ToJpeg memcpy_s failed 2, ret = %{public}d\n", ret);
        buffer->SetEsFrameSize(0);
    }
    CAMERA_LOGI("RKCodecNode::Yuv422ToJpeg jpegSize = %{public}lu\n", jpegSize);
    free(jBuf);
}

void RKCodecNode::Yuv420ToH264(std::shared_ptr<IBuffer>& buffer)
{
    int ret = 0;
    size_t bufSize = 0;
    struct timespec ts = {};
    int64_t timestamp = 0;
    constexpr uint32_t minIFrameBegin = 5;

    CAMERA_LOGD("RKCodecNode::Yuv420ToH264 begin");
    BufferFormatTransform(buffer, CAMERA_FORMAT_YCRCB_420_P);

    if (!buffer->GetIsValidDataInSurfaceBuffer()) {
        CAMERA_LOGD("RKCodecNode::Yuv420ToH264 cp sb to cb");
        auto ret = memcpy_s(buffer->GetSuffaceBufferAddr(), buffer->GetSuffaceBufferSize(),
            buffer->GetVirAddress(), buffer->GetSuffaceBufferSize());
        if (ret != 0) {
            CAMERA_LOGE("RKCodecNode::Yuv420ToH264 memcpy_s failed 1, ret = %{public}d\n", ret);
            buffer->SetBufferStatus(CAMERA_BUFFER_STATUS_INVALID);
            return;
        }
    }

    std::unique_lock<std::mutex> l(halMpp);
    if (halCtx_ == nullptr) {
        MpiEncTestArgs args = {};
        args.width       = buffer->GetWidth();
        args.height      = buffer->GetHeight();
        args.format      = MPP_FMT_YUV420P;
        args.type        = MPP_VIDEO_CodingAVC;
        halCtx_ = hal_mpp_ctx_create(&args);
        CAMERA_LOGI("RKCodecNode::Yuv420ToH264 hal_mpp_ctx_create d, index = %{public}d, mppStatus_ = %{public}d",
            buffer->GetIndex(), mppStatus_);
    }
    if (halCtx_ == nullptr) {
        CAMERA_LOGI("RKCodecNode::Yuv420ToH264 halCtx_ = %{public}p\n", halCtx_);
        return;
    }
    bufSize = ((MpiEncTestData *)halCtx_)->frame_size;
    ret = hal_mpp_encode(halCtx_, buffer->GetFileDescriptor(), (unsigned char *)buffer->GetVirAddress(), &bufSize);
    if (mppStatus_ < minIFrameBegin) {
        mppStatus_++;
        HalMppCtxDelete(halCtx_);
        halCtx_ = nullptr;
    }
    
    SerchIFps((unsigned char *)buffer->GetVirAddress(), bufSize, buffer);

    buffer->SetEsFrameSize(bufSize);
    clock_gettime(CLOCK_MONOTONIC, &ts);
    timestamp = ts.tv_nsec + ts.tv_sec * TIME_CONVERSION_NS_S;
    buffer->SetEsTimestamp(timestamp);
    buffer->SetIsValidDataInSurfaceBuffer(false);
    CAMERA_LOGI("RKCodecNode::Yuv420ToH264, H264 size = %{public}d ret = %{public}d timestamp = %{public}lld\n",
        bufSize, ret, timestamp);
}

void RKCodecNode::DeliverBuffer(std::shared_ptr<IBuffer>& buffer)
{
    if (buffer == nullptr) {
        CAMERA_LOGE("RKCodecNode::DeliverBuffer frameSpec is null");
        return;
    }

    if (buffer->GetBufferStatus() != CAMERA_BUFFER_STATUS_OK) {
        CAMERA_LOGE("RKCodecNode::DeliverBuffer BufferStatus() != CAMERA_BUFFER_STATUS_OK");
        return NodeBase::DeliverBuffer(buffer);
    }

    int32_t id = buffer->GetStreamId();
    CAMERA_LOGI("RKCodecNode::DeliverBuffer, streamId[%{public}d], index[%{public}d],\
format = %{public}d, encode =  %{public}d",
        id, buffer->GetIndex(), buffer->GetFormat(), buffer->GetEncodeType());

    int32_t encodeType = buffer->GetEncodeType();
    if (encodeType == ENCODE_TYPE_JPEG) {
        Yuv420ToJpeg(buffer);
    } else if (encodeType == ENCODE_TYPE_H264) {
        Yuv420ToH264(buffer);
    } else if (encodeType == ENCODE_TYPE_NULL) {
        RkNodeUtils::BufferScaleFormatTransform(buffer);
    } else {
        CAMERA_LOGI("RKCodecNode::DeliverBuffer StreamId %{public}d error, unknow encodeType, %{public}d",
            id, encodeType);
    }

    CameraDumper& dumper = CameraDumper::GetInstance();
    dumper.DumpBuffer("board_RKCodecNode", ENABLE_RKCODEC_NODE_CONVERTED, buffer);

    return NodeBase::DeliverBuffer(buffer);
}

RetCode RKCodecNode::Capture(const int32_t streamId, const int32_t captureId)
{
    CAMERA_LOGV("RKCodecNode::Capture");
    return RC_OK;
}

RetCode RKCodecNode::CancelCapture(const int32_t streamId)
{
    CAMERA_LOGI("RKCodecNode::CancelCapture streamid = %{public}d", streamId);
    std::unique_lock<std::mutex> l(halMpp);
    if (halCtx_ != nullptr) {
        CAMERA_LOGI("RKCodecNode::Stop HalMppCtxDelete\n");
        HalMppCtxDelete(halCtx_);
        halCtx_ = nullptr;
        mppStatus_ = 0;
    }
    return RC_OK;
}

REGISTERNODE(RKCodecNode, {"RKCodec"})
} // namespace OHOS::Camera
