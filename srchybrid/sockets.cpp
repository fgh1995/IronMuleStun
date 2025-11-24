//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#include "emule.h"
#include "Sockets.h"
#include "Opcodes.h"
#include "UDPSocket.h"
#include "Exceptions.h"
#include "OtherFunctions.h"
#include "Statistics.h"
#include "ServerSocket.h"
#include "ServerList.h"
#include "Server.h"
#include "ListenSocket.h"
#include "SafeFile.h"
#include "Packets.h"
#include "SharedFileList.h"
#include "PeerCacheFinder.h"
#include "emuleDlg.h"
#include "SearchDlg.h"
#include "ServerWnd.h"
#include "TaskbarNotifier.h"
#include "Log.h"
#include "WinTcpStun.h"
#include <MenuCmds.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


void CServerConnect::TryAnotherConnectionRequest()
{
	if (connectionattemps.GetCount() < (thePrefs.IsSafeServerConnectEnabled() ? 1 : 2))
	{
		CServer* next_server = theApp.serverlist->GetNextServer(m_bTryObfuscated);
		if (next_server == NULL)
		{
			if (connectionattemps.GetCount() == 0)
			{
				if (m_bTryObfuscated && !thePrefs.IsClientCryptLayerRequired()){
					// try all servers on the non-obfuscated port next
					m_bTryObfuscated = false;
					ConnectToAnyServer(0, true, true, true);
				}
				else if (m_idRetryTimer == 0)
				{
					// 05-Nov-2003: If we have a very short server list, we could put serious load on those few servers
					// if we start the next connection tries without waiting.
					LogWarning(LOG_STATUSBAR, GetResString(IDS_OUTOFSERVERS));
					AddLogLine(false, GetResString(IDS_RECONNECT), CS_RETRYCONNECTTIME);
					m_uStartAutoConnectPos = 0; // default: start at 0
					VERIFY( (m_idRetryTimer = SetTimer(NULL, 0, 1000*CS_RETRYCONNECTTIME, RetryConnectTimer)) != NULL );
					if (thePrefs.GetVerbose() && !m_idRetryTimer)
						DebugLogError(_T("Failed to create 'server connect retry' timer - %s"), GetErrorMessage(GetLastError()));
				}
			}
			return;
		}

		// Barry - Only auto-connect to static server option
		if (thePrefs.GetAutoConnectToStaticServersOnly())
		{
			if (next_server->IsStaticMember())
                ConnectToServer(next_server, true, !m_bTryObfuscated);
		}
		else
			ConnectToServer(next_server, true, !m_bTryObfuscated);
	}
}

void CServerConnect::ConnectToAnyServer(UINT startAt, bool prioSort, bool isAuto, bool bNoCrypt)
{
	StopConnectionTry();
	Disconnect();
	connecting = true;
	singleconnecting = false;
	theApp.emuledlg->ShowConnectionState();
	m_bTryObfuscated = thePrefs.IsServerCryptLayerTCPRequested() && !bNoCrypt;

	// Barry - Only auto-connect to static server option
	if (thePrefs.GetAutoConnectToStaticServersOnly() && isAuto)
	{
		bool anystatic = false;
		CServer *next_server;
		theApp.serverlist->SetServerPosition(startAt);
		while ((next_server = theApp.serverlist->GetNextServer(false)) != NULL)
		{
			if (next_server->IsStaticMember()) {
				anystatic = true;
				break;
			}
		}
		if (!anystatic) {
			connecting = false;
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_NOVALIDSERVERSFOUND));
			return;
		}
	}

	theApp.serverlist->SetServerPosition(startAt);
	if (thePrefs.GetUseUserSortedServerList() && startAt == 0 && prioSort)
		theApp.serverlist->GetUserSortedServers();
	if (thePrefs.GetUseServerPriorities() && prioSort)
		theApp.serverlist->Sort();

	if (theApp.serverlist->GetServerCount() == 0) {
		connecting = false;
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_NOVALIDSERVERSFOUND));
		return;
	}
	theApp.listensocket->Process();

	TryAnotherConnectionRequest();
}

