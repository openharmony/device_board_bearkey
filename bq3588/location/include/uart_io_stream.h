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

#include <stdint.h>
#include <signal.h>
#include <termios.h>
#include <poll.h>

typedef enum {
    UART_OK,                 /* everything is all right */
    UART_ERROR_NOSIGNAL,     /* there's no free signal */
    UART_ERROR_NOTSTARTED,   /* you should first start uart */
    UART_ERROR_NULLPOINTER,  /* you gave a null pointer to the function */
    UART_ERROR_OOPS,         /* internal error, something's erong */
    UART_ERROR_MEMORY,       /* there's no memory for uart structure */
    UART_ERROR_OPEN          /* file doesnt exist or you aren't good user */
} UartErrorCode;

typedef void (*uart_event_callback)(int eid, uint8_t *buffer, int len);

class UartIoStream {
private:
    int m_fd;                /* tty file descriptor */
    struct termios m_tio;    /* termios structure for the port */
    struct termios m_oldtio;  /* old termios structure */
    struct pollfd m_fds;

    int m_eid;             /* id which would be passed to callback */
    uart_event_callback func_callback; /* callback function */

    bool is_started;
    UartErrorCode m_error;

    /* sigactions */
    struct sigaction m_sa;
    struct sigaction m_oldsa;
    /* signal number for serial i/o read */
    int m_signal_num;
public:
    UartIoStream();
    ~UartIoStream();

    /* start the uart */
    int32_t Start();

    /* the uart run state */
    bool IsOpen();

    /* finish all jobs, clear memory, etc. */
    void Stop();

    /* if set this function, then use event driven reading mode, default is pool */
    void SetCallbackMode(uart_event_callback event_callback_func);

    /* open serial port */
    int32_t Open(const char *devpath,
                 int eid,            /*own id for the port, it can help to identify the port in callback function.*/
                 int baud = 115200,    /* baudrate, integer, default 115200 */
                 int bits = 8,        /* data bits: 7 or 8 */
                 int parity = 0,    /* parity: 0 - none, 1-odd, 2-even */
                 int stop = 1);        /* stop bits: 1 or 2 */

    /*
    rtscts: Boolean, 0 - no rts/cts control,1 - rts/cts control
    xonxoff: Boolean, 0 - no xon/xoff, 1 - xon/xoff
    */
    void SetFlowControl(int rtscts, int xonxoff);

    /* Sets port timeout (deciseconds) in blocking mode */
    void SetTimeOut(int timeout);

    /* get the error message */
    const char *GetErrorMsg();

    /* get the error code */
    int GetErrorCode();

    /* sends a char via serial port */
    int PutChar(char c);

    /* sends a null terminated string */
    int PutString(const char *str);

    /* sends a data of known size */
    int PutData(uint8_t *data, int datalen);

    /* reads a char in blocking mode */
    int GetChar();

    /* reads a data to a buffer in blocking mode*/
    int GetData(uint8_t *buffer, int size);

    /* waits until all data has been transmited */
    void Drain();
private:
    /* setups the port, look at cssl_open */
    int SetUpPort(int baud, int bits, int parity, int stop);
    tcflag_t SetPortBaud(int baud);
    tcflag_t SetPortDataBits(int bits);
    tcflag_t SetPortStopBits(int stop);
    tcflag_t SetPortParity(int parity);

    /* close the serial port */
    void Close();
};

