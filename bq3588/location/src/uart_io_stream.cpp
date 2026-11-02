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

#include <cstdio>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "osal_mem.h"

#include "gnss_common.h"
#include "uart_io_stream.h"

#define HDF_LOG_TAG HDF_GNSS_UART

#define HDF_LOG_TAG AGnss
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#endif
#define LOG_DOMAIN 0xD002300

#define UART_RECV_BUF_SIZE 1024

static const char *g_uartErrors[] = {
    "uart: OK",
    "uart: there's no free signal",
    "uart: not started",
    "uart: null pointer",
    "uart: oops",
    "uart: out of memory",
    "uart: cannot open file"
};

UartIoStream::UartIoStream()
{
    m_fd = -1;
    m_eid = 0;
    m_signal_num = 0;
    is_started = false;
    func_callback = NULL;
    m_error = UART_OK;
}

UartIoStream::~UartIoStream()
{
    func_callback = NULL;
}

static void SignalHandler(int signo, siginfo_t *info, void *ignored)
{
    GNSS_FUNC_LOGE("Occurrence of signal event %{public}d", signo);
}

bool UartIoStream::IsOpen()
{
    return is_started;
}

int32_t UartIoStream::Start()
{
    int sig;

    if (is_started) {
        GNSS_FUNC_LOGI("uart is started");
        return HDF_SUCCESS;
    }

    /* Here we scan for unused real time signal */
    sig=SIGRTMIN;
    do {
        /* get old sigaction */
        sigaction(sig, 0, &m_oldsa);

        /* if signal's handler is empty */
        if (m_oldsa.sa_handler == 0) {
            /* set the signal handler, and others */
            m_signal_num=sig;
            m_sa.sa_sigaction = SignalHandler;
            m_sa.sa_flags = SA_SIGINFO;
            m_sa.sa_restorer = NULL;
            sigemptyset(&m_sa.sa_mask);
            sigaction(m_signal_num, &m_sa, 0);

            is_started = true;
            m_error = UART_OK;
            return HDF_SUCCESS;
        } else {
            /* signal handler was not empty, restore original */
            sigaction(m_signal_num, &m_oldsa, 0);
        }
        sig++;
    } while (sig <= SIGRTMAX);

    GNSS_FUNC_LOGE("there's no free signal for use");
    m_error = UART_ERROR_NOSIGNAL;

    return HDF_FAILURE;
}

void UartIoStream::Stop()
{
    if (!is_started) {
        GNSS_FUNC_LOGE("uart: run start first");
        m_error = UART_ERROR_NOTSTARTED;
        return;
    }

    /* we close all ports, and free the list */
    Close();

    /* then we remove the signal handler */
    sigaction(m_signal_num, &m_oldsa, NULL);

    /* And at least : */
    is_started = false;
    m_error = UART_OK;
}

void UartIoStream::SetCallbackMode(uart_event_callback event_callback_func)
{
    func_callback = event_callback_func;
}

int32_t UartIoStream::Open(const char *devpath, int eid, int baud, int bits, int parity, int stop)
{
    if (!is_started) {
        GNSS_FUNC_LOGE("uart: run start first");
        m_error = UART_ERROR_NOTSTARTED;
        return HDF_FAILURE;
    }

    /* opening the file */
    if (func_callback) {
        /* user wants event driven reading */
        m_fd = open(devpath, O_RDWR | O_NOCTTY | O_NONBLOCK);
        fcntl(m_fd, F_SETSIG, m_signal_num);
        fcntl(m_fd, F_SETOWN, getpid());
        fcntl(m_fd, F_SETFL, O_ASYNC | O_NONBLOCK);
    } else {
        /* the read/write operations will be bloking */
        m_fd = open(devpath, O_RDWR | O_NOCTTY | O_NONBLOCK);
    }

    if (m_fd == -1) {
        m_error = UART_ERROR_OPEN;
        return HDF_ERR_IO;
    }
    SetUpPort(baud, bits, parity, stop);
    
    m_fds.fd = m_fd;
    m_fds.events = POLLIN;
    m_error = UART_OK;
    return HDF_SUCCESS;
}

void UartIoStream::SetFlowControl(int rtscts, int xonxoff)
{
    if (!is_started) {
        GNSS_FUNC_LOGE("uart: run start first");
        m_error = UART_ERROR_NOTSTARTED;
        return;
    }

    /* We setup rts/cts (hardware) flow control */
    if (rtscts) {
        m_tio.c_cflag |= CRTSCTS;
    } else {
        m_tio.c_cflag &= ~CRTSCTS;
    }

    /* We setup xon/xoff (soft) flow control */
    if (xonxoff) {
        m_tio.c_iflag |= (IXON|IXOFF);
    } else {
        m_tio.c_iflag &= ~(IXON|IXOFF);
    }

    tcsetattr(m_fd, TCSANOW, &(m_tio));
    m_error = UART_OK;
}

void UartIoStream::SetTimeOut(int timeout)
{
    if (!is_started) {
        GNSS_FUNC_LOGE("uart: run start first");
        m_error = UART_ERROR_NOTSTARTED;
        return;
    }

    m_tio.c_cc[VTIME] = timeout;
    tcsetattr(m_fd, TCSANOW, &(m_tio));
    m_error = UART_OK;
}

void UartIoStream::Close()
{
    if (!is_started) {
        GNSS_FUNC_LOGE("uart: run start first");
        m_error = UART_ERROR_NOTSTARTED;
        return;
    }

    /* first we flush the port, then we restore old settings and close the file */
    tcflush(m_fd, TCOFLUSH);
    tcflush(m_fd, TCIFLUSH);
    tcsetattr(m_fd, TCSANOW, &(m_oldtio));
    close(m_fd);
    m_error = UART_OK;
}

