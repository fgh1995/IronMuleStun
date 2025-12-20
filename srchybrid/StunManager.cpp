#include "stdafx.h"
#include "StunManager.h"
#include "UPnPImplWrapper.h"
#include "WinTCPStun.h"
#include "emule.h"
#include "emuledlg.h"
#include "Preferences.h"
#include "ListenSocket.h"
#include "DownloadQueue.h"
#include "PartFile.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "Kademlia/Kademlia/UDPFirewallTester.h"
#include "Log.h"
#include <TaskbarNotifier.h>
#include <MenuCmds.h>

// 初始化静态成员
CStunManager* CStunManager::s_pInstance = NULL;

CStunManager::CStunManager()
    : m_pUPnPFinder(NULL)
    , m_hStunCheckThread(NULL)
    , m_bStunCheckRunning(false)
    , m_lastPublicPort(0)
    , m_lastUDPPublicPort(0)
    , m_lastStunLocalPort(0)
    , m_lastStunLocalUDPPort(0)
    , m_udpNatRetryCount(0)
    , m_recheckFirewalledCount(0)
{
}

CStunManager::~CStunManager()
{
    StopStunCheck();

    if (m_pUPnPFinder)
    {
        delete m_pUPnPFinder;
        m_pUPnPFinder = NULL;
    }
}

CStunManager* CStunManager::GetInstance()
{
    if (!s_pInstance)
    {
        s_pInstance = new CStunManager();
    }
    return s_pInstance;
}

void CStunManager::Cleanup()
{
    if (s_pInstance)
    {
        delete s_pInstance;
        s_pInstance = NULL;
    }
}

bool CStunManager::Initialize()
{
    if (!thePrefs.GetEnableStun())
    {
        return false;
    }
    // 初始化UPNP管理器
    m_pUPnPFinder = new CUPnPImplWrapper();
    if (!theApp.m_UPnPManager.Initialize(2000, thePrefs.GetBindAddrA()))
    {
        LogError(_T("UPNP: 未找到支持UPNP的路由器，请检查路由器是否已启用UPNP功能"));
        return false;
    }
    else
    {
        theApp.QueueLogLineEx(LOG_SUCCESS, L"UPNP: 已成功连接到路由器UPNP服务");
    }

    // 解析STUN服务器列表
    CString strStunServers = thePrefs.GetStunServer();
    ParseStunServers(strStunServers);

    // 执行TCP和UDP STUN检测
    bool tcpSuccess = DoTcpStunDetection();
    bool udpSuccess = DoUdpStunDetection();
    theApp.downloadqueue->ResumeAllDownloads();
    return tcpSuccess || udpSuccess;
}

void CStunManager::ParseStunServers(const CString& strStunServers)
{
    if (strStunServers.IsEmpty())
    {
        return;
    }

    // 使用 "|" 作为分隔符拆分字符串
    int nPos = 0;
    CString strToken = strStunServers.Tokenize(_T("|"), nPos);

    while (!strToken.IsEmpty())
    {
        // 去除可能的首尾空格
        strToken.Trim();

        // 只有当STUN服务器非空时才添加
        if (!strToken.IsEmpty())
        {
            // 将 CString 转换为 std::string
            // 使用 Unicode 到 UTF-8 转换
            CT2A utf8Token(strToken, CP_UTF8);
            std::string stdToken(utf8Token);
            m_stunServers.push_back(StunServerInfo::FromString(stdToken));
        }

        // 获取下一个STUN服务器
        strToken = strStunServers.Tokenize(_T("|"), nPos);
    }
}

