#ifndef WINTCPSTUN_H
#define WINTCPSTUN_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <mstcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <cstdint>
// STUN消息类型
#define STUN_BINDING_REQUEST  0x0001
#define STUN_BINDING_RESPONSE 0x0101

// STUN属性类型
#define STUN_ATTR_MAPPED_ADDRESS  0x0001
#define STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020

#pragma pack(push, 1)
struct StunHeader {
	uint16_t msg_type;
	uint16_t msg_length;
	uint32_t magic_cookie;
	uint8_t transaction_id[12];
};

struct StunAttribute {
	uint16_t type;
	uint16_t length;
};

struct StunAddress {
	uint8_t unused;
	uint8_t family;
	uint16_t port;
	uint32_t address;
};
#pragma pack(pop)

// TCP STUN客户端类
class CTcpStunClient {
public:
	CTcpStunClient() : m_socket(INVALID_SOCKET) {
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
	}

	~CTcpStunClient() {
		CloseConnection();
		WSACleanup();
	}

	bool DoTcpStun(const std::string& stunServer, uint16_t stunPort,
		std::string& publicIP, uint16_t& publicPort, uint16_t& localPort) {

		// 创建TCP socket
		m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_socket == INVALID_SOCKET) {
			return false;
		}

		// 绑定到任意本地端口
		sockaddr_in localAddr = {};
		localAddr.sin_family = AF_INET;
		localAddr.sin_addr.s_addr = INADDR_ANY;
		localAddr.sin_port = htons(localPort);

