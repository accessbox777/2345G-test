// dllmain.cpp : 定义 DLL 应用程序的入口点。

#include <afx.h>
#include "definition.h"
#include "stdio.h"
#include "sshmain.h"
#include <thread>
#include "writelog.h"
#include "pthread.h"
#include "command_File_definition.h"
#include "loadCommandFile.h"
#include "inst.h"
#include "inst_defines.h"
#include "libssh2_config.h"
#include <libssh2.h>
#include <libssh2_sftp.h>
int  initsession(int id, int iThread);
int writeshell(CStringA cmdCstr, int ithread, int id);

int readshell(char* receiveBuf, bool waiteol, int size, int ithread, int id, int timeoutMs);
#define SKIPDUT 0
extern int SSHiniflag[MAX_DUT_NUM];
int extern sock[MAX_DUT_NUM];

extern LIBSSH2_SESSION* session[MAX_DUT_NUM];
extern int waitsocket(int socket_fd, LIBSSH2_SESSION* session);
extern LIBSSH2_CHANNEL* channel[MAX_DUT_NUM];
char cmdxmlFilePath[2048] = { 0 };
char dlllogPath[2048] = { 0 };
EN_DUT_CONNECT_TYPE dutConnType = EN_SSH;
int isTXRX2connection[MAX_DUT_NUM] = { 0 };
int QuitReadLoop[MAX_DUT_NUM] = { 0 };
std::thread* tread[MAX_DUT_NUM] = { 0 };
extern pthread_mutex_t pthreadMutex[MAX_DUT_NUM];
extern ST_COMMAND_SETTINGS cmdSettings;
int executeCommand(ST_CMD_SETS cmdsets, int iThread, CStringA& ret, int sendCouont);
void parseCommand(ST_CMD_SETS* cmdsets, char* params);
std::vector<CStringA> splitfunction(CStringA str, const char findchar);
CStringA findelement(CStringA* str, CStringA params);
CStringA ReplaceElement(CStringA element, CStringA params);
CStringA replaceelement(CStringA element, CStringA params);
CStringA findValue(CStringA params, CStringA name);
double smbvLoss[MAXDUTNUM] = { 0 };
handle instHandle[MAXDUTNUM] = { 0 };
handle smbvHandle[MAXDUTNUM] = { 0 };
CStringA TxResStr[MAXDUTNUM];
CStringA RxResStr[MAXDUTNUM];
CStringA GNSSRES[MAXDUTNUM];

//BOOL APIENTRY DllMain( HMODULE hModule,
//                       DWORD  ul_reason_for_call,
//                       LPVOID lpReserved
//                     )
//{
//    switch (ul_reason_for_call)
//    {
//    case DLL_PROCESS_ATTACH:
//    case DLL_THREAD_ATTACH:
//    case DLL_THREAD_DETACH:
//    case DLL_PROCESS_DETACH:
//        break;
//    }
//    return TRUE;
//}
int sleepFunc()
{
	Sleep(90000);
	return 0;
}
int str2doublearray(char* sourse, char decollator, double* target, int* count)
//----------------------------------------------------------//
//                 base funtion for data process
//----------------------------------------------------------//
{// string to array(double)
	int len;
	int i = 0, j = 0, list = 0;
	char temp[128];
	len = strlen(sourse);
	if (NULL == sourse)
		return 1;
	else if (NULL == decollator)
		return 1;
	else
	{
		for (i = 0; i < len + 1; i++)
		{
			if ((sourse[i] == decollator) || (sourse[i] == '\0'))
			{
				temp[j] = '\0';
				if (!strcmp(temp, "NCAP"))
				{
					target[list++] = 999.9;
					j = 0;
				}
				else if (!strcmp(temp, "INV"))
				{
					target[list++] = -999.9;
					j = 0;
				}
				else if (!strcmp(temp, "NAV"))
				{
					target[list++] = -999.9;
					j = 0;
				}
				else
				{
					target[list++] = atof(temp);
					j = 0;
				}
			}
			else
			{
				temp[j] = sourse[i];
				j++;
			}
		}
	}
	*count = list;
	return 0;
}
int __stdcall initializeSMBV(char* InstAddress, int iThread, double loss)
{
	char logstr[1024] = "initialize smbv invoked";
	writelogmain(logstr, iThread);

	if (0 != ApiOpen(InstAddress, &(smbvHandle[iThread])))
	{
		writelogmain("connect Instrument Error", iThread);
		return -1;
	}
	smbvLoss[iThread] = loss;

	return 0;
}
int  __stdcall initializeInstrument(char* InstAddress, int iThread,double freq[],double loss[],int count)
{
	//"TCPIP0::127.0.0.1::inst0::INSTR"
	char logstr[1024] = "initialize instrument invoked";
	writelogmain(logstr, iThread);

	if (0 != ApiOpen(InstAddress, &(instHandle[iThread])))
	{
		writelogmain("connect Instrument Error",iThread);
		return -1;
	}
	instHandle[iThread].CMWMode = API_CMW1xx;
	ApiBaseOptionCheck(&(instHandle[iThread]), NULL, 0);
	instHandle[iThread].CMWMode = API_CMW1xx;
	ApiPathLossSet(instHandle[iThread], (ApiRfPortIndex)(iThread+1), API_RF_PORT_DIRECTION_BOTH, freq, loss, count);
	return 0;
}

