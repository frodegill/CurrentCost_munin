/*
 *  CurrentCost_xml.c
 *
 *  Created by Frode Roxrud Gill, 2013.
 *  This code is Public Domain.
 */

#include "CurrentCost.h"

#include <time.h>
#include <libxml/parser.h>
#include <libxml/tree.h>


MsgInfo* g_msg_info_list;


void InitializeXML()
{
	LIBXML_TEST_VERSION
}

void CleanupXML()
{
	xmlCleanupParser();

	//Cleanup list
	MsgInfo* tmp;
	while (g_msg_info_list)
	{
		tmp = g_msg_info_list;
		g_msg_info_list = g_msg_info_list->next;
		delete tmp;
	}
}

void AddMsgInfo(long timestamp, int sensor, double watts, double tmpr)
{
	MsgInfo* msg_info = new MsgInfo();
	msg_info->timestamp = timestamp;
	msg_info->sensor = sensor;
	msg_info->watts = watts;
	msg_info->tmpr = tmpr;

	//Insert at start of list
	msg_info->previous = NULL;
	msg_info->next = g_msg_info_list;
	if (g_msg_info_list)
	{
		g_msg_info_list->previous = msg_info;
	}
	g_msg_info_list = msg_info;

	//Delete obsolete items from list
	long timestamp_to_obsolete = timestamp - MUNIN_INTERVAL;
	MsgInfo* msg_info_iter = g_msg_info_list;
	MsgInfo* tmp;
	while (msg_info_iter)
	{
		tmp = msg_info_iter;
		msg_info_iter = msg_info_iter->next;

		if (tmp->timestamp < timestamp_to_obsolete)
		{
			if (tmp->previous)
				tmp->previous->next = tmp->next;

			if (tmp->next)
				tmp->next->previous = tmp->previous;

			delete tmp;
		}
	}
}

_xmlNode* FindNode(_xmlNode* node_iter, const unsigned char* child_name)
{
	while (node_iter)
	{
		if (XML_ELEMENT_NODE==node_iter->type && 0==xmlStrcmp(child_name, node_iter->name))
			return node_iter;

		node_iter = node_iter->next;
	}
	return NULL;
}

xmlChar* GetContent(_xmlNode* node_iter, const unsigned char* child_name)
{
	_xmlNode* node = FindNode(node_iter, child_name);
	if (!node)
		return NULL;

	_xmlNode* value_node = node->children;
	return (value_node && XML_TEXT_NODE==value_node->type) ? value_node->content : NULL;
}

double GetValue(_xmlNode* node_iter, const unsigned char* child_name)
{
	xmlChar* content = GetContent(node_iter, child_name);
	if (!content)
		return -1.0;

	int sign = 1;
	double value1 = 0.0;
	xmlChar* tmp = content;
	if ('-' == *tmp)
	{
		sign = -1;
		tmp++;
	}
	while ('0'<=*tmp && '9'>=*tmp)
	{
		value1 = 10*value1 + *(tmp++)-'0';
	}

	if ('.' == *tmp)
	{
		tmp++;
		double value2 = 0.0;
		double factor = 0.1;
		while ('0'<=*tmp && '9'>=*tmp)
		{
			value2 += factor*(*(tmp++)-'0');
			factor /= 10.0;
		}
		value1 += value2;
	}
	return sign*value1;
}

double GetTotalWatts(_xmlNode* node_iter)
{
	double total_watts=0.0, current_watts=0.0;
	char ch_string[16];
	int ch = 0;
	_xmlNode* ch_node;
	_xmlNode* watts_node;
	do
	{
		snprintf(ch_string, sizeof(ch_string)/sizeof(ch_string[0])-1, "ch%d", ++ch);
		ch_node = FindNode(node_iter, BAD_CAST ch_string);
		watts_node = ch_node ? FindNode(ch_node->children, BAD_CAST "watts") : NULL;
		if (watts_node)
		{
			current_watts = GetValue(watts_node, BAD_CAST "watts");
			if (-1 != current_watts)
				total_watts += current_watts;
		}
	} while(ch_node);

	return total_watts;
}

void ParseXML(char* content, int length)
{
	_xmlNode* msg;
	_xmlNode* hist;
	long current_time = time(NULL); //Ignore time in XML, use current system timestamp instead
	double tmpr = 0.0;
	int sensor = 0;
	double watts = 0.0;

	xmlDocPtr xml_doc = xmlParseMemory(content, length);
	if (!xml_doc) {
		fprintf(stderr, "Failed to parse document\n");
		goto DoneParsing;
	}

	if (!xml_doc) //If we don't have a document, don't parse it
		goto DoneParsing;

	msg = FindNode(xml_doc->children, BAD_CAST "msg");
	if (!msg) //If we don't have <msg>, it is not Current Cost CC128 Display Unit XML
		goto DoneParsing;

	hist = FindNode(msg->children, BAD_CAST "hist");
	if (hist) //If we have <hist>, we have history data (which we are not interested in)
		goto DoneParsing;

	tmpr = GetValue(msg->children, BAD_CAST "tmpr");
	sensor = GetValue(msg->children, BAD_CAST "sensor");
	watts = GetTotalWatts(msg->children);

	if (0<=sensor && 0.0<=watts)
	{
		AddMsgInfo(current_time, sensor, watts, tmpr);
	}

DoneParsing:
	xmlFreeDoc(xml_doc);
}
