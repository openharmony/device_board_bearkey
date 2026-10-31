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

#ifndef CAMERA_RK_FORK_NODE_H
#define CAMERA_RK_FORK_NODE_H

#include <vector>
#include <queue>
#include <condition_variable>
#include "dma_allocator.h"

#include "camera.h"
#include "node_base.h"

namespace OHOS::Camera {
class RKForkNode : public NodeBase {
public:
    RKForkNode(const std::string& name, const std::string& type, const std::string& cameraId);
    ~RKForkNode() override;

    RetCode Start(const int32_t streamId) override;
    RetCode Stop(const int32_t streamId) override;
    void DeliverBuffer(std::shared_ptr<IBuffer>& buffer) override;
    RetCode Capture(const int32_t streamId, const int32_t captureId) override;
    RetCode CancelCapture(const int32_t streamId) override;
    RetCode Flush(const int32_t streamId);

private:
    void DrainForkBufferPool();
    void DeliverToMatchedOutPort(std::shared_ptr<IBuffer>& buffer);
    void DeliverBufferToNextNode();
    void DeliverForkBuffer(std::shared_ptr<IBuffer>& forkbuffer);
    void CopyBufferToForkBuffer(std::shared_ptr<IBuffer>& buffer, std::shared_ptr<IBuffer>& forkBuffer);
    void CleanupDmaBufs();
    void CleanupOneDmaBuf(std::shared_ptr<IBuffer>& buffer);
    void CleanupPoolDmaBufs();

    std::mutex                            mtx_;
    std::queue<std::shared_ptr<IBuffer>>  bufferQueue_ = {};
    std::condition_variable               bqcv_;
    std::mutex                            bufferMtx;
    std::shared_ptr<std::thread>          forkThread_ = nullptr;
    std::atomic_bool                      stopForkThread_ = false;
    std::vector<std::shared_ptr<IPort>>   inPutPorts_;
    std::vector<std::shared_ptr<IPort>>   outPutPorts_;
    std::atomic_bool                      streamRunning_ = false;
    std::atomic_bool                      forkThreadRunFlag_ = false;
    std::shared_ptr<IBufferPool>          bufferPool_ = nullptr;
    int32_t                               streamId_ = 0;
    std::shared_ptr<IBuffer>              pendingBuffer_ = nullptr;
    std::mutex                            requestLock_;
    std::unordered_map<int32_t, std::list<int32_t>> captureRequests_ = {};
    bool                                  isDeliveryForkBufferInAloneThread_ = true;
    std::mutex                              dmaLock_;
    std::unordered_map<IBuffer*, DmaInfo>   dmaBufs_;
};
} // namespace OHOS::Camera
#endif