bool CStunManager::DoTcpStunDetection()
{
    CStunClient stunClient;
    std::string publicIP;
    uint16_t publicPort = 0;

    if (stunClient.DoTcpStun(m_stunServers, publicIP, publicPort, m_lastStunLocalPort))
    {
        CString stunResult;
        stunResult.Format(_T("TCP端口映射检测完成：本地端口 %d -> 公网端口 %s:%d"),
            m_lastStunLocalPort, CString(publicIP.c_str()), publicPort);
        theApp.QueueLogLineEx(LOG_SUCCESS, stunResult);

        if (!theApp.m_UPnPManager.isInitialized())
        {
            LogError(_T("UPNP: 无法连接路由器UPNP服务"));
            return false;
        }

        // 清理旧的TCP端口映射
        if (theApp.m_UPnPManager.DeletePortMapping(thePrefs.lastStunLocalPort, "TCP"))
        {
            theApp.QueueLogLineEx(LOG_SUCCESS, L"已清除旧的TCP端口映射（端口:%d）", thePrefs.lastStunLocalPort);
        }
        else
        {
            theApp.QueueLogLineEx(LOG_SUCCESS, L"清理旧TCP端口映射失败（端口:%d），可能之前没有映射", thePrefs.lastStunLocalPort);
        }

        // 添加新的TCP端口映射
        if (theApp.m_UPnPManager.AddPortMapping(publicPort, m_lastStunLocalPort, "TCP", "eMule TCP(STUN)"))
        {
            theApp.QueueLogLineEx(LOG_SUCCESS, L"TCP端口映射成功：%d -> %s:%d",
                m_lastStunLocalPort, CString(publicIP.c_str()), publicPort);

            // 保存最后一次STUN穿透发起的本地端口
            thePrefs.lastStunLocalPort = m_lastStunLocalPort;

            theApp.QueueLogLineEx(LOG_SUCCESS, L"已将TCP监听端口从 %d 改为 %d",
                thePrefs.port, publicPort);
            thePrefs.port = publicPort;
            thePrefs.Save();

            // 保存公网IP端口用于后续STUN检测
            m_lastPublicIP = publicIP;
            m_lastPublicPort = publicPort;

            return true;
        }
        else
        {
            LogError(L"TCP端口映射失败：%d -> %s:%d",
                m_lastStunLocalPort, CString(publicIP.c_str()), publicPort);
            return false;
        }
    }
    else
    {
        theApp.QueueLogLineEx(LOG_WARNING, L"TCP穿透检测失败，可能处于内网环境或STUN服务器不可用");
        return false;
    }
}

bool CStunManager::DoUdpStunDetection()
{
    CStunClient stunClient;
    std::string udpPublicIP;
    uint16_t udpPublicPort = 0;

    if (stunClient.DoUdpStun(m_stunServers, udpPublicIP, udpPublicPort, m_lastStunLocalUDPPort))
    {
        CString stunResult;
        stunResult.Format(_T("UDP端口映射检测完成：本地端口 %d -> 公网端口 %s:%d"),
            m_lastStunLocalUDPPort, CString(udpPublicIP.c_str()), udpPublicPort);
        theApp.QueueLogLineEx(LOG_SUCCESS, stunResult);

        if (!theApp.m_UPnPManager.isInitialized())
        {
            LogError(_T("UPNP: 无法连接路由器UPNP服务"));
            return false;
        }

        // 清理旧的UDP端口映射
        if (theApp.m_UPnPManager.DeletePortMapping(thePrefs.lastStunLocalUDPPort, "UDP"))
        {
            theApp.QueueLogLineEx(LOG_SUCCESS, L"已清除旧的UDP端口映射（端口:%d）", thePrefs.lastStunLocalUDPPort);
        }
        else
        {
            theApp.QueueLogLineEx(LOG_SUCCESS, L"清理旧的UDP端口映射失败（端口:%d），可能之前没有映射", thePrefs.lastStunLocalUDPPort);
        }

        // 添加新的UDP端口映射
        if (theApp.m_UPnPManager.AddPortMapping(udpPublicPort, m_lastStunLocalUDPPort, "UDP", "eMule UDP(STUN)"))
        {
            theApp.QueueLogLineEx(LOG_SUCCESS, L"UDP端口映射成功：%d -> %s:%d",
                m_lastStunLocalUDPPort, CString(udpPublicIP.c_str()), udpPublicPort);

            // 保存最后一次STUN穿透发起的本地端口
            thePrefs.lastStunLocalUDPPort = m_lastStunLocalUDPPort;

            theApp.QueueLogLineEx(LOG_SUCCESS, L"已将UDP监听端口从 %d 改为 %d",
                thePrefs.udpport, udpPublicPort);
            thePrefs.udpport = udpPublicPort;
            thePrefs.Save();

            // 保存UDP公网IP端口用于后续STUN检测
            m_lastUDPPublicIP = udpPublicIP;
            m_lastUDPPublicPort = udpPublicPort;

            return true;
        }
        else
        {
            LogError(L"UDP端口映射失败：%d -> %s:%d",
                m_lastStunLocalUDPPort, CString(udpPublicIP.c_str()), udpPublicPort);
            return false;
        }
    }
    else
    {
        theApp.QueueLogLineEx(LOG_WARNING, L"UDP穿透检测失败，可能处于内网环境或STUN服务器不可用");
        return false;
    }
}

