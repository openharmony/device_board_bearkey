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

#ifndef CAMERA_RK_SOURCE_NODE_H
#define CAMERA_RK_SOURCE_NODE_H

#include "camera.h"
#include "node_base.h"
#include "utils.h"
#include "dma_allocator.h"

namespace OHOS::Camera {
constexpr uint32_t RK_LOOP_MAX_COUNT = 10;

class RKSourceNode : virtual public NodeBase {
public:
    RKSourceNode(const std::string& name, const std::string& type, const std::string& cameraId);
    ~RKSourceNode() override;

    RetCode Init(const int32_t streamId) override;
    RetCode Start(const int32_t streamId) override;
    RetCode Flush(const int32_t streamId) override;
    RetCode Stop(const int32_t streamId) override;
    RetCode Capture(const int32_t streamId, const int32_t captureId) override;
    RetCode CancelCapture(const int32_t streamId) override;
    RetCode Config(const int32_t streamId, const CaptureMeta& meta) override;
    void DeliverBuffer(std::shared_ptr<IBuffer>& buffer) override;
    RetCode ProvideBuffers(std::shared_ptr<FrameSpec> frameSpec) override;

    virtual void OnPackBuffer(std::shared_ptr<FrameSpec> frameSpec);
    virtual void SetBufferCallback();

protected:
    class RKPortHandler {
    public:
        RKPortHandler() = default;
        ~RKPortHandler();
        explicit RKPortHandler(std::shared_ptr<IPort>& p, bool isResize);

        RetCode StartCollectBuffers();
        RetCode StopCollectBuffers();
        RetCode StartDistributeBuffers();
        RetCode StopDistributeBuffers();
        RetCode CollectorJoin();
        RetCode DistributorJoin();
        void OnBuffer(std::shared_ptr<IBuffer>& buffer);
        void setWideAndHigh(int32_t wide, int32_t high);

    private:
        void CollectBuffers();
        void DistributeBuffers();
        void FlushBuffers();
        void CleanupDmaBufs();

    private:
        std::shared_ptr<IPort> port = nullptr;

        bool cltRun = false;
        std::unique_ptr<std::thread> collector = nullptr;

        std::atomic<bool> dbtRun = false;
        std::unique_ptr<std::thread> distributor = nullptr;

        std::shared_ptr<IBufferPool> pool = nullptr;

        std::condition_variable rbcv;
        std::mutex rblock;
        std::list<std::shared_ptr<IBuffer>> respondBufferList = {};
        std::mutex cltLock;
        std::map<int32_t, uint8_t*> cBuffer;
        bool isResize_ = false;
        int32_t maxWide_ = 0;
        int32_t maxHigh_ = 0;
        uint32_t loopErrorCount_ = 0;
        std::mutex                              dmaLock_;
        std::unordered_map<IBuffer*, DmaInfo>   dmaBufs_;
    };

    std::mutex hndl_ = {};
    std::unordered_map<int32_t, std::shared_ptr<RKPortHandler>> handler_ = {};

    std::mutex requestLock_;
    std::unordered_map<int32_t, std::list<int32_t>> captureRequests_ = {};
    std::string cameraIds_;
    bool isAdjust_ = false;
};
} // namespace OHOS::Camera
#endif