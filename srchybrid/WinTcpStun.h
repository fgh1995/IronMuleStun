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

// 服务器信息结构体
struct StunServerInfo {
	std::string server;
	uint16_t port;

	StunServerInfo(const std::string& s = "", uint16_t p = 3478) : server(s), port(p) {}

	// 从字符串创建（支持 "server:port" 格式）
	static StunServerInfo FromString(const std::string& str) {
		size_t colonPos = str.find(':');
		if (colonPos != std::string::npos) {
			std::string server = str.substr(0, colonPos);
			uint16_t port = static_cast<uint16_t>(std::stoi(str.substr(colonPos + 1)));
			return StunServerInfo(server, port);
		}
		return StunServerInfo(str, 3478); // 默认STUN端口
	}

	// 从pair创建
	static StunServerInfo FromPair(const std::pair<std::string, uint16_t>& pair) {
		return StunServerInfo(pair.first, pair.second);
	}
};

// STUN客户端类（支持TCP和UDP）
class CStunClient {
public:
	CStunClient() : m_tcpSocket(INVALID_SOCKET), m_udpSocket(INVALID_SOCKET) {
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
	}

	~CStunClient() {
		CloseTcpConnection();
		CloseUdpConnection();
		WSACleanup();
	}

	// TCP STUN功能 - 绑定到任意端口，尝试多个服务器
	bool DoTcpStun(const std::vector<StunServerInfo>& servers,
		std::string& publicIP, uint16_t& publicPort, uint16_t& localPort) {
		return DoTcpStunInternal(0, servers, publicIP, publicPort, localPort);
	}

	// TCP STUN功能 - 绑定到指定端口，尝试多个服务器
	bool DoTcpStun(uint16_t localPort, const std::vector<StunServerInfo>& servers,
		std::string& publicIP, uint16_t& publicPort) {
		uint16_t dummyLocalPort;
		return DoTcpStunInternal(localPort, servers, publicIP, publicPort, dummyLocalPort);
	}

	// UDP STUN功能 - 绑定到任意端口，尝试多个服务器
	bool DoUdpStun(const std::vector<StunServerInfo>& servers,
		std::string& publicIP, uint16_t& publicPort, uint16_t& localPort) {
		return DoUdpStunInternal(0, servers, publicIP, publicPort, localPort);
	}

	// UDP STUN功能 - 绑定到指定端口，尝试多个服务器
	bool DoUdpStun(uint16_t localPort, const std::vector<StunServerInfo>& servers,
		std::string& publicIP, uint16_t& publicPort) {
		uint16_t dummyLocalPort;
		return DoUdpStunInternal(localPort, servers, publicIP, publicPort, dummyLocalPort);
	}

	// 保持向后兼容的单服务器版本
	bool DoTcpStun(const std::string& stunServer, uint16_t stunPort,
		std::string& publicIP, uint16_t& publicPort, uint16_t& localPort) {
		std::vector<StunServerInfo> servers;
		servers.push_back(StunServerInfo(stunServer, stunPort));
		return DoTcpStunInternal(0, servers, publicIP, publicPort, localPort);
	}

	bool DoTcpStun(uint16_t localPort, const std::string& stunServer,
		uint16_t stunPort, std::string& publicIP, uint16_t& publicPort) {
		std::vector<StunServerInfo> servers;
		servers.push_back(StunServerInfo(stunServer, stunPort));
		uint16_t dummyLocalPort;
		return DoTcpStunInternal(localPort, servers, publicIP, publicPort, dummyLocalPort);
	}

	bool DoUdpStun(const std::string& stunServer, uint16_t stunPort,
		std::string& publicIP, uint16_t& publicPort, uint16_t& localPort) {
		std::vector<StunServerInfo> servers;
		servers.push_back(StunServerInfo(stunServer, stunPort));
		return DoUdpStunInternal(0, servers, publicIP, publicPort, localPort);
	}

