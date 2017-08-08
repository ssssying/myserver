//
// Created by DGuco on 17-6-21.
//

#include "../inc/clienthandle.h"
#include "../inc/player.h"
#include "../inc/messagedispatcher.h"
#include "../../framework/mem/shm.h"
#include "../../framework/mem/sharemem.h"
#include "../../framework/mem/codequeue.h"
#include "../../framework/base/my_assert.h"
#include "../../framework/base/commondef.h"
#include "../../framework/net/client_comm_engine.h"
#include "../../framework/json/config.h"
#include "../../framework/base/servertool.h"
#include "../../framework/message/message.pb.h"

CClientHandle::CClientHandle()
{
    Initialize();
}

CClientHandle::~CClientHandle()
{
}

int CClientHandle::Initialize()
{
    int iTempSize = sizeof(CSharedMem) + CCodeQueue::CountQueueSize(PIPE_SIZE);

    ////////////////////////////////mS2CPipe/////////////////////////////////////////
    system("touch ./scpipefile");
    char* pcTmpSCPipeID = getenv("SC_PIPE_ID");
    int iTmpSCPipeID = 0;
    if (pcTmpSCPipeID)
    {
        iTmpSCPipeID = atoi(pcTmpSCPipeID);
    }
    key_t iTmpKeyS2C = MakeKey("./scpipefile", iTmpSCPipeID);
    BYTE* pbyTmpS2CPipe = CreateShareMem(iTmpKeyS2C, iTempSize);
    MY_ASSERT(pbyTmpS2CPipe != NULL, exit(0));
    CSharedMem::pbCurrentShm = pbyTmpS2CPipe;
    CCodeQueue::pCurrentShm = CSharedMem::CreateInstance(iTmpKeyS2C, iTempSize);
    //mS2CPipe地址相对pbyTmpS2CPipe的偏移地址是sizeof(CSharedMem) + sizeof(CCodeQueue)
    mS2CPipe = CCodeQueue::CreateInsance(PIPE_SIZE, EnLockIdx::IDX_PIPELOCK_S2C);

    ////////////////////////////////mC2SPipe/////////////////////////////////////////
    system("touch ./cspipefile");
    char* pcTmpCSPipeID = getenv("CS_PIPE_ID");
    int iTmpCSPipeID = 0;
    if (pcTmpCSPipeID)
    {
        iTmpCSPipeID = atoi(pcTmpCSPipeID);
    }
    key_t iTmpKeyC2S = MakeKey("./cspipefile", iTmpCSPipeID);
    BYTE* pbyTmpC2SPipe = CreateShareMem(iTmpKeyC2S, iTempSize);
    MY_ASSERT(pbyTmpC2SPipe != NULL, exit(0));
    CSharedMem::pbCurrentShm = pbyTmpC2SPipe;
    CCodeQueue::pCurrentShm = CSharedMem::CreateInstance(iTmpKeyC2S, iTempSize);
    //mC2SPipe地址相对pbyTmpS2CPipe的偏移地址是sizeof(CSharedMem) + sizeof(CCodeQueue)
    mC2SPipe = CCodeQueue::CreateInsance(PIPE_SIZE, EnLockIdx::IDX_PIPELOCK_C2S);
    return 0;
}

int CClientHandle::AddMsgToMsgSet(CMessageSet* pMsgSet, Message* pMsg)
{
    return ClientCommEngine::AddMsgToMsgSet(pMsgSet, pMsg);
}