const char *UartIoStream::GetErrorMsg()
{
    return g_uartErrors[m_error];
}

int UartIoStream::GetErrorCode()
{
    return m_error;
}

int UartIoStream::PutChar(char c)
{
    if (!is_started) {
        GNSS_FUNC_LOGE("uart: run start first");
        m_error = UART_ERROR_NOTSTARTED;
        return HDF_FAILURE;
    }

    return (int)write(m_fd, &c, 1);
}

int UartIoStream::PutString(const char *str)
{
    if (!is_started) {
        GNSS_FUNC_LOGE("uart: run start first");
        m_error = UART_ERROR_NOTSTARTED;
        return HDF_FAILURE;
    }

    return (int)write(m_fd, str, strlen(str));
}

int UartIoStream::PutData(uint8_t *data, int datalen)
{
    if (!is_started) {
        m_error = UART_ERROR_NOTSTARTED;
        return HDF_FAILURE;
    }
    return (int)write(m_fd, data, datalen);
}

int UartIoStream::GetChar()
{
    int ret;
    uint8_t c;

    if (poll(&m_fds, 1, -1) > 0 && m_fds.revents) {
        ret = read(m_fd, &c, sizeof(c));
        if (ret > 0) {
            return c;
        }
    }
    GNSS_FUNC_LOGE("Error: read serial failed!");
    return HDF_FAILURE;
}

int UartIoStream::GetData(uint8_t *buffer, int size)
{
    if (poll(&m_fds, 1, -1) > 0 && m_fds.revents) {
        return read(m_fd, buffer, size);
    }
}

void UartIoStream::Drain()
{
    if (!is_started) {
        GNSS_FUNC_LOGE("uart: run start first");
        m_error = UART_ERROR_NOTSTARTED;
        return;
    }

    tcdrain(m_fd);
}

tcflag_t UartIoStream::SetPortBaud(int baud)
{
    const int BAUD_75 = 75;
    const int BAUD_110 = 110;
    const int BAUD_150 = 150;
    const int BAUD_300 = 300;
    const int BAUD_600 = 600;
    const int BAUD_1200 = 1200;
    const int BAUD_2400 = 2400;
    const int BAUD_4800 = 4800;
    const int BAUD_9600 = 9600;
    const int BAUD_19200 = 19200;
    const int BAUD_38400 = 38400;
    const int BAUD_57600 = 57600;
    const int BAUD_115200 = 115200;

    switch (baud) {
        case BAUD_75:
            return B75;
        case BAUD_110:
            return B110;
        case BAUD_150:
            return B150;
        case BAUD_300:
            return B300;
        case BAUD_600:
            return B600;
        case BAUD_1200:
            return B1200;
        case BAUD_2400:
            return B2400;
        case BAUD_4800:
            return B4800;
        case BAUD_9600:
            return B9600;
        case BAUD_19200:
            return B19200;
        case BAUD_38400:
            return B38400;
        case BAUD_57600:
            return B57600;
        case BAUD_115200:
            return B115200;
        default:
            return B9600;
    }
}

tcflag_t UartIoStream::SetPortDataBits(int bits)
{
    const int BITS_7 = 7;
    const int BITS_8 = 8;

    switch (bits) {
        case BITS_7:
            return CS7;
        case BITS_8:
            return CS8;
        default:
            return CS8;
    }
}

tcflag_t UartIoStream::SetPortStopBits(int stop)
{
    const int STOP_1 = 1;
    const int STOP_2 = 2;
    
    switch (stop) {
        case STOP_1:
            return 0;
            break;
        case STOP_2:
            return CSTOPB;
            break;
        default:
            return 0;
    }
}

tcflag_t UartIoStream::SetPortParity(int parity)
{
    const int PARITY_0 = 0;
    const int PARITY_1 = 1;
    const int PARITY_2 = 2;
    
    switch (parity) {
        case PARITY_0:
            return 0;
        case PARITY_1:   //odd
            return PARENB | PARODD;
        case PARITY_2:
            return PARENB;
        default:
            return 0;
    }
}

int UartIoStream::SetUpPort(int baud, int bits, int parity, int stop)
{
    tcflag_t baudrate;
    tcflag_t databits;
    tcflag_t stopbits;
    tcflag_t checkparity;
    int valueZero = 0;
    int valueOne = 1;

    if (!is_started) {
        GNSS_FUNC_LOGE("uart: run start first");
        m_error = UART_ERROR_NOTSTARTED;
        return HDF_FAILURE;
    }
    baudrate = SetPortBaud(baud);
    databits = SetPortDataBits(bits);
    checkparity = SetPortParity(parity);
    stopbits = SetPortStopBits(stop);
    
    /* setup the values in port's termios */
    m_tio.c_cflag = baudrate | databits | checkparity | stopbits | CLOCAL | CREAD;
    m_tio.c_iflag |= (checkparity) ? (INPCK | ISTRIP) : (IGNPAR);
    m_tio.c_oflag = valueZero;
    m_tio.c_lflag = valueZero;
    m_tio.c_cc[VMIN] = valueOne;
    m_tio.c_cc[VTIME] = valueZero;

    /* flush the port */
    tcflush(m_fd, TCOFLUSH);
    tcflush(m_fd, TCIFLUSH);
    
    /* send new config to the port */
    tcsetattr(m_fd, TCSANOW, &(m_tio));

    m_error = UART_OK;
    return HDF_SUCCESS;
}