	bool DoUdpStun(uint16_t localPort, const std::string& stunServer,
		uint16_t stunPort, std::string& publicIP, uint16_t& publicPort) {
		std::vector<StunServerInfo> servers;
		servers.push_back(StunServerInfo(stunServer, stunPort));
		uint16_t dummyLocalPort;
		return DoUdpStunInternal(localPort, servers, publicIP, publicPort, dummyLocalPort);
	}

	// 获取本地端口（TCP）
	uint16_t GetTcpLocalPort() {
		if (m_tcpSocket == INVALID_SOCKET) return 0;

		sockaddr_in localAddr = {};
		int addrLen = sizeof(localAddr);
		if (getsockname(m_tcpSocket, (sockaddr*)&localAddr, &addrLen) == 0) {
			return ntohs(localAddr.sin_port);
		}
		return 0;
	}

	// 获取本地端口（UDP）
	uint16_t GetUdpLocalPort() {
		if (m_udpSocket == INVALID_SOCKET) return 0;

		sockaddr_in localAddr = {};
		int addrLen = sizeof(localAddr);
		if (getsockname(m_udpSocket, (sockaddr*)&localAddr, &addrLen) == 0) {
			return ntohs(localAddr.sin_port);
		}
		return 0;
	}

	void CloseTcpConnection() {
		if (m_tcpSocket != INVALID_SOCKET) {
			closesocket(m_tcpSocket);
			m_tcpSocket = INVALID_SOCKET;
		}
	}

	void CloseUdpConnection() {
		if (m_udpSocket != INVALID_SOCKET) {
			closesocket(m_udpSocket);
			m_udpSocket = INVALID_SOCKET;
		}
	}

private:
	SOCKET m_tcpSocket;
	SOCKET m_udpSocket;

