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

#include "rk_source_node.h"
#include <sys/prctl.h>
#include "buffer_manager.h"

namespace OHOS::Camera {

RKSourceNode::RKSourceNode(const std::string& name, const std::string& type, const std::string& cameraId)
    : NodeBase(name, type, cameraId)
{
    name_ = name;
    type_ = type;
    cameraIds_ = cameraId;
    CAMERA_LOGV("%{public}s enter, type(%{public}s)", name_.c_str(), type_.c_str());
}

RKSourceNode::~RKSourceNode()
{
    CAMERA_LOGV("%{public}s, rk source node dtor.", __FUNCTION__);
}

RetCode RKSourceNode::Init(const int32_t streamId)
{
    (void)streamId;
    return RC_OK;
}

RetCode RKSourceNode::Start(const int32_t streamId)
{
    CAMERA_LOGI("RKSourceNode::Start [%{public}d] start", streamId);
    std::shared_ptr<IPort> port = nullptr;
    auto outPorts = GetOutPorts();
    for (auto& p : outPorts) {
        PortFormat format = {};
        p->GetFormat(format);
        if (streamId == format.streamId_) {
            port = p;
            break;
        }
    }
    if (port == nullptr) {
        return RC_ERROR;
    }
    {
        std::lock_guard<std::mutex> l(hndl_);
        if (handler_.count(streamId) > 0) {
            CAMERA_LOGI("stream [%{public}d] start again, skip", streamId);
            return RC_OK;
        }
    }
    SetBufferCallback();
    std::shared_ptr<RKPortHandler> ph = std::make_shared<RKPortHandler>(port, isAdjust_);
    CHECK_IF_PTR_NULL_RETURN_VALUE(ph, RC_ERROR);
    ph->setWideAndHigh(wide_, high_);
    {
        std::lock_guard<std::mutex> l(hndl_);
        handler_[streamId] = ph;
    }
    RetCode rc = handler_[streamId]->StartCollectBuffers();
    CHECK_IF_NOT_EQUAL_RETURN_VALUE(rc, RC_OK, RC_ERROR);
    rc = handler_[streamId]->StartDistributeBuffers();
    CHECK_IF_NOT_EQUAL_RETURN_VALUE(rc, RC_OK, RC_ERROR);
    return RC_OK;
}

RetCode RKSourceNode::Flush(const int32_t streamId)
{
    CHECK_IF_NOT_EQUAL_RETURN_VALUE(handler_.count(streamId) > 0, true, RC_ERROR);
    handler_[streamId]->StopCollectBuffers();
    return RC_OK;
}

RetCode RKSourceNode::Stop(const int32_t streamId)
{
    CHECK_IF_NOT_EQUAL_RETURN_VALUE(handler_.count(streamId) > 0, true, RC_ERROR);
    handler_[streamId]->StopDistributeBuffers();
    {
        std::lock_guard<std::mutex> l(hndl_);
        auto it = handler_.find(streamId);
        if (it != handler_.end()) {
            handler_.erase(it);
        }
    }
    return RC_OK;
}

RetCode RKSourceNode::Config(const int32_t streamId, const CaptureMeta& meta)
{
    CHECK_IF_NOT_EQUAL_RETURN_VALUE(handler_.count(streamId) > 0, true, RC_ERROR);
    (void)meta;
    return RC_OK;
}

void RKSourceNode::DeliverBuffer(std::shared_ptr<IBuffer>& buffer)
{
    CHECK_IF_PTR_NULL_RETURN_VOID(buffer);
    int32_t id = buffer->GetStreamId();
    {
        std::lock_guard<std::mutex> l(requestLock_);
        CAMERA_LOGV("deliver a buffer to stream id:%{public}d", id);
        if (captureRequests_.count(id) == 0 || captureRequests_[id].empty()) {
            buffer->SetBufferStatus(CAMERA_BUFFER_STATUS_INVALID);
        } else {
            buffer->SetCaptureId(captureRequests_[id].front());
            captureRequests_[id].pop_front();
        }
    }
    buffer->SetIsValidDataInSurfaceBuffer(false);
    NodeBase::DeliverBuffer(buffer);
}

void RKSourceNode::OnPackBuffer(std::shared_ptr<FrameSpec> frameSpec)
{
    CAMERA_LOGI("RKSourceNode::OnPackBuffer enter");
    CHECK_IF_PTR_NULL_RETURN_VOID(frameSpec);
    auto buffer = frameSpec->buffer_;
    CHECK_IF_PTR_NULL_RETURN_VOID(buffer);
    handler_[buffer->GetStreamId()]->OnBuffer(buffer);
    CAMERA_LOGI("RKSourceNode::OnPackBuffer exit");
}

void RKSourceNode::SetBufferCallback() { return; }

RetCode RKSourceNode::ProvideBuffers(std::shared_ptr<FrameSpec> frameSpec)
{
    (void)frameSpec;
    return RC_OK;
}

RetCode RKSourceNode::Capture(const int32_t streamId, const int32_t captureId)
{
    std::lock_guard<std::mutex> l(requestLock_);
    if (captureRequests_.count(streamId) == 0) {
        captureRequests_[streamId] = {captureId};
    } else {
        captureRequests_[streamId].emplace_back(captureId);
    }
    CAMERA_LOGV("received a request from stream [id:%{public}d], size:%{public}u",
        streamId, captureRequests_[streamId].size());
    return RC_OK;
}

RetCode RKSourceNode::CancelCapture(const int32_t streamId)
{
    (void)streamId;
    return RC_OK;
}

RKSourceNode::RKPortHandler::RKPortHandler(std::shared_ptr<IPort>& p, bool isResize)
    : port(p), isResize_(isResize) {}

RKSourceNode::RKPortHandler::~RKPortHandler()
{
    CAMERA_LOGV("%{public}s, rk source port handler dtor.", __FUNCTION__);
    CollectorJoin();
    DistributorJoin();
    CleanupDmaBufs();
}

RetCode RKSourceNode::RKPortHandler::StartCollectBuffers()
{
    CHECK_IF_PTR_NULL_RETURN_VALUE(port, RC_ERROR);
    PortFormat format = {};
    port->GetFormat(format);
    uint32_t streamId = format.streamId_;

    pool = BufferManager::GetInstance()->GetBufferPool(format.bufferPoolId_);
    CHECK_IF_PTR_NULL_RETURN_VALUE(pool, RC_ERROR);
    pool->NotifyStart();
    CAMERA_LOGI("RKSourceNode::RKPortHandler::StartCollectBuffers");

    {
        std::unique_lock<std::mutex> l(cltLock);
        cltRun = true;
    }

#ifdef BATCH_CREATE_BUFFERS
    auto node = port->GetNode();
    if (node == nullptr) {
        CAMERA_LOGI("RKPortHandler::StartCollectBuffers node null");
        return RC_ERROR;
    }
    if (node->CreateBuffers() != RC_OK) {
        CAMERA_LOGI("RKPortHandler::StartCollectBuffers node create buffer error");
        return RC_ERROR;
    }
#endif

    collector = std::make_unique<std::thread>([this, &streamId] {
        std::string name = "collect#" + std::to_string(streamId);
        prctl(PR_SET_NAME, name.c_str());
        CAMERA_LOGI("StartCollectBuffers thread start, name = %{public}s", name.c_str());
        auto isRunning = [this]() -> bool {
            std::unique_lock<std::mutex> l(cltLock);
            return cltRun;
        };
        while (isRunning()) {
            CollectBuffers();
        }
        CAMERA_LOGI("StartCollectBuffers thread end, name = %{public}s", name.c_str());
    });
    return RC_OK;
}

RetCode RKSourceNode::RKPortHandler::CollectorJoin()
{
    CHECK_IF_PTR_NULL_RETURN_VALUE(pool, RC_ERROR);
    {
        std::unique_lock<std::mutex> l(cltLock);
        cltRun = false;
    }
    pool->NotifyStop();
    if (collector != nullptr) {
        collector->join();
        collector.reset(nullptr);
    }
    return RC_OK;
}

RetCode RKSourceNode::RKPortHandler::StopCollectBuffers()
{
    RetCode rc = CollectorJoin();
    CHECK_IF_NOT_EQUAL_RETURN_VALUE(rc, RC_OK, RC_ERROR);
    auto node = port->GetNode();
    if (node != nullptr) {
        uint32_t n = pool->GetIdleBufferCount();
        for (uint32_t i = 0; i < n; i++) {
            auto buffer = pool->AcquireBuffer(-1);
            node->DeliverBuffer(buffer);
        }
    }
    return RC_OK;
}

void RKSourceNode::RKPortHandler::CollectBuffers()
{
    CAMERA_LOGV("RKSourceNode::RKPortHandler::CollectBuffers");
    CHECK_IF_PTR_NULL_RETURN_VOID(pool);
    std::shared_ptr<IBuffer> buffer = pool->AcquireBuffer(-1);
    CHECK_IF_PTR_NULL_RETURN_VOID(buffer);

    PortFormat format = {};
    port->GetFormat(format);
    std::shared_ptr<FrameSpec> frameSpec = std::make_shared<FrameSpec>();
    frameSpec->bufferPoolId_ = format.bufferPoolId_;
    frameSpec->bufferCount_ = format.bufferCount_;
    constexpr uint32_t NewBufferBytePrePiex = 4;
    uint32_t bufferSize = static_cast<uint32_t>(maxWide_) * static_cast<uint32_t>(maxHigh_) * NewBufferBytePrePiex;
    CAMERA_LOGI("streamId[%{public}d], bufferIndex[%{public}d], Size %{public}d => %{public}d",
                buffer->GetStreamId(), buffer->GetIndex(), buffer->GetSize(), bufferSize);

    if (buffer->GetVirAddress() == buffer->GetSuffaceBufferAddr()) {
        CAMERA_LOGI("CollectBuffers begin alloc dma buffer");
        DmaInfo info = DmaAllocator::Alloc(bufferSize);
        if (info.addr != nullptr) {
            buffer->SetVirAddress(info.addr);
            buffer->SetSize(bufferSize);
            std::lock_guard<std::mutex> l(dmaLock_);
            dmaBufs_[buffer.get()] = info;
#ifdef V4L2_EMULATOR
            buffer->SetCurWidth(buffer->GetWidth());
            buffer->SetCurHeight(buffer->GetHeight());
            constexpr uint32_t CaptureFormatInit = 27;
            buffer->SetCurFormat(CaptureFormatInit);
#endif
        } else {
            CAMERA_LOGE("CollectBuffers alloc dma buffer fail");
        }
    }
    frameSpec->buffer_ = buffer;
    auto node = port->GetNode();
    CHECK_IF_PTR_NULL_RETURN_VOID(node);
    RetCode rc = node->ProvideBuffers(frameSpec);
    if (rc == RC_ERROR) {
        CAMERA_LOGE("provide buffer failed.");
    }
}

RetCode RKSourceNode::RKPortHandler::StartDistributeBuffers()
{
    {
        std::unique_lock<std::mutex> l(rblock);
        dbtRun = true;
    }
    distributor = std::make_unique<std::thread>([this] {
        PortFormat format = {};
        port->GetFormat(format);
        int id = format.streamId_;
        std::string name = "distribute#" + std::to_string(id);
        prctl(PR_SET_NAME, name.c_str());
        CAMERA_LOGI("StartDistributeBuffers thread start, name = %{public}s", name.c_str());
        while (dbtRun.load()) {
            DistributeBuffers();
        }

        CAMERA_LOGI("StartDistributeBuffers thread end, name = %{public}s", name.c_str());
    });
    return RC_OK;
}

RetCode RKSourceNode::RKPortHandler::DistributorJoin()
{
    {
        std::unique_lock<std::mutex> l(rblock);
        dbtRun = false;
        rbcv.notify_one();
    }
    if (distributor != nullptr) {
        distributor->join();
        distributor.reset(nullptr);
    }
    return RC_OK;
}

RetCode RKSourceNode::RKPortHandler::StopDistributeBuffers()
{
    RetCode rc = DistributorJoin();
    CHECK_IF_NOT_EQUAL_RETURN_VALUE(rc, RC_OK, RC_ERROR);
    FlushBuffers();
    if (isResize_ == true) {
        for (auto iter : cBuffer) {
            free(iter.second);
        }
        cBuffer.clear();
    }
    return RC_OK;
}

void RKSourceNode::RKPortHandler::DistributeBuffers()
{
    std::shared_ptr<IBuffer> buffer = nullptr;
    {
        std::unique_lock<std::mutex> l(rblock);
        auto timeout = std::chrono::system_clock::now() + std::chrono::milliseconds(500);
        if (!rbcv.wait_until(l, timeout, [this] {
            return (!dbtRun || !respondBufferList.empty());
            })) {
            CAMERA_LOGE("DistributeBuffers timeout, dbtRun=%{public}d, list size=%{public}d",
                dbtRun.load(std::memory_order_acquire), respondBufferList.size());
        }
        if (!dbtRun || respondBufferList.empty()) {
            loopErrorCount_++;
            if (loopErrorCount_ == RK_LOOP_MAX_COUNT) {
                CAMERA_LOGE("DistributeBuffers timeout 5s update dbtRun = false");
                dbtRun = false;
            }
            return;
        }
        loopErrorCount_ = 0;
        buffer = respondBufferList.front();
        respondBufferList.pop_front();
    }
    auto node = port->GetNode();
    CHECK_IF_PTR_NULL_RETURN_VOID(node);
    CAMERA_LOGI("DistributeBuffers Loop, streamId = %{public}d", buffer->GetStreamId());
    node->DeliverBuffer(buffer);
}

void RKSourceNode::RKPortHandler::OnBuffer(std::shared_ptr<IBuffer>& buffer)
{
    {
        std::unique_lock<std::mutex> l(rblock);
        respondBufferList.emplace_back(buffer);
        rbcv.notify_one();
    }
}

void RKSourceNode::RKPortHandler::FlushBuffers()
{
    if (respondBufferList.empty()) {
        return;
    }
    auto node = port->GetNode();
    CHECK_IF_PTR_NULL_RETURN_VOID(node);
    std::unique_lock<std::mutex> l(rblock);
    while (!respondBufferList.empty()) {
        auto buffer = respondBufferList.front();
        node->DeliverBuffer(buffer);
        respondBufferList.pop_front();
    }
}

void RKSourceNode::RKPortHandler::setWideAndHigh(int32_t wide, int32_t high)
{
    maxWide_ = wide;
    maxHigh_ = high;
}

void RKSourceNode::RKPortHandler::CleanupDmaBufs()
{
    std::lock_guard<std::mutex> l(dmaLock_);
    for (auto& kv : dmaBufs_) {
        IBuffer* buf = kv.first;
        DmaInfo& info = kv.second;
        if (buf != nullptr && buf->GetVirAddress() == info.addr) {
            DmaAllocator::Release(info);
            buf->SetVirAddress(buf->GetSuffaceBufferAddr());
        }
    }
    dmaBufs_.clear();
}

REGISTERNODE(RKSourceNode, {"rk_source"})
} // namespace OHOS::Camera