#include "stdafx.h"
#include "NatTestDlg.h"
#include <commctrl.h>
#include <stdio.h>
#include <tchar.h>
#include "resource.h"
#include "stun.h"
#include "udp.h"
#include <Preferences.h>
#include <otherfunctions.h>
// 全局指针定义
CNatTestDlg* g_pCurrentNatTestDlg = NULL;

CNatTestDlg::CNatTestDlg()
    : m_hWnd(NULL)
    , m_hEdit(NULL)
    , m_hInstance(NULL)
    , m_bDebug(FALSE)
    , m_bModal(FALSE)
    , m_bChild(FALSE)
{
    initNetwork();
}

CNatTestDlg::~CNatTestDlg()
{

    if (m_hWnd && IsWindow(m_hWnd))
    {
        Destroy();
    }
}

// 显示模态对话框
INT_PTR CNatTestDlg::ShowModal(HINSTANCE hInstance, HWND hParent)
{
    // 检查对话框资源
    if (!CheckDialogResource(hInstance))
    {
        return -1;
    }

    m_hInstance = hInstance;
    m_bModal = TRUE;
    m_bChild = FALSE;

    // 保存当前实例指针
    g_pCurrentNatTestDlg = this;

    // 显示模态对话框
    INT_PTR nResult = DialogBoxParam(
        hInstance,
        MAKEINTRESOURCE(IDD_NAT_TEST),
        hParent,
        DlgProc,
        (LPARAM)this
    );

    if (nResult == -1)
    {
        DWORD dwError = GetLastError();
        // 检查窗口样式
        HRSRC hRes = FindResource(hInstance, MAKEINTRESOURCE(IDD_NAT_TEST), RT_DIALOG);
        if (hRes)
        {
            HGLOBAL hGlobal = LoadResource(hInstance, hRes);
            if (hGlobal)
            {
                DLGTEMPLATE* pTemplate = (DLGTEMPLATE*)LockResource(hGlobal);
                UnlockResource(hGlobal);
                FreeResource(hGlobal);
            }
        }
    }

    // 清除全局指针
    g_pCurrentNatTestDlg = NULL;

    return nResult;
}

// 创建非模态对话框
BOOL CNatTestDlg::Create(HINSTANCE hInstance, HWND hParent)
{
    if (m_hWnd && IsWindow(m_hWnd))
    {
        ShowWindow(m_hWnd, SW_SHOWNORMAL);
        SetForegroundWindow(m_hWnd);
        BringWindowToTop(m_hWnd);
        return TRUE;
    }

    // 检查对话框资源
    if (!CheckDialogResource(hInstance))
    {
        return FALSE;
    }

    m_hInstance = hInstance;
    m_bModal = FALSE;
    m_bChild = FALSE;
    // 保存当前实例指针
    g_pCurrentNatTestDlg = this;
    // 创建非模态对话框
    m_hWnd = CreateDialogParam(
        hInstance,
        MAKEINTRESOURCE(IDD_NAT_TEST),
        hParent,
        DlgProc,
        (LPARAM)this
    );

    if (!m_hWnd)
    {
        DWORD dwError = GetLastError();
        g_pCurrentNatTestDlg = NULL;
        return FALSE;
    }

    // 显示窗口
    ShowWindow(m_hWnd, SW_SHOWNORMAL);
    UpdateWindow(m_hWnd);

    return TRUE;
}