void CStunManager::StartStunCheck()
{
    m_bStunCheckRunning = true;
    m_hStunCheckThread = ::CreateThread(NULL, 0, StunCheckThread, this, 0, NULL);

    if (m_hStunCheckThread == NULL)
    {
        m_bStunCheckRunning = false;
        LogError(_T("无法启动端口监控线程"));
    }
    else
    {
        theApp.QueueLogLineEx(LOG_SUCCESS, L"已启动端口映射监控服务");
    }
}

void CStunManager::StopStunCheck()
{
    if (m_bStunCheckRunning)
    {
        m_bStunCheckRunning = false;
        if (m_hStunCheckThread)
        {
            WaitForSingleObject(m_hStunCheckThread, 5000);
            CloseHandle(m_hStunCheckThread);
            m_hStunCheckThread = NULL;
        }
        theApp.QueueLogLineEx(LOG_SUCCESS, L"已停止端口映射监控服务");
    }
}

DWORD WINAPI CStunManager::StunCheckThread(LPVOID lpParam)
{
    CStunManager* pThis = static_cast<CStunManager*>(lpParam);

    while (pThis->m_bStunCheckRunning)
    {
        // 每10秒检查一次
        Sleep(10000);

        if (!pThis->m_bStunCheckRunning)
        {
            break;
        }

        pThis->CheckStunMapping();
    }

    return 0;
}

void CStunManager::CheckStunMapping()
{
    if (!m_bStunCheckRunning || m_lastStunLocalPort == 0)
    {
        return;
    }

    CStunClient stunClient;
    std::string currentPublicIP;
    uint16_t currentPublicPort = 0;

    theApp.QueueLogLineEx(LOG_DEBUG, L"正在检测端口映射状态...", m_lastStunLocalPort);

    // 检查TCP映射变化
    if (stunClient.DoTcpStun(m_lastStunLocalPort, m_stunServers,
        currentPublicIP, currentPublicPort))
    {
        // 处理检测结果
        HandleStunDetectionResult(currentPublicIP, currentPublicPort, m_lastStunLocalPort);
    }
    else
    {
        theApp.QueueLogLineEx(LOG_WARNING, L"TCP端口检测失败，本地端口: %d", m_lastStunLocalPort);
    }

	// 检查UDP映射变化,在emule中不能使用同一个端口进行UDP变化检测，否则将导致UDP状态始终为:通过防火墙,原因未知
    /*
    if (stunClient.DoUdpStun(m_lastStunLocalUDPPort, m_stunServers,
        currentPublicIP, currentPublicPort))
    {
        // 处理检测结果
    }*/

    // 定时检查UDP防火墙状态
    CheckUDPFirewallStatus();
}

void CStunManager::CheckUDPFirewallStatus()
{
    if (Kademlia::CKademlia::IsConnected())
    {
        if (m_udpNatRetryCount < MAX_UDP_NAT_RETRY_COUNT)
        {
            bool bFirewalled = Kademlia::CUDPFirewallTester::IsFirewalledUDP(true);
            if (bFirewalled)
            {
                // UDP穿透后,第一次检测时UDP状态必定是:通过防火墙(无法通过网站UDP测试)，因此检测到该状态时,需要重新连接一次KAD网络即可恢复正常,原因未知
                theApp.QueueLogLineEx(LOG_SUCCESS, L"UDP状态:通过防火墙，正在进行第 %i 次KAD网络重连...",
                    (m_udpNatRetryCount + 1));
                m_udpNatRetryCount++;
                Kademlia::CKademlia::Stop();
                Kademlia::CKademlia::Start();
            }
        }
        else
        {
            LogError(L"UDP网络穿透失败");
        }
    }

    bool bFirewalled = Kademlia::CUDPFirewallTester::IsFirewalledUDP(true);
    bool isVerified = Kademlia::CUDPFirewallTester::IsVerified();

    if (!bFirewalled && isVerified)
    {
        if (m_recheckFirewalledCount > MAX_RECHECK_FIREWALLED_COUNT)
        {
            theApp.QueueLogLineEx(LOG_SUCCESS, L"重新检查UDP防火墙");
            Kademlia::CKademlia::RecheckFirewalled();
            m_recheckFirewalledCount = 0;
        }
        m_recheckFirewalledCount++;
    }
}

