/*
# Copyright © 2015-2026 Xiamen Bearkey Technology Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#include "dma_allocator.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/dma-heap.h>
#include <cerrno>
#include <cstring>
#include "camera.h"

namespace OHOS::Camera {

DmaInfo DmaAllocator::Alloc(uint32_t size)
{
    DmaInfo info;
    int heapFd = open("/dev/dma_heap/system-uncached-dma32", O_RDWR);
    if (heapFd < 0) {
        CAMERA_LOGE("DmaAllocator open dma-heap fail: %{public}s", strerror(errno));
        return info;
    }
    struct dma_heap_allocation_data data = {
        .len = size,
        .fd_flags = O_RDWR | O_CLOEXEC,
    };
    if (ioctl(heapFd, DMA_HEAP_IOCTL_ALLOC, &data) < 0) {
        CAMERA_LOGE("DmaAllocator ioctl alloc fail: %{public}s", strerror(errno));
        close(heapFd);
        return info;
    }
    void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, data.fd, 0);
    if (addr == MAP_FAILED) {
        CAMERA_LOGE("DmaAllocator mmap fail: %{public}s", strerror(errno));
        close(data.fd);
        close(heapFd);
        return info;
    }
    close(heapFd);

    info.addr = addr;
    info.size = size;
    info.fd   = static_cast<int32_t>(data.fd);
    return info;
}

void DmaAllocator::Release(const DmaInfo& info)
{
    if (info.addr != nullptr && info.addr != MAP_FAILED) {
        munmap(info.addr, info.size);
    }
    if (info.fd >= 0) {
        close(info.fd);
    }
}

} // namespace OHOS::Camera