void CServerConnect::ConnectToServer(CServer* server, bool multiconnect, bool bNoCrypt)
{
	if (!multiconnect) {
		StopConnectionTry();
		Disconnect();
	}
	
	CTcpStunClient stunClient;
	std::string publicIP;
	uint16_t publicPort = 0;
	// 执行UPnP操作
	if (stunClient.DoTcpStun("stun.voipia.net", 3478, publicIP, publicPort, m_lastLocalPort)) {
		CString stunResult;
		stunResult.Format(_T("获取映射关系完毕:%d -> 公网IP端口: %s:%d"),
			m_lastLocalPort, CString(publicIP.c_str()), publicPort);
		theApp.QueueLogLineEx(LOG_SUCCESS, stunResult);
		if (!theApp.m_UPnPManager.isInitialized()) {
			LogError(_T("UPNP: 找不到UPNP设备"));
		}
		else {
			if (theApp.m_UPnPManager.DeletePortMapping(thePrefs.lastStunLocalPort, "TCP"))
			{
				theApp.QueueLogLineEx(LOG_SUCCESS, L"删除映射成功，端口:%d", thePrefs.lastStunLocalPort);
			}
			else {
				theApp.QueueLogLineEx(LOG_SUCCESS, L"删除映射失败，端口:%d", thePrefs.lastStunLocalPort);
			}
			
			if (theApp.m_UPnPManager.AddPortMapping(publicPort, m_lastLocalPort, "TCP", "eMule TCP(STUN)")) {
				theApp.QueueLogLineEx(LOG_SUCCESS, L"映射成功:%d -> 公网IP端口: %s:%d", m_lastLocalPort, CString(publicIP.c_str()), publicPort);
				// 保存最后一次STUN穿透发起的本地端口,用于删除之前的映射
				thePrefs.lastStunLocalPort = m_lastLocalPort;
				thePrefs.Save();
			}
			else {
				LogError(L"映射失败:%d -> 公网IP端口: %s:%d", m_lastLocalPort, CString(publicIP.c_str()), publicPort);
			}
			// 检查映射关系是否有变化，如果发生了变化则重启骡子
			if (m_lastPublicIP != publicIP || m_lastPublicPort != publicPort)
			{
				if (!theApp.IsPortchangeAllowed())
				{
					StopStunCheck();
					AfxMessageBox(GetResString(IDS_NOPORTCHANGEPOSSIBLE));
					// 设置重启标志为真,用于关闭骡子时,判断是否重启一个进程
					theApp.IsReboot = true;
					LogError(L"正在自动重启...");
					theApp.emuledlg->PostMessage(WM_COMMAND, MP_EXIT, 0);
					return;
				}
			}
			theApp.QueueLogLineEx(LOG_SUCCESS, L"修改监听端口:%d 为:%d", thePrefs.port, publicPort);
			thePrefs.port = publicPort;
			theApp.listensocket->Rebind();
			theApp.QueueLogLineEx(LOG_SUCCESS, L"开始监听新端口:%d",publicPort);
			theApp.QueueLogLineEx(LOG_SUCCESS, L"TCP STUN穿透完成：如果你的网络环境为全锥型，则应该能获得高ID。");
			// 保存公网IP端口用于后续STUN检测
			m_lastPublicIP = publicIP;
			m_lastPublicPort = publicPort;
		}
	}
	else {
		theApp.QueueLogLineEx(LOG_SUCCESS, L"TCP STUN穿透失败！");
	}
	connecting = true;
	singleconnecting = !multiconnect;
	theApp.emuledlg->ShowConnectionState();
	CServerSocket* newsocket = new CServerSocket(this, !multiconnect);
	m_lstOpenSockets.AddTail((void*&)newsocket);
	//由于STUN穿透端口是否变化部分需要使用同一个端口发起检测,因此第五个参数赋值为真,允许端口重用
	newsocket->Create(m_lastLocalPort, SOCK_STREAM, FD_READ | FD_WRITE | FD_CLOSE | FD_CONNECT, thePrefs.GetBindAddrA(),true);
	newsocket->ConnectTo(server, bNoCrypt);
	connectionattemps.SetAt(GetTickCount(), newsocket);
	StartStunCheck();
}
// 启动STUN检查线程
void CServerConnect::StartStunCheck()
{
	if (m_bStunCheckRunning) {
		return;
	}
	m_bStunCheckRunning = true;
	m_hStunCheckThread = CreateThread(NULL, 0, StunCheckThread, this, 0, NULL);
	if (m_hStunCheckThread == NULL) {
		m_bStunCheckRunning = false;
		LogError(_T("无法创建STUN检查线程"));
	}
	else {
		theApp.QueueLogLineEx(LOG_SUCCESS, L"启动STUN映射关系检查线程");
	}
}