//
int __stdcall initializeDut(char* ipaddress, int comportifNeed, char* userName, char* password, int TXRX2connection, int iThread)
{
	char rdbuff[256] = { 0 };
	setLogFilePath(dlllogPath, iThread);
	char logStr[256] = { 0 };
	QuitReadLoop[iThread * 2] = 0;
	isTXRX2connection[iThread] = TXRX2connection;
	//if (iniflag != 0)
	//{
	//    return 0;
	//}
	SSHiniflag[iThread] = 1;
	if (SKIPDUT == 1)
	{
		SSHiniflag[iThread] = 1;
		writelogmain("不控制 DUT SSH", iThread);
		return 0;
	}
	////创立连接发送
	if (dutConnType == EN_SSH)
	{
		sprintf_s(logStr,"IPaddress：%s", ipaddress);
		writelogmain(logStr, iThread);
		sprintf_s(logStr, 256, "BuildDate:%s %s", __DATE__, __TIME__);
		writelogmain(logStr, iThread);
		char port[8] = "22";

		if (TXRX2connection == 1)
		{
			char cmd[128] = "cat /dev/smd8&\r";
			int id = iThread * 2;

			writelogmain("begintest", iThread);
		/*	if (0 != opensocketconnection(ipaddress, port, userName, password, id, iThread))
			{
				sprintf_s(logStr, "Thread: %d ，open socket connection Fail!", iThread);
				writelogmain(logStr, iThread);
				SSHiniflag[iThread] = -1;
				return -1;
			}*/
		//	Sleep(150);
		
		//	tread[id] = new std::thread(recive, &id);
		//	Sleep(1000);
			if (0 != opensocketconnection(ipaddress, port, userName, password, id + 1, iThread))
			{
				sprintf_s(logStr, "Thread: %d ，open socket connection Fail!", iThread);
				writelogmain(logStr, iThread);
				SSHiniflag[iThread] = -1;
				return -1;
			}
			if (0 != initsession(id + 1, iThread))
			{
				sprintf_s(logStr, "initsession error", cmd);
				writelogmain(logStr, iThread);
				SSHiniflag[iThread] = -1;
				return -1;
			}
			char cmd2[255] = "killall cat\r";
			writeshell(cmd2, iThread, id + 1);
			
			readshell(rdbuff, false, sizeof(rdbuff),  iThread, id + 1, 400);
			//char cmd[255] = "cat /dev/smd8&";
			writeshell(cmd, iThread, id + 1);
			/*{
				sprintf_s(logStr, "cmd：%s send error ,return -1", cmd);
				writelogmain(logStr, iThread);
				SSHiniflag[iThread] = -1;
				return -1;
			}*/
			
			readshell(rdbuff, false, sizeof(rdbuff),  iThread, id + 1, 400);
			//char command_array[11][256] = {
			//    "cat /dev/smd8",
			//    //"echo -e \"AT + GTTESTMODE=\\\"TM\\\"\\r\\n\">/dev/smd8",    
			//    //"echo -e \"AT+GTTESTMODE=\\\"TM\\\"\\r\\n\" > /dev/smd8",
			//    "echo -e \"AT+GTTESTMODE=\\\"LTEBAND 1\\\"\\r\\n\" > /dev/smd8",
			//    "echo -e \"AT+GTTESTMODE=\\\"CH\\\",\\\"2140000\\\",\\\"2140000\\\",\\\"1950000\\\"\\r\\n\" > /dev/smd8",
			//    "echo -e \"AT+GTTESTMODE=\\\"RFMDEVICE\\\",\\\"18\\\",\\\"18\\\",\\\"6\\\",\\\"7\\\"\\r\\n\" > /dev/smd8",
			//    "echo -e \"AT+GTTESTMODE=\\\"SIGNAL\\\",\\\"16\\\",\\\"16\\\",\\\"4\\\",\\\"5\\\"\\r\\n\" > /dev/smd8",
			//    //"echo -e \"AT+GTTESTMODE=\\\"TCHENTRY\\\"\\r\\n\" > /dev/smd8",
			//    //"echo -e \"AT+GTTESTMODE=\\\"TXCONFIG\\\"\\r\\n\" > /dev/smd8"                
			//  //  "echo -e \"AT+GTTESTMODE=\\\"GAIN 50\\\"\\r\\n\" > /dev/smd8"
			//};
			//for (int i = 1; i < 5; i++)
			//{

			//    for (int retry = 0; retry < 3; retry++)
			//    {

			//        sendshell(command_array[i], id + 1,iThread);

			//        char readbackbuf[128] = { 0 };
			//        char containstr[8] = "OK";
			//        if (readBack(containstr, readbackbuf, sizeof(readbackbuf), 500, id) == 0)
			//        {
			//            sprintf_s(logStr, "readback success string: %s", readbackbuf);
			//            writelogmain(logStr,iThread);
			//            break;
			//        }
			//        else
			//        {
			//            sprintf_s(logStr, "readback timeOut retry~");
			//            writelogmain(logStr,iThread);
			//        }
			//    }
			//}
		}
	}

	//pthread_t recvThread;	//定义接收消息的线程  
	//int ret = pthread_create(&recvThread, NULL, recive, NULL);
	//Pcommand();
	//pthread_join(recvThread, NULL);
	////销毁互斥锁
	//pthread_mutex_destroy(&pthreadMutex[iThread]);
	////关闭连接
	//shutdownALL();
	SSHiniflag[iThread] = 1;
	return 0;
}
/// <summary>
/// 仅第一次测试时调用一次,不要每次开始都调用
/// </summary>
/// <param name="logPath"></param>
/// <returns></returns>
int __stdcall initializeTest(EN_DUT_CONNECT_TYPE type, char* commandFilePath, char* logPath)
{
	for (int i = 0; i < MAX_DUT_NUM; i++)
	{
		pthreadMutex[i] = PTHREAD_MUTEX_INITIALIZER;
	}
	if (NULL != commandFilePath)
		strcpy_s(cmdxmlFilePath, commandFilePath);
	if (NULL != logPath)
		strcpy_s(dlllogPath, logPath);
	dutConnType = type;
	//MessageBoxA(NULL, "test", "test", 0);
	if (dutConnType == EN_SSH)
	{
		initsshLib();
	}
	if (0 != loadCommands(commandFilePath))
	{

		return -1;
	}
	return 0;
}
int __stdcall deinitializeSMBV(int iThread)
{
	ApiClose(&smbvHandle[iThread]);
	memset(&smbvHandle[iThread], 0, sizeof(handle));
	return 0;
}
int __stdcall EOLCMW100StartGPSSignal(int iThread)
{
	if (SSHiniflag[iThread] == -1)
	{
		return NULL;
	}
	RxResStr[iThread] = "";
	int idx = 0;
	CStringA readback;
	CStringA value;
	CStringA value2;
	CStringA rd2;
	char* rtStr = NULL;
	unsigned long splength = 255;
	ST_TX_RESULT result = { 0 };
	ApiRfPortIndex outputPortIndexes[8] = { API_RF_PORT_RFIO_01,
	API_RF_PORT_RFIO_02,
	API_RF_PORT_RFIO_03,
	API_RF_PORT_RFIO_04,
	API_RF_PORT_RFIO_05,
	API_RF_PORT_RFIO_06,
	API_RF_PORT_RFIO_07,
	API_RF_PORT_RFIO_08 };
	unsigned int outputPortOnOff[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };// 1 means enable, 0 means disable, this sample will enable RF1-4 for generator
	outputPortOnOff[iThread < 8 ? iThread : iThread - 8] = 1;
	ApiRfPortSet(instHandle[iThread],
		API_CONFIG_TECH_LTEFDD,
		API_RF_PORT_OUTPUT, // indicat for RX test
		ApiRfPortIndex(1),// no use for RX RF port Set here,just put something in
		outputPortIndexes,
		outputPortOnOff,
		iThread < 8 ? 8 : 16);

	//if (rfType == EN_V2x||rfType==EN_LTE)
	//{
	ST_CMD_SETS command;
	int exeRes;
	
	ApiWrite(instHandle[iThread], "SOURce:GPRF:GEN:BBMode ARB");
	ApiWrite(instHandle[iThread],"SOURce:GPRF:GEN:ARB:FILE 'D:\\Rohde-Schwarz\\CMW\\Data\\waveform\\library3\\GPS_15S_SV_1.wv'");
	ApiWrite(instHandle[iThread], "SOURce:GPRF:GEN:ARB:REPetition CONT");
	ApiGprfSourceSingleModeConfig(instHandle[iThread], 1575.42, -100);
	
	ApiGprfSourceOpen(instHandle[iThread], 0);
}
int __stdcall EOLSMBVStartGPSSignal(int iThread)
{
	char* retPtr = NULL;
	char rdbackBuff[10240] = { 0 };
	unsigned long length = sizeof(rdbackBuff);
	GNSSRES[iThread] = "";
	ApiWrite(smbvHandle[iThread], "*IDN?");
	ApiRead(smbvHandle[iThread], rdbackBuff, &length);
	memset(rdbackBuff, 0, sizeof(rdbackBuff));
	double dllevel = -115.0 + smbvLoss[iThread];
	char cmdbuf[255] = { 0 };
	ApiWrite(smbvHandle[iThread], "*RST");
	ApiWrite(smbvHandle[iThread], "*OPC?");
	length = sizeof(rdbackBuff);
	ApiRead(smbvHandle[iThread], rdbackBuff, &length);
	ApiWrite(smbvHandle[iThread], ":OUTP:STAT 0");
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:STAT 0");
	ApiWrite(smbvHandle[iThread], "*WAI");
	ApiWrite(smbvHandle[iThread], "*OPC?");
	length = sizeof(rdbackBuff);
	ApiRead(smbvHandle[iThread], rdbackBuff, &length);
	ApiWrite(smbvHandle[iThread], "SOUR:BB:GNSS:SYST:GAL:STAT 1");
	ApiWrite(smbvHandle[iThread], "SOUR:BB:GNSS:SYST:GLON:STAT 1");
	ApiWrite(smbvHandle[iThread], "SOUR:BB:GNSS:SYST:BEID:STAT 1");
	ApiWrite(smbvHandle[iThread], "SOUR:BB:GNSS:SYST:GPS:STAT 1");
	ApiWrite(smbvHandle[iThread], "*WAI");
	ApiWrite(smbvHandle[iThread], "*OPC?");
	length = sizeof(rdbackBuff);
	ApiRead(smbvHandle[iThread], rdbackBuff, &length);
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:REC:V1:LOC:SEL \"Beijing\"");
	ApiWrite(smbvHandle[iThread], "SOUR:BB:GNSS:REC:V1:LOC:COOR:FORM DEC");
	sprintf_s(cmdbuf, ":SOUR:BB:GNSS:POW:REF %.2lf", dllevel);
	ApiWrite(smbvHandle[iThread], cmdbuf);
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:TIME:STAR:TBAS UTC");
	ApiWrite(smbvHandle[iThread], "SOUR:BB:GNSS:TIME:STAR:SCT");
	//ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:TIME:STAR:DATE 2014,2,20");
	//ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:TIME:STAR:TIME 16,0,0");
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:SV:SEL:GPS:MIN 1");
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:SV:SEL:GPS:MAX 12");
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:SVID5:GPS:STAT 1");
	ApiWrite(smbvHandle[iThread], "*WAI");
	ApiWrite(smbvHandle[iThread], "*OPC?");
	length = sizeof(rdbackBuff);
	ApiRead(smbvHandle[iThread], rdbackBuff, &length);
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:STAT 1");
	ApiWrite(smbvHandle[iThread], ":OUTP:STAT 1");
	writelogmain("SMBVStartSignal finished", iThread);
	return 0;
}
char* __stdcall EOLdoGPSTest(int iThread)
{	
	char* retPtr = NULL;
	int i = 0;
	int retrycnt = 1;
	char rdbackBuff[16184] = { 0 };
	if (SSHiniflag[iThread] == -1)
	{
		GNSSRES[iThread] = "ERROR";
		retPtr = GNSSRES[iThread].GetBuffer();
		GNSSRES[iThread].ReleaseBuffer();
		return retPtr;
	}
	for (i = 0; i < retrycnt; i++)
	{
		if (cmdSettings.GNSS_RX_INIT_COMMAND.cmdSet.size() > 0)
		{
			for (int i = 0; i < cmdSettings.GNSS_RX_INIT_COMMAND.cmdSet.size(); i++)
			{
				memset(rdbackBuff, 0, sizeof(rdbackBuff));
				sendandReceive(cmdSettings.GNSS_RX_INIT_COMMAND.cmdSet[i].GetBuffer(), iThread * 2 + 1, iThread, rdbackBuff, sizeof(rdbackBuff));
				cmdSettings.GNSS_RX_INIT_COMMAND.cmdSet[i].ReleaseBuffer();
			}
		}
		/////////////
		///////////
		if (cmdSettings.GNSS_RX_START_COMMAND.cmdSet.size() > 0)
		{
			memset(rdbackBuff, 0, sizeof(rdbackBuff));
			sendandReceive(cmdSettings.GNSS_RX_START_COMMAND.cmdSet[0].GetBuffer(), iThread * 2 + 1, iThread, rdbackBuff, sizeof(rdbackBuff));
			cmdSettings.GNSS_RX_START_COMMAND.cmdSet[0].ReleaseBuffer();
		}
		if (cmdSettings.GNSS_RX_STOP_COMMAND.cmdSet.size() > 0)
		{
			Sleep(3000);
			memset(rdbackBuff, 0, sizeof(rdbackBuff));
			sendandReceive(cmdSettings.GNSS_RX_STOP_COMMAND.cmdSet[0].GetBuffer(), iThread * 2 + 1, iThread, rdbackBuff, sizeof(rdbackBuff));
			cmdSettings.GNSS_RX_STOP_COMMAND.cmdSet[0].ReleaseBuffer();
		}

		if (strnlen_s(rdbackBuff, sizeof(rdbackBuff)) > 1)
		{
			writelogmain(rdbackBuff, iThread);
			CStringA findStr = "fix=0 id =1 cnr =";
			//CStringA findStr = "fix=1";
			//CStringA findStr2 = "cnr =";
			CStringA rb(rdbackBuff);
			int idx = rb.Find(findStr);
			if (idx != -1)
			{
				idx=rb.Find(findStr, idx);
				rb = rb.Mid(idx + findStr.GetLength());
				rb = rb.Mid(0, rb.Find("\r"));
				GNSSRES[iThread] = rb;

				break;
			}
			else
			{
				writelogmain("卫星没找着, continue search.....", iThread);
				continue;
			}
			GNSSRES[iThread] = rdbackBuff;
			break;
		}
		else
		{
			writelogmain("无值, continue search.....", iThread);

			continue;
		}
	}
	if (i == retrycnt)
		GNSSRES[iThread] = "NO GPS DATA";
	retPtr = GNSSRES[iThread].GetBuffer();
	GNSSRES[iThread].ReleaseBuffer();
	return retPtr;
}
char* __stdcall doGPSTest(int iThread)
{
	char* retPtr = NULL;
	char rdbackBuff[10240] = { 0 };
	unsigned long length = sizeof(rdbackBuff);
	GNSSRES[iThread] = "";
	if (SSHiniflag[iThread] == -1)
	{
		GNSSRES[iThread] = "ERROR";
		retPtr = GNSSRES[iThread].GetBuffer();
		GNSSRES[iThread].ReleaseBuffer();
		return retPtr;
	}
	ApiWrite(smbvHandle[iThread], "*IDN?");
	ApiRead(smbvHandle[iThread], rdbackBuff, &length);
	memset(rdbackBuff, 0, sizeof(rdbackBuff));
	double dllevel = -115.0 + smbvLoss[iThread];
	char cmdbuf[255] = { 0 };
	//ApiWrite(smbvHandle[iThread], "*RST");
	//ApiWrite(smbvHandle[iThread], "*OPC?");
	length = sizeof(rdbackBuff);
	//ApiRead(smbvHandle[iThread], rdbackBuff, &length);
	ApiWrite(smbvHandle[iThread], ":OUTP:STAT 0");
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:STAT 0");
	ApiWrite(smbvHandle[iThread], "*WAI");
	ApiWrite(smbvHandle[iThread], "*OPC?");
	length = sizeof(rdbackBuff);
	ApiRead(smbvHandle[iThread], rdbackBuff, &length);
	ApiWrite(smbvHandle[iThread], "SOUR:BB:GNSS:SYST:GAL:STAT 0");
	ApiWrite(smbvHandle[iThread], "SOUR:BB:GNSS:SYST:GLON:STAT 0");
	ApiWrite(smbvHandle[iThread], "SOUR:BB:GNSS:SYST:BEID:STAT 0");
	ApiWrite(smbvHandle[iThread], "SOUR:BB:GNSS:SYST:GPS:STAT 1");
	ApiWrite(smbvHandle[iThread], "*WAI");
	ApiWrite(smbvHandle[iThread], "*OPC?");
	length = sizeof(rdbackBuff);
	ApiRead(smbvHandle[iThread], rdbackBuff, &length);
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:REC:V1:LOC:SEL \"User Defined\"");
	ApiWrite(smbvHandle[iThread], "SOUR:BB:GNSS:REC:V1:LOC:COOR:FORM DEC");
	sprintf_s(cmdbuf, ":SOUR:BB:GNSS:POW:REF %.2lf", dllevel);
	ApiWrite(smbvHandle[iThread], cmdbuf);
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:TIME:STAR:TBAS UTC");
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:TIME:STAR:DATE 2014,2,20");
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:TIME:STAR:TIME 16,0,0");
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:SV:SEL:GPS:MIN 1");
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:SV:SEL:GPS:MAX 12");
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:SVID5:GPS:STAT 1");
	ApiWrite(smbvHandle[iThread], "*WAI");
	ApiWrite(smbvHandle[iThread], "*OPC?");
	length = sizeof(rdbackBuff);
	ApiRead(smbvHandle[iThread], rdbackBuff, &length);
	ApiWrite(smbvHandle[iThread], ":SOUR:BB:GNSS:STAT 1");
	ApiWrite(smbvHandle[iThread], ":OUTP:STAT 1");
	writelogmain("set SMBV finished",iThread);
	CStringA readback="";
	if (SKIPDUT == 1)
		return NULL;