void CStunManager::HandleStunDetectionResult(const std::string& currentPublicIP,
    uint16_t currentPublicPort, uint16_t localPort)
{
    if (m_lastPublicPort == 0)
    {
        theApp.QueueLogLineEx(LOG_WARNING, L"STUN-TCP端口检测异常：未找到有效的公网端口记录");
        return;
    }

    // 检查映射关系是否有变化
    if (currentPublicIP != m_lastPublicIP || currentPublicPort != m_lastPublicPort)
    {
        CString changeMsg;
        changeMsg.Format(_T("检测到TCP端口映射发生变化：\n之前：%s:%d\n现在：%s:%d\n本地端口：%d"),
            CString(m_lastPublicIP.c_str()), m_lastPublicPort,
            CString(currentPublicIP.c_str()), currentPublicPort, localPort);
        LogError(changeMsg);
        theApp.emuledlg->ShowNotifier(changeMsg, TBN_NEWVERSION);
        theApp.emuledlg->CloseConnection();

        if (!theApp.IsPortchangeAllowed())
        {
            theApp.OnDisconnectNetwork();
        }
        DoUdpStunDetection();
        if (!theApp.m_UPnPManager.isInitialized())
        {
            LogError(_T("无法连接路由器UPNP服务"));
        }
        else
        {
            // 清理旧的TCP端口映射
            if (theApp.m_UPnPManager.DeletePortMapping(thePrefs.lastStunLocalPort, "TCP"))
            {
                theApp.QueueLogLineEx(LOG_SUCCESS, L"已清除旧的TCP端口映射（端口:%d）", m_lastStunLocalPort);
            }
            else
            {
                theApp.QueueLogLineEx(LOG_SUCCESS, L"清理旧的TCP端口映射失败（端口:%d），可能之前没有映射", m_lastStunLocalPort);
            }

            // 添加新的TCP端口映射
            if (theApp.m_UPnPManager.AddPortMapping(currentPublicPort, m_lastStunLocalPort, "TCP", "eMule TCP(STUN)"))
            {
                theApp.QueueLogLineEx(LOG_SUCCESS, L"TCP端口重新映射成功：%d -> %s:%d",
                    m_lastStunLocalPort, CString(currentPublicIP.c_str()), currentPublicPort);

                // 保存最后一次STUN穿透发起的本地端口
                thePrefs.lastStunLocalPort = m_lastStunLocalPort;
                theApp.QueueLogLineEx(LOG_SUCCESS, L"已将TCP监听端口更新为 %d", currentPublicPort);
                thePrefs.port = currentPublicPort;
                theApp.listensocket->Rebind();
                thePrefs.Save();

                // 保存公网IP端口用于后续STUN检测
                m_lastPublicIP = currentPublicIP;
                m_lastPublicPort = currentPublicPort;
            }
            else
            {
                LogError(L"TCP端口重新映射失败：%d -> %s:%d",
                    m_lastStunLocalPort, CString(currentPublicIP.c_str()), currentPublicPort);
            }
        }

        theApp.emuledlg->StartConnection();
        theApp.downloadqueue->ResumeAllDownloads();
        theApp.emuledlg->ShowNotifier(L"正在恢复下载任务...", TBN_NEWVERSION);
    }
    else
    {
        // 映射关系稳定
        theApp.QueueLogLineEx(LOG_DEBUG,
            L"TCP端口映射状态正常：本地 %d -> 公网 %s:%d",
            localPort,
            CString(currentPublicIP.c_str()), currentPublicPort);
    }
}

void CStunManager::RebootEmule()
{
    if (theApp.emuledlg && theApp.emuledlg->m_hWnd)
    {
        theApp.needReboot = true;
        theApp.emuledlg->PostMessage(WM_COMMAND, MP_EXIT, 0);
    }
}