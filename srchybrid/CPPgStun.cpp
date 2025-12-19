// CPPgStun.cpp
#include "stdafx.h"
#include "CPPgStun.h"
#include "emule.h"
#include "Preferences.h"
#include "OtherFunctions.h"
#include "emuledlg.h"
#include <Log.h>
#include <NatTestDlg.h>
#include <StunManager.h>
#include <MenuCmds.h>
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNAMIC(CStunSettingsPage, CPropertyPage)

BEGIN_MESSAGE_MAP(CStunSettingsPage, CPropertyPage)
    ON_BN_CLICKED(IDC_ENABLE_STUN, OnBnClickedEnableStun)
    ON_EN_CHANGE(IDC_STUN_SERVER, OnEnChangeStunServer)
    ON_BN_CLICKED(IDC_TEST_STUN, OnBnClickedTestStun)
    ON_BN_CLICKED(IDC_GET_STUN_SERVER, &CStunSettingsPage::OnBnClickedGetStunServer)
END_MESSAGE_MAP()

CStunSettingsPage::CStunSettingsPage()
    : CPropertyPage(CStunSettingsPage::IDD)
    , m_bEnableStun(FALSE)
    , m_strStunServer(_T(""))
    , m_strOriginalStunServer(_T(""))
{
    // 从配置加载初始值
    LoadSettings();
}

CStunSettingsPage::~CStunSettingsPage()
{
}

void CStunSettingsPage::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_STUN_SERVER, m_ctlStunServer);
    DDX_Control(pDX, IDC_ENABLE_STUN, m_ctlEnableStun);
    DDX_Text(pDX, IDC_STUN_SERVER, m_strStunServer);
    DDX_Check(pDX, IDC_ENABLE_STUN, m_bEnableStun);
}

BOOL CStunSettingsPage::OnInitDialog()
{
    CPropertyPage::OnInitDialog();

    // 加载设置
    LoadSettings();
    Localize();
    // 更新控件状态
    UpdateControls();

    return TRUE;
}

void CStunSettingsPage::LoadSettings()
{
    // 从 thePrefs（全局配置对象）加载 STUN 设置
    // 启用状态
    m_bEnableStun = thePrefs.GetEnableStun();
    // STUN 服务器地址
    m_strStunServer = thePrefs.GetStunServer();
    if (m_strStunServer.IsEmpty())
    {
        m_strStunServer = "";
    }
    // 保存原始值用于比较
    m_strOriginalStunServer = m_strStunServer;

    // 如果已经绑定到对话框，更新控件
    if (m_hWnd)
    {
        UpdateData(FALSE);
        UpdateControls();
    }
}

void CStunSettingsPage::SaveSettings()
{
    // 保存设置到 thePrefs

    // 保存启用状态
    thePrefs.SetEnableStun(m_bEnableStun);

    // 保存 STUN 服务器
    if (m_strStunServer.IsEmpty())
    {
        thePrefs.SetStunServer(L"");
    }
    else
    {
        // 去除首尾空格
        m_strStunServer.Trim();
        thePrefs.SetStunServer(m_strStunServer);
    }
}

void CStunSettingsPage::UpdateControls()
{
    // 根据启用状态更新控件可用性
    BOOL bEnable = m_bEnableStun;

    // STUN 服务器编辑框
    m_ctlStunServer.EnableWindow(bEnable);

    // 测试按钮
    CWnd* pTestBtn = GetDlgItem(IDC_TEST_STUN);
    if (pTestBtn)
        pTestBtn->EnableWindow(bEnable && !m_strStunServer.IsEmpty());
}