// 作为子窗口创建
BOOL CNatTestDlg::CreateAsChild(HINSTANCE hInstance, HWND hParent, int x, int y)
{
    if (m_hWnd && IsWindow(m_hWnd))
    {
        return TRUE;
    }

    if (!hParent || !IsWindow(hParent))
    {
        return FALSE;
    }

    // 检查对话框资源
    if (!CheckDialogResource(hInstance))
    {
        return FALSE;
    }

    m_hInstance = hInstance;
    m_bModal = FALSE;
    m_bChild = TRUE;

    // 保存当前实例指针
    g_pCurrentNatTestDlg = this;

    // 创建子对话框
    m_hWnd = CreateDialog(
        hInstance,
        MAKEINTRESOURCE(IDD_NAT_TEST),
        hParent,
        ChildDlgProc
    );

    if (!m_hWnd)
    {
        DWORD dwError = GetLastError();
        g_pCurrentNatTestDlg = NULL;
        return FALSE;
    }

    // 设置位置
    if (x != 0 || y != 0)
    {
        SetWindowPos(m_hWnd, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    // 显示窗口
    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);
    return TRUE;
}

// 关闭对话框
void CNatTestDlg::Close()
{
    if (m_hWnd && IsWindow(m_hWnd))
    {
        if (m_bModal)
        {
            EndDialog(m_hWnd, IDCANCEL);
            m_hWnd = NULL;
        }
        else
        {
            DestroyWindow(m_hWnd);
        }
    }
}

// 销毁对话框
void CNatTestDlg::Destroy()
{
    if (m_hWnd && IsWindow(m_hWnd))
    {
        ::DestroyWindow(m_hWnd);
        m_hWnd = NULL;
        m_hEdit = NULL;
    }
}

// 检查窗口是否可见
BOOL CNatTestDlg::IsVisible() const
{
    if (m_hWnd && IsWindow(m_hWnd))
    {
        BOOL bVisible = IsWindowVisible(m_hWnd);
        return bVisible;
    }
    return FALSE;
}

// 设置窗口文本
BOOL CNatTestDlg::SetWindowText(LPCTSTR lpszText)
{
    if (m_hWnd && IsWindow(m_hWnd))
    {
        return ::SetWindowText(m_hWnd, lpszText);
    }
    return FALSE;
}

// 添加日志文本
void CNatTestDlg::AddLogText(LPCTSTR lpszText)
{
    if (m_hEdit && IsWindow(m_hEdit))
    {
        // 获取当前文本长度
        int len = GetWindowTextLength(m_hEdit);

        // 将光标移动到文本末尾
        SendMessage(m_hEdit, EM_SETSEL, len, len);

        // 添加新文本
        SendMessage(m_hEdit, EM_REPLACESEL, 0, (LPARAM)lpszText);

        // 滚动到最后
        SendMessage(m_hEdit, EM_SCROLLCARET, 0, 0);
    }
}

// 检查对话框资源
BOOL CNatTestDlg::CheckDialogResource(HINSTANCE hInstance)
{
    // 检查资源是否存在
    HRSRC hRes = FindResource(hInstance, MAKEINTRESOURCE(IDD_NAT_TEST), RT_DIALOG);
    if (!hRes)
    {
        DWORD dwError = GetLastError();
        return FALSE;
    }

    DWORD dwSize = SizeofResource(hInstance, hRes);
    // 检查资源内容
    HGLOBAL hGlobal = LoadResource(hInstance, hRes);
    if (hGlobal)
    {
        DLGTEMPLATE* pTemplate = (DLGTEMPLATE*)LockResource(hGlobal);
        if (pTemplate)
        {
            UnlockResource(hGlobal);
        }
        FreeResource(hGlobal);
    }

    return TRUE;
}

// 对话框过程
INT_PTR CALLBACK CNatTestDlg::DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CNatTestDlg* pDlg = NULL;

    // 获取类实例指针
    if (uMsg == WM_INITDIALOG)
    {
        pDlg = (CNatTestDlg*)lParam;
        if (pDlg)
        {
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pDlg);
        }
        else
        {
            // 如果没有通过 lParam 传递，使用全局指针
            pDlg = g_pCurrentNatTestDlg;
            if (pDlg)
            {
                SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pDlg);
            }
        }
    }
    else
    {
        pDlg = (CNatTestDlg*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (pDlg)
    {
        return pDlg->HandleMessage(hWnd, uMsg, wParam, lParam);
    }
    return FALSE;
}

// 子窗口对话框过程
INT_PTR CALLBACK CNatTestDlg::ChildDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CNatTestDlg* pDlg = (CNatTestDlg*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!pDlg)
    {
        if (uMsg == WM_INITDIALOG)
        {
            // 第一次创建，从全局指针获取
            pDlg = g_pCurrentNatTestDlg;
            if (pDlg)
            {
                SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pDlg);
                pDlg->m_hWnd = hWnd;
            }
        }
        else
        {
            return FALSE;
        }
    }

    if (pDlg)
    {
        return pDlg->HandleMessage(hWnd, uMsg, wParam, lParam);
    }

    return FALSE;
}

