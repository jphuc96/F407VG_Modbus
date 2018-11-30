#include "mbed.h"
#include "EthernetInterface.h"
#include "ModbusMaster.h"
#include "ModBusTCP.h"
/*TFT*/
#include <stdio.h>
#include <string.h>
#include "SPI_TFT_ILI9341.h"
#include "Arial12x12.h"
#include "Arial24x23.h"


#define PIN_MOSI        PB_5
#define PIN_MISO        PB_4
#define PIN_SCLK        PA_5
#define PIN_RS_TFT      PB_7
#define PIN_CS_TFT      PA_4
#define PIN_DC_TFT      PB_6

/*Basic configurations*/
DigitalOut LedRed(PD_12);
DigitalOut LedBlue(PD_13);
DigitalOut LedGreen(PD_14);
DigitalOut LedYellow(PD_15);

Serial pc(PA_9,PA_10,115200);

//TFT interface
SPI_TFT_ILI9341 TFT(PIN_MOSI, PIN_MISO, PIN_SCLK, PIN_CS_TFT, PIN_RS_TFT, PIN_DC_TFT,"TFT");


/*Modbus TCP component*/
#define MBTCP_PORT 502
EthernetInterface eth;
ModbusTCP modbustcp_s;

/*Modbus RTU component*/
#define MBRTU_BAUD_RATE 9600
#define MBRTU_DATA_LENGH 8
#define MBRTU_PARITY Serial::None
#define MBRTU_STOP_BIT 1
BufferedSerial buffered_serial(PD_5,PA_3);
ModbusMaster modbusrtu_m_s1, modbusrtu_m_s2;

/*Global variables*/
uint32_t uptime_count=0;

/*OS Threads*/
Thread thread_modbus_rtu_tcp_sync(osPriorityNormal,1024,NULL,"modbus_rtu_tcp_sync");
Thread thread_modbustcp_sync(osPriorityNormal,OS_STACK_SIZE,NULL,"update_registers");
void task_uptime_count();
void task_modbus_rtu_tcp_sync();

int main()
{
    LedRed = 1;
    wait_ms(100);
    LedRed = 0;

    TFT.claim(stdout);
    TFT.background(Black);    // set background to black
    TFT.foreground(White);    // set chars to white
    TFT.cls();                // clear the screen
    TFT.set_font((unsigned char*) Arial12x12);  // select the font
    TFT.set_orientation(1);
    TFT.locate(0,0);

    pc.printf("===========================\r\r\n");
    pc.printf("====mbed Modbus TCP-RTU====\r\r\n");
    pc.printf("===========================\r\r\n");

    pc.printf("Initializing EthernetInterface.\r\r\n");
    eth.connect();
    pc.printf("MAC: %s\r\r\n",eth.get_mac_address());
    pc.printf("IP: %s\r\r\n",eth.get_ip_address());
    pc.printf("Port: %d\r\r\n",MBTCP_PORT);
    pc.printf("\r\r\n");

    pc.printf("Initializing Modbus RTU master.\r\r\n");
    buffered_serial.baud(MBRTU_BAUD_RATE);
    buffered_serial.format(MBRTU_DATA_LENGH,MBRTU_PARITY,MBRTU_STOP_BIT);
    pc.printf("Baudrate: %d\r\r\n",MBRTU_BAUD_RATE);
    pc.printf("Format: %d | %d | %d \r\r\n",MBRTU_DATA_LENGH,MBRTU_PARITY,MBRTU_STOP_BIT);
    modbusrtu_m_s1.begin(1,buffered_serial);
    pc.printf("Started RTU CLient 1\r\r\n");
    modbusrtu_m_s2.begin(2,buffered_serial);
    pc.printf("Started RTU CLient 2\r\r\n");
    pc.printf("\r\r\n");


    modbustcp_s.start(&eth,MBTCP_PORT);
    pc.printf("ModbusTCP server started at port %d\r\r\n",MBTCP_PORT);

    const char *ip = eth.get_ip_address();
    const char *netmask = eth.get_netmask();
    const char *gateway = eth.get_gateway();
    TFT.printf("IP address: %s\n", ip ? ip : "None");
    TFT.printf("Netmask: %s\n", netmask ? netmask : "None");
    TFT.printf("Gateway: %s\n", gateway ? gateway : "None");
    TFT.printf("MB_TCP Port: %d\n", MBTCP_PORT);
    TFT.printf("MB_RTU Baudrate: %d\n",MBRTU_BAUD_RATE);
    TFT.printf("MB_RTU Format: %d | %d | %d \n",MBRTU_DATA_LENGH,MBRTU_PARITY,MBRTU_STOP_BIT);
    TFT.printf("Uptime: \n");

    /*Start threads here*/
    thread_modbustcp_sync.start(task_uptime_count);
    thread_modbus_rtu_tcp_sync.start(task_modbus_rtu_tcp_sync);

    while(1)
    {

    }
    return 1;
}

