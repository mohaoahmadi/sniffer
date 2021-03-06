#ifndef CLOUD_ROUTER_BASE_H
#define CLOUD_ROUTER_BASE_H


#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <map>
#include <string>
#include <arpa/inet.h>

#include "cloud_router.h"

#ifdef CLOUD_ROUTER_CLIENT
#include "../tools.h"
#else
#include "tools.h"
#endif


using namespace std;


#define SYNC_LOCK_USLEEP 100


struct sCloudRouterVerbose {
	sCloudRouterVerbose() {
		memset(this, 0, sizeof(*this));
		start_server = true;
		start_client = true;
		connect_command = true;
		connect_info = true;
	}
	bool start_server;
	bool start_client;
	bool socket_connect;
	bool connect_command;
	bool connect_info;
	bool create_thread;
};


class cResolver {
private:
	struct sIP_time {
		u_int32_t ipl;
		time_t at;
	};
public:
	cResolver();
	u_int32_t resolve(const char *host);
	u_int32_t resolve(string &host) {
		return(resolve(host.c_str()));
	}
private:
	void lock() {
		while(__sync_lock_test_and_set(&_sync_lock, 1)) {
			if(SYNC_LOCK_USLEEP) {
				usleep(SYNC_LOCK_USLEEP);
			}
		}
	}
	void unlock() {
		__sync_lock_release(&_sync_lock);
	}
private:
	bool use_lock;
	bool res_timeout;
	map<string, sIP_time> res_table;
	volatile int _sync_lock;
};


class cSocket {
public:
	enum eSocketError {
		_se_na,
		_se_bad_connection,
		_se_error_str
	};
	struct sTimeouts {
		sTimeouts() {
			await = 1;
			read = 1;
			write = 1;
		}
		int await;
		int read;
		int write;
	};
public:
	cSocket(const char *name, bool autoClose = false);
	virtual ~cSocket();
	void setHostPort(string host, u_int16_t port);
	void setKey(string key);
	bool connect(unsigned loopSleepS = 0);
	bool listen();
	void close();
	bool await(cSocket **clientSocket);
	bool write(u_char *data, size_t dataLen);
	bool write(const char *data);
	bool write(string &data);
	bool _write(u_char *data, size_t *dataLen);
	bool read(u_char *data, size_t *dataLen);
	bool writeEnc(u_char *data, size_t dataLen);
	bool readDec(u_char *data, size_t *dataLen);
	void encodeWriteBuffer(u_char *data, size_t dataLen);
	void decodeReadBuffer(u_char *data, size_t dataLen);
	bool checkHandleRead();
	bool checkHandleWrite();
	bool okHandle() {
		return(handle >= 0);
	}
	bool isError() {
		return(error != _se_na || !error_str.empty());
	}
	string getError() {
		return(error == _se_error_str ?
			(!error_str.empty() ? error_str : "unknown error") :
		       error == _se_bad_connection ?
			"bad connection":
		       error == _se_na ?
		        "" :
		        "unknown error");
	}
	void logError();
	string getHost() {
		return(host.empty() ? getIP() : host);
	}
	string getIP() {
		return(inet_ntostring(htonl(ipl)));
	}
	u_int16_t getPort() {
		return(port);
	}
	string getHostPort() {
		return(getHost() + " : " + intToString(port));
	}
	bool isTerminate() {
		return(terminate);
	}
	void setError(eSocketError error);
	void setError(const char *formatError, ...);
protected:
	void clearError();
	void sleep(int s);
protected:
	string name;
	bool autoClose;
	string host;
	u_int16_t port;
	u_int32_t ipl;
	sTimeouts timeouts;
	int handle;
	bool enableWriteReconnect;
	bool terminate;
	eSocketError error;
	string error_str;
	string key;
	u_int64_t writeEncPos;
	u_int64_t readDecPos;
};


