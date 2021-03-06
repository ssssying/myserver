//
//  net_work.h
//  Created by DGuco on 18/01/20.
//  Copyright © 2018年 DGuco. All rights reserved.
//


#ifndef _NET_WORK_H_
#define _NET_WORK_H_

#include <map>
#include <queue>
#include <unordered_map>
#include <server_tool.h>
#include "network_interface.h"
#include "event_reactor.h"
#include "mythread.h"
#include "listener.h"

using namespace std;
class CNetWork: public CSingleton
{
public:
    typedef unordered_map<unsigned int, shared_ptr<CConnector>> MAP_CONNECTOR;
    typedef unordered_map<unsigned int, shared_ptr<CAcceptor>> MAP_ACCEPTOR;
    typedef std::queue<shared_ptr<CSystemSignal>> Queue_TimerOrSignals;

public:
    //构造函数
    CNetWork();
    //析构函数
    virtual ~CNetWork();

    //开始监听
    bool BeginListen(const char *szNetAddr,
                     unsigned int uPort,
                     FuncAcceptorOnNew pOnNew,
                     FuncBufferEventOnDataSend funcAcceptorOnDataSend,
                     FuncBufferEventOnDataSend funcAcceptorOnDataRecv,
                     FuncBufferEventOnDataSend funcAcceptorDisconnected,
                     FuncOnTimeOut funcAcceptorTimeOut,
                     int listenQueue,
                     unsigned int uCheckPingTickTime);
    //结束监听
    void EndListen();
    //连接
    bool Connect(const char *szNetAddr,
                 uint16 uPort,
                 int targetId,
                 FuncBufferEventOnDataSend funcOnSomeDataSend,
                 FuncBufferEventOnDataSend funcOnSomeDataRecv,
                 FuncBufferEventOnDisconnected funcOnDisconnected,
                 FuncConnectorOnConnectFailed funcOnConnectFailed,
                 FuncConnectorOnConnectted funcOnConnectted,
                 FuncConnectorOnPingServer funcOnPingServer,
                 unsigned int uPingTick);
    //关闭acceptor
    bool ShutDownAcceptor(unsigned int uId);
    //设置信号回调
    void SetCallBackSignal(unsigned int uSignal, FuncOnSignal pFunc, void *pContext, bool bLoop = false);
    //关闭connector
    bool ShutDownConnectorEx(unsigned int uId);
    //启动
    void DispatchEvents();
    //查找connector
    std::shared_ptr<CConnector> FindConnector(unsigned int uId);
    //查找acceptor
    std::shared_ptr<CAcceptor> FindAcceptor(unsigned int uId);
    //添加新的acceptor
    void InsertNewAcceptor(unsigned int uid, std::shared_ptr<CAcceptor> pAcceptor);
    //添加新的connector
    void InsertNewConnector(unsigned int uid, std::shared_ptr<CConnector> pConnector);
    //获取event
    std::shared_ptr<IEventReactor> GetEventReactor();
    //获取连接map
    MAP_ACCEPTOR &GetAcceptorMap();
private:
    //新的连接 accept回调
    static void lcb_OnAccept(IEventReactor *pReactor, SOCKET socket, sockaddr *sa);
private:
    //创建acceptor
    void NewAcceptor(IEventReactor *pReactor, SOCKET socket, sockaddr *sa);
private:
    shared_ptr<IEventReactor> m_pEventReactor;
    unsigned int m_uGcTime;

    MAP_CONNECTOR m_mapConnector;
    MAP_ACCEPTOR m_mapAcceptor;
    Queue_TimerOrSignals m_qTimerOrSignals;
    shared_ptr<CListener> m_pListener;
    shared_ptr<CTimerEvent> m_pCheckTimerOut;
    int m_iPingCheckTime;  //单位毫秒

    FuncAcceptorOnNew m_pOnNew;
    FuncBufferEventOnDataSend m_pFuncAcceptorOnDataSend;
    FuncBufferEventOnDataRecv m_pFuncAcceptorOnDataRecv;
    FuncBufferEventOnDisconnected m_pFuncAcceptorDisconnected;
};

#endif
