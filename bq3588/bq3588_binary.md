<table style="width: 100%; table-layout: fixed; border-collapse: collapse;">
  <thead>
    <tr>
      <th style="width: 10%; border: 1px solid #ddd; padding: 8px;">模块</th>
      <th style="width: 20%; border: 1px solid #ddd; padding: 8px;">文件名</th>
      <th style="width: 45%; border: 1px solid #ddd; padding: 8px;">文件路径</th>
      <th style="width: 25%; border: 1px solid #ddd; padding: 8px;">文件说明</th>
    </tr>
  </thead>
  <tbody>
  <!-- 第1行：相机 -->
  <tr>
    <td rowspan="2">CAMERA/isp_lib</td>
    <td>librkaiq.so</td>
    <td>device/soc/rockchip/rk3588/hardware/isp-linux-6.6/lib64/librkaiq.so</td>
    <td rowspan="1">64位系统linux-6.6内核摄像头算法库</td>
  </tr>
  <tr>
    <td>ispserver</td>
    <td>device/soc/rockchip/rk3588/hardware/isp-linux-6.6/bin64/ispserver</td>
    <td rowspan="1">64位系统linux-6.6内核摄像头图像处理程序</td>
  </tr>
  <!-- 第2行：WIFI -->
  <tr>
    <td rowspan="2">WIFI</td>
    <td>clm_bcm43752a2_pcie_ag.blob</td>
    <td>device/soc/rockchip/rk3588/hardware/wifi/ap6xxx/clm_bcm43752a2_pcie_ag.blob</td>
    <td rowspan="2">WIFI模块固件</td>
  </tr>
  <tr>
    <td>fw_bcm43752a2_pcie_ag.bin</td>
    <td>device/soc/rockchip/rk3588/hardware/wifi/ap6xxx/fw_bcm43752a2_pcie_ag.bin</td>
  </tr>
  <!-- 第3行：蓝牙 -->
  <tr>
    <td rowspan="1">蓝牙</td>
    <td>BCM4362A2.hcd</td>
    <td>vendor/bearkey/bq3588/BCM4362A2.hcd</td>
    <td rowspan="1">蓝牙通信模块固件</td>
  </tr>
  <!-- 第4行：RKNPU -->
  <tr>
    <td rowspan="1">RKNPU</td>
    <td>librknnrt_v2.4.1b0_ohos_arm64.so</td>
    <td>device/soc/rockchip/rk3588/hardware/npu/lib64/librknnrt_v2.4.1b0_ohos_arm64.so</td>
    <td rowspan="1">NPU神经网络加速推理核心库</td>
  </tr>
  <!-- 第5行：OMX -->
  <tr>
    <td rowspan="7">OMX</td>
    <td>libmpp.z.so</td>
    <td>device/soc/rockchip/rk3588/hardware/mpp/lib64/libmpp.z.so</td>
    <td rowspan="7">多媒体硬件编解码 (OpenMAX IL) 组件</td>
  </tr>
  <tr>
    <td>libvpu.z.so</td>
    <td>device/soc/rockchip/rk3588/hardware/mpp/lib64/libvpu.z.so</td>
  </tr>
  <tr>
    <td>libOMX_Core.z.so</td>
    <td>device/soc/rockchip/rk3588/hardware/omx_il/lib64/libOMX_Core.z.so</td>
  </tr>
  <tr>
    <td>libOMX_Pluginhw.z.so</td>
    <td>device/soc/rockchip/rk3588/hardware/omx_il/lib64/libOMX_Pluginhw.z.so</td>
  </tr>
  <tr>
    <td>libomxvpu_dec.z.so</td>
    <td>device/soc/rockchip/rk3588/hardware/omx_il/lib64/libomxvpu_dec.z.so</td>
  </tr>
  <tr>
    <td>libomxvpu_enc.z.so</td>
    <td>device/soc/rockchip/rk3588/hardware/omx_il/lib64/libomxvpu_enc.z.so</td>
  </tr>
  <tr>
    <td>libRkOMX_Resourcemanager.z.so</td>
    <td>device/soc/rockchip/rk3588/hardware/omx_il/lib64/libRkOMX_Resourcemanager.z.so</td>
  </tr>
  <!-- 第6行：RKRGA -->
  <tr>
    <td rowspan="1">RKRGA</td>
    <td>librga.z.so</td>
    <td>device/soc/rockchip/rk3588/hardware/npu/rga/librga.z.so</td>
    <td rowspan="1">64位rga运行库</td>
  </tr>
  <!-- 第7行：BOOT -->
  <tr>
    <td rowspan="4">BOOT</td>
    <td>config_ab.cfg</td>
    <td>device/board/bearkey/bq3588/loader/config_ab.cfg</td>
    <td rowspan="1">开发烧录工具配置文件（支持AB分区）</td>
  </tr>
  <tr>
    <td>config.cfg</td>
    <td>device/board/bearkey/bq3588/loader/config.cfg</td>
    <td rowspan="1">开发烧录工具配置文件</td>
  </tr>
  <tr>
    <td>MiniLoaderAll.bin</td>
    <td>device/board/bearkey/bq3588/loader/MiniLoaderAll.bin</td>
    <td rowspan="1">一级引导固化前置loader</td>
  </tr>
  <tr>
    <td>uboot.img</td>
    <td>device/board/bearkey/bq3588/loader/uboot.img</td>
    <td rowspan="1">完整U-Boot第二阶段引导程序</td>
  </tr>
  <!-- 第8行：GPU -->
  <tr>
    <td rowspan="2">GPU/display</td>
    <td>mali_csffw.bin</td>
    <td>device/board/bearkey/bq3588/kernel/mali_csffw.bin</td>
    <td rowspan="1">GPU模块固件</td>
  </tr>
  <tr>
    <td>libmali-g7p0-ohos.so</td>
    <td>device/soc/rockchip/rk3588/hardware/gpu/lib64/libmali-g7p0-ohos.so</td>
    <td rowspan="1">64位GPU运行库</td>
  </tr>
  </tbody>
</table>

