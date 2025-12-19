#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <afxstr.h>

// 前向声明
class CUPnPImplWrapper;
class CTcpStunClient;
struct StunServerInfo;
class CemuleApp;

class CStunManager
{
public:
    CStunManager();
    ~CStunManager();

    // 初始化STUN管理器
    bool Initialize();

    // 启动STUN检查线程
    void StartStunCheck();

    // 停止STUN检查线程
    void StopStunCheck();

    // 检查STUN映射状态
    void CheckStunMapping();

    // 处理TCP检测结果
    void HandleStunDetectionResult(const std::string& currentPublicIP,
        uint16_t currentPublicPort, uint16_t localPort);

    // 处理UDP检测结果
    void HandleStunUDPDetectionResult(const std::string& currentPublicIP,
        uint16_t currentPublicPort, uint16_t localPort);

    // 重启eMule
    void RebootEmule();

    // 获取实例
    static CStunManager* GetInstance();

    // 清理实例
    static void Cleanup();

private:
    // STUN检查线程
    static DWORD WINAPI StunCheckThread(LPVOID lpParam);

    // 解析STUN服务器列表
    void ParseStunServers(const CString& strStunServers);

    // 执行TCP STUN检测
    bool DoTcpStunDetection();

    // 执行UDP STUN检测
    bool DoUdpStunDetection();

    // 更新端口映射
    bool UpdatePortMapping(uint16_t localPort, uint16_t publicPort,
        const std::string& protocol, const std::string& description);

    // 检查UDP防火墙状态
    void CheckUDPFirewallStatus();

private:
    // 单例实例
    static CStunManager* s_pInstance;

    // STUN相关
    std::vector<StunServerInfo> m_stunServers;
    std::string m_lastPublicIP;
    uint16_t m_lastPublicPort;
    std::string m_lastUDPPublicIP;
    uint16_t m_lastUDPPublicPort;
    uint16_t m_lastStunLocalPort;
    uint16_t m_lastStunLocalUDPPort;

    // UPNP相关
    CUPnPImplWrapper* m_pUPnPFinder;

    // 线程相关
    HANDLE m_hStunCheckThread;
    bool m_bStunCheckRunning;

    // 重试计数器
    int m_udpNatRetryCount;
    int m_recheckFirewalledCount;

    // 常量
    static const int MAX_UDP_NAT_RETRY_COUNT = 5;
    static const int MAX_RECHECK_FIREWALLED_COUNT = 20;
};