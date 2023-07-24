#include "../VM/bridge/bridge.hpp"
#include <ws2tcpip.h>
#include <cstring>

#pragma comment(lib,"Ws2_32.lib")

using namespace VMStaticExport;

extern "C" __declspec(dllimport) tlong system_net_ip2longNative(tpointer dataAdd, VM * vm);

tlong system_net_ip2longNative(tpointer dataAdd, VM* vm)
{
	auto pointer = (HeapItem*)(dataAdd - sizeof(HeapItem));
	auto buffer = new char[pointer->sol.length + 1];
	std::memcpy(buffer, pointer->data, pointer->sol.length);
	buffer[pointer->sol.length] = '\0';
	struct in_addr s;
	int ret = inet_pton(AF_INET, buffer, (void*)&s);
	if (ret == 0) {
		return 0;
	}
	else {
		return s.S_un.S_addr;
	}
}

void SocketClose(tpointer p)
{
	closesocket(*(SOCKET*)p);
	delete (SOCKET*)p;
}

extern "C" __declspec(dllimport) tpointer system_net_createServerSocket(tlong host, tint port, VM * vm);
tpointer system_net_createServerSocket(tlong host, tint port, VM* vm)
{
	WSADATA wsa_data;
	SOCKADDR_IN server_addr, client_addr;

	int ret;
	ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (ret < 0)
	{
		return 0;
	}

	SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
	if (server < 0)
	{
		return 0;
	}

	server_addr.sin_addr.s_addr = host;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	ret = bind(server, reinterpret_cast<SOCKADDR*>(&server_addr), sizeof(server_addr));
	if (ret < 0)
	{
		return 0;
	}

	SOCKET* socketBuf = new SOCKET;
	*socketBuf = server;
	return vm->addNativeResourcePointer((tlong)socketBuf, (tlong)SocketClose);
}

extern "C" __declspec(dllimport) tint system_net_listen(tpointer dataAdd, tint len, VM * vm);
tint system_net_listen(tpointer dataAdd, tint len, VM* vm)
{
	//VM传递进来的是一个指针，这个指针指向原来被保存的指针，所以取两次就能拿到真实SOCKET
	SOCKET socket = *((SOCKET*)(*(u64*)dataAdd));
	return listen(socket, len);
}

extern "C" __declspec(dllimport) tpointer system_net_accept(tpointer dataAdd, VM * vm);
tpointer system_net_accept(tpointer dataAdd, VM* vm)
{
	SOCKET server = *((SOCKET*)(*(u64*)dataAdd));
	SOCKADDR_IN client_addr;
	int client_addr_size = sizeof(client_addr);
	SOCKET client = accept(server, reinterpret_cast<SOCKADDR*>(&client_addr), &client_addr_size);
	if (client == INVALID_SOCKET)
	{
		return 0;
	}
	SOCKET* socketBuf = new SOCKET;
	*socketBuf = client;
	return vm->addNativeResourcePointer((tlong)socketBuf, (tlong)SocketClose);
}

extern "C" __declspec(dllimport) tint system_net_send(tpointer _scoket, tpointer buf, VM * vm);
tint system_net_send(tpointer _scoket, tpointer buf, VM* vm)
{
	SOCKET scoket = *((SOCKET*)(*(u64*)_scoket));
	auto pointer = (HeapItem*)(buf - sizeof(HeapItem));
	return send(scoket, pointer->data, pointer->sol.length, 0);
}

extern "C" __declspec(dllimport) tint system_net_read(tpointer _scoket, tpointer buf, VM * vm);
tint system_net_read(tpointer _scoket, tpointer buf, VM* vm)
{
	SOCKET scoket = *((SOCKET*)(*(u64*)_scoket));
	auto pointer = (HeapItem*)(buf - sizeof(HeapItem));
	return recv(scoket, pointer->data, pointer->sol.length, 0);
}

extern "C" __declspec(dllimport) tlong system_net_getSocketAddress(tpointer _scoket, VM * vm);
tlong system_net_getSocketAddress(tpointer _scoket, VM* vm)
{
	SOCKET scoket = *((SOCKET*)(*(u64*)_scoket));
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	int res = getpeername(scoket, (struct sockaddr*)&addr, &addr_size);
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
tshort system_net_getSocketPort(tpointer _scoket, VM* vm)
{
	SOCKET scoket = *((SOCKET*)(*(u64*)_scoket));
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	int res = getpeername(scoket, (struct sockaddr*)&addr, &addr_size);
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
tshort system_net_getSocketFamily(tpointer _scoket, VM* vm)
{
	SOCKET scoket = *((SOCKET*)(*(u64*)_scoket));
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	int res = getpeername(scoket, (struct sockaddr*)&addr, &addr_size);
	if (res == 0)
	{
		return addr.sin_family;
	}
	else
	{
		return 0;
	}
}