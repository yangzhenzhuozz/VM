#include "../VM/bridge/bridge.hpp"
#include <ws2tcpip.h>
#include <cstring>
#include <iostream>

#pragma comment(lib,"Ws2_32.lib")

using namespace VMStaticExport;

struct SocketResource
{
	bool hasClosed = false;
	SOCKET socket;
};

BOOL WINAPI DllMain(
	HINSTANCE hinstDLL,  // handle to DLL module
	DWORD fdwReason,     // reason for calling function
	LPVOID lpvReserved)  // reserved
{
	// Perform actions based on the reason for calling.
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		WSADATA wsa_data;
		return WSAStartup(MAKEWORD(2, 0), &wsa_data) == 0;

	case DLL_THREAD_ATTACH:
		// Do thread-specific initialization.
		break;

	case DLL_THREAD_DETACH:
		// Do thread-specific cleanup.
		break;

	case DLL_PROCESS_DETACH:

		if (lpvReserved != nullptr)
		{
			break; // do not do cleanup if process termination scenario
		}
		WSACleanup();
		// Perform any necessary cleanup.
		break;
	}
	return TRUE;  // Successful DLL_PROCESS_ATTACH.
}

extern "C" __declspec(dllimport) tlong system_net_ip2longNative(tpointer dataAdd, VM * vm);
tlong system_net_ip2longNative(tpointer dataAdd, VM* vm)
{
	auto pointer = (HeapItem*)(dataAdd - sizeof(HeapItem));
	auto buffer = new char[pointer->sol.length + 1];
	std::memcpy(buffer, pointer->data, pointer->sol.length);
	buffer[pointer->sol.length] = '\0';
	struct in_addr s;
	int ret = inet_pton(AF_INET, buffer, (void*)&s);
	delete[] buffer;
	if (ret == 0) {
		return -1;
	}
	else {
		return s.S_un.S_addr;
	}
}

void SocketDispose(tpointer p)
{
	SocketResource* res = (SocketResource*)p;
	if (!res->hasClosed)
	{
		closesocket(res->socket);
		res->hasClosed = true;
	}
	delete res;
}

extern "C" __declspec(dllimport) void system_net_close(tpointer p, VM * vm);
void system_net_close(tpointer p, VM* vm)
{
	//VM传递进来的是一个指针，这个指针指向原来被保存的指针，所以取两次就能拿到真实SOCKET
	SocketResource* res = ((SocketResource*)(*(u64*)p));
	closesocket(res->socket);
	res->hasClosed = true;
}


extern "C" __declspec(dllimport) tpointer system_net_createServerSocket(tlong host, tshort port, VM * vm);
tpointer system_net_createServerSocket(tlong host, tshort port, VM* vm)
{
	SOCKADDR_IN server_addr;
	SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
	if (server < 0)
	{
		return 0;
	}

	server_addr.sin_addr.s_addr = host;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	int ret = bind(server, reinterpret_cast<SOCKADDR*>(&server_addr), sizeof(server_addr));
	if (ret < 0)
	{
		return 0;
	}

	SocketResource* resource = new SocketResource();
	resource->socket = server;
	resource->hasClosed = false;
	return vm->addNativeResourcePointer((tlong)resource, (tlong)SocketDispose);
}

extern "C" __declspec(dllimport) tpointer system_net_createSocket(VM * vm);
tpointer system_net_createSocket(VM* vm)
{
	SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
	if (server == INVALID_SOCKET)
	{
		return 0;
	}
	SocketResource* resource = new SocketResource();
	resource->socket = server;
	resource->hasClosed = false;
	return vm->addNativeResourcePointer((tlong)resource, (tlong)SocketDispose);
}

