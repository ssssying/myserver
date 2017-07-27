//
// Created by DGuco on 17-6-21.
// 客户端连接管理
//

#ifndef SERVER_CLIENT_HANDLE_H
#define SERVER_CLIENT_HANDLE_H

#include "../../framework/net/nethead.h"

// 管道标识符
enum enLockIdx
{
    IDX_PIPELOCK_C2S = 0,
    IDX_PIPELOCK_S2C = 1,
    IDX_PIPELOCK_A2C = 2,
    IDX_PIPELOCK_S2L = 3,
};


// 提取消息错误码
enum ClienthandleErrCode
{
    CLIENTHANDLE_SUCCESS									= 0,			// 序列化消息成功
    CLIENTHANDLE_QUEUE_CRASH							= 1,			// 从管道提取消息出错
    CLIENTHANDLE_QUEUE_EMPTY							= 2,			// 从管道取出长度为空
    CLIENTHANDLE_SMALL_LENGTH						= 3,			// 小于最小长度
    CLIENTHANDLE_TOTAL_LENGTH						= 4,			// tcp转发过来的总长度不匹配
    CLIENTHANDLE_NETHEAD									= 5,			// 序列化CNetHead失败
    CLIENTHANDLE_NETHEAD_LENGTH					= 6,			// CNetHead长度不匹配
    CLIENTHANDLE_NO_ENTITY								= 7,			// CNetHead中没有实体信息
    CLIENTHANDLE_HASCLOSED								= 8,			// 连接池中找不到玩家实体，已经关闭连接
    CLIENTHANDLE_NOTSAMETEAM						= 9,			// 不是同一个玩家
    CLIENTHANDLE_NOTEAM									= 10,		// 找不到玩家实体
    CLIENTHANDLE_CREATETEAMFAILED					= 11,		// 创建玩家实体失败
    CLIENTHANDLE_STATUSINVALID						= 12,		// 找到玩家但状态不对
    CLIENTHANDLE_ONLINEFULL							= 13,		// 服务器在线已满
    CLIENTHANDLE_MSGINVALID							= 14,		// 消息异常
    CLIENTHANDLE_ISNOTNORMAL						= 15,		// 服务器状态异常
    CLIENTHANDLE_LOGINLIMITTIME						= 16,		// 登陆受限制
    CLIENTHANDLE_LOGINCHECK							= 17,		// 登陆去验证
};

class CMessage;
class CCSHead;
class CCodeQueue;
class CSharedMem;
class CPlayer;
class CMessageSet;

class CClientHandle
{
public:
    CClientHandle();
    ~CClientHandle();

    int Initialize();

protected:
    // tcp --> game 共享内存管道起始地址
    CCodeQueue* mC2SPipe;
    // game --> tcp 共享内存管道起始地址
    CCodeQueue* mS2CPipe;
    CNetHead mNetHead;

public:
    int AddMsgToMsgSet(CMessageSet* pMsgSet, CMessage* pMsg);
    int Send(CMessage* message,CPlayer* pPlayer);
    int Send(CMessageSet* pMsgSet, stPointList* pPlayerList);
    int Send(CMessageSet* pMsgSet, long lMsgGuid, int iSocket, time_t tCreateTime, unsigned int uiIP, unsigned short unPort, bool bKickOff = false);
    int Send2Tcp(CMessageSet* pMsgSet, long lMsgGuid);
    int SendLog(BYTE* pCodeBuff, int vLen);
    int Recv();

    int DecodeNetMsg(BYTE* pCodeBuff, int& nLen, CCSHead* pCSHead, CMessage* pMsg);

    // 断开玩家连接
    void DisconnectClient(CPlayer* cPlayer);
    // 断开玩家连接
    void DisconnectClient(int iSocket, time_t tCreateTime, unsigned int uiIP, unsigned short unPort);

    // 打印管道状态
    void Dump(char* pBuffer, unsigned int& uiLen);

private:
    static char macMessageBuff[MAX_PACKAGE_LEN];
};

#endif //SERVER_CLIENT_HANDLE_H