int CClientHandle::Send2Tcp(CMessageSet* pMsgSet, long lMsgGuid)
{
    MY_ASSERT((pMsgSet != NULL), return -1);

    BYTE abyTmpCodeBuf[MAX_PACKAGE_LEN] = { 0 };
    unsigned short unTmpCodeLength = sizeof(abyTmpCodeBuf);

    char* pcTmpBuff = (char*) abyTmpCodeBuf;

    CTcpHead tmpCSHead;
    tmpCSHead.set_timestamp(lMsgGuid);
    // 如果需要加密，在这里修改参数
    int iRet = ClientCommEngine::ConvertMsgToStream
           (
                   &mNetHead,
                   &tmpCSHead,
                   pMsgSet,
                   pcTmpBuff,
                   unTmpCodeLength
           );
   if (iRet != 0)
   {
       MY_ASSERT_STR(0, return -2, "CClientHandle::Send failed, ClientCommEngine::ConvertMsgToStream failed.");
   }

   iRet = mS2CPipe->AppendOneCode(abyTmpCodeBuf, unTmpCodeLength);
   if (iRet < 0)
   {
       MY_ASSERT_STR(0, return -3, "CClientHandle::Send failed, AppendOneCode return %d.", iRet);
   }

   LOG_DEBUG("default", "---- Send To Client Succeed ----");
	for (int i = 0; i < unTmpCodeLength; i++)
	{
		LOG_DEBUG("default", "[%d : %d]", i, abyTmpCodeBuf[i]);
	}

    return 0;
}

int CClientHandle::Send(Message* message,CPlayer* pPlayer) {
    MY_ASSERT((message != NULL && pPlayer != NULL), return -1);
    BYTE abyTmpCodeBuf[MAX_PACKAGE_LEN] = { 0 };
    unsigned short unTmpCodeLength = sizeof(abyTmpCodeBuf);

    char* pcTmpBuff = (char*) abyTmpCodeBuf;

    // 是否需要加密，在这里修改参数
   int iRet = ClientCommEngine::ConvertClientMsgToStream(pcTmpBuff,unTmpCodeLength,message,true);
   if (iRet != 0)
   {
       MY_ASSERT_STR(0, return -2, "CClientHandle::Send failed, ClientCommEngine::ConvertClientMsgToStream failed.");
   }

   iRet = mS2CPipe->AppendOneCode(abyTmpCodeBuf, unTmpCodeLength);
   if (iRet < 0)
   {
       MY_ASSERT_STR(0, return -3, "CClientHandle::Send failed, AppendOneCode return %d.", iRet);
   }

   LOG_DEBUG("default", "---- Send To Client Succeed ----");
	for (int i = 0; i < unTmpCodeLength; i++)
	{
		LOG_DEBUG("default", "[%d : %d]", i, abyTmpCodeBuf[i]);
	}

    return 0;
}

int CClientHandle::Send(CMessageSet* pMsgSet, stPointList* pTeamList)
{
    MY_ASSERT((pMsgSet != NULL && pTeamList != NULL), return -1);

    time_t tTmpNow = time(NULL);
    bool bTmpKickoff = false;
    // 判断是否发送消息后断开连接(这个主动断开只针对与第一个玩家)
    mNetHead.Initialize(tTmpNow, (bTmpKickoff == true ? -1 : 0));
    for (int i = 0; i < pTeamList->GetBroadcastNum(); i++)
    {
        // 将列表中的实体信息加入nethead头中
        CPlayer* pTmpTeam = (CPlayer*)pTeamList->GetPointByIdx(i);
        if (pTmpTeam)
        {
            if (pTmpTeam->GetSocketInfoPtr()->iSocket != 0)
            {
                mNetHead.AddEntity(pTmpTeam->GetSocketInfoPtr()->iSocket, pTmpTeam->GetSocketInfoPtr()->tCreateTime);
                LOG_DEBUG("default", "---- Send To Client( %d | %lu | %s ) socket(%d) createtime(%ld) ----",
                          pTmpTeam->GetEntityID(), pTmpTeam->GetPlayerId(), pTmpTeam->GetPlayerName(),
                          pTmpTeam->GetSocketInfoPtr()->iSocket, pTmpTeam->GetSocketInfoPtr()->tCreateTime);
            }
            else
            {
                LOG_DEBUG("default", "Client(%d | %lu | %s) has disconnected.", pTmpTeam->GetEntityID(), pTmpTeam->GetPlayerId(), pTmpTeam->GetPlayerName());
            }
        }
    }

    int iRet = -11;
    CPlayer* pTmpTeam = (CPlayer*)pTeamList->GetPointByIdx(0);
    if (pTmpTeam)
    {
        iRet = Send2Tcp(pMsgSet, pTmpTeam->GetSocketInfoPtr()->lMsgGuid);
    }

    return iRet;
}

