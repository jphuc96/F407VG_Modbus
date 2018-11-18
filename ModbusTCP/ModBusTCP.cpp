
#include "ModBusTCP.h"

ModbusTCP * ModbusTCP::_inst;

ModbusTCP::ModbusTCP () {
    _inst = this;
    memset(_state, 0, sizeof(_state));
}

int ModbusTCP::start (EthernetInterface *ns, int port) {
    int i;

    m_ns = ns;
    
    for (i = 0; i < MODBUS_MAX_CLIENTS; i ++) {
       // _state[i].buf = new CircBuffer<char>(MODBUS_BUF_SIZE);
        _state[i].thread = new Thread(osPriorityNormal, MODBUS_STACK_SIZE);
        _state[i].client = new TCPSocket();
        _state[i].thread->start(callback(child, (void*)i));
    }
    
#ifdef MODBUS_ENABLE_CLOSER
    _state[MODBUS_MAX_CLIENTS].thread = new Thread(closer, (void*)MODBUS_MAX_CLIENTS, osPriorityNormal, 128);
    _state[MODBUS_MAX_CLIENTS].client = new TCPSocket(m_ns);
#endif

    _server.open(m_ns);
    _server.bind(port);
    _server.set_blocking(true);
    _server.listen(MODBUS_MAX_CLIENTS);
    _daemon = new Thread(osPriorityNormal,MODBUS_STACK_SIZE);
    _daemon->start(ModbusTCP::daemon);
    return 0;
}

void ModbusTCP::daemon () {
    ModbusTCP *modbus = ModbusTCP::getInstance();
    int i, t = 0;

    
    for (;;) {
        //DBG("Wait for new connection... child %i",t);
        if (t >= 0) {
            if (modbus->_server.accept(modbus->_state[t].client) == 0) {
                INFO("accept %d\r\n", t);
                modbus->_state[t].thread->signal_set(0x1);
            }
        }
        
        t = -1;
        for (i = 0; i < MODBUS_MAX_CLIENTS; i ++) {
            //DBG("Child %i in State : %u", i, modbus->_state[i].thread->get_state());
            if ( modbus->_state[i].thread->get_state() == Thread::WaitingThreadFlag) {
                if (t < 0) t = i; // next empty thread
            }
        }
    }
}

void ModbusTCP::child (void const *arg) {
    ModbusTCP *modbus = ModbusTCP::getInstance();
    int id = (int)arg;
    int _n, _i;

    for (;;) {
        //DBG("child %i waiting for connection",id);
        Thread::signal_wait(0x1);
        
       
        INFO("Connection from client on child %i", id);
//      INFO("Connection from %s\r\n", httpd->_state[id].client->get_ip_address());

        modbus->_state[id].client->set_blocking(true);
        modbus->_state[id].client->set_timeout(15000);
        time_t t1 = time(NULL);
        for (;;) {
            //if (! httpd->_state[id].client->is_connected()) break;
            modbus->_state[id].client->set_blocking(true);
            modbus->_state[id].client->set_timeout(15000);
            _n = modbus->_state[id].client->recv(modbus->_recv_buffer,MODBUSTCP_BUFFER_SIZE);
            modbus->_recv_buffer[_n] = '\0';
            
            if ( _n < 0 ) {
                DBG("Modbus::child breaking n = %d\r\n", _n);
                break;
            }
            
            if( _n > 0 ) { // We received something
                
                t1 = time(NULL);
            
                // DBG("Recv %d bytes Content: \r\r\n", _n)
                // for(int i=0;i<_n;i++) pc.printf("%.2x  ",modbus->_recv_buffer[i]);
                // pc.printf("\r\r\n");
            
                for(_i=0;_i<7;_i++) modbus->_MBAP[_i] = modbus->_recv_buffer[_i];
                modbus->_len = modbus->_MBAP[4] << 8 | modbus->_MBAP[5];
                modbus->_len--;  // Do not count with last byte from MBAP
            }
            if (modbus->_MBAP[2] !=0 || modbus->_MBAP[3] !=0) return;   //Not a MODBUSIP packet
            if (modbus->_len > MODBUSTCP_MAXFRAME) return;      //Length is over MODBUSTCP_MAXFRAME

            modbus->_recv_frame = (byte*)malloc(modbus->_len);
            for(_i=0;_i<modbus->_len;_i++) modbus->_recv_frame[_i] = modbus->_recv_buffer[_i+7];

            modbus->receivePDU(modbus->_recv_frame);
            if (modbus->_reply != MB_REPLY_OFF) {
                //MBAP
                modbus->_MBAP[4] = (modbus->_len+1) >> 8;     //_len+1 for last byte from MBAP
                modbus->_MBAP[5] = (modbus->_len+1) & 0x00FF;

                modbus->_send_buffer = (byte*) malloc(7 + modbus->_len);

                //Write MBAP
                for (_i = 0 ; _i < 7 ; _i++) modbus->_send_buffer[_i] = modbus->_MBAP[_i];
                //Write PDU
                for (_i = 0 ; _i < modbus->_len ; _i++) modbus->_send_buffer[_i+7] = modbus->_recv_frame[_i];
                //Send buffer
                modbus->_state[id].client->send(modbus->_send_buffer,modbus->_len + 7);

                // pc.printf("Sent: ");
                // for(int i=0;i<_len + 7;i++) pc.printf("%.2x  ",_send_buffer[i]);
                // pc.printf("\r\r\n");
            }
            free(modbus->_recv_frame);
            free(modbus->_send_buffer);
            modbus->_len = 0;
            
            if(abs((int)(time(NULL) - t1))> 15) {
                DBG("Timeout in child %i",id);
                break;    
            }
            
        }

        modbus->_state[id].client->close(); // Needs to bere moved 
        INFO("Closed client connection");
        //INFO("Close %s\r\n", httpd->_state[id].client->get_ip_address());
    }
}

void ModbusTCP::closer (void const *arg) {
    ModbusTCP *modbus = ModbusTCP::getInstance();
    int id = (int)arg;

    for (;;) {
        Thread::signal_wait(1);

        modbus->_state[id].client->close();
        INFO("Closed client connection\r\n");
        //INFO("Close %s\r\n", httpd->_state[id].client->get_ip_address());
    }
}