//	executeCommand(cmdSettings.GNSS_RX_INIT_COMMAND, iThread, readback);
	//memset(rdbackBuff, 0, sizeof(rdbackBuff));
	//sendandReceive(CStringA("export -p").GetBuffer(), iThread * 2 + 1, iThread, rdbackBuff, 1024);
	//memset(rdbackBuff, 0, sizeof(rdbackBuff));
	//sendandReceive(CStringA("export LD_LIBRARY_PATH='aaabbb';export -p").GetBuffer(), iThread * 2 + 1, iThread, rdbackBuff, 1024);
	//memset(rdbackBuff, 0, sizeof(rdbackBuff));
	
	
	/*sendandReceive(CStringA("rm /tmp/nmea.txt").GetBuffer(), iThread * 2 + 1, iThread, rdbackBuff, 1024);
	memset(rdbackBuff, 0, sizeof(rdbackBuff));
	sendandReceive(CStringA("export LD_LIBRARY_PATH='/oemapp/lib/:/umdp/lib';/tmp/gnss_test 4 0 0 1000").GetBuffer(), iThread * 2 + 1, iThread, rdbackBuff, 1024);
	memset(rdbackBuff, 0, sizeof(rdbackBuff));
		memset(rdbackBuff, 0, sizeof(rdbackBuff));
	sendandReceive(CStringA("cat /tmp/nmea.txt").GetBuffer(), iThread * 2 + 1, iThread, rdbackBuff, 10240);*/

	int i = 0;
	int retrycnt = 45;
	for (i = 0; i < retrycnt; i++)
	{
		if (cmdSettings.GNSS_RX_INIT_COMMAND.cmdSet.size() > 0)
		{
			for (int i = 0; i < cmdSettings.GNSS_RX_INIT_COMMAND.cmdSet.size(); i++)
			{
				memset(rdbackBuff, 0, sizeof(rdbackBuff));
				sendandReceive(cmdSettings.GNSS_RX_INIT_COMMAND.cmdSet[i].GetBuffer(), iThread * 2 + 1, iThread, rdbackBuff, sizeof(rdbackBuff));
				cmdSettings.GNSS_RX_INIT_COMMAND.cmdSet[i].ReleaseBuffer();
			}
		}
		/////////////
		///////////
		if (cmdSettings.GNSS_RX_START_COMMAND.cmdSet.size() > 0)
		{
			memset(rdbackBuff, 0, sizeof(rdbackBuff));
			sendandReceive(cmdSettings.GNSS_RX_START_COMMAND.cmdSet[0].GetBuffer(), iThread * 2 + 1, iThread, rdbackBuff, sizeof(rdbackBuff));
			cmdSettings.GNSS_RX_START_COMMAND.cmdSet[0].ReleaseBuffer();
		}
		if (cmdSettings.GNSS_RX_STOP_COMMAND.cmdSet.size() > 0)
		{
			Sleep(7000);
			memset(rdbackBuff, 0, sizeof(rdbackBuff));
			sendandReceive(cmdSettings.GNSS_RX_STOP_COMMAND.cmdSet[0].GetBuffer(), iThread * 2 + 1, iThread, rdbackBuff, sizeof(rdbackBuff));
			cmdSettings.GNSS_RX_STOP_COMMAND.cmdSet[0].ReleaseBuffer();
		}

		if (strnlen_s(rdbackBuff, sizeof(rdbackBuff)) > 0)
		{
			writelogmain(rdbackBuff, iThread);
			CStringA findStr = "fix=1 id =5 cnr =";
			CStringA rb(rdbackBuff);
			int idx = rb.Find(findStr);
			if (idx != -1)
			{
				rb=rb.Mid(idx + findStr.GetLength());
				rb = rb.Mid(0,rb.Find("\r"));
				GNSSRES[iThread] = rb;
				
				break;
			}
			else
			{
				writelogmain("卫星没找着, continue search.....", iThread);
				continue;
			}
		}
		else
		{
			writelogmain("无值, continue search.....", iThread);
		
			continue;
		}
	}
	if (i == retrycnt)
		GNSSRES[iThread] = "NO GPS DATA";
	retPtr = GNSSRES[iThread].GetBuffer();
	GNSSRES[iThread].ReleaseBuffer();
	return retPtr;
}
int writeshell(CStringA cmdCstr,int ithread, int id)
{

	int rc = 0;
	
	while ((rc = libssh2_channel_write_ex(channel[id], 0, cmdCstr, cmdCstr.GetLength())) == LIBSSH2_ERROR_EAGAIN)
	{
		waitsocket(sock[id], session[id]);
	}
	
	/*while ((rc = libssh2_channel_send_eof(channel[id])) == LIBSSH2_ERROR_EAGAIN)
	{
		waitsocket(sock[id], session[id]);
	}*/
	Sleep(2000);
	return rc;
}
int readshell(char * receiveBuf,bool waiteol,int size, int ithread, int id,int timeoutMs)
{
	int rc = 0;
	int bytecount = 0;
	clock_t start = clock();
	char buffer[16184] = { 0 };
	for (;; )
	{		
		/* loop until we block */
		int rc;
		do
		{
			memset(buffer, 0, sizeof(buffer));
			rc = libssh2_channel_read(channel[id], buffer, sizeof(buffer));
			if (rc > 0)
			{
				int i;
				bytecount += rc;
				if (bytecount > 15000)
					break;
			//	fprintf(stderr, "We read:\n");
				for (i = 0; i < rc; ++i)
					fputc(buffer[i], stderr);
			//	fprintf(stderr, "\n");
				strcat_s(receiveBuf, size, buffer);
			}
			else 
			{
				//if (rc != LIBSSH2_ERROR_EAGAIN)
					/* no need to output this for the EAGAIN case */
				//	fprintf(stderr, "libssh2_channel_read returned %d\n", rc);
			}
			clock_t stop = clock();
			if ((stop - start) > timeoutMs)
			{			
				writelogmain("读超时", ithread);
				return -1;
			}
		} while (rc > 0);
		/* this is due to blocking that would occur otherwise so we loop on
		this condition */
		if (rc == LIBSSH2_ERROR_EAGAIN)
		{
			if (waiteol)
			{
				waitsocket(sock[id], session[id]);
			}
			else 
				break;
		}
		else
			break;
	}
	if (bytecount > 0)
	{
		
		writelogmain(receiveBuf, ithread);		
	}
	return bytecount;
}

