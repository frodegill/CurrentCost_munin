/*
 *  CurrentCost_network.c
 *
 *  Created by Frode Roxrud Gill, 2013.
 *  This code is Public Domain.
 */

#include "CurrentCost.h"
#include <unistd.h>
#include <netdb.h>
#include "ctb-0.16/ctb.h"


#define MAX(a,b) ((a)>(b) ? (a) : (b))

#define INITIAL_BUFFER_SIZE        (8*1024)
#define BUFFER_SIZE_GROW_TRESHOLD  (1*1024)


ctb::SerialPort* g_serial_port;
int g_serial_fd = -1;
int g_socket_watts_fd = -1;
int g_socket_tmpr_fd = -1;
int g_max_fd = -1;
fd_set g_readfds;

size_t g_buf_size;
char* g_buf;


bool ExpandBuffer(char*& buf, size_t& buf_size, const size_t bytes_read)
{
	//Double the buffer size
	buf_size += buf_size;
	char* new_buf = new char[buf_size+1];
	if (!new_buf)
		return false;

	memcpy(new_buf, buf, bytes_read);
	delete[] buf;
	buf = new_buf;
	return true;
}

void SendBuffer(int socket_fd, const char* buf, int len)
{
	int offset = 0;
	do {
		offset += send(socket_fd, buf+offset, len-offset, 0);
	} while (offset<len);
}

void DumpMuninData(int socket_fd, bool is_watts)
{
	long start_of_munin_interval = time(NULL) - MUNIN_INTERVAL;
	double watts;
	double tmpr, total_tmpr=0.0;
	int count, total_tmpr_count=0;
	MsgInfo* msg_iter;
	int i;

	char buf[100];
	for (i=0; ENVIR_SENSOR_COUNT>i; i++)
	{
		watts = 0;
		tmpr = 0.0;
		count = 0;
		msg_iter = g_msg_info_list;
		while (msg_iter)
		{
			if (msg_iter->sensor==i && msg_iter->timestamp>=start_of_munin_interval)
			{
				count++;
				watts += msg_iter->watts;
				tmpr += msg_iter->tmpr;
			}
			msg_iter = msg_iter->next;
		}

		if (0 < count)
		{
			watts /= count;
			tmpr /= count;
		}

		if (is_watts)
		{
			sprintf(buf, "sensor%d.value %f\n", i, watts);
			SendBuffer(socket_fd, buf, strlen(buf));
		}
		else if (0.0 != tmpr)
		{
			total_tmpr += tmpr;
			total_tmpr_count++;
		}
	}

	if (!is_watts)
	{
		if (total_tmpr_count > 1)
		{
			total_tmpr /= total_tmpr_count;
		}
		sprintf(buf, "sensor.value %f\n", total_tmpr);
		SendBuffer(socket_fd, buf, strlen(buf));
	}
}

void HandleClient(int socket_fd, bool is_watts)
{
	struct sockaddr_storage their_addr;
	socklen_t addr_size = sizeof their_addr;
	int new_fd = accept(socket_fd, (struct sockaddr*)&their_addr, &addr_size);
	DumpMuninData(new_fd, is_watts);
	close(new_fd);
}

int InitializeServer(const char* port)	// (code based on Beej's Guide to Network Programming example)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
	struct addrinfo* res;
	int status;
	if (0 != (status=getaddrinfo(NULL, port, &hints, &res)))
		return -1;

	int socket_fd = -1;
	int yes=1;        // for setsockopt() SO_REUSEADDR, below
	struct addrinfo* p;
	for (p = res; p != NULL; p = p->ai_next) {
		socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (socket_fd < 0) { 
			continue;
		}

		// lose the pesky "address already in use" error message
		setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		if (bind(socket_fd, p->ai_addr, p->ai_addrlen) < 0) {
			close(socket_fd);
			continue;
		}
		break;
	}
	freeaddrinfo(res); // all done with this

	if (-1!=socket_fd && -1==listen(socket_fd, 5)) {
		socket_fd = -1;
	}
    
 	return socket_fd;
}

