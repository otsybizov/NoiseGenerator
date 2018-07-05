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

#define ARG_NUMBER 6
#define TRANSMIT_INTERVAL_MILLISECONDS 50
#define MILLISECONDS_PER_SECOND 1000
#define NANOSECONDS_PER_MILLISECOND 1000000

void Usage()
{
	cout << "Usage: NoiseEmitter [IP_ADDRESS] [PORT] [PROTOCOL] [BITRATE] [DURATION]" << endl << endl;
	cout << "Generates noise (zeroed payload) traffic with specified [BITRATE]" << endl;
	cout << "evenly distributed in time (no bursts) for specified [DURATION]." << endl;
	cout << "The traffic is transferred to server specified by [IP_ADDRESS]" << endl;
	cout << "and [PORT] communicating using [PROTOCOL] (TCP/UDP)." << endl << endl;
	cout << "[IP_ADDRESS]	IPv4 address of the server." << endl;
	cout << "[PORT]		Port the server is listening on." << endl;
	cout << "[PROTOCOL]	Network protocol: 'tcp' or 'udp'" << endl;
	cout << "[BITRATE]	Bitrate of the noise generating." << endl;
	cout << "[DURATION]	Number of seconds to generate noise." << endl;
}

class SocketHandle
{
	int hFileM;

public:
	SocketHandle(int hFile) : hFileM(hFile) {}
	~SocketHandle() { if (hFileM >= 0) close(hFileM); }
	operator int() const { return hFileM; }
};

struct Arg
{
	sockaddr_in address{};
	int port{0};
	int socketType{0};
	size_t byterate{0};
	int duration{0};
};

void ParseArgs(int argc, char *argv[], Arg& arg);
int ParseInt(string str);

int main(int argc, char *argv[])
{
	// Parse arguments.
	Arg arg;
	try
	{
		ParseArgs(argc, argv, arg);
	}
	catch (const char* msg)
	{
		cout << "ERROR: " << msg << endl << endl;
		Usage();
		return 0;
	}

	SocketHandle hSocket = socket(AF_INET, arg.socketType, 0);
    if (hSocket == -1)
    {
    	cout << "Couldn't create socket: " << errno << "." << endl;
    	return 0;
    }

    int res = setsockopt(hSocket, SOL_SOCKET, SO_SNDBUF, &arg.byterate, sizeof(arg.byterate));
    if (res)
    {
    	cout << "Couldn't set socket buffer size: " << errno << "." << endl;
    	return 0;
    }

    if (connect(hSocket, (sockaddr*)&arg.address, sizeof(arg.address)) < 0)
    {
    	cout << "Couldn't connect to server: " << errno << "." << endl;
    	return 0;
    }

    size_t bytesPerInterval = arg.byterate * TRANSMIT_INTERVAL_MILLISECONDS / MILLISECONDS_PER_SECOND / 2;
    string noise(bytesPerInterval, 0);

    auto begin = chrono::high_resolution_clock::now();
    auto startTransmit = begin;
    auto endTransmit = begin;
	while (true)
	{
		size_t bytesToWrite = bytesPerInterval;
	    while (bytesToWrite > 0)
	    {
	        auto sent = send(hSocket, noise.c_str(), bytesToWrite, 0);
	        if (sent < 0)
	        {
	        	cout << "Failed transmit data: " << errno << "." << endl;
	        	return 0;
	        }
	        bytesToWrite -= sent;
	    }

	    startTransmit = endTransmit;
		endTransmit = chrono::high_resolution_clock::now();
		auto elapsed = chrono::duration_cast<chrono::milliseconds>(endTransmit - begin).count();
		if (elapsed >= arg.duration)
		{
			return 0;
		}

		int remain = TRANSMIT_INTERVAL_MILLISECONDS -
			chrono::duration_cast<chrono::milliseconds>(endTransmit - startTransmit).count();
		if (remain > 0)
		{
			this_thread::sleep_for(chrono::milliseconds(remain));
		}
	}

	return 0;
}

int ParseInt(string str)
{
	size_t num{0};
	int res = static_cast<int>(stoul(str, &num));
	if (num < str.size())
	{
		throw 1;
	}
	return res;
}

void ParseArgs(int argc, char *argv[], Arg& arg)
{
	if (argc < ARG_NUMBER)
	{
		throw "Insufficient number of arguments.";
	}

	// [PORT]
	try
	{
		arg.port = ParseInt(argv[2]);
	}
	catch(...)
	{
		throw "Invalid server port.";
	}

	// [IP_ADDRESS]
	// Create a sockaddr struct, which represents the socket address.
	// This represents the ip address and the port number. If address
	// is in numbers-and-dots notation (e.g. 192.168.22.49), then
	// inet_aton returns non-zero and the sin_addr is set. If inet_aton
	// returns zero, then address needs to be resolved using gethostbyname().
	const char* pIpAddress = argv[1];
	memset(&arg.address, 0, sizeof(arg.address));
	arg.address.sin_family = AF_INET;
	arg.address.sin_port = htons(arg.port);
	int res = inet_aton(pIpAddress, &arg.address.sin_addr);
	if (res == 0)
	{
		hostent *host = gethostbyname(pIpAddress);
		if (host == nullptr)
		{
			throw "Couldn't resolve server name.";
		}
		memcpy(&arg.address.sin_addr, host->h_addr, host->h_length);
	}

	// [PROTOCOL]
	string protocol{argv[3]};
	if (protocol == "tcp")
	{
		arg.socketType = static_cast<int>(SOCK_STREAM);
	}
	else if (protocol == "udp")
	{
		arg.socketType = static_cast<int>(SOCK_DGRAM);
	}
	else
	{
		throw "Invalid protocol.";
	}

	// [BITRATE]
	try
	{
		size_t num{0};
		string buf{argv[4]};
		double val = stod(buf, &num);
		size_t lastIndex = buf.size() - 1;
		if (val <= 0.0 || num < lastIndex)
		{
			throw 1;
		}
		else if (num == lastIndex)
		{
			switch (buf[lastIndex])
			{
				case 'G':
				case 'g':
				{
					val *= 1000.0 * 1000.0 * 1000.0;
					break;
				}
				case 'M':
				case 'm':
				{
					val *= 1000.0 * 1000.0;
					break;
				}
				case 'K':
				case 'k':
				{
					val *= 1000.0;
					break;
				}
				default:
				{
					throw 1;
				}
			}
			val /= CHAR_BIT;
			arg.byterate = static_cast<size_t>(val);
		}
	}
	catch(...)
	{
		throw "Invalid bitrate.";
	}

	// [DURATION]
	try
	{
		arg.duration = ParseInt(argv[5]) * MILLISECONDS_PER_SECOND;
		if (arg.duration <= 0)
		{
			throw 1;
		}
	}
	catch(...)
	{
		throw "Invalid duration.";
	}
}