int CClientHandle::Send(CMessageSet* pMsgSet, long lMsgGuid, int iSocket, time_t tCreateTime, unsigned int uiIP, unsigned short unPort, bool bKickOff)
{
    MY_ASSERT((pMsgSet != NULL), return -1);

    time_t tTmpNow = time(NULL);
    // 判断是否发送消息后断开连接(这个主动断开只针对与第一个玩家)
    mNetHead.Initialize(tTmpNow, (bKickOff == true ? -1 : 0), uiIP, unPort);
    mNetHead.AddEntity(iSocket, tCreateTime);
    LOG_DEBUG("default", "---- Send To Client socket(%d) createtime(%ld) ----", iSocket, tCreateTime);

    int iRet = Send2Tcp(pMsgSet, lMsgGuid);

    return iRet;
}

int CClientHandle::Recv()
{
    BYTE abyTmpCodeBuf[MAX_PACKAGE_LEN] =
            { 0 };
    int iTmpCodeLength = sizeof(abyTmpCodeBuf);

    // 从共享内存管道提取消息
    int iRet = mC2SPipe->GetHeadCode((BYTE *) abyTmpCodeBuf,
                                     &iTmpCodeLength);

    if (iRet < 0)
    {
        LOG_ERROR("default", "[%s : %d : %s] When GetHeadCode from C2SPipe, error ocurr %d",
                  __MY_FILE__, __LINE__, __FUNCTION__, iRet);
        return CLIENTHANDLE_QUEUE_CRASH;
    }

    if (iTmpCodeLength == 0)
    {
        return CLIENTHANDLE_QUEUE_EMPTY;
    }

    CCSHead tmpCSHead;
    CMessage tmpMessage;

    iRet = DecodeNetMsg(abyTmpCodeBuf, iTmpCodeLength, &tmpCSHead, &tmpMessage);
    if (iRet != 0)
    {
        return iRet;
    }

    CMessageDispatcher::GetSingletonPtr()->ProcessClientMessage(&tmpCSHead, &tmpMessage);

    return CLIENTHANDLE_SUCCESS;
}