// 停止STUN检查线程
void CServerConnect::StopStunCheck()
{
	if (m_bStunCheckRunning) {
		m_bStunCheckRunning = false;
		if (m_hStunCheckThread) {
			WaitForSingleObject(m_hStunCheckThread, 5000);
			CloseHandle(m_hStunCheckThread);
			m_hStunCheckThread = NULL;
		}
		theApp.QueueLogLineEx(LOG_SUCCESS, L"停止STUN映射关系检查线程");
	}
}

// STUN检查线程函数
DWORD WINAPI CServerConnect::StunCheckThread(LPVOID lpParam)
{
	CServerConnect* pThis = static_cast<CServerConnect*>(lpParam);

	while (pThis->m_bStunCheckRunning) {
		// 每10秒检查一次
		Sleep(10000);

		if (!pThis->m_bStunCheckRunning) {
			break;
		}

		pThis->CheckStunMapping();
	}

	return 0;
}

// 检查STUN映射关系
void CServerConnect::CheckStunMapping()
{
	if (!m_bStunCheckRunning || m_lastLocalPort == 0) {
		return;
	}

	// 检查服务器连接状态
	if (!connectedsocket || !connectedsocket->IsConnected()) {
		theApp.QueueLogLineEx(LOG_WARNING, L"服务器连接已断开，停止STUN检测");
		StopStunCheck();
		return;
	}

	CTcpStunClient stunClient;
	std::string currentPublicIP;
	uint16_t currentPublicPort = 0;

	theApp.QueueLogLineEx(LOG_DEBUG, L"开始STUN检测，使用本地端口: %d", m_lastLocalPort);

	// 使用独立的socket但绑定到同一个本地端口进行STUN检测
	if (stunClient.DoTcpStunWithSamePort(m_lastLocalPort, "stun.voipia.net", 3478,
		currentPublicIP, currentPublicPort)) {
		// 处理检测结果
		HandleStunDetectionResult(currentPublicIP, currentPublicPort, m_lastLocalPort);
	}
	else {
		theApp.QueueLogLineEx(LOG_WARNING, L"STUN检测失败，本地端口: %d", m_lastLocalPort);

		// 如果连续多次失败，可能需要重新进行完整的STUN穿透
		static int failedCount = 0;
		failedCount++;
		if (failedCount >= 3) {
			theApp.QueueLogLineEx(LOG_WARNING, L"STUN检测连续失败 %d 次，可能需要重新穿透", failedCount);
			failedCount = 0;
		}
	}
}