	// TCP STUN内部实现（多服务器）
	bool DoTcpStunInternal(uint16_t desiredLocalPort, const std::vector<StunServerInfo>& servers,
		std::string& publicIP, uint16_t& publicPort, uint16_t& actualLocalPort) {

		// 创建新socket
		SOCKET stunSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (stunSocket == INVALID_SOCKET) {
			return false;
		}

		// 设置SO_REUSEADDR选项以便端口重用
		int reuse = 1;
		if (setsockopt(stunSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
			closesocket(stunSocket);
			return false;
		}

		// 绑定到本地端口
		sockaddr_in localAddr = {};
		localAddr.sin_family = AF_INET;
		localAddr.sin_addr.s_addr = INADDR_ANY;
		localAddr.sin_port = htons(desiredLocalPort);

		if (bind(stunSocket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
			closesocket(stunSocket);
			return false;
		}

		// 获取实际绑定的端口
		sockaddr_in boundAddr = {};
		int addrLen = sizeof(boundAddr);
		if (getsockname(stunSocket, (sockaddr*)&boundAddr, &addrLen) == SOCKET_ERROR) {
			closesocket(stunSocket);
			return false;
		}
		actualLocalPort = ntohs(boundAddr.sin_port);

		// 设置超时
		DWORD timeout = 5000;
		setsockopt(stunSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
		setsockopt(stunSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

		// 准备STUN请求
		std::vector<uint8_t> stunRequest = CreateStunBindingRequest();

		// 尝试每个服务器
		for (size_t i = 0; i < servers.size(); i++) {
			const StunServerInfo& server = servers[i];

			// 解析STUN服务器地址
			sockaddr_in serverAddr = {};
			serverAddr.sin_family = AF_INET;
			serverAddr.sin_port = htons(server.port);

			hostent* host = gethostbyname(server.server.c_str());
			if (!host) {
				continue; // 解析失败，尝试下一个服务器
			}
			serverAddr.sin_addr = *((in_addr*)host->h_addr);

			// 连接到STUN服务器
			if (connect(stunSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) != SOCKET_ERROR) {
				// 发送STUN请求
				int sent = send(stunSocket, (char*)stunRequest.data(), (int)stunRequest.size(), 0);
				if (sent != SOCKET_ERROR) {
					// 接收STUN响应
					char buffer[1024];
					int received = recv(stunSocket, buffer, sizeof(buffer), 0);
					if (received != SOCKET_ERROR) {
						// 解析STUN响应
						if (ParseStunResponse(buffer, received, publicIP, publicPort)) {
							closesocket(stunSocket);
							return true; // 成功获取公网地址
						}
					}
				}
			}

			// 当前服务器失败，关闭socket重新创建以尝试下一个服务器
			closesocket(stunSocket);

			// 创建新的socket
			stunSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (stunSocket == INVALID_SOCKET) {
				return false;
			}

			// 重新设置选项
			setsockopt(stunSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
			setsockopt(stunSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
			setsockopt(stunSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

			// 重新绑定
			if (bind(stunSocket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
				closesocket(stunSocket);
				return false;
			}
		}

		closesocket(stunSocket);
		return false; // 所有服务器都尝试失败
	}

	// UDP STUN内部实现（多服务器）
	bool DoUdpStunInternal(uint16_t desiredLocalPort, const std::vector<StunServerInfo>& servers,
		std::string& publicIP, uint16_t& publicPort, uint16_t& actualLocalPort) {

		// 创建UDP socket
		SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (udpSocket == INVALID_SOCKET) {
			return false;
		}

		// 设置SO_REUSEADDR选项
		int reuse = 1;
		if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
			closesocket(udpSocket);
			return false;
		}

		// 绑定到指定的本地端口
		sockaddr_in localAddr = {};
		localAddr.sin_family = AF_INET;
		localAddr.sin_addr.s_addr = INADDR_ANY;
		localAddr.sin_port = htons(desiredLocalPort);

		if (bind(udpSocket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
			closesocket(udpSocket);
			return false;
		}

		// 获取实际绑定的端口
		sockaddr_in boundAddr = {};
		int addrLen = sizeof(boundAddr);
		if (getsockname(udpSocket, (sockaddr*)&boundAddr, &addrLen) == SOCKET_ERROR) {
			closesocket(udpSocket);
			return false;
		}
		actualLocalPort = ntohs(boundAddr.sin_port);

		// 设置超时
		DWORD timeout = 5000;
		setsockopt(udpSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
		setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

		// 准备STUN请求
		std::vector<uint8_t> stunRequest = CreateStunBindingRequest();

		// 尝试每个服务器
		for (size_t i = 0; i < servers.size(); i++) {
			const StunServerInfo& server = servers[i];

			// 解析STUN服务器地址
			sockaddr_in serverAddr = {};
			serverAddr.sin_family = AF_INET;
			serverAddr.sin_port = htons(server.port);

			hostent* host = gethostbyname(server.server.c_str());
			if (!host) {
				continue; // 解析失败，尝试下一个服务器
			}
			serverAddr.sin_addr = *((in_addr*)host->h_addr);

			// 发送STUN请求
			int sent = sendto(udpSocket, (char*)stunRequest.data(), (int)stunRequest.size(), 0,
				(sockaddr*)&serverAddr, sizeof(serverAddr));
			if (sent != SOCKET_ERROR) {
				// 接收STUN响应
				char buffer[1024];
				sockaddr_in fromAddr;
				int fromLen = sizeof(fromAddr);

				// 尝试接收，最多重试3次
				for (int retry = 0; retry < 3; retry++) {
					int received = recvfrom(udpSocket, buffer, sizeof(buffer), 0,
						(sockaddr*)&fromAddr, &fromLen);
					if (received != SOCKET_ERROR) {
						// 解析STUN响应
						if (ParseStunResponse(buffer, received, publicIP, publicPort)) {
							closesocket(udpSocket);
							return true; // 成功获取公网地址
						}
					}
				}
			}
		}

		closesocket(udpSocket);
		return false; // 所有服务器都尝试失败
	}

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