int CClientHandle::DecodeNetMsg(BYTE* pCodeBuff, int& nLen, C2SHead* pCSHead, CMessage* pMsg);
{
    //长度小于消息头的长度+数据总长度+字节对齐长度
    if (!pCodeBuff || nLen < int(pCSHead::MinSize() + (sizeof(unsigned short) * 2)))
    {
        return CLIENTHANDLE_SMALL_LENGTH;
    }

    BYTE* pbyTmpBuff = pCodeBuff;
    int iTmpLen = nLen;

    // 序列化总长度
    // protobuf打包或解析时没有自带长度信息或终结符，需要由应用程序自己在发生和接收的时候做正确的切分。
    // 用unsigned short类型来存储len。
    // *(unsigned short*) pbyTmpBuff此处pbyTmpBuff强制转换为unsigned short类型指针，
    // 并且取此指针的数据，此数据为整个buff长度。
    unsigned short unTmpTotalLen = *(unsigned short*) pbyTmpBuff;
    pbyTmpBuff += sizeof(unsigned short);		// 指针指向数据处
    iTmpLen -= sizeof(unsigned short);			// 从长度减少一个len的长度

    // 总长度不匹配
    if (unTmpTotalLen != nLen)
    {
        return CLIENTHANDLE_TOTAL_LENGTH;
    }

	// 字节对齐补充长度（采用8字节对齐）
    unsigned short unTmpAddlLen = *(unsigned short*) pbyTmpBuff;
    pbyTmpBuff += sizeof(unsigned short);
    iTmpLen -= sizeof(unsigned short);	

	// // 补齐的长度一定小于8字节
	// if (tTmpLen >= 8)
	// {
    //     return CLIENTHANDLE_TOTAL_LENGTH;
	// }
    //扔掉字节对齐长度
    iTmpLen -= unTmpAddlLen;

    // 序列化CClientMessage
    // 当前pbyTmpBuff指向CClientMessage数据起始位置，iTmpLen为CClientMessage长度。
    CClientMessage tmpClientMessage;
    tmpClientMessage.Clear();
    int iRet = tmpClientMessage.ParseFromArray(pbyTmpBuff, iTmpLen);
    if (iRet < 0)
    {
        return CLIENTHANDLE_NETHEAD;
    }
    if (iTmpLen != tmpClientMessage.Size())
    {
        return CLIENTHANDLE_NETHEAD_LENGTH;
    }

    C2SHead* tmpHead = tmpClientMessage.mutable_msghead();
    CSocketInfo* tmpSocketInfo = tmpHead->mutable_socketinfos();
    int iTmpSocket = tmpSocketInfo->socketid();
    int tTmpCreateTime = tmpSocketInfo->createtime();
    if (tmpSocketInfo->state() < 0)
    {
        // 客户端主动关闭连接，也有可能是连接错误被关闭
        LOG_INFO("default", "client(%d : %d) commhandle closed by err = %d. ", iTmpSocket, tTmpCreateTime, tmpSocketInfo->state());

        // 从连接容器中取出玩家实体
        CPlayer* pTmpTeam = CCoreModule::GetSingletonPtr()->GetTeamBySocket(iTmpSocket);
        if (NULL == pTmpTeam)
        {
            // 找不到玩家，连接已经关闭了
            LOG_ERROR("default", "[%s : %d : %s] socket(%d : %d) EntityID = %d has closed.",
                      __MY_FILE__, __LINE__, __FUNCTION__, iTmpSocket, tTmpCreateTime);
            return CLIENTHANDLE_HASCLOSED;
        }
        else
        {
            if (pTmpTeam->GetSocketInfoPtr()->iSocket != iTmpSocket
                || pTmpTeam->GetSocketInfoPtr()->tCreateTime != tTmpCreateTime)
            {
                // 当前玩家与该连接信息不匹配,说明该玩家的连接已经失效
                LOG_WARN("default", "[%s : %d : %s] socket(%d : %d) not match, now(%d : %d).",
                         __MY_FILE__, __LINE__, __FUNCTION__, iTmpSocket, tTmpCreateTime, pTmpTeam->GetSocketInfoPtr()->iSocket, pTmpTeam->GetSocketInfoPtr()->tCreateTime);
                // 重置玩家连接信息
                CCoreModule::GetSingletonPtr()->LeaveGame(pTmpTeam, false);
//				CCoreModule::GetSingletonPtr()->EraseSockInfoList(iTmpSocket);
//				pTmpTeam->GetSocketInfoPtr()->Clear();
//				pTmpTeam->SetTeamState(CTeam::ETS_INGAMEDISCONNECT);

                return CLIENTHANDLE_NOTSAMETEAM;
            }
            else
            {
                // 模拟玩家下线
                pMsg->mutable_msghead()->set_messageid(CMsgLeaveGameRequest::MsgID);

                CMsgLeaveGameRequest* pTmpMsgPara = new (macMessageBuff) CMsgLeaveGameRequest;

                pTmpMsgPara->set_leavetype(mNetHead.m_cState);
                pMsg->set_msgpara((unsigned long) pTmpMsgPara);

                pCSHead->set_teamid(pTmpTeam->GetTeamID());
                pCSHead->set_entityid(pTmpTeam->GetEntityID());
                pCSHead->set_timestamp(mNetHead.m_tStamp);

                return CLIENTHANDLE_SUCCESS;
            }
        }
    }

    pMsg->ParseArray()
    // 服务器数据拉完了才能让玩家正常游戏
    if (CGameServer::GetSingletonPtr()->CanProcessingClientMsg() == false)
    {
        // 通知客户端服务器未开启并断开连接
        CGameServer::GetSingletonPtr()->SendMsgSystemErrorResponse(emSystem_noservice, lTmpMsgGuid, iTmpSocket, tTmpCreateTime, mNetHead.m_iSrcIP,mNetHead.m_nSrcPort, true);
        return CLIENTHANDLE_ISNOTNORMAL;
    }
    CTeam* pTmpTeam = NULL;
    // 如果是登陆消息
    if (pMsg  && pMsg->mutable_msghead()->messageid() == CMsgLoginGameRequest::MsgID)
    {
        // 5500踢掉断线玩家 检测是否需要踢掉断连玩家
        if( CCoreModule::GetSingletonPtr()->CheckOnlineIsFull()  <  0)
        {
            LOG_INFO("default", "[%s : %d : %s]  login failed, team list is full.",__YQ_FILE__, __LINE__, __FUNCTION__);

            // 通知客户端服务器已满并断开连接
            CGameServer::GetSingletonPtr()->SendMsgSystemErrorResponse(emSystem_isfull,lTmpMsgGuid,iTmpSocket, tTmpCreateTime,
                                                                       mNetHead.m_iSrcIP, mNetHead.m_nSrcPort,true);
            return CLIENTHANDLE_ONLINEFULL;
        }
        pTmpTeam = CCoreModule::GetSingletonPtr()->GetTeamBySocket(iTmpSocket);
        if( pTmpTeam)
        {
            // 同一个fd
            if (tTmpCreateTime == pTmpTeam->GetSocketInfoPtr()->tCreateTime)
            {
                // 连续多次发TryLogin消息则直接把消息抛掉
                LOG_DEBUG("default", "[%s : %d : %s]  Socket(%d) is useed, don't TryLogin again.",
                          __YQ_FILE__, __LINE__, __FUNCTION__, iTmpSocket);
                // 通知客户端解析消息失败并断开连接
                CGameServer::GetSingletonPtr()->SendMsgSystemErrorResponse(emSystem_msgerr, lTmpMsgGuid, iTmpSocket,
                                                                           tTmpCreateTime, mNetHead.m_iSrcIP,mNetHead.m_nSrcPort, true);
                return CLIENTHANDLE_MSGINVALID;
            }
            else
            {
                // 假如是同一个fd,时间戳不一样, 可能socket断掉未检测到
                LOG_INFO("default", "[%s : %d : %s]  Socket(%d) is used, old team(TeamID=%lu, EntityID=%d), reset.",
                         __YQ_FILE__, __LINE__, __FUNCTION__, iTmpSocket, pTmpTeam->GetTeamID(), pTmpTeam->GetEntityID());

                // 先把老玩家断连
                CCoreModule::GetSingletonPtr()->LeaveGame(pTmpTeam, false);
            }
        }

        // 解析登陆消息
        CMsgLoginGameRequest* pTmpMsg = (CMsgLoginGameRequest*) pMsg->msgpara();
        if (pTmpMsg == NULL)
        {
            LOG_ERROR("default", "[%s : %d : %s]  team(Account=%s, ServerID=%d) login failed, msg is NULL.",
                      __YQ_FILE__, __LINE__, __FUNCTION__);

            // 通知客户端解析消息失败并断开连接
            CGameServer::GetSingletonPtr()->SendMsgSystemErrorResponse(emSystem_msginvalid, lTmpMsgGuid, iTmpSocket,
                                                                       tTmpCreateTime, mNetHead.m_iSrcIP,mNetHead.m_nSrcPort, true);
            return CLIENTHANDLE_MSGINVALID;
        }
        if (strcmp(pTmpMsg->account().c_str(), "") == 0 || strcmp(pTmpMsg->password().c_str(), "") == 0)
        {
            // 通知客户端解析消息失败并断开连接
            CGameServer::GetSingletonPtr()->SendMsgSystemErrorResponse(emSystem_msginvalid, lTmpMsgGuid, iTmpSocket,
                                                                       tTmpCreateTime, mNetHead.m_iSrcIP,mNetHead.m_nSrcPort, true);
            return CLIENTHANDLE_MSGINVALID;
        }

        // 登陆时候判断session是否已验证
        string sSession = CCoreModule::GetSingletonPtr()->GetTeamIDBySession(pTmpMsg->account().c_str(), pTmpMsg->serverid(), pTmpMsg->pfrom());
        LOG_INFO("default", "[%s : %d : %s]  uid: %s, new session: %s, old session: %s",__YQ_FILE__, __LINE__, __FUNCTION__,pTmpMsg->account().c_str(),pTmpMsg->password().c_str(), sSession.c_str());
        // 是否验证sdk登陆
        bool bSdkCheck = CConfigMgr::GetSingletonPtr()->GetConfig()->gameconfig().sdkcheck();
        if( bSdkCheck && strcmp(pTmpMsg->password().c_str(), sSession.c_str()) != 0)
        {
            CWebModule::GetSingletonPtr()->XiaoMiLoginRequest(pMsg, iTmpSocket, tTmpCreateTime, lTmpMsgGuid, mNetHead.m_iSrcIP, mNetHead.m_nSrcPort, mNetHead.m_tStamp);
            return CLIENTHANDLE_LOGINCHECK;
        }
        else
        {
            CCoreModule::GetSingletonPtr()->SDKLoginSucces(pTmpMsg->account().c_str(),pTmpMsg->password().c_str(),pTmpMsg->serverid(),pTmpMsg->pfrom(),iTmpSocket,tTmpCreateTime,lTmpMsgGuid,mNetHead.m_iSrcIP,mNetHead.m_nSrcPort,mNetHead.m_tStamp);
            return CLIENTHANDLE_LOGINCHECK;
        }
    }
    else
    {
        // 消息链接玩家
        pTmpTeam = CCoreModule::GetSingletonPtr()->GetTeamBySocket(iTmpSocket);
        // 判断 玩家跟消息合法性
        if(pTmpTeam  ==  NULL  || pMsg  ==  NULL  || tTmpCreateTime != pTmpTeam->GetSocketInfoPtr()->tCreateTime || pCSHead->teamid() != pTmpTeam->GetTeamID()
           ||  pTmpTeam->GetTeamDebug() != 0)
        {
            LOG_DEBUG("default", "teamid[%lu]msgid[%u] return -1008", pCSHead->teamid(), pMsg->mutable_msghead()->messageid());
            // 通知客户端解析消息失败并断开连接
            CGameServer::GetSingletonPtr()->SendMsgSystemErrorResponse(emSystem_msgerr, lTmpMsgGuid, iTmpSocket,
                                                                       tTmpCreateTime, mNetHead.m_iSrcIP,mNetHead.m_nSrcPort, true);

            if( pTmpTeam )
            {
                LOG_DEBUG("default", "team leavegame 1008");
                // 断开链接
                CCoreModule::GetSingletonPtr()->LeaveGame(pTmpTeam, false);
                // 测试
                pTmpTeam->SetTeamDebug(0);
            }

            return CLIENTHANDLE_MSGINVALID;
        }

        if( pTmpTeam->IsLoginLimitTime() )
        {
            // 通知客户端解析消息失败并断开连接
            CGameServer::GetSingletonPtr()->SendMsgSystemErrorResponse(emSystem_loginlimit, lTmpMsgGuid, iTmpSocket,
                                                                       tTmpCreateTime, mNetHead.m_iSrcIP,mNetHead.m_nSrcPort, true);
        }

        pTmpTeam->SetTeamState(CTeam::ETS_INGAMECONNECT);
    }


    pTmpTeam->GetSocketInfoPtr()->tLastActiveTime = mNetHead.m_tStamp;
    pCSHead->set_entityid( pTmpTeam->GetEntityID() );
	if (pTmpTeam->GetSocketInfoPtr()->lMsgGuid == lTmpMsgGuid)
	{
		// 如果消息的GUID相等,说明这是客户端重发的消息,服务器已经处理过了,直接抛弃
		LOG_INFO("default", "[%s : %d : %s] message guid(%ld) is same, ignore it.",
				__YQ_FILE__, __LINE__, __FUNCTION__, lTmpMsgGuid);
		return CLIENTHANDLE_SUCCESS;
	}
    pTmpTeam->GetSocketInfoPtr()->lMsgGuid = lTmpMsgGuid;

    //////////////////////////////////////////////////////////////////////////////////////////////////

    return CLIENTHANDLE_SUCCESS;
}

