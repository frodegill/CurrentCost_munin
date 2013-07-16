/*
 *  CurrentCost_main.c
 *
 *  Created by Frode Roxrud Gill, 2013.
 *  This code is Public Domain.
 */

#include "CurrentCost.h"


bool g_terminate = false;
int g_exitcode = EXIT_OK;

int main(int argc, char* argv[])
{
	InitializeXML();
	InitializeNetwork(2<=argc ? argv[1] : DEFAULT_DEVICE);

	MainNetworkLoop();

	CleanupNetwork();
	CleanupXML();
	
	return g_exitcode;
}
