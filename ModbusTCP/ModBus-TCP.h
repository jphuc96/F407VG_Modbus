
#ifndef MODBUS_TCP_H
#define MODBUS_TCP

#include "mbed.h"
#include "rtos.h"
#include "Modbus.h"
#include "NetworkInterface.h"
#include "EthernetInterface.h"

#define DEBUG

#define MODBUS_TCP_PORT 502
#define MODBUS_MAX_CLIENTS 3
#define MODBUS_KEEPALIVE 10
#define MODBUS_TIMEOUT 50000
#define MODBUS_MAX_HANDLES 2
#define MODBUSTCP_MAXFRAME 200
#define MODBUSTCP_BUFFER_SIZE 64
#define MODBUS_MAX_REGISTER 256
 
 
#define MODBUS_CMD_SIZE 100
#define MODBUS_BUF_SIZE 128
#define MODBUS_STACK_SIZE (1024*6)
#define HTTPD_ENABLE_CLOSER

//Debug is disabled by default
// #if defined DEBUG
extern Serial pc;
#define DBG(x, ...) pc.printf("[DBG]" x "\r\n", ##__VA_ARGS__);
#define WARN(x, ...) pc.printf("[WARN]" x "\r\n", ##__VA_ARGS__);
#define ERR(x, ...) pc.printf("[ERR]" x "\r\n", ##__VA_ARGS__);
#define INFO(x, ...) pc.printf("[INFO]" x "\r\n", ##__VA_ARGS__);
// #else
// #define DBG(x, ...)
// #define WARN(x, ...)
// #define ERR(x, ...)
// #define INFO(x, ...)
// #endif


class ModbusTCP : public Modbus {
public:

    struct STATE {
        Thread *thread;
        TCPSocket *client;
    };
    ModbusTCP ();
    
    int start (EthernetInterface *ns, int port = MODBUS_TCP_PORT);
    static ModbusTCP * getInstance() {
        return _inst;
    };

private :
    static ModbusTCP *_inst;
    Thread* _daemon;
    TCPServer _server;
    
    EthernetInterface *m_ns;

#ifdef MODBUS_ENABLE_CLOSER
    struct STATE _state[MODBUS_MAX_CLIENTS + 1];
#else
    struct STATE _state[MODBUS_MAX_CLIENTS];
#endif

    static void daemon ();
    static void child (void const *arg);
    static void closer (void const *arg);

    byte _recv_buffer[MODBUSTCP_BUFFER_SIZE];
    byte* _send_buffer;
    byte _MBAP[7];
    
};

#endif