void CServerConnect::HandleStunDetectionResult(const std::string& currentPublicIP,
	uint16_t currentPublicPort, uint16_t localPort)
{
	if (m_lastPublicPort == 0)
	{
		theApp.QueueLogLineEx(LOG_WARNING, L"检测STUN穿透异常:上一次公网端口为0，无法进行比对! %d", m_lastPublicPort);
		return;
	}
	// 检查映射关系是否有变化
	if (currentPublicIP != m_lastPublicIP || currentPublicPort != m_lastPublicPort) {
		CString changeMsg;
		changeMsg.Format(_T("STUN映射关系发生变化: %s:%d -> %s:%d (本地端口:%d)"),
			CString(m_lastPublicIP.c_str()), m_lastPublicPort,
			CString(currentPublicIP.c_str()), currentPublicPort, localPort);
		LogError(changeMsg);
		// 设置重启标志为真,用于关闭骡子时,判断是否重启一个进程
		theApp.IsReboot = true;
		LogError(L"正在自动重启...");
		// 设置自动连接为真 即 启动后就开始连接服务器以达到重启后自动连接的功能
		thePrefs.autoconnect = true;
		thePrefs.Save();
		theApp.emuledlg->PostMessage(WM_COMMAND, MP_EXIT, 0);
	}
	else {
		// 映射关系稳定
		theApp.QueueLogLineEx(LOG_SUCCESS,
			L"STUN映射关系稳定:%d -> %s:%d",
			localPort,
			CString(currentPublicIP.c_str()), currentPublicPort);
	}
}
void CServerConnect::StopConnectionTry()
{
	connectionattemps.RemoveAll();
	connecting = false;
	singleconnecting = false;
	theApp.emuledlg->ShowConnectionState();

	if (m_idRetryTimer) 
	{ 
		KillTimer(NULL, m_idRetryTimer); 
		m_idRetryTimer= 0; 
	} 

	// close all currenty opened sockets except the one which is connected to our current server
	for( POSITION pos = m_lstOpenSockets.GetHeadPosition(); pos != NULL; )
	{
		CServerSocket* pSck = (CServerSocket*)m_lstOpenSockets.GetNext(pos);
		if (pSck == connectedsocket)		// don't destroy socket which is connected to server
			continue;
		if (pSck->m_bIsDeleting == false)	// don't destroy socket if it is going to destroy itself later on
			DestroySocket(pSck);
	}
}

void CServerConnect::ConnectionEstablished(CServerSocket* sender)
{
	if (!connecting) {
		// we are already connected to another server
		DestroySocket(sender);
		return;
	}
	
	InitLocalIP();
	if (sender->GetConnectionState() == CS_WAITFORLOGIN)
	{
		AddLogLine(false, GetResString(IDS_CONNECTEDTOREQ), sender->cur_server->GetListName(), sender->cur_server->GetAddress(), sender->IsServerCryptEnabledConnection() ? sender->cur_server->GetObfuscationPortTCP() : sender->cur_server->GetPort());

		CServer* pServer = theApp.serverlist->GetServerByAddress(sender->cur_server->GetAddress(), sender->cur_server->GetPort());
		if (pServer) {
			pServer->ResetFailedCount();
			theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
		}

		// Send login packet
		CSafeMemFile data(256);
		data.WriteHash16(thePrefs.GetUserHash());
		data.WriteUInt32(GetClientID());
		data.WriteUInt16(thePrefs.GetPort());

		UINT tagcount = 4 + 1; //ipv6
		data.WriteUInt32(tagcount);

		CTag tagName(CT_NAME, thePrefs.GetUserNick());
		tagName.WriteTagToFile(&data);

		CTag tagVersion(CT_VERSION, EDONKEYVERSION);
		tagVersion.WriteTagToFile(&data);

		uint32 dwCryptFlags = 0;
		if (thePrefs.IsClientCryptLayerSupported())
			dwCryptFlags |= SRVCAP_SUPPORTCRYPT;
		if (thePrefs.IsClientCryptLayerRequested())
			dwCryptFlags |= SRVCAP_REQUESTCRYPT;
		if (thePrefs.IsClientCryptLayerRequired())
			dwCryptFlags |= SRVCAP_REQUIRECRYPT;

		CTag tagFlags(CT_SERVER_FLAGS, SRVCAP_ZLIB | SRVCAP_NEWTAGS | SRVCAP_LARGEFILES | SRVCAP_UNICODE | dwCryptFlags);
		tagFlags.WriteTagToFile(&data);

		// eMule Version (14-Mar-2004: requested by lugdunummaster (need for LowID clients which have no chance 
		// to send an Hello packet to the server during the callback test))
		CTag tagMuleVersion(CT_EMULE_VERSION, 
							//(uCompatibleClientID		<< 24) |
							(CemuleApp::m_nVersionMjr	<< 17) |
							(CemuleApp::m_nVersionMin	<< 10) |
							(CemuleApp::m_nVersionUpd	<<  7) );
		tagMuleVersion.WriteTagToFile(&data);
		
		CTag ipv6(CT_IPV6, theApp.GetPublicIPv6().ToCString());
		ipv6.WriteTagToFile(&data);

		Packet* packet = new Packet(&data);
		packet->opcode = OP_LOGINREQUEST;
		if (thePrefs.GetDebugServerTCPLevel() > 0)
			Debug(_T(">>> Sending OP__LoginRequest\n"));
		theStats.AddUpDataOverheadServer(packet->size);
		SendPacket(packet, true, sender);
	}
	else if (sender->GetConnectionState() == CS_CONNECTED)
	{
		theStats.reconnects++;
		theStats.serverConnectTime = GetTickCount();
		connected = true;
		CString strMsg;
		if (sender->IsObfusicating())
			strMsg.Format(GetResString(IDS_CONNECTEDTOOBFUSCATED) + _T(" (%s:%u)"), sender->cur_server->GetListName(), sender->cur_server->GetAddress(), sender->cur_server->GetObfuscationPortTCP());
		else
			strMsg.Format(GetResString(IDS_CONNECTEDTO) + _T(" (%s:%u)"), sender->cur_server->GetListName(), sender->cur_server->GetAddress(), sender->cur_server->GetPort());

		Log(LOG_SUCCESS | LOG_STATUSBAR, strMsg);
		theApp.emuledlg->ShowConnectionState();
		connectedsocket = sender;
		StopConnectionTry();
		theApp.sharedfiles->ClearED2KPublishInfo();
		theApp.sharedfiles->SendListToServer();
		theApp.emuledlg->serverwnd->serverlistctrl.RemoveAllDeadServers();

		// tecxx 1609 2002 - serverlist update
		if (thePrefs.GetAddServersFromServer())
		{
			Packet* packet = new Packet(OP_GETSERVERLIST,0);
			if (thePrefs.GetDebugServerTCPLevel() > 0)
				Debug(_T(">>> Sending OP__GetServerList\n"));
			theStats.AddUpDataOverheadServer(packet->size);
			SendPacket(packet, true);
		}

		CServer* pServer = theApp.serverlist->GetServerByAddress(sender->cur_server->GetAddress(), sender->cur_server->GetPort());
		if (pServer)
			theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
	}
	theApp.emuledlg->ShowConnectionState();
}