BOOL CStunSettingsPage::ValidateStunServer(CString strServer)
{
    strServer.Trim();

    if (strServer.IsEmpty())
    {
        AfxMessageBox(GetResString(IDS_STUN_SERVER_EMPTY), MB_ICONWARNING);
        return FALSE;
    }

    // 支持的分隔符：分号、逗号、竖线
    CString strDelimiters = _T("|");

    int nPos = 0;
    CString strToken;
    int nServerCount = 0;

    // 遍历所有服务器
    while (!(strToken = strServer.Tokenize(strDelimiters, nPos)).IsEmpty())
    {
        strToken.Trim();
        if (strToken.IsEmpty())
            continue;

        nServerCount++;

        // 验证单个服务器格式
        if (!ValidateSingleStunServer(strToken))
        {
            CString strMsg;
            strMsg.Format(GetResString(IDS_STUN_SERVER_INVALID), strToken);
            AfxMessageBox(strMsg, MB_ICONWARNING);
            return FALSE;
        }
    }

    if (nServerCount == 0)
    {
        AfxMessageBox(GetResString(IDS_STUN_SERVER_EMPTY), MB_ICONWARNING);
        return FALSE;
    }

    return TRUE;
}

BOOL CStunSettingsPage::ValidateSingleStunServer(CString strServer)
{
    strServer.Trim();

    if (strServer.IsEmpty())
        return FALSE;

    // 基本检查：不能包含非法字符
    // 允许的字符：字母、数字、点、冒号、减号、下划线
    CString strAllowedChars = _T("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.:-_");

    for (int i = 0; i < strServer.GetLength(); i++)
    {
        TCHAR ch = strServer[i];

        // 检查是否在允许的字符范围内
        BOOL bValid = FALSE;
        for (int j = 0; j < strAllowedChars.GetLength(); j++)
        {
            if (ch == strAllowedChars[j])
            {
                bValid = TRUE;
                break;
            }
        }

        if (!bValid)
        {
            // 发现非法字符
            return FALSE;
        }
    }

    // 检查格式：必须包含冒号分隔主机和端口
    int nColonPos = strServer.Find(_T(':'));
    if (nColonPos == -1)
    {
        // 如果没有冒号，假设是默认端口，返回true
        return TRUE;
    }

    // 如果有冒号，检查冒号后的部分是否为数字
    CString strPort = strServer.Mid(nColonPos + 1);
    strPort.Trim();

    if (strPort.IsEmpty())
        return FALSE;

    // 检查端口是否为纯数字
    for (int i = 0; i < strPort.GetLength(); i++)
    {
        if (!_istdigit(strPort[i]))
            return FALSE;
    }

    // 检查端口范围
    int nPort = _ttoi(strPort);
    if (nPort < 1 || nPort > 65535)
        return FALSE;

    return TRUE;
}

void CStunSettingsPage::Localize()
{
    if (m_hWnd) {
        GetDlgItem(IDC_STUN_GROUP)->SetWindowText(GetResString(IDS_STUN_GROUP));
        GetDlgItem(IDC_ENABLE_STUN)->SetWindowText(GetResString(IDS_ENABLE_STUN));
        GetDlgItem(IDC_STUN_SERVER_LBL)->SetWindowText(GetResString(IDS_STUN_SERVER));
        GetDlgItem(IDC_GET_STUN_SERVER)->SetWindowText(GetResString(IDS_GET_STUN_SERVER));
        GetDlgItem(IDC_TEST_STUN)->SetWindowText(GetResString(IDS_TEST_STUN));
        GetDlgItem(IDC_STUN_TRAVERSAL)->SetWindowText(GetResString(IDS_STUN_TRAVERSAL));
        GetDlgItem(IDC_STUN_TRAVERSAL_LLB)->SetWindowText(GetResString(IDS_STUN_TRAVERSAL_LLB));
    }
}

