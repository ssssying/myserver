#ifndef CLIENT_COMM_ENGINE_H_
#define CLIENT_COMM_ENGINE_H_

class CMessage;
class CTcpHead;

#include "message_interface.h"
#include "../../message/message.pb.h"
#include "../../message/tcpmessage.pb.h"

// 加密类型定义
enum EncryptType
{
	EncryptNone = 0,
	EncryptType1 = 1,
	EncryptType2 = 2,
};

void pbmsg_settcphead(CTcpHead& rHead, int iSrcFE, int iSrcID, int iDstFE, int iDstID, time_t tTimestamp, EGateCmd eCmd = EGC_NULL);

class ClientCommEngine
{
public:
	static unsigned char tKey[16];
	static unsigned char* tpKey;

	// 反序列化CTcpHead, 返回剩余长度
	static int ConvertStreamToMsg(const void* pBuff, unsigned short unBuffLen, unsigned short& rOffset, CTcpHead* pTcpHead);
	// 反序列化CMessage
	static int ConvertStreamToMsg(const void* pBuff, unsigned short unBuffLen, CMessage* pMsg, CFactory* pMsgFactory = NULL, bool bEncrypt = false, const unsigned char* pEncrypt = ClientCommEngine::tpKey);
	// 序列化消息(CMessage为空代表服务器内部消息)
	static int ConvertMsgToStream(void* pBuff, unsigned short& unBuffLen, const CTcpHead* pTcpHead, CMessage* pMsg = NULL, bool bEncrypt = false, const unsigned char* pEncrypt = ClientCommEngine::tpKey);

	// --------------------------------------------------------------------------------
	// Function:	EncryptData
	// Description:	加密数据
	// Input:		nAlgorithm 		- 加密算法类型
	// 				pbyKey 			- 密钥
	// 				pbyIn 			- 输入buff
	// 				nInLength 		- 输入长度
	// 				pbyOut 			- 输出buff（加密数据）
	// 				pnOutLength 	- 输出长度
	// Return：		无
	// Others：		pbyOut为密文格式,pnOutLength为pbyOut的长度,是8byte的倍数,至少应预留nInLength+17
	// --------------------------------------------------------------------------------
	static void EncryptData(short nAlgorithm,
					 const unsigned char* pbyKey,
					 const unsigned char* pbyIn,
					 int nInLength,
					 unsigned char* pbyOut,
					 int* pnOutLength);

	// --------------------------------------------------------------------------------
	// Function:	DecryptData
	// Description:	解密数据
	// Input:		nAlgorithm 		- 加密算法类型
	// 				pbyKey 			- 密钥
	// 				pbyIn 			- 输入buff（加密数据）
	// 				nInLength 		- 输入长度
	// 				pbyOut 			- 输出buff（解密数据）
	// 				pnOutLength 	- 输出长度
	// Return：		1 - 解密成功; 0 - 解密失败
	// --------------------------------------------------------------------------------
	static int DecryptData(short nAlgorithm,
					const unsigned char* pbyKey,
					const unsigned char* pbyIn,
					int nInLength,
					unsigned char* pbyOut,
					int* pnOutLength);
};


#endif /* CLIENT_COMM_ENGINE_H_ */