bool CServerConnect::SendPacket(Packet* packet,bool delpacket, CServerSocket* to){
	if (!to){
		if (connected){
			connectedsocket->SendPacket(packet,delpacket,true);
		}
		else{
			if (delpacket)
				delete packet;
			return false;
		}
	}
	else{
		to->SendPacket(packet,delpacket,true);
	}
	return true;
}

bool CServerConnect::SendUDPPacket(Packet* packet, CServer* host, bool delpacket, uint16 nSpecialPort, BYTE* pRawPacket, uint32 nLen){
	if (theApp.IsConnected()){
		if (udpsocket != NULL)
			udpsocket->SendPacket(packet, host, nSpecialPort, pRawPacket, nLen);
	}
	if (delpacket){
		delete packet;
		delete[] pRawPacket;
	}
	return true;
}

void CServerConnect::ConnectionFailed(CServerSocket* sender)
{
	if (!connecting && sender != connectedsocket) {
		// just return, cleanup is done by the socket itself
		return;
	}

	CServer* pServer = theApp.serverlist->GetServerByAddress(sender->cur_server->GetAddress(), sender->cur_server->GetPort());
	switch (sender->GetConnectionState())
	{
		case CS_FATALERROR:
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_FATAL));
			break;
		case CS_DISCONNECTED:
			theApp.sharedfiles->ClearED2KPublishInfo();
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_LOSTC), sender->cur_server->GetListName(), sender->cur_server->GetAddress(), sender->cur_server->GetPort());
			break;
		case CS_SERVERDEAD:
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_DEAD), sender->cur_server->GetListName(), sender->cur_server->GetAddress(), sender->cur_server->GetPort());
			if (pServer) {
				pServer->AddFailedCount();
				theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
			}
			break;
		case CS_ERROR:
			break;
		case CS_SERVERFULL:
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_FULL), sender->cur_server->GetListName(), sender->cur_server->GetAddress(), sender->cur_server->GetPort());
			break;
		case CS_NOTCONNECTED:
			break;
	}

	// IMPORTANT: mark this socket not to be deleted in StopConnectionTry(),
	// because it will delete itself after this function!
	sender->m_bIsDeleting = true;

	switch (sender->GetConnectionState())
	{
		case CS_FATALERROR:{
			bool autoretry = !singleconnecting;
			StopConnectionTry();
			if (thePrefs.Reconnect() && autoretry && !m_idRetryTimer) {
				LogWarning(GetResString(IDS_RECONNECT), CS_RETRYCONNECTTIME);

				// There are situations where we may get Winsock error codes which indicate
				// that the network is down, although it is not. Those error codes may get
				// thrown only for particular IPs. If the first server in our list has such
				// an IP and will therefor throw such an error we would never connect to
				// any server at all. To circumvent that, start the next auto-connection
				// attempt with a different server (use the next server in the list).
				m_uStartAutoConnectPos = 0; // default: start at 0
				if (pServer) {
					// If possible, use the "next" server.
					int iPosInList = theApp.serverlist->GetPositionOfServer(pServer);
					if (iPosInList >= 0)
						m_uStartAutoConnectPos = (iPosInList + 1) % theApp.serverlist->GetServerCount();
				}
				VERIFY( (m_idRetryTimer = SetTimer(NULL, 0, 1000*CS_RETRYCONNECTTIME, RetryConnectTimer)) != NULL );
				if (thePrefs.GetVerbose() && !m_idRetryTimer)
					DebugLogError(_T("Failed to create 'server connect retry' timer - %s"), GetErrorMessage(GetLastError()));
			}
			break;
		}
		case CS_DISCONNECTED:{
			theApp.sharedfiles->ClearED2KPublishInfo();
			connected = false;
			if (connectedsocket) {
				connectedsocket->Close();
				connectedsocket = NULL;
			}
			theApp.emuledlg->searchwnd->CancelEd2kSearch();
			theStats.serverConnectTime = 0;
			theStats.Add2TotalServerDuration();
			if (thePrefs.Reconnect() && !connecting)
				ConnectToAnyServer();		
			if (thePrefs.GetNotifierOnImportantError())
				theApp.emuledlg->ShowNotifier(GetResString(IDS_CONNECTIONLOST), TBN_IMPORTANTEVENT);
			break;
		}
		case CS_ERROR:
		case CS_NOTCONNECTED:{
			if (!connecting)
				break;
		}
		case CS_SERVERDEAD:
		case CS_SERVERFULL:{
			if (!connecting)
				break;
			if (singleconnecting){
				if (pServer != NULL && sender->IsServerCryptEnabledConnection() && !thePrefs.IsClientCryptLayerRequired()){
					// try reconnecting without obfuscation
					ConnectToServer(pServer, false, true);
					break;
				}
				StopConnectionTry();
				break;
			}

			DWORD tmpkey;
			CServerSocket* tmpsock;
			POSITION pos = connectionattemps.GetStartPosition();
			while (pos) {
				connectionattemps.GetNextAssoc(pos, tmpkey, tmpsock);
				if (tmpsock == sender) {
					connectionattemps.RemoveKey(tmpkey);
					break;
				}
			}
			TryAnotherConnectionRequest();
		}
	}
	theApp.emuledlg->ShowConnectionState();
}

