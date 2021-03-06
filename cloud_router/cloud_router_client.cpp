#include "cloud_router_client.h"

#include <iostream>
#include <sstream>
#include <syslog.h>


extern sCloudRouterVerbose CR_VERBOSE();


cCR_Receiver_service::cCR_Receiver_service(const char *token, int32_t sensor_id) {
	this->token = token;
	this->sensor_id = sensor_id;
	port = 0;
	connection_ok = false;
}

bool cCR_Receiver_service::start(string host, u_int16_t port) {
	this->host = host;
	this->port = port;
	_receive_start();
	return(true);
}

bool cCR_Receiver_service::receive_process_loop_begin() {
	if(connection_ok) {
		if(receive_socket->isError()) {
			_close();
			connection_ok = false;
		} else {
			return(true);
		}
	}
	if(!receive_socket) {
		_connect(host, port, 5);
	}
	if(!receive_socket) {
		return(false);
	}
	string connectCmd = "{\"type_connection\":\"sniffer_service\"}\r\n";
	if(!receive_socket->write(connectCmd)) {
		if(receive_socket->isError()) {
			receive_socket->logError();
		} else {
			receive_socket->setError("failed send command sniffer_service");
		}
		_close();
		return(false);
	}
	string rsltConnectCmd;
	if(!receive_socket->readBlock(&rsltConnectCmd) || rsltConnectCmd != "OK") {
		if(receive_socket->isError()) {
			receive_socket->logError();
		} else {
			receive_socket->setError("failed read command ok");
		}
		_close();
		return(false);
	}
	// TODO: encrypt token
	string connectData = "{\"token\":\"" + token + "\",\"sensor_id\":" + intToString(sensor_id) + "}";
	if(!receive_socket->writeBlock(connectData)) {
		if(receive_socket->isError()) {
			receive_socket->logError();
		} else {
			receive_socket->setError("failed send connection data");
		}
		_close();
		return(false);
	}
	string rsltConnectData;
	if(!receive_socket->readBlock(&rsltConnectData) || rsltConnectData != "OK") {
		if(receive_socket->isError()) {
			receive_socket->logError();
		} else {
			receive_socket->setError("failed read connection data ok");
		}
		_close();
		return(false);
	}
	connection_ok = true;
	if(CR_VERBOSE().start_client) {
		ostringstream verbstr;
		verbstr << "START SNIFFER SERVICE";
		syslog(LOG_INFO, "%s", verbstr.str().c_str());
	}
	return(true);
}

void cCR_Receiver_service::evData(u_char *data, size_t dataLen) {
	receive_socket->writeBlock("OK");
	string idCommand = string((char*)data, dataLen);
	size_t idCommandSeparatorPos = idCommand.find('/'); 
	if(idCommandSeparatorPos != string::npos) {
		cCR_Client_response *response = new cCR_Client_response(idCommand.c_str() + idCommandSeparatorPos + 1, atoll(idCommand.c_str()));
		response->start(receive_socket->getHost(), receive_socket->getPort());
	}
}


cCR_Client_response::cCR_Client_response(const char *command, u_int64_t gui_task_id) {
	this->command = command;
	this->gui_task_id = gui_task_id;
}

bool cCR_Client_response::start(string host, u_int16_t port) {
	if(!_connect(host, port)) {
		return(false);
	}
	string connectCmd = "{\"type_connection\":\"sniffer_response\",\"gui_task_id\":" + intToString(gui_task_id) + "}\r\n";
	if(!client_socket->write(connectCmd)) {
		delete client_socket;
		client_socket = NULL;
		return(false);
	}
	
	_client_start();
	
	return(true);
	
}

void cCR_Client_response::client_process() {
	extern int parse_command(string cmd, int client, cCR_Client_response *cr_client);
	parse_command(command, 0, this);
	delete this;
}

bool cCR_Client_response::write(u_char *data, size_t dataLen) {
	return(client_socket->write(data, dataLen));
}

bool cCR_Client_response::writeEnc(u_char *data, size_t dataLen, const char *key) {
	client_socket->setKey(key);
	return(client_socket->writeEnc(data, dataLen));
}