int __stdcall deinitializeEolV2xTest(int iThread)
{
	return 0;
}
int closesession(int iThread)
{
	int rc = 0;
	int id = iThread * 2 + 1;
	if (SSHiniflag[iThread] == -1)
	{
		return -1;
	}
	while ((rc = libssh2_channel_close(channel[id])) == LIBSSH2_ERROR_EAGAIN)
		waitsocket(sock[id], session[id]);
	channel[id] = NULL;
	return 0;
}
int  initsession(int id,int iThread)
{
	int rc;
	if (channel[id] == NULL)
	{
		//MessageBoxA(NULL, "before open session", "test3", 0);
		while ((channel[id] = libssh2_channel_open_session(session[id])) == NULL && libssh2_session_last_error(session[id], NULL, NULL, 0) == LIBSSH2_ERROR_EAGAIN)
		{
			waitsocket(sock[id], session[id]);
		}
		//MessageBoxA(NULL, "after open session", "test3", 0);
	}
	if (channel[id] == NULL)
	{
		printf_s("Error\n");
		return -1;
	}
	while (rc = libssh2_channel_request_pty(channel[id], "ansi"))
	{
		waitsocket(sock[id], session[id]);
	}
	while ((rc = libssh2_channel_shell(channel[id]))
		== LIBSSH2_ERROR_EAGAIN)
	{
		waitsocket(sock[id], session[id]);
	}
	return 0;
}
int __stdcall initializeEolV2xTest(int iThread)
{	
	char cmd[1024] = { 0 };
	char receiveBuf[10240] = { 0 };
	LIBSSH2_CHANNEL * channelID;
	int i = 0;
	int rc = 0;
	int id = iThread * 2 + 1;
	if (SSHiniflag[iThread] == -1)
	{		
		return -1;
	}
	//if (channel[id] == NULL)
	//{
	//	//MessageBoxA(NULL, "before open session", "test3", 0);
	//	while ((channel[id] = libssh2_channel_open_session(session[id])) == NULL && libssh2_session_last_error(session[id], NULL, NULL, 0) == LIBSSH2_ERROR_EAGAIN)
	//	{
	//		waitsocket(sock[id], session[id]);
	//	}
	//	//MessageBoxA(NULL, "after open session", "test3", 0);
	//}
	//if (channel[id] == NULL)
	//{
	//	printf_s("Error\n");
	//	return -1;
	//}
	//while (rc = libssh2_channel_request_pty(channel[id], "ansi"))
	//{
	//	waitsocket(sock[id], session[id]);
	//}
	//while ((rc = libssh2_channel_shell(channel[id]))
	//	== LIBSSH2_ERROR_EAGAIN)
	//{
	//	waitsocket(sock[id], session[id]);
	//}

	CStringA cmdCstr = "ssh root@192.168.225.40\r";
	writeshell(cmdCstr, iThread, id);
	Sleep(500);
	memset(receiveBuf, 0, sizeof(receiveBuf));
	readshell(receiveBuf,false, sizeof(receiveBuf), iThread, id, 2000);
	cmdCstr = "yes\r";
	writeshell(cmdCstr, iThread, id);
	Sleep(500);
	memset(receiveBuf, 0, sizeof(receiveBuf));
	readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);
	cmdCstr = "oelinux123\r";
	writeshell(cmdCstr, iThread, id);
	Sleep(500);
	/*while ((rc = libssh2_channel_send_eof(channel[id])) == LIBSSH2_ERROR_EAGAIN)
	{
		waitsocket(sock[id], session[id]);
	}*/
	//cmdCstr = "killall acme\r";
	//writeshell(cmdCstr, iThread, id);
	//memset(receiveBuf, 0, sizeof(receiveBuf));
	//readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);

	//
	//cmdCstr = "rm /tmp/111.txt\r";
	//writeshell(cmdCstr, iThread, id);
	//memset(receiveBuf, 0, sizeof(receiveBuf));
	//readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);

	//cmdCstr = "acme -R -k 20 > /tmp/111.txt &\r";
	//writeshell(cmdCstr, iThread, id);
	//memset(receiveBuf, 0, sizeof(receiveBuf));
	//readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);

	//cmdCstr = "killall acme\r";
	//writeshell(cmdCstr, iThread, id);
	//memset(receiveBuf, 0, sizeof(receiveBuf));
	//readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);

	//cmdCstr = "cat /tmp/111.txt\r";
	//writeshell(cmdCstr, iThread, id);
	//memset(receiveBuf, 0, sizeof(receiveBuf));
	//readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);



	//while ((rc = libssh2_channel_close(channel[id])) == LIBSSH2_ERROR_EAGAIN)
	//	waitsocket(sock[id], session[id]);
	//channel[id] = NULL;

	//cmdCstr = "killall acme";
	//memset(receiveBuf, 0, sizeof(receiveBuf));
	//rc = sendandReceive(cmdCstr.GetBuffer(), id, iThread, receiveBuf, sizeof(receiveBuf));
	//cmdCstr.ReleaseBuffer();

	//cmdCstr = "rm /tmp/111.txt";
	//memset(receiveBuf, 0, sizeof(receiveBuf));
	//rc = sendandReceive(cmdCstr.GetBuffer(), id, iThread, receiveBuf, sizeof(receiveBuf));
	//cmdCstr.ReleaseBuffer();

	//cmdCstr = "acme -R -k 20 > /tmp/111.txt &";
	//memset(receiveBuf, 0, sizeof(receiveBuf));
	//rc = sendandReceive(cmdCstr.GetBuffer(), id, iThread, receiveBuf, sizeof(receiveBuf));
	//cmdCstr.ReleaseBuffer();

 //   cmdCstr = "killall acme";
	//memset(receiveBuf, 0, sizeof(receiveBuf));
	//rc=sendandReceive(cmdCstr.GetBuffer(), id, iThread, receiveBuf, sizeof(receiveBuf));
	//cmdCstr.ReleaseBuffer();

	//cmdCstr = "cat /tmp/111.txt";
	//memset(receiveBuf, 0, sizeof(receiveBuf));
	//rc=sendandReceive(cmdCstr.GetBuffer(), id, iThread, receiveBuf, sizeof(receiveBuf));
	//cmdCstr.ReleaseBuffer();










	//startEolV2xRxTest(0, iThread);

	
	

	




	return 0;
}

