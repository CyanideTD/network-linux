#ifndef _QUERY_NET_IO_H_
#define _QUERY_NET_IO_H_

#include "std_header.h"
#include "client_netio.pb.h"
using namespace client_netio_protocol;

class CQueryNetIO : public ITasksGroupCallBack
{
public:
    // 初始化
    TINT32 Init(CConf *pobjConf, CTseLogger *pLog, CTaskQueue *poTaskQueue);

    // 网络IO线程驱动函数
    static TVOID * RoutineNetIO(void *pParam);

    // 网络消息回调
    virtual void OnTasksFinishedCallBack(LTasksGroup *pTasksgrp);

    // 网络消息请求
    virtual void OnUserRequest(LongConnHandle hSession, const TUCHAR *pData, TUINT32 uLen, BOOL &bWillResponse);

public:
    //  创建监听socket
    SOCKET CreateListenSocket(TCHAR *pszListenHost, TUINT16 uwPort);

    // 关闭监听socket
    TINT32 CloseListenSocket();

    // 获取ip和端口
    TINT32 GetIp2PortByHandle(LongConnHandle stHandle, TUINT16 *puwPort, TCHAR **ppszIp);
private:
    // 在接受请求时就发生错误时,向front返回错误
    TINT32 SendHttpBackErr(LongConnHandle stHandle, TUINT32 udwSeqno);

public:
    // 长连接对象
    ILongConn               *m_pLongConn;
    // 日志对象
    CTseLogger                *m_pLog;
    // session队列
    CTaskQueue                *m_poTaskQueue;
    // 配置
    CConf                    *m_poConf;
    // 打包/解包工具
    CBaseProtocolPack       *m_pPackTool;
    CBaseProtocolUnpack     *m_pUnPackTool;
    // 监听socket
    TINT32                  m_hListenSock;
    // 模式
    TBOOL                   m_bHttpOp;
    // 序列号
    TUINT32                    m_udwSeqno;

    TUINT32                 m_udwContentType;

private:
    HRESULT OnClientCommandRequest_Binary(LongConnHandle stHandle, const TUCHAR *pszData, TUINT32 udwDataLen);
    ClientRequest           *m_pobjClientReq;

private:
    HRESULT OnClientCommandRequest_Http(LongConnHandle stHandle, const TUCHAR *pszData, TUINT32 udwDataLen);
};

#endif