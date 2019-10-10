/* mbed Microcontroller Library
 * Copyright (c) 2018 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"
#include "ThisThread.h"
#include "stats_report.h"

#include "Thread.h"
#include "EthernetInterface.h"
#include "TCPServer.h"
#include "TCPSocket.h"

#include "stdio.h"

DigitalOut led1(LED1);

#define HTTP_STATUS_LINE "HTTP/1.0 200 OK"
#define HTTP_HEADER_FIELDS "Content-Type: text/html; charset=utf-8"
#define HTTP_MESSAGE_BODY ""                                     \
"<html>" "\r\n"                                                  \
"  <body style=\"display:flex;text-align:center\">" "\r\n"       \
"    <div style=\"margin:auto\">" "\r\n"                         \
"      <h1>Hello World</h1>" "\r\n"                              \
"      <p>It works !</p>" "\r\n"                                 \
"    </div>" "\r\n"                                              \
"  </body>" "\r\n"                                               \
"</html>"

#define HTTP_RESPONSE HTTP_STATUS_LINE "\r\n"   \
                      HTTP_HEADER_FIELDS "\r\n" \
                      "\r\n"                    \
                      HTTP_MESSAGE_BODY "\r\n"

#define HTTP_GYRO_RESPONSE	HTTP_STATUS_LINE "\r\n"   \
        					HTTP_HEADER_FIELDS "\r\n" \
							"\r\n"
#define HTTP_CTRL_RESPONSE	HTTP_STATUS_LINE "\r\n"   \
							HTTP_HEADER_FIELDS "\r\n" \
							"\r\n"					  \
							"OK\r\n"

#define SLEEP_TIME                  500 // (msec)
#define PRINT_AFTER_N_LOOPS         20
#define PORT						1111
#define PORT_Gyro					2222
#define PORT_Recv					3333


#define    MPU9250_ADDRESS            0x69
#define    MAG_ADDRESS                0x0C
#define    GYRO_FULL_SCALE_250_DPS    0x00
#define    GYRO_FULL_SCALE_500_DPS    0x08
#define    GYRO_FULL_SCALE_1000_DPS   0x10
#define    GYRO_FULL_SCALE_2000_DPS   0x18
#define    ACC_FULL_SCALE_2_G        0x00
#define    ACC_FULL_SCALE_4_G        0x08
#define    ACC_FULL_SCALE_8_G        0x10
#define    ACC_FULL_SCALE_16_G       0x18


EthernetInterface eth1;

TCPServer svr1;
TCPServer svr_gyro;
TCPServer svr_recv;

TCPSocket clt_sock;
SocketAddress clt_addr;

I2C i2c(I2C_SDA, I2C_SCL);

char *buff;

void I2CwriteByte(uint8_t Register, uint8_t Data)
{
	char data_send[2];
	data_send[0] = Register;
	data_send[1] = Data;
	i2c.write(MPU9250_ADDRESS, data_send, 2);
}

struct gyro_data_t
{
	// Accelerometer
	uint16_t ax;
	uint16_t ay;
	uint16_t az;
	// Gyroscope
	uint16_t gx;
	uint16_t gy;
	uint16_t gz;
};

struct gyro_data_t Get_Gyro()
{
	char data[14];
	char cmd;
	struct gyro_data_t gyro_data;
	cmd = 0x3B;
	uint16_t res;

	i2c.write(0xD0, &cmd, 1, true);
	i2c.read(0xD1, data, 14, false);

	gyro_data.ax = (data[0]<<8) | data[1];
	gyro_data.ay = (data[2]<<8) | data[3];
	gyro_data.az = (data[4]<<8) | data[5];

	gyro_data.gx = (data[8]<<8) | data[9];
	gyro_data.gy = (data[10]<<8) | data[11];
	gyro_data.gz = (data[12]<<8) | data[13];

	return gyro_data;

}
// main() runs in its own thread in the OS
void task1()
{
	 while (true)
	 {
		 svr1.accept(&clt_sock, &clt_addr);
		 printf("[Task 1]: accept %s:%d\n", clt_addr.get_ip_address(), clt_addr.get_port());
		 clt_sock.send(HTTP_RESPONSE, strlen(HTTP_RESPONSE));
		 clt_sock.close();
	}
}

void task2()
{
	struct gyro_data_t gyro_data;
	while(true)
	{
		memset(buff, 0, 256);
		svr_gyro.accept(&clt_sock,  &clt_addr);
		gyro_data = Get_Gyro();
		printf("[Task 2]: accept %s:%d\n", clt_addr.get_ip_address(), clt_addr.get_port());
		sprintf(buff, "%sGyro data\r\n"
				"ax: %d\r\n"
				"ay: %d\r\n"
				"az: %d\r\n"
				"gx: %d\r\n"
				"gy: %d\r\n"
				"gz: %d\r\n", \
				HTTP_GYRO_RESPONSE, gyro_data.ax, gyro_data.ay, gyro_data.az, \
				gyro_data.gx, gyro_data.gy, gyro_data.gx);
		printf("[Task 2]: %s", buff);
		clt_sock.send(buff, strlen(buff));
		clt_sock.close();
	}
}

void task3()
{
	while(true)
	{
		memset(buff, 0, 256);
		svr_recv.accept(&clt_sock, &clt_addr);
		printf("[Task 3]: accept %s:%d\r\n", clt_addr.get_ip_address(), clt_addr.get_port());
		clt_sock.recv(buff, 256);
		printf("[Task 3]: DATA RECIEVE:\r\n%s\r\n", buff);
		if(strstr(buff, "LED_ON") != NULL)
		{
			led1 = 1;
		}
		else if(strstr(buff, "LED_OFF") != NULL)
		{
			led1 = 0;
		}
		clt_sock.send(HTTP_CTRL_RESPONSE, strlen(HTTP_CTRL_RESPONSE));
		clt_sock.close();
	}
}

void task4()
{
	while(true)
	{
		printf("[task 4]: say konichiwa\n");
		ThisThread::sleep_for(2000);
	}
}

void task5()
{
	while(true)
	{
		printf("[task 5]: say kombanwa\n");
		ThisThread::sleep_for(2500);
	}
}
int main()
{
	printf("start program\n");
	buff = (char*)malloc(256);
	SystemReport sys_state( SLEEP_TIME * PRINT_AFTER_N_LOOPS /* Loop delay time in ms */);

	/* set up ethernet interface */
	eth1.set_as_default();
	eth1.connect();
	printf("IP address: %s\r\n", eth1.get_ip_address());

	/* set up TCP/IP socket */
	nsapi_error_t res;
	svr1.open(&eth1);										//open server on ethernet
	if(res = svr1.bind(eth1.get_ip_address(), PORT) < 0)	// Bind the HTTP port (TCP 80) to the server
	{
		printf("[Error %d] bind failed\r\n", res);
		return -1;
	}
	else
	{
		printf("Bind successed\r\n");
	}

	svr1.listen(2);											// server can handle 2 simultaneous connections

	svr_gyro.open(&eth1);
	svr_gyro.bind(eth1.get_ip_address(), PORT_Gyro);
	svr_gyro.listen(2);

	svr_recv.open(&eth1);
	svr_recv.bind(eth1.get_ip_address(), PORT_Recv);
	svr_recv.listen(2);
	/* set up I2C */
	i2c.frequency(100);



//	I2CwriteByte(29,0x06);									// Set accelerometers low pass filter at 5Hz
//	I2CwriteByte(26,0x06);									// Set gyroscope low pass filter at 5Hz
//	I2CwriteByte(27,GYRO_FULL_SCALE_1000_DPS);				// Configure gyroscope range
//	I2CwriteByte(28,ACC_FULL_SCALE_4_G);					// Configure accelerometers range
//	I2CwriteByte(0x37,0x02);								// Set by pass mode for the magnetometers
//	I2CwriteByte(0x0A,0x16);								// Request continuous magnetometer measurements in 16 bits

	Thread th1(osPriorityNormal, 1000, NULL);
	Thread th2(osPriorityNormal, 1000, NULL);
	Thread th3(osPriorityNormal, 1000, NULL);
//	Thread th4(osPriorityNormal3, 1000, NULL);
//	Thread th5(osPriorityNormal4, 1000, NULL);
	th1.start(task1);
	th2.start(task2);
	th3.start(task3);
//	th4.start(task4);
//	th5.start(task5);

	 while(true)
	 {

	 }
}