char* __stdcall startEolV2xTxTest( char* params, int iThread)
{
	char receiveBuf[10240] = { 0 };
	int id = iThread * 2 + 1;

	if (SSHiniflag[iThread] == -1)
	{
	
		return NULL;
	}
	CString cmdCstr = "killall acme\r";
	memset(receiveBuf, 0, sizeof(receiveBuf));
	writeshell(cmdCstr, iThread, id);
	memset(receiveBuf, 0, sizeof(receiveBuf));
	int rc = readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);
	cmdCstr.ReleaseBuffer();
	if (rc == -1)
		return NULL;
	cmdCstr = "rm /tmp/111.txt\r";
	memset(receiveBuf, 0, sizeof(receiveBuf));
	writeshell(cmdCstr, iThread, id);
	memset(receiveBuf, 0, sizeof(receiveBuf));
	rc = readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);
	if (rc == -1)
		return NULL;
	cmdCstr.ReleaseBuffer();



	cmdCstr = "acme -k 20 > /tmp/111.txt &\r";
	memset(receiveBuf, 0, sizeof(receiveBuf));
	writeshell(cmdCstr, iThread, id);
	memset(receiveBuf, 0, sizeof(receiveBuf));
	rc = readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);
	if (rc == -1)
		return NULL;
	cmdCstr.ReleaseBuffer();

	return NULL;
}
char* __stdcall stopEolV2xTxTest( char* params, int iThread)
{
	int id = iThread * 2 + 1;
	char receiveBuf[10240] = { 0 };
	int rc = 0;
	if (SSHiniflag[iThread] == -1)
	{
		
		return NULL;
	}
	CString cmdCstr = "killall acme\r";
	memset(receiveBuf, 0, sizeof(receiveBuf));

	writeshell(cmdCstr, iThread, id);
	memset(receiveBuf, 0, sizeof(receiveBuf));
	rc = readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);
	if (rc == -1)
		return NULL;
	cmdCstr.ReleaseBuffer();
	cmdCstr = "cat /tmp/111.txt\r";
	memset(receiveBuf, 0, sizeof(receiveBuf));
	writeshell(cmdCstr, iThread, id);
	memset(receiveBuf, 0, sizeof(receiveBuf));
	rc = readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);
	if (rc == -1)
		return NULL;
	cmdCstr.ReleaseBuffer();
	TxResStr[iThread] = receiveBuf;
	char* pchar = TxResStr[iThread].GetBuffer();
	TxResStr[iThread].ReleaseBuffer();
	return pchar;
	
}
char* __stdcall startEolV2xRxTest( char* params, int iThread)
{	
	char receiveBuf[10240] = { 0 };
	int rc = 0;
	int id = iThread * 2 + 1;

	CString cmdCstr = "killall acme\r";
	if (SSHiniflag[iThread] == -1)
	{
		return NULL;
	}
	writeshell(cmdCstr, iThread, id);
	memset(receiveBuf, 0, sizeof(receiveBuf));
	rc=readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);
	if (rc==-1)
		return NULL;
	cmdCstr = "rm /tmp/111.txt\r";
	memset(receiveBuf, 0, sizeof(receiveBuf));
	writeshell(cmdCstr, iThread, id);
	memset(receiveBuf, 0, sizeof(receiveBuf));
	rc = readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);
	cmdCstr.ReleaseBuffer();
	if (rc == -1)
		return NULL;
	cmdCstr = "acme -R -k 20 > /tmp/111.txt &\r";
	memset(receiveBuf, 0, sizeof(receiveBuf));
	writeshell(cmdCstr, iThread, id);
	memset(receiveBuf, 0, sizeof(receiveBuf));
	rc = readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);
	cmdCstr.ReleaseBuffer();
	if (rc == -1)
		return NULL;

	return NULL;
}
char* __stdcall stopEolV2xRxTest(char* params, int iThread)
{
	int id = iThread * 2 + 1;
	int rc = 0;

	char receiveBuf[10240] = { 0 };
	CString cmdCstr = "killall acme\r";
	memset(receiveBuf, 0, sizeof(receiveBuf));
	if (SSHiniflag[iThread] == -1)
	{
		return NULL;
	}
	writeshell(cmdCstr, iThread, id);
	memset(receiveBuf, 0, sizeof(receiveBuf));
	rc = readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);
	cmdCstr.ReleaseBuffer();
	if (rc == -1)
		return NULL;
	cmdCstr = "cat /tmp/111.txt\r";
	memset(receiveBuf, 0, sizeof(receiveBuf));
	writeshell(cmdCstr, iThread, id);
	memset(receiveBuf, 0, sizeof(receiveBuf));
	rc = readshell(receiveBuf, false, sizeof(receiveBuf), iThread, id, 2000);
	cmdCstr.ReleaseBuffer();
	if (rc == -1)
		return NULL;
	RxResStr[iThread] = receiveBuf;
	char* pchar = RxResStr[iThread].GetBuffer();
	RxResStr[iThread].ReleaseBuffer();
	return pchar;
}
char* __stdcall doRfTxPowTest(EN_RF_TYPE rfType, char* params, int iThread)
{
	if (SSHiniflag[iThread] == -1)
	{

		return NULL;
	}
	TxResStr[iThread] = "";
	//MessageBoxA(NULL, "test11", "test11",0);
	unsigned long splength = 255;
	ST_TX_RESULT result = { 0 };
	ApiRfPortIndex outputPortIndexes[8] = { API_RF_PORT_RFIO_01,
	API_RF_PORT_RFIO_02,
	API_RF_PORT_RFIO_03,
	API_RF_PORT_RFIO_04,
	API_RF_PORT_RFIO_05,
	API_RF_PORT_RFIO_06,
	API_RF_PORT_RFIO_07,
	API_RF_PORT_RFIO_08 };

	unsigned int outputPortOnOff[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };// 1 means enable, 0 means disable, this sample will enable RF1-4 for generator
	outputPortOnOff[iThread < 8 ? iThread : iThread - 8] = 1;
	CStringA readback;
	if (SSHiniflag[iThread] == -1)
	{

		return NULL;
	}
	ST_CMD_SETS command;
	if (SKIPDUT != 1)
	{
		
		if (rfType == EN_LTE)
			command = cmdSettings.LTE_TX_INIT_COMMAND;
		else if (rfType == EN_V2x)
			command = cmdSettings.V2x_TX_INIT_COMMAND;
		else if (rfType == EN_V2x2)
			command = cmdSettings.V2x_TX2_INIT_COMMAND;
		else if (rfType == EN_WCDMA)
			command = cmdSettings.WCDMA_TX_INIT_COMMAND;
		else if (rfType == EN_GSM)
			command = cmdSettings.GSM_TX_INIT_COMMAND;
		else if (rfType == EN_NR5G)
			command = cmdSettings.NR_TX_INIT_COMMAND;
		parseCommand(&command, params);
		if (0 != executeCommand(command, iThread, readback, rfType == EN_WCDMA ? 1 : 2))
		{
			TxResStr[iThread] = "ERROR";
			char* pStr = (TxResStr[iThread]).GetBuffer();
			(TxResStr[iThread]).ReleaseBuffer();
			return pStr;
		}
	}

		// set the input rf port Sample for TX measurement
		ApiRfPortIndex onOFFPortIdx[1];// no use for TX,just need parameter
		ApiRfPortSet(instHandle[iThread],
			API_CONFIG_TECH_GPRF_POWER,// set the TX measurement RF port for GPRF, if you want to test other signal like GSM,WIFI, please use other value
			API_RF_PORT_INPUT,// indicat for TX test
			(ApiRfPortIndex)(iThread + 1),// set which RF port should use for TX test
			onOFFPortIdx,//no use for TX test,just put a parameter
			outputPortOnOff,//no use for TX test,just put a parameter
			1);// set 1
		bool isfdd = false;
		char scpiBuff[255] = { 0 };
		CStringA value;
		int band = 0;
		if (rfType == EN_LTE || rfType == EN_V2x||rfType==EN_V2x2)
		{
			if (rfType == EN_LTE)
				value = findValue(CStringA(params), "LTEBand");
			else if (rfType == EN_V2x||rfType== EN_V2x2)
				value = findValue(CStringA(params), "V2XBand");
			band = atoi(value);

			isfdd = band < 33 ? true : false;
		}
		else if (rfType == EN_WCDMA || rfType == EN_NR5G)
		{
			isfdd = true;
		}
		else if (rfType == EN_GSM)
		{
			isfdd = false;
		}
		CStringA value2 = "0";
		if (rfType == EN_LTE || rfType == EN_V2x || rfType == EN_V2x2)
			value2 = findValue(CStringA(params), "TX0_CH");
		else if (rfType == EN_GSM)
			value2 = findValue(CStringA(params), "ChannelUl");
		else if (rfType == EN_NR5G)
			value2 = findValue(CStringA(params), "NR_TX0_CH");
		else if (rfType == EN_WCDMA)
			value2 = findValue(CStringA(params), "WCDMA_TX0_CH");


		sprintf_s(scpiBuff, "CONFigure:GPRF:MEAS:POWer:FILTer:TYPE BANDpass");
		ApiWrite(instHandle[iThread], scpiBuff);

		sprintf_s(scpiBuff, "CONFigure:GPRF:MEAS:POWer:FILTer:BANDpass:BWIDth 20 MHz");
		ApiWrite(instHandle[iThread], scpiBuff);
		if(rfType==EN_GSM)
			sprintf_s(scpiBuff, "CONFigure:GPRF:MEAS:RFSettings:ENPower 43");
		else
			sprintf_s(scpiBuff, "CONFigure:GPRF:MEAS:RFSettings:ENPower 30");
		ApiWrite(instHandle[iThread], scpiBuff);

		sprintf_s(scpiBuff, "CONFigure:GPRF:MEAS:RFSettings:FREQuency %se6",value2.GetBuffer());	
		ApiWrite(instHandle[iThread], scpiBuff);
		value2.ReleaseBuffer();
		if(isfdd)
			sprintf_s(scpiBuff, "TRIGger:GPRF:MEAS:POWer:SOURce 'Free Run'");
		else
			sprintf_s(scpiBuff, "TRIGger:GPRF:MEAS:POWer:SOURce 'IF Power'");
		ApiWrite(instHandle[iThread], scpiBuff);

		ApiMeasurementOpen(instHandle[iThread], API_CONFIG_TECH_GPRF_POWER);
		ApiGprfMeasurementPowerSingleModeResultsGet(instHandle[iThread], &(result.power));
		if (SKIPDUT != 1)
		{
			if (rfType == EN_LTE)
				command = cmdSettings.LTE_TX_STOP_COMMAND;
			else if (rfType == EN_V2x)
				command = cmdSettings.V2x_TX_STOP_COMMAND;
			else if (rfType == EN_V2x2)
				command = cmdSettings.V2x_TX2_STOP_COMMAND;
			else if (rfType == EN_GSM)
				command = cmdSettings.GSM_TX_STOP_COMMAND;
			else if (rfType == EN_WCDMA)
				command = cmdSettings.WCDMA_TX_STOP_COMMAND;
			else if (rfType == EN_NR5G)
				command = cmdSettings.NR_TX_STOP_COMMAND;
			parseCommand(&command, params);
			if (0 != executeCommand(command, iThread, readback, rfType == EN_WCDMA ? 1 : 2))
			{
				TxResStr[iThread] = "ERROR";
				char* pStr = (TxResStr[iThread]).GetBuffer();
				(TxResStr[iThread]).ReleaseBuffer();
				return pStr;
			}
		}
	TxResStr[iThread].Format("power=%.3lf",
		result.power);
	char* pStr = (TxResStr[iThread]).GetBuffer();
	(TxResStr[iThread]).ReleaseBuffer();
	return pStr;
}
char* __stdcall doRfTxTest(EN_RF_TYPE rfType ,char* params, int iThread)
{
	if (SSHiniflag[iThread] == -1)
	{

		return NULL;
	}
	TxResStr[iThread] = "";
	//MessageBoxA(NULL, "test11", "test11",0);
	unsigned long splength = 255;
	ST_TX_RESULT result = { 0 };
	ApiRfPortIndex outputPortIndexes[8] = { API_RF_PORT_RFIO_01,
API_RF_PORT_RFIO_02,
API_RF_PORT_RFIO_03,
API_RF_PORT_RFIO_04,
API_RF_PORT_RFIO_05,
API_RF_PORT_RFIO_06,
API_RF_PORT_RFIO_07,
API_RF_PORT_RFIO_08 };

	unsigned int outputPortOnOff[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };// 1 means enable, 0 means disable, this sample will enable RF1-4 for generator
	outputPortOnOff[iThread < 8 ? iThread : iThread - 8] = 1;
	CStringA readback;
	ST_CMD_SETS command;
	
	if (rfType == EN_LTE || rfType == EN_V2x || rfType == EN_V2x2)
	{
		if (SKIPDUT != 1)
		{
			if (rfType == EN_LTE)
				command = cmdSettings.LTE_TX_INIT_COMMAND;
			else if (rfType == EN_V2x)
				command = cmdSettings.V2x_TX_INIT_COMMAND;
			else if (rfType == EN_V2x2)
				command = cmdSettings.V2x_TX2_INIT_COMMAND;
			parseCommand(&command, params);
			if (0 != executeCommand(command, iThread, readback, rfType == EN_WCDMA ? 1 : 2))
			{
				TxResStr[iThread] = "ERROR";
				char* pStr = (TxResStr[iThread]).GetBuffer();
				(TxResStr[iThread]).ReleaseBuffer();
				return pStr;
			}
		}

		Sleep(2000);
		ApiRfPortSet(instHandle[iThread],
			API_CONFIG_TECH_LTEFDD,
			API_RF_PORT_OUTPUT, // indicat for RX test
			ApiRfPortIndex(1),// no use for RX RF port Set here,just put something in
			outputPortIndexes,
			outputPortOnOff,
			iThread < 8 ? 8 : 16);

		// set the input rf port Sample for TX measurement
		ApiRfPortIndex onOFFPortIdx[1];// no use for TX,just need parameter
		ApiRfPortSet(instHandle[iThread],
			API_CONFIG_TECH_LTEFDD,// set the TX measurement RF port for GPRF, if you want to test other signal like GSM,WIFI, please use other value
			API_RF_PORT_INPUT,// indicat for TX test
			(ApiRfPortIndex)(iThread + 1),// set which RF port should use for TX test
			onOFFPortIdx,//no use for TX test,just put a parameter
			outputPortOnOff,//no use for TX test,just put a parameter
			1);// set 1

		char scpiBuff[255] = { 0 };
		CStringA value;
		if (rfType == EN_LTE)
			value = findValue(CStringA(params), "LTEBand");
		else if (rfType == EN_V2x)
			value = findValue(CStringA(params), "V2XBand");
		int band = atoi(value);
		bool isfdd = band < 33 ? true : false;
		CStringA value2 = findValue(CStringA(params), "TX0_CH");
		sprintf_s(scpiBuff, "CONFigure:LTE:MEAS:BAND OB%s", value);
		ApiWrite(instHandle[iThread], scpiBuff);
		sprintf_s(scpiBuff, "CONFigure:LTE:MEAS:RFSettings:FREQuency %se6", value2);
		ApiWrite(instHandle[iThread], scpiBuff);
		sprintf_s(scpiBuff, "CONFigure:LTE:MEAS:RFSettings:ENPower %s", "30");
		ApiWrite(instHandle[iThread], scpiBuff);
		//
		ApiWrite(instHandle[iThread], "CONFigure:LTE:MEAS:MEValuation:SCOunt:MODulation 1");
		ApiWrite(instHandle[iThread], "CONFigure:LTE:MEAS:MEValuation:SCOunt:POW 1");
		ApiWrite(instHandle[iThread], "CONFigure:LTE:MEAS:MEValuation:SCOunt:SPEC:ACLR 1");
		ApiWrite(instHandle[iThread], "CONFigure:LTE:MEAS:MEValuation:SCOunt:SPEC:SEM 1");
		ApiWrite(instHandle[iThread], "CONFigure:LTE:MEAS:MEValuation:RESult ON,ON,ON,OFF,OFF,OFF,OFF,ON,ON,ON,OFF,OFF,OFF,OFF");
		ApiWrite(instHandle[iThread], "*OPC?");
		splength = 255;
		ApiRead(instHandle[iThread], scpiBuff, &splength);
		ApiWrite(instHandle[iThread], "CONFigure:LTE:MEAS:MEValuation:REPetition SING");
		if (isfdd)
		{
			sprintf_s(scpiBuff, "CONFigure:LTE:MEAS:DMODe FDD");
			ApiWrite(instHandle[iThread], scpiBuff);
			ApiWrite(instHandle[iThread], "TRIGger:LTE:MEAS:MEValuation:SOURce 'Free Run (No Sync)'");
		}
		else
		{
			sprintf_s(scpiBuff, "CONFigure:LTE:MEAS:DMODe TDD");
			ApiWrite(instHandle[iThread], scpiBuff);
			ApiWrite(instHandle[iThread], "TRIGger:LTE:MEAS:MEValuation:SOURce 'IF Power'");
		}
		ApiMeasurementOpen(instHandle[iThread], API_CONFIG_TECH_LTEFDD);
		splength = 255;
		ApiWrite(instHandle[iThread], "*OPC?");
		ApiRead(instHandle[iThread], scpiBuff, &splength);
		ApiWrite(instHandle[iThread], "FETCh:LTE:MEAS:MEValuation:MODulation:AVERage?");
		splength = 255;
		ApiRead(instHandle[iThread], scpiBuff, &splength);
		double target_double[64] = { 0 };
		int len = 0;
		str2doublearray(scpiBuff, ',', target_double, &len);

		result.power = target_double[17];
		result.freqErr = target_double[15];
		result.EVM = target_double[2] > target_double[3] ? target_double[2] : target_double[3];
		result.phaseErr = target_double[10] > target_double[11] ? target_double[10] : target_double[11];
		ApiWrite(instHandle[iThread], "FETC:LTE:MEAS:MEV:ACLR:AVER?");
		splength = 255;
		ApiRead(instHandle[iThread], scpiBuff, &splength);
		len = 0;
		str2doublearray(scpiBuff, ',', target_double, &len);
		result.ACLR1 = target_double[1];
		result.ACLR2 = target_double[2];
		result.ACLR3 = target_double[3];
		result.ACLR4 = target_double[5];
		result.ACLR5 = target_double[6];
		result.ACLR6 = target_double[7];
		ApiWrite(instHandle[iThread], "CALC:LTE:MEAS:MEV:ACLR:AVER?");
		splength = 255;
		ApiRead(instHandle[iThread], scpiBuff, &splength);
		if ((NULL != strstr(scpiBuff, "ULEU"))
			|| (NULL != strstr(scpiBuff, "ULEL")))
		{
			result.ACLR = 1;
		}
		else
		{
			result.ACLR = 0;
		}

		ApiWrite(instHandle[iThread], "FETC:LTE:MEAS:MEV:SEM:MARG?");
		splength = 255;
		ApiRead(instHandle[iThread], scpiBuff, &splength);
		len = 0;
		str2doublearray(scpiBuff, ',', target_double, &len);
		if (0 < target_double[1])
		{
			result.SEM = 1;
		}
		else
		{
			result.SEM = 0;
		}
		result.semMargin1 = target_double[22];
		result.semMargin2 = target_double[23];
		result.semMargin3 = target_double[24];
		result.semMargin4 = target_double[25];
		result.semMargin5 = target_double[32];
		result.semMargin6 = target_double[33];
		result.semMargin7 = target_double[34];
		result.semMargin8 = target_double[35];

		ApiWrite(instHandle[iThread], "FETC:LTE:MEAS:MEV:SEM:AVER?");
		splength = 255;
		ApiRead(instHandle[iThread], scpiBuff, &splength);
		len = 0;
		str2doublearray(scpiBuff, ',', target_double, &len);
		result.OBW = target_double[2];
		if (rfType == EN_LTE)
			command = cmdSettings.LTE_TX_STOP_COMMAND;
		else if (rfType == EN_V2x)
			command = cmdSettings.V2x_TX_STOP_COMMAND;
		else if (rfType == EN_V2x2)
			command = cmdSettings.V2x_TX2_STOP_COMMAND;
		parseCommand(&command, params);
		if (0 != executeCommand(command, iThread, readback, rfType == EN_WCDMA ? 1 : 2))
		{
			TxResStr[iThread] = "ERROR";
			char* pStr = (TxResStr[iThread]).GetBuffer();
			(TxResStr[iThread]).ReleaseBuffer();
			return pStr;
		}
	}
	else if (rfType == EN_GSM)
	{
		//MessageBoxA(NULL, "test2", "test2", 0);
		ST_CMD_SETS command = cmdSettings.GSM_TX_INIT_COMMAND;
		if (SKIPDUT != 1)
		{
			parseCommand(&command, params);
			//MessageBoxA(NULL, "parsecommand", "parsecommand", 0);
			if (0 != executeCommand(command, iThread, readback, rfType == EN_WCDMA ? 1 : 2))
			{
				TxResStr[iThread] = "ERROR";
				char* pStr = (TxResStr[iThread]).GetBuffer();
				(TxResStr[iThread]).ReleaseBuffer();
				return pStr;
			}
		}
		//MessageBoxA(NULL, "test3", "test3", 0);
		ApiWrite(instHandle[iThread], "*IDN?");
		unsigned long length = 256;
		char buff[256] = { 0 };
		ApiRead(instHandle[iThread], buff,&length);
			ApiRfPortSet(instHandle[iThread],
			API_CONFIG_TECH_GSM,
			API_RF_PORT_OUTPUT, // indicat for RX test
			ApiRfPortIndex(1),// no use for RX RF port Set here,just put something in
			outputPortIndexes,
			outputPortOnOff,
			iThread < 8 ? 8 : 16);

		// set the input rf port Sample for TX measurement
		ApiRfPortIndex onOFFPortIdx[1];// no use for TX,just need parameter
		ApiRfPortSet(instHandle[iThread],
			API_CONFIG_TECH_GSM,// set the TX measurement RF port for GPRF, if you want to test other signal like GSM,WIFI, please use other value
			API_RF_PORT_INPUT,// indicat for TX test
			(ApiRfPortIndex)(iThread+1),// set which RF port should use for TX test
			onOFFPortIdx,//no use for TX test,just put a parameter
			outputPortOnOff,//no use for TX test,just put a parameter
			1);// set 1 
		char scpiBuff[255] = { 0 };
		CStringA value= findValue(CStringA(params), "GSMBand");
		CStringA value2 = findValue(CStringA(params), "ChannelUl");
		CStringA band="";
		if (value.Compare("1800")==0)
		{
			band = "G18";
		}
		else if (value2.Compare("1900") == 0)
		{
			value = "G19";
		}
		else if (value2.Compare("900") == 0)
		{
			band = "G09";
		}
		else if( value2.Compare("850")==0)
		{
			band = "G085";
		}
		sprintf_s(scpiBuff, "CONFigure:GSM:MEAS:BAND %s", band);
		ApiWrite(instHandle[iThread], scpiBuff);
		sprintf_s(scpiBuff, "CONFigure:GSM:MEAS:RFSettings:FREQuency %se6", value2);
		ApiWrite(instHandle[iThread], scpiBuff);
		sprintf_s(scpiBuff, "CONFigure:GSM:MEAS:RFSettings:ENPower %s","40");
		ApiWrite(instHandle[iThread], scpiBuff);
		ApiWrite(instHandle[iThread], "CONFigure:GSM:MEAS:MEValuation:REPetition SING");
		ApiWrite(instHandle[iThread], "CONFigure:GSM:MEAS:MEValuation:SCOunt:MODulation 1");
		ApiWrite(instHandle[iThread], "CONFigure:GSM:MEAS:MEValuation:SCOunt:POW 1");
		ApiWrite(instHandle[iThread], "CONFigure:GSM:MEAS:MEValuation:SCOunt:SPEC:ACLR 1");
		ApiWrite(instHandle[iThread], "CONFigure:GSM:MEAS:MEValuation:SCOunt:SPEC:SEM 1");
		ApiMeasurementOpen(instHandle[iThread], API_CONFIG_TECH_GSM);
		ApiWrite(instHandle[iThread], "*OPC?");
		length = 256;
		ApiRead(instHandle[iThread],buff,&length);
		ApiWrite(instHandle[iThread], "FETCh:GSM:MEAS:MEValuation:MODulation:AVERage?");
		length = 256;
		ApiRead(instHandle[iThread], buff, &length);
		double target_double[64];
		int len = 0;
		str2doublearray(buff, ',', target_double, &len);
		
		result.power = target_double[12];
		result.freqErr = target_double[10];
		result.EVM= target_double[2];
		result.phaseErr = target_double[6];
		command = cmdSettings.GSM_TX_STOP_COMMAND;
		if (SKIPDUT != 1)
		{
			parseCommand(&command, params);
			if (0 != executeCommand(command, iThread, readback, rfType == EN_WCDMA ? 1 : 2))
			{
				TxResStr[iThread] = "ERROR";
				char* pStr = (TxResStr[iThread]).GetBuffer();
				(TxResStr[iThread]).ReleaseBuffer();
				return pStr;
			}
		}
		
	}
	else if (rfType == EN_NR5G)
	{
		ST_CMD_SETS command = cmdSettings.NR_TX_INIT_COMMAND;
		parseCommand(&command, params);
		if (0 != executeCommand(command, iThread, readback, rfType == EN_WCDMA ? 1 : 2))
		{
			TxResStr[iThread] = "ERROR";
			char* pStr = (TxResStr[iThread]).GetBuffer();
			(TxResStr[iThread]).ReleaseBuffer();
			return pStr;
		}
		ApiWrite(instHandle[iThread], "*IDN?");
		unsigned long length = 256;
		char buff[256] = { 0 };
		ApiRead(instHandle[iThread], buff, &length);
		ApiRfPortSet(instHandle[iThread],
			API_CONFIG_TECH_GSM,
			API_RF_PORT_OUTPUT, // indicat for RX test
			ApiRfPortIndex(1),// no use for RX RF port Set here,just put something in
			outputPortIndexes,
			outputPortOnOff,
			iThread < 8 ? 8 : 16);
		// set the input rf port Sample for TX measurement
		ApiRfPortIndex onOFFPortIdx[1];// no use for TX,just need parameter
		ApiRfPortSet(instHandle[iThread],
			API_CINFIG_TECH_5G_NR,// set the TX measurement RF port for GPRF, if you want to test other signal like GSM,WIFI, please use other value
			API_RF_PORT_INPUT,// indicat for TX test
			(ApiRfPortIndex)(iThread + 1),// set which RF port should use for TX test
			onOFFPortIdx,//no use for TX test,just put a parameter
			outputPortOnOff,//no use for TX test,just put a parameter
			1);// set 1 
		char scpiBuff[255] = { 0 };
		CStringA value = findValue(CStringA(params), "NRBand");
		CStringA value2 = findValue(CStringA(params), "NR_TX0_CH");
		ApiWrite(instHandle[iThread], "CONFigure:NRS:MEAS:MEValuation:SCOunt:MODulation 1");
		ApiWrite(instHandle[iThread], "CONFigure:NRS:MEAS:MEValuation:SCOunt:POW 1");
		ApiWrite(instHandle[iThread], "CONFigure:NRS:MEAS:MEValuation:SCOunt:SPEC:ACLR 1");
		ApiWrite(instHandle[iThread], "CONFigure:NRS:MEAS:MEValuation:SCOunt:SPEC:SEM 1");
		sprintf_s(scpiBuff, "CONFigure:NRS:MEAS:BAND OB%s", value);
		ApiWrite(instHandle[iThread], buff);
		sprintf_s(scpiBuff, "CONFigure:NRS:MEAS:RFSettings:FREQuency %se6", value2);
		ApiWrite(instHandle[iThread], scpiBuff);
		sprintf_s(scpiBuff, "CONFigure:NRS:MEAS:RFSettings:ENPower %s", "30");
		ApiWrite(instHandle[iThread], scpiBuff);

		ApiWrite(instHandle[iThread], "CONFigure:NRS:MEAS:MEValuation:REPetition SING");
		ApiWrite(instHandle[iThread], "CONFigure:NRS:MEAS:MEValuation:RESult ON,ON,ON,OFF,ON,OFF,OFF,ON,ON,ON,OFF,OFF");
		ApiWrite(instHandle[iThread], "TRIGger:NRSub:MEAS:MEValuation:SOURce 'Free Run (No Sync)'");
		ApiWrite(instHandle[iThread], "CONFigure:NRSub:MEAS:CBANdwidth B100");
		//ApiMeasurementOpen(instHandle[iThread], API_CINFIG_TECH_5G_NR);
		ApiWrite(instHandle[iThread], "INIT:NRSub:MEAS:MEV");
		ApiWrite(instHandle[iThread], "*OPC?");
		length = 256;
		ApiRead(instHandle[iThread], buff, &length);
		ApiWrite(instHandle[iThread], "FETCh:NRS:MEAS:MEValuation:MODulation:AVERage?");
		length = 256;
		ApiRead(instHandle[iThread], buff, &length);
		double target_double[64];
		int len = 0;

		str2doublearray(buff, ',', target_double, &len);

		result.power = target_double[17];
		result.freqErr = target_double[15];
		result.EVM = target_double[2]>target_double[3]?target_double[2]:target_double[3];
		result.phaseErr = target_double[25]>target_double[26]?target_double[25]:target_double[26];
		if (SKIPDUT != 1)
		{
			command = cmdSettings.NR_TX_STOP_COMMAND;
			parseCommand(&command, params);
			if (0 != executeCommand(command, iThread, readback, rfType == EN_WCDMA ? 1 : 2))
			{
				TxResStr[iThread] = "ERROR";
				char* pStr = (TxResStr[iThread]).GetBuffer();
				(TxResStr[iThread]).ReleaseBuffer();
				return pStr;
			}
		}
	}
	else if (rfType == EN_WCDMA)
	{
		ST_CMD_SETS command = cmdSettings.WCDMA_TX_INIT_COMMAND;
		parseCommand(&command, params);
		//MessageBoxA(NULL, "parsecommand", "parsecommand", 0);
		if (0 != executeCommand(command, iThread, readback, rfType == EN_WCDMA ? 1 : 2))
		{
			TxResStr[iThread] = "ERROR";
			char* pStr = (TxResStr[iThread]).GetBuffer();
			(TxResStr[iThread]).ReleaseBuffer();
			return pStr;
		}
		ApiWrite(instHandle[iThread], "*IDN?");
		unsigned long length = 256;
		char buff[256] = { 0 };
		ApiRead(instHandle[iThread], buff, &length);
		ApiRfPortSet(instHandle[iThread],
			API_CONFIG_TECH_GSM,
			API_RF_PORT_OUTPUT, // indicat for RX test
			ApiRfPortIndex(1),// no use for RX RF port Set here,just put something in
			outputPortIndexes,
			outputPortOnOff,
			iThread < 8 ? 8 : 16);

		// set the input rf port Sample for TX measurement
		ApiRfPortIndex onOFFPortIdx[1];// no use for TX,just need parameter
		ApiRfPortSet(instHandle[iThread],
			API_CONFIG_TECH_WCDMA,// set the TX measurement RF port for GPRF, if you want to test other signal like GSM,WIFI, please use other value
			API_RF_PORT_INPUT,// indicat for TX test
			(ApiRfPortIndex)(iThread + 1),// set which RF port should use for TX test
			onOFFPortIdx,//no use for TX test,just put a parameter
			outputPortOnOff,//no use for TX test,just put a parameter
			1);// set 1 
		char scpiBuff[255] = { 0 };
		CStringA value = findValue(CStringA(params), "WCDMABand");
		CStringA value2 = findValue(CStringA(params), "WCDMA_TX0_CH");
		sprintf_s(scpiBuff, "CONFigure:WCDMa:MEAS:BAND OB%s", value);
		ApiWrite(instHandle[iThread], buff);
		ApiWrite(instHandle[iThread], "CONFigure:WCDMa:MEAS:RFSettings:ENPower 30");
		ApiWrite(instHandle[iThread], "CONFigure:WCDMa:MEAS:MEValuation:REPetition SING");
		sprintf_s(scpiBuff, "CONFigure:WCDMa:MEAS:RFSettings:CARRier:FREQuency %se6", value2);
		ApiWrite(instHandle[iThread], scpiBuff);
		ApiWrite(instHandle[iThread], "CONFigure:WCDMa:MEAS:UESignal:ULConfig QPSK");
		ApiWrite(instHandle[iThread], "CONFigure:WCDMa:MEAS:MEValuation:SCOunt:MODulation 1");
		ApiWrite(instHandle[iThread], "CONFigure:WCDMa:MEAS:MEValuation:SCOunt:POW 1");
		ApiWrite(instHandle[iThread], "CONFigure:WCDMa:MEAS:MEValuation:SCOunt:SPEC:ACLR 1");
		ApiWrite(instHandle[iThread], "CONFigure:WCDMa:MEAS:MEValuation:SCOunt:SPEC:SEM 1");
		ApiMeasurementOpen(instHandle[iThread], API_CONFIG_TECH_WCDMA);
		ApiWrite(instHandle[iThread], "*OPC?");
		length = 256;
		ApiRead(instHandle[iThread], buff, &length);
		ApiWrite(instHandle[iThread], "FETCh:WCDMa:MEAS:MEValuation:MODulation:AVERage?");
		length = 256;
		ApiRead(instHandle[iThread], buff, &length);
		double target_double[64];
		int len = 0;
		str2doublearray(buff, ',', target_double, &len);

		result.power = target_double[11];
		result.freqErr = target_double[9];
		result.EVM = target_double[1];
		result.phaseErr = target_double[5];
		command = cmdSettings.WCDMA_TX_STOP_COMMAND;
		if (SKIPDUT != 1)
		{
			parseCommand(&command, params);
			if (0 != executeCommand(command, iThread, readback, rfType == EN_WCDMA ? 1 : 2))
			{
				TxResStr[iThread] = "ERROR";
				char* pStr = (TxResStr[iThread]).GetBuffer();
				(TxResStr[iThread]).ReleaseBuffer();
				return pStr;
			}

		}
		CStringA band = "";
	 }
	TxResStr[iThread].Format("power=%.3lf,freqErr=%.3lf,EVM=%.3lf,SEM=%d,ACLR=%d,phaseErr=%.3lf,ACLR1=%.3lf,ACLR2=%.3lf,ACLR3=%.3lf,ACLR4=%.3lf,ACLR5=%.3lf,ACLR6=%.3lf,ACLR7=%.3lf,semMargin1=%.3lf,semMargin2=%.3lf,semMargin3=%.3lf,semMargin4=%.3lf,semMargin5=%.3lf,semMargin6=%.3lf,semMargin7=%.3lf,semMargin8=%.3lf,OBW=%.3lf",
		result.power,
		result.freqErr,
		result.EVM,
		result.SEM,
		result.ACLR,
		result.phaseErr,
		result.ACLR1,
		result.ACLR2,
		result.ACLR3,
		result.ACLR4,
		result.ACLR5,
		result.ACLR6,
		result.ACLR7,
		result.semMargin1,
		result.semMargin2,
		result.semMargin3,
		result.semMargin4,
		result.semMargin5,
		result.semMargin6,
		result.semMargin7,
		result.semMargin8,
		result.OBW);
	char* pStr =(TxResStr[iThread]).GetBuffer();
	(TxResStr[iThread]).ReleaseBuffer();
	return pStr;
}
CStringA findValue(CStringA params, CStringA name)
{	
	int startidx = params.Find(name);
	int startidx2 = params.Find("=", startidx);
	int stopidx = params.Find(",", startidx2);
	if (stopidx != -1)
	{
		return params.Mid(startidx2+1,stopidx- startidx2-1);
	}
	else
		return params.Mid(startidx2+1);
}
//char* __stdcall doRfRxTest(EN_RF_TYPE rfType, char* params, int iThread);

