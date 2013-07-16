#ifndef _CURRENTCOST_H_
#define _CURRENTCOST_H_

/*
 *  CurrentCost.h
 *
 *  Created by Frode Roxrud Gill, 2013.
 *  This code is Public Domain.
 */

#define WATTS_PORT         "12286"
#define TMPR_PORT          "12287"

#define DEFAULT_DEVICE     "/dev/ttyUSB0"
#define ENVIR_BAUDRATE     (57600)
#define ENVIR_PROTOCOL     "8N1"
#define ENVIR_FLOWCONTROL  (ctb::SerialPort::NoFlowControl)
#define ENVIR_SENSOR_COUNT (10) //CurrentCost EnviR can have 10 sensors (<URL: http://www.currentcost.com/product-envir-specifications.html >)

#define MUNIN_INTERVAL     (5*60) //Five minutes, in seconds


#define EXIT_OK                       (0)
#define EXIT_OUT_OF_MEMORY            (1)
#define EXIT_SERIAL_OPEN_FAILED       (2)
#define EXIT_SERVER_INITIALIZE_FAILED (3)
#define EXIT_READ_FAILED              (4)

struct MsgInfo {
	long timestamp;
	int sensor;
	double watts;
	double tmpr;
	MsgInfo* previous;
	MsgInfo* next;
};

extern bool g_terminate;
extern int g_exitcode;
extern MsgInfo* g_msg_info_list;

void InitializeNetwork(const char* device);
void MainNetworkLoop();
void CleanupNetwork();

void InitializeXML();
void ParseXML(char* content, int length);
void CleanupXML();


#endif // _CURRENTCOST_H_