extern "C" __declspec(dllimport) tint system_net_connect(tpointer dataAdd, tlong host, tshort port, tshort family, tint len, VM * vm);
tint system_net_connect(tpointer dataAdd, tlong host, tshort port, tshort family, tint len, VM* vm)
{
	//VM传递进来的是一个指针，这个指针指向原来被保存的指针，所以取两次就能拿到真实SOCKET
	SOCKET socket = ((SocketResource*)(*(u64*)dataAdd))->socket;
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = host;
	return connect(socket, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr));
}


extern "C" __declspec(dllimport) tint system_net_listen(tpointer dataAdd, tint len, VM * vm);
tint system_net_listen(tpointer dataAdd, tint len, VM* vm)
{
	//VM传递进来的是一个指针，这个指针指向原来被保存的指针，所以取两次就能拿到真实SOCKET
	SOCKET socket = ((SocketResource*)(*(u64*)dataAdd))->socket;
	int ret = listen(socket, len);
	return ret;
}

extern "C" __declspec(dllimport) tpointer system_net_accept(tpointer dataAdd, VM * vm);
tpointer system_net_accept(tpointer dataAdd, VM* vm)
{
	SOCKET socket = ((SocketResource*)(*(u64*)dataAdd))->socket;
	SOCKADDR_IN client_addr;
	int client_addr_size = sizeof(client_addr);
	SOCKET client = accept(socket, reinterpret_cast<SOCKADDR*>(&client_addr), &client_addr_size);
	if (client == INVALID_SOCKET)
	{
		return 0;
	}
	SocketResource* resource = new SocketResource();
	resource->socket = client;
	resource->hasClosed = false;
	return vm->addNativeResourcePointer((tlong)resource, (tlong)SocketDispose);
}

extern "C" __declspec(dllimport) tint system_net_send(tpointer dataAdd, tpointer buf, VM * vm);
tint system_net_send(tpointer dataAdd, tpointer buf, VM* vm)
{
	SOCKET socket = ((SocketResource*)(*(u64*)dataAdd))->socket;
	auto pointer = (HeapItem*)(buf - sizeof(HeapItem));
	return send(socket, pointer->data, pointer->sol.length, 0);
}

extern "C" __declspec(dllimport) tint system_net_read(tpointer _scoket, tpointer buf, VM * vm);
tint system_net_read(tpointer dataAdd, tpointer buf, VM* vm)
{
	SOCKET socket = ((SocketResource*)(*(u64*)dataAdd))->socket;
	auto pointer = (HeapItem*)(buf - sizeof(HeapItem));
	return recv(socket, pointer->data, pointer->sol.length, 0);
}

extern "C" __declspec(dllimport) tlong system_net_getSocketAddress(tpointer _scoket, VM * vm);
tlong system_net_getSocketAddress(tpointer dataAdd, VM* vm)
{
	SOCKET socket = ((SocketResource*)(*(u64*)dataAdd))->socket;
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	int res = getpeername(socket, (struct sockaddr*)&addr, &addr_size);
	if (res == 0)
	{
		return addr.sin_addr.S_un.S_addr;
	}
	else
	{
		return -1;
	}
}
extern "C" __declspec(dllimport) tshort system_net_getSocketPort(tpointer _scoket, VM * vm);
tshort system_net_getSocketPort(tpointer dataAdd, VM* vm)
{
	SOCKET socket = ((SocketResource*)(*(u64*)dataAdd))->socket;
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	int res = getpeername(socket, (struct sockaddr*)&addr, &addr_size);
	if (res == 0)
	{
		return addr.sin_port;
	}
	else
	{
		return 0;
	}
}
extern "C" __declspec(dllimport) tshort system_net_getSocketFamily(tpointer _scoket, VM * vm);
tshort system_net_getSocketFamily(tpointer dataAdd, VM* vm)
{
	SOCKET socket = ((SocketResource*)(*(u64*)dataAdd))->socket;
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	int res = getpeername(socket, (struct sockaddr*)&addr, &addr_size);
	if (res == 0)
	{
		return addr.sin_family;
	}
	else
	{
		return 0;
	}
}