// 消息处理
INT_PTR CNatTestDlg::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInitDialog(hWnd);

    case WM_COMMAND:
        OnCommand(wParam);
        break;

    case WM_CLOSE:
        OnClose(hWnd);
        return TRUE;

    case WM_DESTROY:
        m_hWnd = NULL;
        m_hEdit = NULL;
        break;
    case WM_SHOWWINDOW:
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

// 初始化对话框
BOOL CNatTestDlg::OnInitDialog(HWND hWnd)
{
    ::SetWindowText(hWnd, GetResString(IDS_NAT_TEST));
    HWND hStartTestBtn = GetDlgItem(hWnd, IDC_START_TEST);
    if (hStartTestBtn)
    {
        ::SetWindowText(hStartTestBtn, GetResString(IDS_START_TEST));
    }

    // 设置"打开防火墙"按钮文本
    HWND hOpenFirewallBtn = GetDlgItem(hWnd, IDC_OPEN_FIREWALL);
    if (hOpenFirewallBtn)
    {

        ::SetWindowText(hOpenFirewallBtn, GetResString(IDS_OPEN_FIREWALL));
    }
    m_hWnd = hWnd;
    
    // 获取编辑框句柄
    m_hEdit = GetDlgItem(hWnd, IDC_EDIT2);
    if (m_hEdit)
    {
        // 设置编辑框为多行，带垂直滚动条
        LONG_PTR style = GetWindowLongPtr(m_hEdit, GWL_STYLE);
        style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;
        SetWindowLongPtr(m_hEdit, GWL_STYLE, style);
    }
    // 如果不是子窗口，居中显示
    if (!m_bChild && !m_bModal)
    {
        RECT rcDlg, rcParent;
        GetWindowRect(hWnd, &rcDlg);

        HWND hParent = GetParent(hWnd);
        if (hParent && IsWindow(hParent))
        {
            GetWindowRect(hParent, &rcParent);
        }
        else
        {
            SystemParametersInfo(SPI_GETWORKAREA, 0, &rcParent, 0);
        }

        int x = rcParent.left + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;

        SetWindowPos(hWnd, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    return TRUE;
}

// 处理命令消息
void CNatTestDlg::OnCommand(WPARAM wParam)
{
    WORD id = LOWORD(wParam);
    WORD notifyCode = HIWORD(wParam);
    switch (id)
    {
    case IDC_START_TEST:
    if (m_hEdit)
    {
        AddLogText(GetResString(IDS_TESTING_STUN_SERVER));

        // Get server string from configuration
        CString strServers = thePrefs.GetStunServer();
        if (strServers.IsEmpty()) {
            AfxMessageBox(GetResString(IDS_STUN_SERVER_EMPTY), MB_ICONWARNING);
            return;
        }
        CString strLog;
        // Pars server list
        std::vector<StunServerInfo> serverList;
        ParseStunServers(strServers, serverList);

        if (serverList.empty()) {
            AfxMessageBox(GetResString(IDS_STUN_SERVER_INVALID), MB_ICONWARNING);
            return;
        }
        AddLogText(GetResString(IDS_PARSED_STUN_SERVER));
        CString strFormat = GetResString(IDS_SERVER_LIST_FORMAT);
        for (size_t i = 0; i < serverList.size(); i++) {
            CString strLog;

            // 将std::string转换为CString
            CString desc(serverList[i].description.c_str());
            CString host(serverList[i].hostname.c_str());

            strLog.Format(strFormat,
                i + 1,
                desc,
                host,
                serverList[i].port);
            AddLogText(strLog);
        }
        AddLogText(_T("\r\n"));
        // Test maximum 3 servers
        int maxTestCount = min(3, (int)serverList.size());
        strFormat = GetResString(IDS_STUN_WILL_NAT_TEST);

        strLog.Format(strFormat, maxTestCount);
        AddLogText(strLog);

        NatType finalNatType = StunTypeUnknown;
        bool testSuccess = false;
        CString selectedServer;
        char logBuffer[1024];
        // Test each server
        for (int i = 0; i < maxTestCount; i++) {
            const StunServerInfo& server = serverList[i];
            AddLogText(GetResString(IDS_STUN_DETEECTING_NAT_TYPE));//Detecting NAT type...\r\n
            // Parse STUN server address
            StunAddress4 serverAddr;
            char host1[256];
            strcpy_s(host1, sizeof(host1), server.hostname.c_str());
            if (!stunParseServerName(host1, serverAddr)) {
                AddLogText(GetResString(IDS_STUN_DNS_RESOLUTION_FAILED));//DNS resolution failed\r\n\r\n
                continue;
            }
            serverAddr.port = server.port;
            // Variables for results
            bool preservePort = false;
            bool hairpin = false;
            int actualLocalPort = 0;
            strFormat = GetResString(IDS_STUN_TEST_CONNECTING_TO);// "TEST %d/%d  Connecting to: %s:%d\r\n"
            CString strLog;
            CString host(serverList[i].hostname.c_str());
            strLog.Format(strFormat,
                i + 1, maxTestCount,
                host,
                server.port);

            AddLogText(strLog);
            // Perform NAT detection
            NatType natType = stunNatType(
                serverAddr,
                false, // non-verbose mode
                &preservePort,
                &hairpin,
                0, // random port
                nullptr, // default interface
                &actualLocalPort
            );

            // Check result
            if (natType == StunTypeBlocked || natType == StunTypeFailure) {
                AddLogText(GetResString(IDS_STUN_CONNECTION_FAILED));
                continue;
            }

            // Connected successfully!\r\n
            //IDS_STUN_CONNECTED_SUCCESSFULLY
            AddLogText(GetResString(IDS_STUN_CONNECTED_SUCCESSFULLY));

            // Display detailed results
            char logBuffer[1024];
            CString strTitle, strPort, strPreserve, strHairpin;
            CString strYes, strNo;

            // 加载所有资源字符串
            strYes = GetResString(IDS_STUN_YES);                            // "YES"
            strNo = GetResString(IDS_STUN_NO);                              // "NO"
            CString strOutput;
            strOutput.Format(_T("%s%s%s%s"),
                GetResString(IDS_STUN_NAT_DETECTION_RESULTS),
                GetResString(IDS_STUN_ACTUAL_LOCAL_PORT),
                GetResString(IDS_STUN_PORT_PRESERVATION),
                GetResString(IDS_STUN_HAIRPIN_SUPPORT));
            CString strFinal;
            strFinal.Format(strOutput,
                actualLocalPort,
                preservePort ? strYes.GetString() : strNo.GetString(),
                hairpin ? strYes.GetString() : strNo.GetString());
            AddLogText(strFinal);

            // Save results
            finalNatType = natType;
            testSuccess = true;
            selectedServer = server.description.c_str();
            break; // Stop after first successful server
        }

        // Display final summary
        AddLogText(GetResString(IDS_STUN_FINAL_DETECTION_SUMMARY));     // FINAL DETECTION SUMMARY\r\n
		
        if (testSuccess) {
            // Format NAT type string
            CString natTypeStr;
            switch (finalNatType) {
            case StunTypeOpen:
                natTypeStr = GetResString(IDS_STUN_OPEN_INTERNET);
                break;  // Open Internet

            case StunTypeIndependentFilter:
                natTypeStr = GetResString(IDS_STUN_FULL_CONE_NAT);
                break;  // Full Cone NAT

            case StunTypeDependentFilter:
                natTypeStr = GetResString(IDS_STUN_RESTRICTED_CONE_NAT);
                break;  // Restricted Cone NAT

            case StunTypePortDependedFilter:
                natTypeStr = GetResString(IDS_STUN_PORT_RESTRICTED_CONE_NAT);
                break;  // Port Restricted Cone NAT

            case StunTypeDependentMapping:
                natTypeStr = GetResString(IDS_STUN_SYMMETRIC_NAT);
                break;  // Symmetric NAT

            case StunTypeFirewall:
                natTypeStr = GetResString(IDS_STUN_SYMMETRIC_FIREWALL);
                break;  // Symmetric Firewall

            case StunTypeBlocked:
                natTypeStr = GetResString(IDS_STUN_BLOCKED);
                break;  // Blocked

            default:
                natTypeStr = GetResString(IDS_STUN_UNKNOWN_TYPE);
                break;  // Unknown Type
            }
            CString srtNatType = GetResString(IDS_STUN_NAT_TYPE);       //NAT Type:%s\r\n
			srtNatType.Format(srtNatType,natTypeStr);
            AddLogText(srtNatType);
            AddLogText(GetResString(IDS_STUN_CONNECTIVITY_ASSESSMENT)); // CONNECTIVITY ASSESSMENT\r\n
            CString strAdvice;
            switch (finalNatType) {
            case StunTypeOpen:
                strAdvice = GetResString(IDS_STUN_DIRECT_HIGH_ID); //Excellent: Direct High-ID connections possible\r\n
                break;
            case StunTypeIndependentFilter:
                strAdvice = GetResString(IDS_STUN_TRAVERSA_HIGH_ID);// Good: STUN traversal can establish High-ID\r\n
                break;
            case StunTypeDependentFilter:
            case StunTypePortDependedFilter:
                strAdvice = GetResString(IDS_STUN_HOLE_PUNCHING_REQUIRED);// Moderate: P2P hole punching required\r\n
                break;
            case StunTypeDependentMapping:
                strAdvice = GetResString(IDS_STUN_RELAY_SERVER_TYPICALLY_NEEDED);// Difficult: Relay server typically needed\r\n
                break;
            case StunTypeFirewall:
                strAdvice = GetResString(IDS_STUN_FIREWALL_CONFIG_MAY_HELP);// Moderate: Firewall configuration may help\r\n
                break;
            default:
                strAdvice = GetResString(IDS_STUN_CONNECTION_QUALITY_UNCERTAIN);// Unknown: Connection quality uncertain\r\n
                break;
            }
            AddLogText(strAdvice);
        }
        else {
            AddLogText(GetResString(IDS_STUN_ALL_STUN_CONNECTIONS_FAILED));// All STUN server connections failed.\r\n\r\n
            AddLogText(GetResString(IDS_STUN_TROUBLESHOOTING_STEPS)); // Troubleshooting Steps:\r\n
            AddLogText(GetResString(IDS_STUN_VERIFY_NETWORK_CONNECTIVITY));//1.Verify network connectivity\r\n
            AddLogText(GetResString(IDS_STUN_CHECK_FIREWALL_ALLOWS_UDP)); // 2.Check firewall allows UDP traffic\r\n
            AddLogText(GetResString(IDS_STUN_TRY_DIFFERENT_STUN_SERVERS));// 3.Try different STUN servers\r\n
            AddLogText(GetResString(IDS_STUN_VERIFY_DNS_RESOLUTION_WORKING)); // 4.Verify DNS resolution is working\r\n
        }

        AddLogText(GetResString(IDS_STUN_STUN_NAT_DETECTION_COMPLETED)); //STUN NAT Detection Completed.\r\n
    }
    else
    {
    }
    break;
    case IDC_OPEN_FIREWALL:
        if (m_hEdit)
        {
            if (m_hEdit)
            {
                AddLogText(GetResString(IDS_STUN_OPENING_FIREWALL_SETTINGS));// Opening firewall settings...\r\n
                // Show error message
                // Method 1: Direct firewall.cpl (Windows XP/7/8/10/11)
                AddLogText(_T("Method 1: Trying firewall.cpl directly...\r\n"));
                HINSTANCE hResult = ShellExecute(NULL, _T("open"), _T("firewall.cpl"), NULL, NULL, SW_SHOW);
                if ((INT_PTR)hResult > 32)
                {
                    AddLogText(_T("Firewall.cpl executed successfully\r\n"));
                    AddLogText(_T("Firewall stub called.\r\n"));
                    return;
                }

                // Method 2: Using control.exe (Windows XP compatible)
                AddLogText(_T("Method 2: Trying control.exe firewall.cpl...\r\n"));
                hResult = ShellExecute(NULL, _T("open"), _T("control.exe"), _T("firewall.cpl"), NULL, SW_SHOW);
                if ((INT_PTR)hResult > 32)
                {
                    AddLogText(_T("Control.exe firewall.cpl executed successfully\r\n"));
                    AddLogText(_T("Firewall stub called.\r\n"));
                    return;
                }

                // Method 3: Using rundll32 (Windows XP compatible)
                AddLogText(_T("Method 3: Trying rundll32 method...\r\n"));
                hResult = ShellExecute(NULL, _T("open"), _T("rundll32.exe"),
                    _T("shell32.dll,Control_RunDLL firewall.cpl"), NULL, SW_SHOW);
                if ((INT_PTR)hResult > 32)
                {
                    AddLogText(_T("Rundll32 method executed successfully\r\n"));
                    AddLogText(_T("Firewall stub called.\r\n"));
                    return;
                }

                // Method 4: Direct path to firewall.cpl (XP: system32\firewall.cpl)
                AddLogText(_T("Method 4: Trying direct system32 path...\r\n"));
                TCHAR szSysDir[MAX_PATH];
                if (GetSystemDirectory(szSysDir, MAX_PATH))
                {
                    TCHAR szFirewallPath[MAX_PATH];
                    _stprintf_s(szFirewallPath, MAX_PATH, _T("%s\\firewall.cpl"), szSysDir);

                    hResult = ShellExecute(NULL, _T("open"), szFirewallPath, NULL, NULL, SW_SHOW);
                    if ((INT_PTR)hResult > 32)
                    {
                        AddLogText(_T("Direct system32 path executed successfully\r\n"));
                        AddLogText(_T("Firewall stub called.\r\n"));
                        return;
                    }
                }

                // Method 5: Using command prompt (Windows XP compatible)
                AddLogText(_T("Method 5: Trying command prompt method...\r\n"));
                hResult = ShellExecute(NULL, _T("open"), _T("cmd.exe"),
                    _T("/c start firewall.cpl"), NULL, SW_HIDE);
                if ((INT_PTR)hResult > 32)
                {
                    // Wait a bit for the command to execute
                    Sleep(500);
                    AddLogText(_T("Command prompt method executed\r\n"));
                    AddLogText(_T("Firewall stub called.\r\n"));
                    return;
                }

                // Method 6: Try wscui.cpl (Windows Security Center in XP, includes firewall link)
                AddLogText(_T("Method 6: Trying Windows Security Center...\r\n"));
                hResult = ShellExecute(NULL, _T("open"), _T("wscui.cpl"), NULL, NULL, SW_SHOW);
                if ((INT_PTR)hResult > 32)
                {
                    AddLogText(_T("Windows Security Center opened\r\n"));
                    AddLogText(_T("Firewall stub called.\r\n"));
                    return;
                }

                // Method 7: Try ncpa.cpl (Network Connections, has firewall link in some versions)
                AddLogText(_T("Method 7: Trying Network Connections...\r\n"));
                hResult = ShellExecute(NULL, _T("open"), _T("ncpa.cpl"), NULL, NULL, SW_SHOW);
                if ((INT_PTR)hResult > 32)
                {
                    AddLogText(_T("Network Connections opened\r\n"));
                    AddLogText(_T("Firewall stub called.\r\n"));
                    return;
                }

                // Method 8: Try Windows Firewall with Advanced Security (Vista+)
                // Only try this if we suspect it's Vista or later
                AddLogText(_T("Method 8: Trying wf.msc (Vista+)...\r\n"));
                hResult = ShellExecute(NULL, _T("open"), _T("wf.msc"), NULL, NULL, SW_SHOW);
                if ((INT_PTR)hResult > 32)
                {
                    AddLogText(_T("Windows Firewall with Advanced Security opened\r\n"));
                    AddLogText(_T("Firewall stub called.\r\n"));
                    return;
                }
                // All methods failed
                AddLogText(GetResString(IDS_STUN_FIREWALL_OPEN_ALL_FAILED));// All methods failed to open firewall settings\r\nPlease open it manually from Control Panel.
                
                // Show error message
                AfxMessageBox(GetResString(IDS_STUN_FIREWALL_OPEN_ALL_FAILED),
                    MB_ICONERROR | MB_OK);
            }
        }
        break;

    case IDOK:
        if (m_bModal)
        {
            EndDialog(m_hWnd, IDOK);
        }
        else
        {
            Close();
        }
        break;

    case IDCANCEL:
        if (m_bModal)
        {
            EndDialog(m_hWnd, IDCANCEL);
        }
        else
        {
            Close();
        }
        break;
    }
}
// 解析STUN服务器配置字符串
void CNatTestDlg::ParseStunServers(const CString& strConfig, std::vector<StunServerInfo>& servers)
{
    servers.clear();

    // 分割服务器字符串（支持竖线或分号分隔）
    CString strDelimiters = _T("|;");
    int pos = 0;
    CString strItem;

    while (!(strItem = strConfig.Tokenize(strDelimiters, pos)).IsEmpty()) {
        strItem.Trim();
        if (strItem.IsEmpty()) {
            continue;
        }

        // 解析主机名和端口
        StunServerInfo info;
        CString strHostPort = strItem;

        // 查找端口分隔符
        int colonPos = strHostPort.ReverseFind(':');
        if (colonPos != -1) {
            // 提取主机名
            info.hostname = CT2A(strHostPort.Left(colonPos));

            // 提取端口
            CString strPort = strHostPort.Mid(colonPos + 1);
            info.port = _ttoi(strPort);

            // 如果没有端口或端口为0，使用默认端口
            if (info.port == 0) {
                info.port = 3478; // STUN默认端口
            }
        }
        else {
            // 没有端口，使用默认端口
            info.hostname = CT2A(strHostPort);
            info.port = 3478;
        }
        servers.push_back(info);

        // 最多只取前几个
        if (servers.size() >= 10) { // 限制最多10个，后面会再限制为3个
            break;
        }
    }
}
// 处理关闭消息
void CNatTestDlg::OnClose(HWND hWnd)
{
    if (m_bModal)
    {
        // 模态对话框，使用 EndDialog
        EndDialog(hWnd, IDCANCEL);
        m_hWnd = NULL;
    }
    else
    {
        // 非模态对话框，使用 DestroyWindow
        DestroyWindow(hWnd);
    }
}