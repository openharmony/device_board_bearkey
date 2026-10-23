# device_board_bearkey

## Overview

This repository is used to store content related to Rockchip-based development boards. For details, please refer to the readme file in each development board subdirectory.

| Board Name | SoC | System Type | Domain |
| --- | --- | --- | --- |
| [BQ3588HM](bq3588/README_en.md) | RK3588 | Standard System | Smart Industry/Multimedia |

####About Bearkey

Xiamen Bearkey Technology Co., Ltd. was founded in 2015. The company has assembled a core team composed of elites from Rockchip and Star-net R&D teams. As a leading expert in Rockchip solution applications in the industry, we are committed to the R&D, production, sales and service of embedded system hardware and software. The company's customized products and solutions are applied in fields such as low-altitude economy, unmanned aerial vehicles (UAVs), industrial Internet of Things (IIoT), robotics, smart home, smart space, smart communities, exhibition hall applications, multimedia, commercial displays, automotive electronics, education, and medical care. Products adapted to the OpenHarmony system have been verified in multiple scenarios, demonstrating high stability and reliability, and have the capabilities of large-scale commercial use and cross-industry empowerment.

#### About the Development Board

Bearkey BQ3588HM Development Kit(model: BQ3588HM) is based on Rockchip RK3588, integrating a quad-core Cortex-A76 and a quad-core Cortex-A55 core with a main frequency of 2.4GHz. It is equipped with a Mali-G610 GPU and a 2D acceleration engine, supporting multiple graphics standards to achieve a smooth visual experience. It supports 8K video decoding and 60Hz refresh rate output, and integrates a 6TOPS computing power NPU. It supports the privatized deployment of ultra-large-scale parameter models under the Transformer architecture. It supports deep learning frameworks including Caffe, Pytorch, OnnxNN, etc. The Bearkey RK3588 development board supports Bluetooth, Wi-Fi, NearLink, Beidou/GPS positioning, and has rich expansion interfaces. It supports various audio and video input/output interfaces, including MIPI display, EDP display, HDMI display, dual-channel MIPI camera/USB camera, and supports headphone jack, microphone jack, and speaker jack. It is configured with dual Gigabit adaptive RJ45 Ethernet ports, supports 4G/5G internet access via expansion modules, is equipped with 8GB of memory and 64GB of storage, and supports expansion via TF card, solid-state drive (SSD)/SATA hard disk. It has abundant external interfaces and communication methods, including RS485, RS232, DI, DO, USB, etc.

## Directory Structure

```
/device/board/bearkey
└── bq3588                            # Bearkey BQ3588HM Development Kit
    ├── audio_drivers                 # audio alsa scheme driver code
    ├── camera                        # camera code
    ├── cfg                           # Boot-related cfg files such as fstab, init
    ├── distributedhardware           # Distributed-related configurations
    ├── kernel                        # Kernel compilation scripts, tools, defconfig configuration files
    ├── loader                        # Precompiled boot images and configurations like uboot.img, MiniLoaderAll.bin
    ├── tools                         # Flashing tools, etc.
    ├── updater                       # Boot configurations related to the update subsystem
    ├── ws73e                         # NearLink-related ko files, firmware, ini files and test demos
    ├── BUILD.gn
    ├── config.gni
    ├── device.gni
    └── ohos.build
```

## Compilation/Build & Usage Instructions

[BQ3588 Development Board Instructions](https://gitcode.com/openharmony-sig/device_board_bearkey/blob/master/bq3588/README_en.md)

## License Information

See the LICENSE files and code declarations in the corresponding directories.

## Related Repositories

* [device_soc_rockchip](https://gitcode.com/openharmony/device_soc_rockchip)
* [vendor_bearkey](https://gitcode.com/openharmony-sig/vendor_bearkey)
