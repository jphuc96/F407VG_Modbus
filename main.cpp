#include "mbed.h"
#include "EthernetInterface.h"
#include "ModbusMaster.h"
#include "ModBusTCP.h"

/*Basic configurations*/
DigitalOut LedRed(PD_12);
DigitalOut LedBlue(PD_13);
DigitalOut LedGreen(PD_14);
DigitalOut LedYellow(PD_15);

Serial pc(PA_9,PA_10,115200);

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

/*OS Threads*/
Thread thread_modbusrtu_routine(osPriorityNormal,1024,NULL,"modbusrtu_routine");
Thread thread_update_registers(osPriorityNormal,OS_STACK_SIZE,NULL,"update_registers");
void task_update_registers();
void task_modbusrtu_routine();

int main()
{
    LedBlue = 1;
    wait_ms(100);
    LedBlue = 0;

    pc.printf("===========================\r\r\n");
    pc.printf("====mbed Modbus TCP-RTU====\r\r\n");
    pc.printf("===========================\r\r\n");

    pc.printf("Initializing EthernetInterface.\r\r\n");
    eth.connect();
    pc.printf("MAC: %s\r\r\n",eth.get_mac_address());
    pc.printf("IP: %s\r\r\n",eth.get_ip_address());
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

    modbustcp_s.addHreg(0,100);
    modbustcp_s.addHreg(1,200);
    modbustcp_s.addHreg(2,300);
    modbustcp_s.addHreg(3,400);

    modbustcp_s.start(&eth,MBTCP_PORT);
    pc.printf("ModbusTCP server started at port %d\r\r\n",MBTCP_PORT);

    /*Start threads here*/
    // thread_update_registers.start(task_update_registers);
    thread_modbusrtu_routine.start(task_modbusrtu_routine);

    while(1)
    {
        LedGreen = 0;
        Thread::wait(500);
        LedGreen = 1;
        Thread::wait(500);
    }
    return 1;
}

void task_update_registers()
{
    //update registers here, need to check Mutex
    
    modbustcp_s.addHreg(0,100);
    modbustcp_s.addHreg(1,200);
    modbustcp_s.addHreg(2,300);
    modbustcp_s.addHreg(3,400);

    AnalogIn cpu_temp(ADC_TEMP);
    AnalogIn vbat(ADC_VBAT);
    for(;;)
    { 
        modbustcp_s.Hreg(0,cpu_temp.read_u16());
        modbustcp_s.Hreg(1,vbat.read_u16());
        modbustcp_s.Hreg(2,rand());
        modbustcp_s.Hreg(3,rand());

        Thread::wait(500);
    }
}

void task_modbusrtu_routine()
{
    uint8_t rd_result;
    uint16_t MBdata[0x16];
    for(;;)
    {
        rd_result = modbusrtu_m_s1.readHoldingRegisters(0x00, 0x16);
        if (rd_result == modbusrtu_m_s1.ku8MBSuccess)
        {
            for(int i=0x00;i<0x16;i++){
                MBdata[i] = modbusrtu_m_s1.getResponseBuffer(i);
            }
            uint32_t total_energy = (uint32_t)MBdata[0]<<16|MBdata[1];

            pc.printf("Freq: %.02f Hz | Volt: %.01f V | Curr: %.02f A | Total: %.02f KWh | Act: %.03f KW | Rea: %.03f Kvar | PF: %.03f\r\r\n",
                ((float)MBdata[0x11])*0.01,                 //Frequency
                ((float)MBdata[0x0C])*0.1,                  //Voltage
                ((float)MBdata[0x0D])*0.01,                 //Current
                ((float)total_energy)*0.01,                 //Total energy
                ((float)((int16_t)MBdata[0x0E]))*0.001,     //Active power
                ((float)((int16_t)MBdata[0x0F]))*0.001,     //Reactive power
                ((float)MBdata[0x10])*0.001);               //Power Factor

        }
        else
        {
            pc.printf("Error...\r\r\n");
        }
        Thread::wait(100);
    }  
}