class cSocketBlock : public cSocket {
public:
	struct sBlockHeader {
		sBlockHeader() {
			init();
		}
		void init() {
			memset(this, 0, sizeof(sBlockHeader));
			strcpy(header, "vm_cloud_router_block_header");
		}
		bool okHeader() {
			return(!strncmp(header, "vm_cloud_router_block_header", 28));
		}
		char header[30];
		u_int32_t length;
		u_int32_t sum;
	};
	struct sReadBuffer {
		sReadBuffer() {
			init();
		}
		~sReadBuffer() {
			clear();
		}
		void add(u_char *data, size_t dataLen) {
			if(!data || !dataLen) {
				return;
			}
			if(!buffer || capacity < length + dataLen) {
				if(!buffer) {
					capacity = dataLen * 2;
					buffer = new u_char[capacity];
				} else {
					capacity = length + dataLen * 2;
					u_char *buffer_new = new u_char[capacity];
					memcpy(buffer_new, buffer, length);
					delete [] buffer;
					buffer = buffer_new;
				}
			}
			memcpy(buffer + length, data, dataLen);
			length += dataLen;
		}
		void clear() {
			if(buffer) {
				delete [] buffer;
			}
			init();
		}
		void init() {
			buffer = NULL;
			length = 0;
			capacity = 0;
		}
		bool okBlockHeader() {
			return(length >= sizeof(sBlockHeader) &&
			       ((sBlockHeader*)buffer)->okHeader());
		}
		u_int32_t lengthBlockHeader(bool addHeaderSize = false) {
			return(length >= sizeof(sBlockHeader) ?
				((sBlockHeader*)buffer)->length + (addHeaderSize ? sizeof(sBlockHeader) : 0) :
				0);
		}
		u_int32_t sumBlockHeader() {
			return(length >= sizeof(sBlockHeader) ?
				((sBlockHeader*)buffer)->sum :
				0);
		}
		u_char *buffer;
		size_t length;
		size_t capacity;
	};
public:
	cSocketBlock(const char *name, bool autoClose = false);
	bool writeBlock(u_char *data, size_t dataLen, string key = "");
	bool writeBlock(string str, string key = "");
	u_char *readBlock(size_t *dataLen, string key = "");
	bool readBlock(string *str, string key = "");
	string readLine(u_char **remainder = NULL, size_t *remainder_length = NULL);
protected:
	bool checkSumReadBuffer();
	u_int32_t dataSum(u_char *data, size_t dataLen);
protected:
	sReadBuffer readBuffer;
};


class cServer {
public:
	 cServer();
	 virtual ~cServer();
	 bool listen_start(const char *name, string host, u_int16_t port);
	 static void *listen_process(void *arg);
	 void listen_process();
	 virtual void createConnection(cSocket *socket);
protected:
	 cSocketBlock *listen_socket;
	 pthread_t listen_thread;
};


class cServerConnection {
public:
	cServerConnection(cSocket *socket);
	virtual ~cServerConnection();
	bool connection_start();
	static void *connection_process(void *arg);
	virtual void connection_process();
	virtual void evData(u_char *data, size_t dataLen);
protected:
	cSocketBlock *socket;
	pthread_t thread;
};


class cReceiver {
public:
	cReceiver();
	virtual ~cReceiver();
	bool receive_start(string host, u_int16_t port);
	bool _connect(string host, u_int16_t port, unsigned loopSleepS);
	void _close();
	void _receive_start();
	static void *receive_process(void *arg);
	virtual void receive_process();
	virtual bool receive_process_loop_begin();
	virtual void evData(u_char *data, size_t dataLen);
protected:
	cSocketBlock *receive_socket;
	pthread_t receive_thread;
};


class cClient {
public:
	cClient();
	virtual ~cClient();
	bool client_start(string host, u_int16_t port);
	bool _connect(string host, u_int16_t port);
	void _client_start();
	static void *client_process(void *arg);
	virtual void client_process();
protected:
	cSocketBlock *client_socket;
	pthread_t client_thread;
};


#endif //CLOUD_ROUTER_BASE_H
