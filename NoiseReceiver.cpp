// (c) 2018 Oleg Tsybizov (otsybizov@gmail.com). All Rights Reserved.

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iostream>
#include <limits.h>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#if defined _BSD_SOURCE || defined _SVID_SOURCE
# define __USE_MISC 1
#include <netdb.h>
#include <netinet/tcp.h>
#endif

using namespace std;

#define ARG_NUMBER 2
#define MILLISECONDS_PER_SECOND 1000
#define BUFFER_SIZE 150000000

void Usage()
{
	cout << "Usage: NoiseReceiver [PROTOCOL]" << endl << endl;
	cout << "Measures noise bitrate sent by NoiseEmitter communicating using [PROTOCOL] (TCP/UDP)." << endl << endl;
	cout << "[PROTOCOL]	Network protocol: 'tcp' or 'udp'" << endl;
}

class SocketHandle
{
	int hFileM;

public:
	SocketHandle(int hFile) : hFileM(hFile) {}
	~SocketHandle() { if (hFileM >= 0) close(hFileM); }
	operator int() const { return hFileM; }
};

double GetBitrate(size_t bytes, size_t msecs);
void ThreadFunc(int socket, string client);

int main(int argc, char *argv[])
{
	// Parse protocol.
	int socketType{0};
	try
	{
		if (argc < ARG_NUMBER)
		{
			throw "Insufficient number of arguments.";
		}

		string protocol{argv[1]};
		if (protocol == "tcp")
		{
			socketType = static_cast<int>(SOCK_STREAM);
		}
		else if (protocol == "udp")
		{
			socketType = static_cast<int>(SOCK_DGRAM);
		}
		else
		{
			throw "Invalid protocol.";
		}
	}
	catch (const char* msg)
	{
		cout << "ERROR: " << msg << endl << endl;
		Usage();
		return 0;
	}

	SocketHandle hServer = socket(AF_INET, socketType, 0);
    if (hServer == -1)
    {
    	cout << "Couldn't create socket: " << errno << "." << endl;
    	return 0;
    }

    int on = 1;
	(void)setsockopt(hServer, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    sockaddr_in addr_struct;
    socklen_t addr_struct_len = sizeof(addr_struct);
    memset(&addr_struct, 0, sizeof(addr_struct));
    addr_struct.sin_family = AF_INET;
    addr_struct.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(hServer, (sockaddr*) &addr_struct, sizeof(addr_struct)) != 0 )
    {
    	cout << "Socket bind failed: " << errno << "." << endl;
    	return 0;
    }

    if (listen(hServer, 1) != 0)
    {
    	cout << "Socket listen failed: " << errno << "." << endl;
    	return 0;
    }

    // Print address and port the socket is listening on.
    memset(&addr_struct, 0, sizeof(addr_struct));
    if (getsockname(hServer, (sockaddr*)&addr_struct, &addr_struct_len) == -1)
    {
    	cout << "Get address failed: " << errno << "." << endl;
    	return 0;
    }
    cout << "Listening on port " << ntohs(addr_struct.sin_port) << endl;

    while (true)
    {
		memset(&addr_struct, 0, sizeof(addr_struct));
		int hClient = accept(hServer, (sockaddr*)&addr_struct, &addr_struct_len);
		if (hClient == -1)
		{
			cout << "Incoming connection failed." << endl;
			continue;
		}

		string client{inet_ntoa(addr_struct.sin_addr)};
		cout << "Connected to " << client << endl;

		thread t(ThreadFunc, hClient, client);
    }

	return 0;
}

void ThreadFunc(int socket, string client)
{
	SocketHandle hClient{socket};
    char* pBuffer = new char[BUFFER_SIZE];
    shared_ptr<char> pDeleter(pBuffer, std::default_delete<char[]>());

    auto begin = chrono::high_resolution_clock::now();
    size_t totalReceived = 0;
	while (true)
	{
        int received = recv(hClient, pBuffer, BUFFER_SIZE, 0);
	    if (received <= 0)
	    {
	        break;
	    }
	    totalReceived += received;
	}

    auto end = chrono::high_resolution_clock::now();
	auto totalElapsed = chrono::duration_cast<chrono::milliseconds>(end - begin).count();
	cout << "Average bitrate (" << client << "): " << GetBitrate(totalReceived, totalElapsed) << " bps" << endl;
}

double GetBitrate(size_t bytes, size_t msecs)
{
	double bitrate = 0.0;

	if (msecs > 0)
	{
		bitrate = static_cast<double>(bytes * CHAR_BIT * MILLISECONDS_PER_SECOND) / static_cast<double>(msecs);
	}

	return bitrate;
}