void InitializeNetwork(const char* device)
{
	fprintf(stdout, "Connecting to %s\n", device);

	//Initialize CurrentCost EnviR
	g_serial_port = new ctb::SerialPort();
	g_serial_fd = g_serial_port->Open(device, ENVIR_BAUDRATE, ENVIR_PROTOCOL, ENVIR_FLOWCONTROL);
	if (0>g_serial_fd || !g_serial_port->IsOpen())
	{
		delete g_serial_port;
		g_serial_port = NULL;
	}

	if (!g_serial_port)
	{
		g_exitcode = EXIT_SERIAL_OPEN_FAILED;
		fprintf(stderr, "SerialPort Open failed\n");
		g_terminate = true;
		return;
	}

	//Initialize server
	fprintf(stdout, "Listening for WATTS on port %s\n", WATTS_PORT);
	g_socket_watts_fd = InitializeServer(WATTS_PORT);
	fprintf(stdout, "Listening for TMPR on port %s\n", TMPR_PORT);
	g_socket_tmpr_fd = InitializeServer(TMPR_PORT);
	if (-1==g_socket_watts_fd || -1==g_socket_watts_fd) {
		g_exitcode = EXIT_SERVER_INITIALIZE_FAILED;
		fprintf(stderr, "Initializing server failed\n");
		g_terminate = true;
		return;
	}

	g_buf_size = INITIAL_BUFFER_SIZE;
	g_buf = new char[g_buf_size+1];
	if (!g_buf)
	{
		g_exitcode = EXIT_OUT_OF_MEMORY;
		fprintf(stderr, "Out Of Memory\n");
		g_terminate = true;
		return;
	}
	g_buf[g_buf_size] = 0;
}

void MainNetworkLoop()
{
	int buf_offset = 0;
	int bytes_read;
	char* end_element;
	int msg_len;

	g_max_fd = MAX(g_serial_fd, MAX(g_socket_watts_fd, g_socket_tmpr_fd));

	while (!g_terminate)
	{
		//Set sockets to listen on
		FD_ZERO(&g_readfds);
		if (-1 != g_serial_fd)
			FD_SET(g_serial_fd, &g_readfds);

		if (-1 != g_socket_watts_fd)
			FD_SET(g_socket_watts_fd, &g_readfds);

		if (-1 != g_socket_tmpr_fd)
			FD_SET(g_socket_tmpr_fd, &g_readfds);

		//Wait for content/connections
		select(g_max_fd+1, &g_readfds, NULL, NULL, NULL);

		//Respond
		if (FD_ISSET(g_socket_watts_fd, &g_readfds))
		{
			HandleClient(g_socket_watts_fd, true);
		}
		if (FD_ISSET(g_socket_tmpr_fd, &g_readfds))
		{
			HandleClient(g_socket_tmpr_fd, false);
		}
		if (FD_ISSET(g_serial_fd, &g_readfds))
		{
			bytes_read = g_serial_port->Read(g_buf+buf_offset, g_buf_size-buf_offset);

			if (0 > bytes_read) //Read failed. Abort
			{
				g_exitcode = EXIT_READ_FAILED;
				fprintf(stderr, "SerialPort Read failed\n");
				g_terminate = true;
				continue;
			}
			else if (0 == bytes_read) //Connection closed
			{
				g_exitcode = EXIT_READ_FAILED;
				fprintf(stderr, "SerialPort connection closed\n");
				g_terminate = true;
				continue;
			}
			else //We got content!
			{
				buf_offset += bytes_read;
				while (true)
				{
					g_buf[buf_offset] = 0; //prepare for strstr
					end_element = strstr(g_buf, "</msg>\r\n"); //Search for end of XML (we can get start of next content)
					msg_len = end_element ? (end_element+8 /*sizeof("</msg>\r\n")*/) - g_buf : -1;
					if (0 >= msg_len)
						break; //We don't have more complete XML buffers!

					ParseXML(g_buf, msg_len);

					if (msg_len < buf_offset) //We have more than the parsed buffer. Move remainder to start of buffer
					{
						memmove(g_buf, g_buf+msg_len, buf_offset-msg_len);
						buf_offset -= msg_len;
					}
					else //We've parsed everything. Start fresh
					{
						buf_offset = 0;
					}
				}

				//If we didn't parse everything, check if we need to grow buffer
				if (BUFFER_SIZE_GROW_TRESHOLD > (g_buf_size-buf_offset)) //If we are getting close to end of buffer, grow buffer
				{
					if (!ExpandBuffer(g_buf, g_buf_size, buf_offset))
					{
						g_exitcode = EXIT_OUT_OF_MEMORY;
						fprintf(stderr, "Out Of Memory\n");
						g_terminate = true;
						continue;
					}
				}
			}
		}
	}
}

void CleanupNetwork()
{
	delete[] g_buf;

	close(g_socket_watts_fd);
	close(g_socket_tmpr_fd);
	close(g_serial_fd);
	delete g_serial_port;
}
