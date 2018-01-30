#include "my_macro.h"
#include "net_addr.h"
#include "network_interface.h"
#include "net_work.h"
#include "ppe_signal.h"
#include "connector.h"
#include "acceptor.h"

template<> CNetWork *CSingleton<CNetWork>::spSingleton = NULL;

CNetWork::CNetWork(eNetModule netModule)
	:
	m_pEventReactor(new CEventReactor(netModule)),
	m_uGcTime(0),
	m_uCheckPingTickTime(0),
	m_pListener(NULL),
	m_pOnNew(NULL),
	m_pOnDisconnected(NULL),
	m_pOnSomeDataSend(NULL),
	m_pOnSomeDataRecv(NULL)
{
}

CNetWork::~CNetWork(void)
{
	for (auto it : m_mapConnector) {
		it.second->ShutDown();
		SAFE_DELETE(it.second);
	}
	m_mapConnector.clear();

	for (auto it : m_mapAcceptor) {
		it.second->ShutDown();
		SAFE_DELETE(it.second);
	}
	m_mapAcceptor.clear();
	while (!m_quSystemSignals.empty()) {
		CSystemSignal *pSystemSignal = m_quSystemSignals.front();
		SAFE_DELETE(pSystemSignal);
		m_quSystemSignals.pop();
	}
	OnTick();
	SAFE_DELETE(m_pEventReactor);

}

void CNetWork::SetCallBackSignal(uint32 uSignal, FuncOnSignal pFunc, void *pContext, bool bLoop)
{
	CSystemSignal *pSystemSignal = new CSystemSignal(m_pEventReactor);
	pSystemSignal->SetCallBackSignal(uSignal, pFunc, pContext, bLoop);
	m_quSystemSignals.push(pSystemSignal);
}

void CNetWork::OnAccept(IEventReactor *pReactor, SOCKET Socket, sockaddr *sa)
{
	CNetWork::GetSingletonPtr()->NewAcceptor(pReactor, Socket, sa);
}

void CNetWork::NewAcceptor(IEventReactor *pReactor, SOCKET Socket, sockaddr *sa)
{
	sockaddr_in sin;
	memcpy(&sin, sa, sizeof(sin));
	//  取得ip和端口号
	char ip[100];
	sprintf(ip, inet_ntoa(sin.sin_addr));
	CAcceptor *pAcceptor = new CAcceptor(Socket, pReactor, new CNetAddr(ip, sin.sin_port));
	SOCKET socket = pAcceptor->GetSocket().GetSocket();
	pAcceptor->SetCallbackFunc(m_pOnDisconnected,
							   m_pOnSomeDataSend,
							   m_pOnSomeDataRecv);
	m_mapAcceptor.insert(std::make_pair(socket, pAcceptor));
	m_pOnNew(socket, pAcceptor);
}

bool CNetWork::BeginListen(const char *szNetAddr, uint16 uPort,
						   FuncAcceptorOnNew pOnNew,
						   FuncAcceptorOnDisconnected pOnDisconnected,
						   FuncAcceptorOnSomeDataSend pOnSomeDataSend,
						   FuncAcceptorOnSomeDataRecv pOnSomeDataRecv,
						   uint32 uCheckPingTickTime)
{
	m_pListener = new CListener(m_pEventReactor);
	if (m_pListener == NULL) {
		return false;
	}
	CNetAddr addr(szNetAddr, uPort);

	bool bRes = m_pListener->Listen(addr, &CNetWork::OnAccept);
	m_uCheckPingTickTime = uCheckPingTickTime;
	m_pOnNew = pOnNew;
	m_pOnDisconnected = pOnDisconnected;
	m_pOnSomeDataSend = pOnSomeDataSend;
	m_pOnSomeDataRecv = pOnSomeDataRecv;

	return bRes;
}

void CNetWork::EndListen()
{
	SAFE_DELETE(m_pListener);
}

int CNetWork::Connect(const char *szNetAddr,
					  uint16 uPort,
					  FuncConnectorOnDisconnected pOnDisconnected,
					  FuncConnectorOnConnectFailed pOnConnectFailed,
					  FuncConnectorOnConnectted pOnConnectted,
					  FuncConnectorOnSomeDataSend pOnSomeDataSend,
					  FuncConnectorOnSomeDataRecv pOnSomeDataRecv,
					  FuncConnectorOnPingServer pOnPingServer,
					  uint32 uPingTick /* = 45000 */, uint32 uTimeOut)
{
	CConnector *pConnector = new CConnector(m_pEventReactor);
	pConnector->SetCallbackFunc(std::move(pOnDisconnected),
								std::move(pOnConnectFailed),
								std::move(pOnConnectted),
								std::move(pOnSomeDataSend),
								std::move(pOnSomeDataRecv));

	CNetAddr addr(szNetAddr, uPort);
	timeval time;
	time.tv_sec = uTimeOut;
	time.tv_usec = 0;
	pConnector->Connect(addr, &time);
	int fd = pConnector->GetSocket().GetSocket();
	m_mapConnector.insert(std::make_pair(fd, pConnector));
	return fd;
}

uint32 CNetWork::GetConnectorExPingValue(uint32 uId)
{
	CConnector *pConnector = FindConnector(uId);
	if (pConnector) {
//		return pConnector->GetPingValue();
	}
	else {
		return 9999999;
	}
	return 0;
}

void CNetWork::DispatchEvents()
{
	m_pEventReactor->DispatchEvents();
}

bool CNetWork::ShutDownAcceptor(uint32 uId)
{
	auto iter = m_mapAcceptor.find(uId);
	if (m_mapAcceptor.end() != iter) {
		CAcceptor *pAcceptor = iter->second;
		pAcceptor->ShutDown();
		m_mapAcceptor.erase(iter);
		return true;
	}
	else {
		return false;
	}
}

bool CNetWork::ShutDownConnectorEx(uint32 uId)
{
	auto iter = m_mapConnector.find(uId);
	if (m_mapConnector.end() != iter) {
		CConnector *pConnector = iter->second;
		pConnector->ShutDown();
		m_quIdleConnectorExs.push(pConnector);
		m_mapConnector.erase(iter);
		return true;
	}
	else {
		return false;
	}
}

void CNetWork::OnTick()
{
	if (!m_quIdleConnectorExs.empty()) {
		while (!m_quIdleConnectorExs.empty()) {
			CConnector *pConnector = m_quIdleConnectorExs.front();
			SAFE_DELETE(pConnector)
			m_quIdleConnectorExs.pop();
		}
	}
}

CConnector *CNetWork::FindConnector(unsigned int uId)
{
	auto iter = m_mapConnector.find(uId);
	if (m_mapConnector.end() == iter) {
		return NULL;
	}
	else {
		return iter->second;
	}
}

CAcceptor *CNetWork::FindAcceptor(unsigned int uId)
{
	auto iter = m_mapAcceptor.find(uId);
	if (m_mapAcceptor.end() == iter) {
		return NULL;
	}
	else {
		return iter->second;
	}
}

PipeResult CNetWork::ConnectorSendData(uint32 uId, const void *pData, uint32 uSize)
{
	CConnector *pConnectorEx = FindConnector(uId);
	if (pConnectorEx) {
		PipeResult eRes = pConnectorEx->Send(pData, uSize);
		return eRes;
	}
	return ePR_Disconnected;
}

