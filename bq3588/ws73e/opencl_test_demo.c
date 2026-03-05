#define USE_OPENCL_WRAPPER
#include "opencl_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 向量加法内核源码
const char* program_source =
    "__kernel void vector_add(__global const int* a, "
    "                        __global const int* b, "
    "                        __global int* result) {\n"
    "  size_t idx = get_global_id(0);\n"  // 获取全局索引（对应数组下标）
    "  result[idx] = a[idx] + b[idx];\n"
    "}";

// 检查OpenCL函数执行错误
#define CHECK_ERR(err, msg) \
    if (err != CL_SUCCESS) { \
        fprintf(stderr, "%s failed: %d\n", msg, err); \
        exit(EXIT_FAILURE); \
    }

int main() {
    // ===================== 1. 初始化参数 =====================
    const int VECTOR_SIZE = 10;  // 向量长度
    int* host_a = NULL;          // 主机端数组a
    int* host_b = NULL;          // 主机端数组b
    int* host_result = NULL;     // 主机端结果数组

    cl_int err;
    cl_platform_id platform_id;
    cl_device_id device_id;
    cl_context context = NULL;
    cl_command_queue queue = NULL;
    cl_program program = NULL;
    cl_kernel kernel = NULL;
    cl_mem dev_a = NULL;         // 设备端数组a
    cl_mem dev_b = NULL;         // 设备端数组b
    cl_mem dev_result = NULL;    // 设备端结果数组

    // 初始化主机端数据
    host_a = (int*)malloc(VECTOR_SIZE * sizeof(int));
    host_b = (int*)malloc(VECTOR_SIZE * sizeof(int));
    host_result = (int*)malloc(VECTOR_SIZE * sizeof(int));
    if (!host_a || !host_b || !host_result) {
        fprintf(stderr, "Malloc host memory failed\n");
        exit(EXIT_FAILURE);
    }

    // 填充测试数据
    for (int i = 0; i < VECTOR_SIZE; i++) {
        host_a[i] = i;          // a = [0,1,2,...,9]
        host_b[i] = 10 + i;     // b = [10,11,...,19]
    }
    memset(host_result, 0, VECTOR_SIZE * sizeof(int));

    // ===================== 2. 初始化OpenCL =====================
    bool cl_ok = OHOS::InitOpenCL();
    if (!cl_ok) {
        fprintf(stderr, "OpenCL init failed (no driver)\n");
        exit(EXIT_FAILURE);
    }

    // ===================== 3. 获取平台和设备 =====================
    // 获取第一个平台
    err = clGetPlatformIDs(1, &platform_id, NULL);
    CHECK_ERR(err, "clGetPlatformIDs");

    // 获取第一个GPU设备
    err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
    CHECK_ERR(err, "clGetDeviceIDs");

    // ===================== 4. 创建上下文和命令队列 =====================
    // 创建上下文
    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
    CHECK_ERR(err, "clCreateContext");

    // 创建命令队列
    queue = clCreateCommandQueueWithProperties(context, device_id, 0, &err);
    CHECK_ERR(err, "clCreateCommandQueueWithProperties");

    // ===================== 5. 创建设备端内存对象 =====================
    // 创建输入内存（从主机端拷贝数据）
    dev_a = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           VECTOR_SIZE * sizeof(int), host_a, &err);
    CHECK_ERR(err, "clCreateBuffer(dev_a)");

    dev_b = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           VECTOR_SIZE * sizeof(int), host_b, &err);
    CHECK_ERR(err, "clCreateBuffer(dev_b)");

    // 创建输出内存
    dev_result = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
                                VECTOR_SIZE * sizeof(int), NULL, &err);
    CHECK_ERR(err, "clCreateBuffer(dev_result)");

    // ===================== 6. 创建程序和内核 =====================
    // 创建程序（从源码）
    program = clCreateProgramWithSource(context, 1, &program_source, nullptr, &err);
    CHECK_ERR(err, "clCreateProgramWithSource");

    // 编译程序（简单编译，无额外选项）
    err = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        // 编译失败时打印日志
        char build_log[1024];
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG,
                              sizeof(build_log), build_log, NULL);
        fprintf(stderr, "clBuildProgram failed: %s\n", build_log);
        exit(EXIT_FAILURE);
    }

    // 创建内核（对应源码中的函数名）
    kernel = clCreateKernel(program, "vector_add", &err);
    CHECK_ERR(err, "clCreateKernel");

    // ===================== 7. 设置内核参数 =====================
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &dev_a);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &dev_b);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &dev_result);
    CHECK_ERR(err, "clSetKernelArg");

    // ===================== 8. 执行内核 =====================
    size_t global_work_size = VECTOR_SIZE;  // 全局工作项数（向量长度）
    size_t local_work_size = 1;             // 局部工作项数（简单设为1）

    // 入队执行内核
    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL,
                                 &global_work_size, &local_work_size,
                                 0, NULL, NULL);
    CHECK_ERR(err, "clEnqueueNDRangeKernel");

    // 等待内核执行完成
    clFinish(queue);

    // ===================== 9. 读取设备端结果到主机端 =====================
    err = clEnqueueReadBuffer(queue, dev_result, CL_TRUE, 0,
                              VECTOR_SIZE * sizeof(int), host_result,
                              0, NULL, NULL);
    CHECK_ERR(err, "clEnqueueReadBuffer");

    // ===================== 10. 打印结果 =====================
    printf("===== OpenCL Vector Add Result =====\n");
    for (int i = 0; i < VECTOR_SIZE; i++) {
        printf("%d + %d = %d\n", host_a[i], host_b[i], host_result[i]);
    }

    // ===================== 11. 释放资源 =====================
    // 释放设备端内存
    clReleaseMemObject(dev_a);
    clReleaseMemObject(dev_b);
    clReleaseMemObject(dev_result);

    // 释放内核/程序/队列/上下文
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    // 释放主机端内存
    free(host_a);
    free(host_b);
    free(host_result);

    printf("\nOpenCL test demo executed successfully!\n");
    return EXIT_SUCCESS;
}