		if (bind(m_socket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
			CloseConnection();
			return false;
		}

		// 获取本地端口
		int addrLen = sizeof(localAddr);
		if (getsockname(m_socket, (sockaddr*)&localAddr, &addrLen) == SOCKET_ERROR) {
			CloseConnection();
			return false;
		}
		localPort = ntohs(localAddr.sin_port);

		// 解析STUN服务器地址
		sockaddr_in serverAddr = {};
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(stunPort);

		hostent* host = gethostbyname(stunServer.c_str());
		if (!host) {
			CloseConnection();
			return false;
		}
		serverAddr.sin_addr = *((in_addr*)host->h_addr);

		// 设置连接超时
		DWORD timeout = 5000; // 5秒超时
		setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
		setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

		// 连接到STUN服务器
		if (connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
			CloseConnection();
			return false;
		}

		// 发送STUN绑定请求
		std::vector<uint8_t> stunRequest = CreateStunBindingRequest();
		int sent = send(m_socket, (char*)stunRequest.data(), (int)stunRequest.size(), 0);
		if (sent == SOCKET_ERROR) {
			CloseConnection();
			return false;
		}

		// 接收STUN响应
		char buffer[1024];
		int received = recv(m_socket, buffer, sizeof(buffer), 0);
		if (received == SOCKET_ERROR) {
			CloseConnection();
			return false;
		}

		// 解析STUN响应获取公网IP和端口
		bool result = ParseStunResponse(buffer, received, publicIP, publicPort);

		// STUN完成后立即关闭连接，以便端口可以重用
		CloseConnection();

		return result;
	}
	bool CTcpStunClient::DoTcpStunWithSamePort(uint16_t localPort, const std::string& stunServer,
		uint16_t stunPort, std::string& publicIP,
		uint16_t& publicPort)
	{
		// 创建新socket
		SOCKET stunSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (stunSocket == INVALID_SOCKET) {
			return false;
		}

		// 重要：设置SO_REUSEADDR选项
		int reuse = 1;
		if (setsockopt(stunSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
			theApp.QueueLogLineEx(LOG_WARNING, L"设置SO_REUSEADDR失败: %d", WSAGetLastError());
			closesocket(stunSocket);
			return false;
		}

		// 绑定到指定的本地端口
		sockaddr_in localAddr = {};
		localAddr.sin_family = AF_INET;
		localAddr.sin_addr.s_addr = INADDR_ANY;
		localAddr.sin_port = htons(localPort);

		if (bind(stunSocket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
			DWORD dwError = WSAGetLastError();
			theApp.QueueLogLineEx(LOG_WARNING, L"绑定到端口 %d 失败: %d", localPort, dwError);
			closesocket(stunSocket);
			return false;
		}

		// 验证绑定端口
		sockaddr_in boundAddr = {};
		int addrLen = sizeof(boundAddr);
		if (getsockname(stunSocket, (sockaddr*)&boundAddr, &addrLen) == SOCKET_ERROR) {
			closesocket(stunSocket);
			return false;
		}

		uint16_t actualPort = ntohs(boundAddr.sin_port);
		if (actualPort != localPort) {
			theApp.QueueLogLineEx(LOG_WARNING, L"绑定端口不匹配: 期望 %d, 实际 %d", localPort, actualPort);
			closesocket(stunSocket);
			return false;
		}

		theApp.QueueLogLineEx(LOG_DEBUG, L"成功绑定到本地端口: %d", localPort);

		// 解析STUN服务器地址
		sockaddr_in serverAddr = {};
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(stunPort);

		hostent* host = gethostbyname(stunServer.c_str());
		if (!host) {
			theApp.QueueLogLineEx(LOG_WARNING, L"解析STUN服务器地址失败");
			closesocket(stunSocket);
			return false;
		}
		serverAddr.sin_addr = *((in_addr*)host->h_addr);

		// 设置超时
		DWORD timeout = 5000;
		setsockopt(stunSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
		setsockopt(stunSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

		bool result = false;

		// 连接到STUN服务器
		theApp.QueueLogLineEx(LOG_DEBUG, L"连接到STUN服务器: %s:%d", CString(stunServer.c_str()), stunPort);

		if (connect(stunSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
			DWORD dwError = WSAGetLastError();
			theApp.QueueLogLineEx(LOG_WARNING, L"连接STUN服务器失败: %d", dwError);
			closesocket(stunSocket);
			return false;
		}

		theApp.QueueLogLineEx(LOG_DEBUG, L"已连接到STUN服务器，发送绑定请求");

		// 发送STUN请求
		std::vector<uint8_t> stunRequest = CreateStunBindingRequest();
		int sent = send(stunSocket, (char*)stunRequest.data(), (int)stunRequest.size(), 0);
		if (sent == SOCKET_ERROR) {
			DWORD dwError = WSAGetLastError();
			theApp.QueueLogLineEx(LOG_WARNING, L"发送STUN请求失败: %d", dwError);
			closesocket(stunSocket);
			return false;
		}

		theApp.QueueLogLineEx(LOG_DEBUG, L"已发送STUN请求，等待响应");

		// 接收STUN响应
		char buffer[1024];
		int received = recv(stunSocket, buffer, sizeof(buffer), 0);
		if (received == SOCKET_ERROR) {
			DWORD dwError = WSAGetLastError();
			theApp.QueueLogLineEx(LOG_WARNING, L"接收STUN响应失败: %d", dwError);
			closesocket(stunSocket);
			return false;
		}

		theApp.QueueLogLineEx(LOG_DEBUG, L"收到STUN响应，大小: %d 字节", received);

		// 解析STUN响应
		result = ParseStunResponse(buffer, received, publicIP, publicPort);

		if (result) {
			theApp.QueueLogLineEx(LOG_DEBUG, L"STUN检测成功: %s:%d", CString(publicIP.c_str()), publicPort);
		}
		else {
			theApp.QueueLogLineEx(LOG_WARNING, L"解析STUN响应失败");
		}

		closesocket(stunSocket);
		return result;
	}
	uint16_t GetLocalPort() {
		if (m_socket == INVALID_SOCKET) return 0;

		sockaddr_in localAddr = {};
		int addrLen = sizeof(localAddr);
		if (getsockname(m_socket, (sockaddr*)&localAddr, &addrLen) == 0) {
			return ntohs(localAddr.sin_port);
		}
		return 0;
	}

	void CloseConnection() {
		if (m_socket != INVALID_SOCKET) {
			closesocket(m_socket);
			m_socket = INVALID_SOCKET;
		}
	}

private:
	SOCKET m_socket;

	std::vector<uint8_t> CreateStunBindingRequest() {
		std::vector<uint8_t> message(sizeof(StunHeader));
		StunHeader* header = (StunHeader*)message.data();

		header->msg_type = htons(STUN_BINDING_REQUEST);
		header->msg_length = htons(0);
		header->magic_cookie = htonl(0x2112A442);

		for (int i = 0; i < 12; i++) {
			header->transaction_id[i] = (uint8_t)(rand() % 256);
		}

		return message;
	}

	bool ParseStunResponse(const char* buffer, int length, std::string& publicIP, uint16_t& publicPort) {
		if (length < (int)sizeof(StunHeader)) return false;

		const StunHeader* header = (const StunHeader*)buffer;
		if (ntohs(header->msg_type) != STUN_BINDING_RESPONSE) return false;

		const uint8_t* attrStart = (const uint8_t*)(buffer + sizeof(StunHeader));
		const uint8_t* attrEnd = (const uint8_t*)(buffer + length);

		while (attrStart < attrEnd - 4) {
			const StunAttribute* attr = (const StunAttribute*)attrStart;
			uint16_t attrType = ntohs(attr->type);
			uint16_t attrLength = ntohs(attr->length);

			if (attrStart + 4 + attrLength > attrEnd) break;

			if (attrType == STUN_ATTR_MAPPED_ADDRESS || attrType == STUN_ATTR_XOR_MAPPED_ADDRESS) {
				if (attrLength >= 8) {
					const StunAddress* stunAddr = (const StunAddress*)(attrStart + 4);

					if (stunAddr->family == 0x01) {
						uint32_t ip = ntohl(stunAddr->address);
						uint16_t port = ntohs(stunAddr->port);

						if (attrType == STUN_ATTR_XOR_MAPPED_ADDRESS) {
							port ^= 0x2112;
							ip ^= 0x2112A442;
						}

						in_addr addr;
						addr.s_addr = htonl(ip);
						publicIP = inet_ntoa(addr);
						publicPort = port;

						return true;
					}
				}
			}

			int padding = (4 - (attrLength % 4)) % 4;
			attrStart += 4 + attrLength + padding;
		}

		return false;
	}
};


#endif // WINTCPSTUN_H