void task_uptime_count()
{   
    uint32_t uptime_hour,uptime_minute,uptime_second,uptime_day;

    for(;;)
    {   
        uptime_hour = (uptime_count/3600); 
	    uptime_minute = (uptime_count -(3600*uptime_hour))/60;
	    uptime_second = (uptime_count -(3600*uptime_hour)-(uptime_minute*60));
        uptime_day = uptime_hour/24;

        TFT.locate(55,72);
        TFT.printf("%3d days, %2d:h %2d:m %2ds",uptime_day,uptime_hour,uptime_minute,uptime_second);
        uptime_count++;
        Thread::wait(1000);
    }
}

void task_modbus_rtu_tcp_sync()
{
    uint8_t rd_result;
    uint16_t MBdata_s1[0x16], MBdata_s2[0x16];
    uint8_t counter_s1 = 0, counter_s2 = 0;
    /*Add holding regs*/
    modbustcp_s.addHreg(0,0);   //Sensor 1: Frequency
    modbustcp_s.addHreg(1,0);   //Sensor 1: Voltage
    modbustcp_s.addHreg(2,0);   //Sensor 1: Current
    modbustcp_s.addHreg(3,0);   //Sensor 1: Total energy high byte
    modbustcp_s.addHreg(4,0);   //Sensor 1: Total energy low byte
    modbustcp_s.addHreg(5,0);   //Sensor 1: Active power
    modbustcp_s.addHreg(6,0);   //Sensor 1: Reactive power
    modbustcp_s.addHreg(7,0);   //Sensor 1: Power factor

    modbustcp_s.addHreg(8,0);   //Sensor 2: Frequency
    modbustcp_s.addHreg(9,0);   //Sensor 2: Voltage
    modbustcp_s.addHreg(10,0);   //Sensor 2: Current
    modbustcp_s.addHreg(11,0);   //Sensor 2: Total energy high byte
    modbustcp_s.addHreg(12,0);   //Sensor 2: Total energy low byte
    modbustcp_s.addHreg(13,0);   //Sensor 2: Active power
    modbustcp_s.addHreg(14,0);   //Sensor 2: Reactive power
    modbustcp_s.addHreg(15,0);   //Sensor 2: Power factor

    /*Error flags*/
    modbustcp_s.addHreg(16,0);   //Sensor 1 error (if error then 1)
    modbustcp_s.addHreg(17,0);  //Sensor 2 error 
    modbustcp_s.addHreg(18,0);  //Sensor 1 counter (max 15)
    modbustcp_s.addHreg(19,0);  //Sensor 2 counter

    /*Uptime count*/
    modbustcp_s.addHreg(20,0);

    Thread::wait(1000);

    for(;;)
    {
        LedYellow = !LedYellow.read(); //Status led
        rd_result = modbusrtu_m_s1.readHoldingRegisters(0x00, 0x16);
        /*After every read, count up 1 to 15*/
        if(counter_s1 == 16) counter_s1 = 0;
        modbustcp_s.Hreg(18,counter_s1++);

        if (rd_result == modbusrtu_m_s1.ku8MBSuccess)
        {
            /*Get data from S1*/
            for(int i=0x00;i<0x16;i++) MBdata_s1[i] = modbusrtu_m_s1.getResponseBuffer(i);
            uint32_t l_total_energy_s1 = (uint32_t)MBdata_s1[0x00]<<16|MBdata_s1[0x01];
            
            /*Sync TCP with RTU*/
            modbustcp_s.Hreg(0,MBdata_s1[0x11]);
            modbustcp_s.Hreg(1,MBdata_s1[0x0C]);
            modbustcp_s.Hreg(2,MBdata_s1[0x0D]);
            modbustcp_s.Hreg(3,MBdata_s1[0x00]);
            modbustcp_s.Hreg(4,MBdata_s1[0x01]);
            modbustcp_s.Hreg(5,MBdata_s1[0x0E]);
            modbustcp_s.Hreg(6,MBdata_s1[0x0F]);
            modbustcp_s.Hreg(7,MBdata_s1[0x10]);
            modbustcp_s.Hreg(16,0);

            /*Print to debug*/
            
            // pc.printf("Freq: %.02f Hz | Volt: %.01f V | Curr: %.02f A | Total: %.02f KWh | Act: %.03f KW | Rea: %.03f Kvar | PF: %.03f\r\r\n",
            //     ((float)MBdata_s1[0x11])*0.01,                 //Frequency
            //     ((float)MBdata_s1[0x0C])*0.1,                  //Voltage
            //     ((float)MBdata_s1[0x0D])*0.01,                 //Current
            //     ((float)l_total_energy_s1)*0.01,               //Total energy
            //     ((float)((int16_t)MBdata_s1[0x0E]))*0.001,     //Active power
            //     ((float)((int16_t)MBdata_s1[0x0F]))*0.001,     //Reactive power
            //     ((float)MBdata_s1[0x10])*0.001);                //Power Factor

            // pc.printf("S1: Success\r\r\n");
        }
        else
        {
            modbustcp_s.Hreg(16,1);
            // pc.printf("S1: Fail\r\r\n");
        }

        Thread::wait(100);

        rd_result = modbusrtu_m_s2.readHoldingRegisters(0x00, 0x16);
        /*After every read, count up 1 to 15*/
        if(counter_s2 == 16) counter_s2 = 0;
        modbustcp_s.Hreg(19,counter_s2++);

        if (rd_result == modbusrtu_m_s2.ku8MBSuccess)
        {
            /*Get data from S2*/
            for(int i=0x00;i<0x16;i++) MBdata_s2[i] = modbusrtu_m_s2.getResponseBuffer(i);
            uint32_t l_total_energy_s2 = (uint32_t)MBdata_s2[0x00]<<16|MBdata_s2[0x01];

            /*Sync TCP with RTU*/
            modbustcp_s.Hreg(8,MBdata_s1[0x11]);
            modbustcp_s.Hreg(9,MBdata_s1[0x0C]);
            modbustcp_s.Hreg(10,MBdata_s1[0x0D]);
            modbustcp_s.Hreg(11,MBdata_s1[0x00]);
            modbustcp_s.Hreg(12,MBdata_s1[0x01]);
            modbustcp_s.Hreg(13,MBdata_s1[0x0E]);
            modbustcp_s.Hreg(14,MBdata_s1[0x0F]);
            modbustcp_s.Hreg(15,MBdata_s1[0x10]);
            modbustcp_s.Hreg(17,0);

            // pc.printf("S2: Success\r\r\n");
        }
        else
        {
            modbustcp_s.Hreg(17,1);
            // pc.printf("S2: Fail\r\r\n");
        }

        modbustcp_s.Hreg(20,uptime_count);

        Thread::wait(100);
    }  
}