BOOL CStunSettingsPage::OnApply()
{
    if (!UpdateData(TRUE))
        return FALSE;

    // 验证输入
    if (m_bEnableStun && !ValidateStunServer(m_strStunServer))
    {
        GetDlgItem(IDC_STUN_SERVER)->SetFocus();
        return FALSE;
    }
    // 保存更改前的状态
    BOOL bWasEnabled = thePrefs.GetEnableStun();
    // 检查是否有更改
    BOOL bChanged = FALSE;

    if (thePrefs.GetEnableStun() != (m_bEnableStun == TRUE))
        bChanged = TRUE;

    if (m_strOriginalStunServer != m_strStunServer)
        bChanged = TRUE;

    if (bChanged)
    {
        // 保存设置
        SaveSettings();
        if (bWasEnabled != m_bEnableStun)
        {
            // 立即触发 STUN 状态改变事件
            OnStunEnabledChanged(m_bEnableStun == TRUE);
        }
        // 更新原始值
        m_strOriginalStunServer = m_strStunServer;

        return CPropertyPage::OnApply();
    }

    return TRUE;
}
void CStunSettingsPage::OnStunEnabledChanged(BOOL isEnableStun) {
    if (isEnableStun)
    {
        // 获取资源字符串（如果没有定义，可以使用硬编码字符串）
        CString strTitle, strMessage, strWarning;

        //strTitle = GetResString(IDS_CONFIRM_STUN_ENABLE);
        if (strTitle.IsEmpty())
            strTitle = _T("启用STUN穿透确认");

        //strMessage = GetResString(IDS_STUN_ENABLE_WARNING);
        if (strMessage.IsEmpty())
            strMessage = _T("启用STUN穿透功能需要重启emlue才能生效，重启后将自动尝试穿透并连接emlue服务器和KAD网络。\n重要提醒：\n\n请确保路由器/光猫已开启UPnP功能\n确认您的网络环境支持\n\n确定要启用STUN穿透吗？");

        //strWarning = GetResString(IDS_STUN_SECURITY_WARNING);
        if (!strWarning.IsEmpty())
        {
            strMessage += _T("\n\n");
            strMessage += strWarning;
        }

        // 显示确认对话框
        int nResult = AfxMessageBox(strMessage, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2, 0);

        if (nResult == IDNO)
        {
            // 用户取消，恢复原来的设置
            m_bEnableStun = FALSE;

            // 更新控件显示
            m_ctlEnableStun.SetCheck(BST_UNCHECKED);

            // 更新控件状态
            UpdateControls();

            // 标记为已修改（这样应用按钮会保持可用）
            SetModified(TRUE);

            // 焦点回到启用复选框
            m_ctlEnableStun.SetFocus();

            return;
        }
    }
}
void CStunSettingsPage::OnOK()
{
    OnApply();
    CPropertyPage::OnOK();
}

BOOL CStunSettingsPage::OnCommand(WPARAM wParam, LPARAM lParam)
{
    if (wParam == ID_HELP)
    {
        OnHelp();
        return TRUE;
    }

    return CPropertyPage::OnCommand(wParam, lParam);
}

// 消息处理函数
void CStunSettingsPage::OnBnClickedEnableStun()
{
    UpdateData(TRUE);
    UpdateControls();
    SetModified();
}

void CStunSettingsPage::OnEnChangeStunServer()
{
    CString strTemp;
    m_ctlStunServer.GetWindowText(strTemp);

    // 检查是否改变
    if (strTemp != m_strStunServer)
    {
        m_strStunServer = strTemp;
        SetModified();

        // 更新测试按钮状态
        CWnd* pTestBtn = GetDlgItem(IDC_TEST_STUN);
        if (pTestBtn)
            pTestBtn->EnableWindow(m_bEnableStun && !strTemp.IsEmpty());
    }
}

void CStunSettingsPage::OnBnClickedTestStun()
{
    UpdateData(TRUE);

    if (!ValidateStunServer(m_strStunServer))
        return;
    CNatTestDlg natDlg;
    // 现在应该能正常显示了
    natDlg.ShowModal(AfxGetInstanceHandle(),
        GetParent()->GetSafeHwnd());
}


void CStunSettingsPage::OnHelp()
{
    // 显示帮助
    theApp.ShowHelp(IDD_PPG_STUN);
}

BOOL CStunSettingsPage::OnHelpInfo(HELPINFO* pHelpInfo)
{
    OnHelp();
    return TRUE;
}
void CStunSettingsPage::OnBnClickedGetStunServer()
{
    CString strHelpURL;
    strHelpURL.Format(_T("https://github.com/pradt2/always-online-stun/blob/master/valid_hosts_tcp.txt"));
    ShellExecute(NULL, NULL, strHelpURL, NULL, thePrefs.GetMuleDirectory(EMULE_EXECUTEABLEDIR), SW_SHOWDEFAULT);

}