VOID CALLBACK CServerConnect::RetryConnectTimer(HWND /*hWnd*/, UINT /*nMsg*/, UINT /*nId*/, DWORD /*dwTime*/) 
{ 
	// NOTE: Always handle all type of MFC exceptions in TimerProcs - otherwise we'll get mem leaks
	try
	{
		CServerConnect *_this = theApp.serverconnect;
		ASSERT( _this );
		if (_this)
		{
			_this->StopConnectionTry();
			if (_this->IsConnected())
				return;
			if (_this->m_uStartAutoConnectPos >= theApp.serverlist->GetServerCount())
				_this->m_uStartAutoConnectPos = 0;
			_this->ConnectToAnyServer(_this->m_uStartAutoConnectPos, true, true);
		}
	}
	CATCH_DFLT_EXCEPTIONS(_T("CServerConnect::RetryConnectTimer"))
}

void CServerConnect::CheckForTimeout()
{ 
	DWORD dwServerConnectTimeout = CONSERVTIMEOUT;
	// If we are using a proxy, increase server connection timeout to default connection timeout
	if (thePrefs.GetProxySettings().UseProxy)
		dwServerConnectTimeout = max(dwServerConnectTimeout, CONNECTION_TIMEOUT);

	DWORD dwCurTick = GetTickCount();
	DWORD tmpkey;
	CServerSocket* tmpsock;
	POSITION pos = connectionattemps.GetStartPosition();
	while (pos){
		connectionattemps.GetNextAssoc(pos,tmpkey,tmpsock);
		if (!tmpsock){
			if (thePrefs.GetVerbose())
				DebugLogError(_T("Error: Socket invalid at timeoutcheck"));
			connectionattemps.RemoveKey(tmpkey);
			return;
		}

		if (dwCurTick - tmpkey > dwServerConnectTimeout){
			LogWarning(GetResString(IDS_ERR_CONTIMEOUT), tmpsock->cur_server->GetListName(), tmpsock->cur_server->GetAddress(), tmpsock->cur_server->GetPort());
			connectionattemps.RemoveKey(tmpkey);
			DestroySocket(tmpsock);
			if (singleconnecting)
				StopConnectionTry();
			else
				TryAnotherConnectionRequest();

		}
	}
}