// 断开玩家连接
void CClientHandle::DisconnectClient(CPlayer* cPlayer)
{
    if (!cPlayer)
    {
        return;
    }

    DisconnectClient(
            cPlayer->GetSocketInfoPtr()->iSocket,
            cPlayer->GetSocketInfoPtr()->tCreateTime,
            cPlayer->GetSocketInfoPtr()->uiIP,
            cPlayer->GetSocketInfoPtr()->unPort);
}

// 断开玩家连接
void CClientHandle::DisconnectClient(int iSocket, time_t tCreateTime,
                                     unsigned int uiIP, unsigned short unPort)
{
    BYTE abyTmpCodeBuff[MAX_PACKAGE_LEN];
    unsigned char* pucTmpBuff = (unsigned char*) abyTmpCodeBuff;
    unsigned short unTmpLen = 0;

    // 这里必须用临时变量，因为有可能是在接收到消息的时候发起断连
    CNetHead tmpNetHead;
    tmpNetHead.Initialize(time(NULL), -1, uiIP, unPort);
    tmpNetHead.AddEntity(iSocket, tCreateTime);

    // 总长度
    *(unsigned short*) pucTmpBuff =
            ((sizeof(unsigned short) * 2) + tmpNetHead.Size());
    pucTmpBuff += sizeof(unsigned short);
    unTmpLen += sizeof(unsigned short);

    // CNetHead长度
    *(unsigned short*) pucTmpBuff = tmpNetHead.Size();
    pucTmpBuff += sizeof(unsigned short);
    unTmpLen += sizeof(unsigned short);

    // 序列化CNetHead
    unsigned short unRet = tmpNetHead.SerializeToArray(pucTmpBuff,
                                                       MAX_PACKAGE_LEN - unTmpLen);
    if (unRet < 0)
    {
        LOG_ERROR("default", "[%s : %d : %s] SerializeToArray failed, tRet = %d.",
                  __MY_FILE__, __LINE__, __FUNCTION__, unRet);
        return;
    }
    if (unRet != tmpNetHead.Size())
    {
        LOG_ERROR("default", "[%s : %d : %s] length is not same (%d : %d).",
                  __MY_FILE__, __LINE__, __FUNCTION__, unRet, tmpNetHead.Size());
        return;
    }
    pucTmpBuff += unRet;
    unTmpLen += unRet;

    unRet = mS2CPipe->AppendOneCode(abyTmpCodeBuff, unTmpLen);
    if (unRet < 0)
    {
        LOG_ERROR("default", "[%s : %d : %s] AppendOneCode failed, tRet = %d.",
                  __MY_FILE__, __LINE__, __FUNCTION__, unRet);
        return;
    }

    char acTmpAddress[ADDR_LENGTH] =
            { 0 };
    char* pcTmpAddress = &acTmpAddress[0];
    if (SockAddrToString(uiIP, unPort, pcTmpAddress) == 0)
    {
        LOG_NOTICE("default", "Disconnect Client [%s : Socket = %d : CreateTime = %d].", pcTmpAddress, iSocket, tCreateTime);
    }
}

// 打印管道状态
void CClientHandle::Dump(char* pBuffer, unsigned int& uiLen)
{
    unsigned int uiTmpMaxLen = uiLen;
    uiLen = 0;

    int iTmpBegin = 0;
    int iTmpEnd = 0;
    int iTmpLeft = 0;

    uiLen += snprintf(pBuffer + uiLen, uiTmpMaxLen - uiLen,
                      "------------------------------CClientHandle------------------------------");
    uiLen += snprintf(pBuffer + uiLen, uiTmpMaxLen - uiLen, "\n%30s\t%10s\t%10s",
                      "name", "free", "total");

    mC2SPipe->GetCriticalData(iTmpBegin, iTmpEnd, iTmpLeft);
    uiLen += snprintf(pBuffer + uiLen, uiTmpMaxLen - uiLen, "\n%30s\t%10d\t%10d",
                      "c2s pipe", iTmpLeft, PIPE_SIZE);

    mS2CPipe->GetCriticalData(iTmpBegin, iTmpEnd, iTmpLeft);
    uiLen += snprintf(pBuffer + uiLen, uiTmpMaxLen - uiLen, "\n%30s\t%10d\t%10d",
                      "s2c pipe", iTmpLeft, PIPE_SIZE);
}