char* __stdcall closeRfRxTest(EN_RF_TYPE rfType, char* params, int iThread)
{
	ApiGprfSourceClose(instHandle[iThread]);
	return NULL;
}

char* __stdcall doRfRxTest(EN_RF_TYPE rfType, char* params, int iThread)
{
	if (SSHiniflag[iThread] == -1)
	{
		return NULL;
	}
	RxResStr[iThread] = "";
	int idx = 0;
	CStringA readback;
	CStringA value;
	CStringA value2;
	CStringA rd2;
	char* rtStr = NULL;
	unsigned long splength = 255;
	ST_TX_RESULT result = { 0 };
	ApiRfPortIndex outputPortIndexes[8] = { API_RF_PORT_RFIO_01,
	API_RF_PORT_RFIO_02,
	API_RF_PORT_RFIO_03,
	API_RF_PORT_RFIO_04,
	API_RF_PORT_RFIO_05,
	API_RF_PORT_RFIO_06,
	API_RF_PORT_RFIO_07,
	API_RF_PORT_RFIO_08 };
	unsigned int outputPortOnOff[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };// 1 means enable, 0 means disable, this sample will enable RF1-4 for generator
	outputPortOnOff[iThread < 8 ? iThread : iThread - 8] = 1;
	ApiRfPortSet(instHandle[iThread],
		API_CONFIG_TECH_LTEFDD,
		API_RF_PORT_OUTPUT, // indicat for RX test
		ApiRfPortIndex(1),// no use for RX RF port Set here,just put something in
		outputPortIndexes,
		outputPortOnOff,
		iThread < 8 ? 8 : 16);

	//if (rfType == EN_V2x||rfType==EN_LTE)
	//{
	ST_CMD_SETS command;
	int exeRes;
	value = findValue(CStringA(params), "RX0_CH");
	value2 = findValue(CStringA(params), "LEVEL");
	ApiWrite(instHandle[iThread], "SOURce:GPRF:GEN:BBMode CW");
	ApiGprfSourceSingleModeConfig(instHandle[iThread], atof(value), atof(value2));
	ApiGprfSourceOpen(instHandle[iThread], 0);
	Sleep(1000);
	if (SKIPDUT != 1)
	{
		if (rfType == EN_LTE)
			command = cmdSettings.LTE_RX_INIT_COMMAND;
		else if (rfType == EN_V2x)
			command = cmdSettings.V2x_RX_INIT_COMMAND;
		else if (rfType == EN_V2x2)
			command = cmdSettings.V2x_RX2_INIT_COMMAND;
		else if (rfType == EN_WCDMA)
			command = cmdSettings.WCDMA_RX_INIT_COMMAND;
		else if (rfType == EN_GSM)
			command = cmdSettings.GSM_RX_INIT_COMMAND;
		else if (rfType == EN_NR5G)
			command = cmdSettings.NR_RX_INIT_COMMAND;
		parseCommand(&command, params);
		exeRes = executeCommand(command, iThread, readback, rfType == EN_WCDMA ? 1 : 2);
		if (exeRes == -1)
		{
			RxResStr[iThread] = "ERROR";
			char* pStr = (RxResStr[iThread]).GetBuffer();
			(RxResStr[iThread]).ReleaseBuffer();
			ApiGprfSourceClose(instHandle[iThread]);
			return pStr;
		}
		else if (exeRes == -2)
		{
			goto THE_END;
		}
	}

	if (SKIPDUT != 1)
	{
		if (rfType == EN_LTE)
			command = cmdSettings.LTE_RX_START_COMMAND;
		else if (rfType == EN_V2x)
			command = cmdSettings.V2x_RX_START_COMMAND;
		else if (rfType == EN_V2x2)
			command = cmdSettings.V2x_RX2_START_COMMAND;
		else if (rfType == EN_WCDMA)
			command = cmdSettings.WCDMA_RX_START_COMMAND;
		else if (rfType == EN_GSM)
			command = cmdSettings.GSM_RX_START_COMMAND;
		else if (rfType == EN_NR5G)
			command = cmdSettings.NR_RX_START_COMMAND;


		parseCommand(&command, params);
		Sleep(2000);
		exeRes = executeCommand(command, iThread, rd2, rfType == EN_WCDMA ? 1 : 2);
		if (-1 == exeRes)
		{
			RxResStr[iThread] = "ERROR";
			char* pStr = (RxResStr[iThread]).GetBuffer();
			(RxResStr[iThread]).ReleaseBuffer();
			ApiGprfSourceClose(instHandle[iThread]);
			return pStr;
		}
		else if (-2 == exeRes)
		{
			goto THE_END;
		}

		rd2 = rd2.Trim();
		idx = rd2.Find(command.returnValue) + command.returnValue.GetLength();
		rd2 = rd2.Mid(idx + 1);
		rd2.Replace('\r', '\0');
		rd2.Replace('\n', '\0');
		RxResStr[iThread] = rd2;

		rtStr = RxResStr[iThread].GetBuffer();
		RxResStr[iThread].ReleaseBuffer();
		Sleep(200);
	THE_END:
		if (rfType == EN_LTE)
			command = cmdSettings.LTE_RX_STOP_COMMAND;
		else if (rfType == EN_V2x)
			command = cmdSettings.V2x_RX_STOP_COMMAND;
		else if (rfType == EN_V2x2)
			command = cmdSettings.V2x_RX2_STOP_COMMAND;
		else if (rfType == EN_WCDMA)
			command = cmdSettings.WCDMA_RX_STOP_COMMAND;
		else if (rfType == EN_GSM)
			command = cmdSettings.GSM_RX_STOP_COMMAND;
		else if (rfType == EN_NR5G)
			command = cmdSettings.NR_RX_STOP_COMMAND;
		parseCommand(&command, params);
		if (0 != executeCommand(command, iThread, readback, rfType == EN_WCDMA ? 1 : 2))
		{
			RxResStr[iThread] = "ERROR";
			char* pStr = (RxResStr[iThread]).GetBuffer();
			(RxResStr[iThread]).ReleaseBuffer();
			ApiGprfSourceClose(instHandle[iThread]);
			return pStr;
		}

		//}	

		ApiGprfSourceClose(instHandle[iThread]);
	}
	writelogmain("RX 测试结束，结果：", iThread);
	writelogmain(rtStr, iThread);
	return rtStr;
}
int __stdcall deInitializeInstrument(int iThread)
{
	ApiClose(&instHandle[iThread]);
	memset(&instHandle[iThread], 0, sizeof(handle));
	return 0;
}
int __stdcall deInitializeDut(int iThread)
{
	int id = 0;
	id = iThread * 2+1;
	char cmd[128] = "killall cat\r";
	char cmd2[128] = "at+cfun=0\r";
	char receivebuf[512] = { 0 };
	if (SKIPDUT == 1)
	{
		return 0;
	}
		if (SSHiniflag[iThread] == 1)
		{
			writeshell(cmd, iThread,id);
			Sleep(200);
			readshell(receivebuf,false,sizeof(receivebuf),iThread,id,400);
		/*	writeshell(cmd2, iThread, id );
			readshell(receivebuf, false, sizeof(receivebuf), iThread, id, 400);*/
			//Sleep(2000);
		}
	
	Sleep(200);
	closesession(iThread);
	QuitReadLoop[id] = 1;
	//close_socket(id);
	setLogFilePath(dlllogPath, iThread);
	/*if (tread[id] != 0 && tread[id]->joinable())
	{
		tread[id]->join();
		delete tread[id];
		tread[id] = NULL;
	}*/
	shutdownALL(id);
	//shutdownALL(id + 1);
	//close_socket(id);
	//close_socket(id+1);
	SSHiniflag[iThread] = -1;
	return 0;
}
int __stdcall deInitializeDLL()
{
	if (dutConnType == EN_SSH)
	{
		exitsshLib();
	}
	return 0;
}
void parseCommand(ST_CMD_SETS* cmdsets, char* params)
{
	if (cmdsets->cmdSet.size() < 0) return;
	for (size_t i = 0; i < cmdsets->cmdSet.size(); i++)
	{
		cmdsets->cmdSet[i] =findelement(&cmdsets->cmdSet[i], params);
	}
}
int executeCommand(ST_CMD_SETS cmdsets, int iThread,CStringA &ret,int sendCouont=1)
{
	if (SKIPDUT == 1)
		return 0;
	int id = iThread * 2;
	
		for (int i = 0; i < cmdsets.cmdSet.size(); i++)
		{
			char readbackbuf[1024] = { 0 };
			char logStr[1024] = { 0 };
			int retry = 0;
			for (int j = 0; j < sendCouont; j++)
			{
				for (retry = 0; retry < 3; retry++)
				{
					sendandReceive(cmdsets.cmdSet[i].GetBuffer(), id + 1, iThread, readbackbuf, sizeof(readbackbuf));
					if (cmdsets.needCheckReturn)
					{
						if (NULL != strstr(readbackbuf, cmdsets.returnValue.GetBuffer()))
						{
							sprintf_s(logStr, "readback success string: %s", readbackbuf);
							writelogmain(logStr, iThread);
							ret = readbackbuf;
							break;
						}
						else
						{
							Sleep(5000);
							sprintf_s(logStr, "readback timeOut retry", readbackbuf);
							writelogmain(logStr, iThread);
						}
					}


					/*	if (0!= sendshell(cmdsets.cmdSet[i].GetBuffer(), id + 1, iThread))
							return -1;
						char readbackbuf[128] = { 0 };
						if (cmdsets.needCheckReturn)
						{
							if (readBack(cmdsets.returnValue.GetBuffer(), readbackbuf, sizeof(readbackbuf), 500, iThread, id) == 0)
							{
								ret = readbackbuf;
								sprintf_s(logStr, "readback success string: %s", readbackbuf);
								writelogmain(logStr, iThread);
								break;
							}
							else
							{
								sprintf_s(logStr, "readback timeOut retry~");
								writelogmain(logStr, iThread);
							}
						}
						else
							break;
						cmdsets.returnValue.ReleaseBuffer();*/
				}
				if (retry == 3)
				{
					writelogmain("尝试3次发送失败，返回-1", iThread);
					return -2;
				}
			}
		}
	
	return 0;
}
std::vector<CStringA> splitfunction(CStringA str, const char findchar)
{
	std::vector<CStringA> ret;
	int start = 0;
	str.AppendChar(findchar);
	for (size_t i = 0; i < str.GetLength(); i++)
	{
		if (str[i] == findchar)
		{
			CStringA ddf = (CStringA)str.Mid(start, i - start);
			ret.push_back(ddf);
			start = i + 1;
		}
	}
	return ret;
}
CStringA findelement(CStringA* str, CStringA params) {
	typedef struct Key_value
	{
		CStringA key;
		CStringA value;
	}KEY_VAL;
	KEY_VAL keyvalue;
	std::vector<KEY_VAL> keyvalueall;
	CStringA command = *str;
	int iii = 0;
	std::vector<int> arr;
	while (iii != -1)
	{
		iii = command.Find("#@", iii + 1);
		if (iii != -1)
		{
			arr.push_back(iii);
		}
	}
	for (size_t i = 0; i < arr.size(); i=i+2)
	{
		keyvalue.key = command.Mid(arr[i]+2, arr[i + 1] - arr[i]-2);
		keyvalue.value = ReplaceElement(keyvalue.key, params);
		keyvalueall.push_back(keyvalue);
	}
	for (size_t i = 0; i < keyvalueall.size(); i++)
	{
		command.Replace("#@" + keyvalueall[i].key + "#@", keyvalueall[i].value);
	}
	return command;
}
CStringA ReplaceElement(CStringA element, CStringA param) {
	std::vector<ST_INFO_MAP> infomap = cmdSettings.vecInfo_map;
	CStringA element1 = " ";
	for (size_t i = 0; i < infomap.size(); i++)
	{
		if (infomap[i].Name == element)
		{
			if (infomap[i].isDirectReplace==true)
			{
				element1 = replaceelement(element, param);
				break;
			}
			else if(infomap[i].isDirectReplace == false)
			{
				CStringA elementreplace = " ";
				CStringA elementreplace1 = " ";
				elementreplace = infomap[i].replaceValueProperty;
				elementreplace1 = replaceelement(elementreplace, param);
				for (size_t j = 0; j < infomap[i].vecKeyValue.size(); j++)
				{
					if (infomap[i].vecKeyValue[j].key== elementreplace1)
					{
						element1 = infomap[i].vecKeyValue[j].value;
						break;
					}
				}
			}
		}
	}
	return element1;
}
CStringA replaceelement(CStringA element, CStringA params) {
	CStringA element1 = "";
	std::vector<CStringA> sdsd = splitfunction(params, ',');
	for (size_t j = 0; j < sdsd.size(); j++)
	{
		if (sdsd[j].Find(element)!=-1)
		{
			element1 = sdsd[j].Mid(sdsd[j].Find('=')+1, sdsd[j].GetLength()- sdsd[j].Find('=')-1);
			break;
		}
	}
	return element1;
}