bool CServerConnect::Disconnect()
{
	if (connected && connectedsocket)
	{
		theApp.sharedfiles->ClearED2KPublishInfo();
		connected = false;
		CServer* pServer = theApp.serverlist->GetServerByAddress(connectedsocket->cur_server->GetAddress(), connectedsocket->cur_server->GetPort());
		if (pServer)
			theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
		theApp.SetPublicIP(0);
		DestroySocket(connectedsocket);
		connectedsocket = NULL;
		theApp.emuledlg->ShowConnectionState();
		theStats.serverConnectTime = 0;
		theStats.Add2TotalServerDuration();
		return true;
	}
	return false;
}

CServerConnect::CServerConnect()
{   
	// 添加STUN相关成员变量的初始化
	m_hStunCheckThread = NULL;
	m_bStunCheckRunning = false;
	m_lastLocalPort = 0;
	m_lastPublicPort = 0;
	connectedsocket = NULL;
	max_simcons = (thePrefs.IsSafeServerConnectEnabled()) ? 1 : 2;
	connecting = false;
	connected = false;
	clientid = 0;
	singleconnecting = false;
	if (thePrefs.GetServerUDPPort() != 0){
	    udpsocket = new CUDPSocket(); // initalize socket for udp packets
		if (!udpsocket->Create()){
			delete udpsocket;
			udpsocket = NULL;
		}
	}
	else
		udpsocket = NULL;
	m_idRetryTimer = 0;
	m_uStartAutoConnectPos = 0;
	InitLocalIP();
}

CServerConnect::~CServerConnect(){
	// stop all connections
	StopConnectionTry();
	// close connected socket, if any
	DestroySocket(connectedsocket);
	connectedsocket = NULL;
	// close udp socket
	if (udpsocket){
	    udpsocket->Close();
	    delete udpsocket;
    }
}

CServer* CServerConnect::GetCurrentServer(){
	if (IsConnected() && connectedsocket)
		return connectedsocket->cur_server;
	return NULL;
}

void CServerConnect::SetClientID(uint32 newid){
	clientid = newid;

	if (!::IsLowID(newid))
		theApp.SetPublicIP(newid);
	
	theApp.emuledlg->ShowConnectionState();
}

void CServerConnect::DestroySocket(CServerSocket* pSck){
	if (pSck == NULL)
		return;
	// remove socket from list of opened sockets
	for( POSITION pos = m_lstOpenSockets.GetHeadPosition(); pos != NULL; )
	{
		POSITION posDel = pos;
		CServerSocket* pTestSck = (CServerSocket*)m_lstOpenSockets.GetNext(pos);
		if (pTestSck == pSck)
		{
			m_lstOpenSockets.RemoveAt(posDel);
			break;
		}
	}
	if (pSck->m_SocketData.hSocket != INVALID_SOCKET){ // deadlake PROXYSUPPORT - changed to AsyncSocketEx
		pSck->AsyncSelect(0);
		pSck->Close();
	}

	delete pSck;
}

bool CServerConnect::IsLocalServer(uint32 dwIP, uint16 nPort){
	if (IsConnected()){
		if (connectedsocket->cur_server->GetIP() == dwIP && connectedsocket->cur_server->GetPort() == nPort)
			return true;
	}
	return false;
}

void CServerConnect::InitLocalIP()
{
	m_nLocalIP = 0;

	// Using 'gethostname/gethostbyname' does not solve the problem when we have more than 
	// one IP address. Using 'gethostname/gethostbyname' even seems to return the last IP 
	// address which we got. e.g. if we already got an IP from our ISP, 
	// 'gethostname/gethostbyname' will returned that (primary) IP, but if we add another
	// IP by opening a VPN connection, 'gethostname' will still return the same hostname, 
	// but 'gethostbyname' will return the 2nd IP.
	// To weaken that problem at least for users which are binding eMule to a certain IP,
	// we use the explicitly specified bind address as our local IP address.
	if (thePrefs.GetBindAddrA() != NULL) {
		unsigned long ulBindAddr = inet_addr(thePrefs.GetBindAddrA());
		if (ulBindAddr != INADDR_ANY && ulBindAddr != INADDR_NONE) {
			m_nLocalIP = ulBindAddr;
			return;
		}
	}

	// Don't use 'gethostbyname(NULL)'. The winsock DLL may be replaced by a DLL from a third party
	// which is not fully compatible to the original winsock DLL. ppl reported crash with SCORSOCK.DLL
	// when using 'gethostbyname(NULL)'.
	__try{
		char szHost[256];
		if (gethostname(szHost, sizeof szHost) == 0){
			hostent* pHostEnt = gethostbyname(szHost);
			if (pHostEnt != NULL && pHostEnt->h_length == 4 && pHostEnt->h_addr_list[0] != NULL)
				m_nLocalIP = *((uint32*)pHostEnt->h_addr_list[0]);
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER){
		// at least two ppl reported crashs when using 'gethostbyname' with third party winsock DLLs
		if (thePrefs.GetVerbose())
			DebugLogError(_T("Unknown exception in CServerConnect::InitLocalIP"));
		ASSERT(0);
	}
}

void CServerConnect::KeepConnectionAlive()
{
	DWORD dwServerKeepAliveTimeout = thePrefs.GetServerKeepAliveTimeout();
	if (dwServerKeepAliveTimeout && connected && connectedsocket && connectedsocket->connectionstate == CS_CONNECTED &&
		GetTickCount() - connectedsocket->GetLastTransmission() >= dwServerKeepAliveTimeout)
	{
		// "Ping" the server if the TCP connection was not used for the specified interval with 
		// an empty publish files packet -> recommended by lugdunummaster himself!
		CSafeMemFile files(4);
		files.WriteUInt32(0); // nr. of files
		Packet* packet = new Packet(&files);
		packet->opcode = OP_OFFERFILES;
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("Refreshing server connection"));
		if (thePrefs.GetDebugServerTCPLevel() > 0)
			Debug(_T(">>> Sending OP__OfferFiles(KeepAlive) to server\n"));
		theStats.AddUpDataOverheadServer(packet->size);
		connectedsocket->SendPacket(packet,true);
	}
}

bool CServerConnect::IsLowID()
{
	return ::IsLowID(clientid);
}

// true if the IP is one of a server which we currently try to connect to
bool CServerConnect::AwaitingTestFromIP(uint32 dwIP) const{
	if (connectionattemps.IsEmpty())
		return false;
	DWORD tmpkey;
	CServerSocket* tmpsock;
	POSITION pos = connectionattemps.GetStartPosition();
	while (pos) {
		connectionattemps.GetNextAssoc(pos, tmpkey, tmpsock);
		if (tmpsock != NULL && tmpsock->cur_server != NULL && tmpsock->cur_server->GetIP() == dwIP && tmpsock->GetConnectionState() == CS_WAITFORLOGIN)
			return true;
	}
	return false;
}

bool CServerConnect::IsConnectedObfuscated() const {
	return connectedsocket != NULL && connectedsocket->IsObfusicating();
}
