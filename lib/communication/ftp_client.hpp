#pragma once

#define FTP_CLIENT_DEBUG 1

#define FTP_CLIENT_BUFFER_SIZE 4096
#define FTP_CLIENT_RESPONSE_BUFFER_SIZE 1024
#define FTP_CLIENT_TEMP_BUFFER_SIZE 1024
#define FTP_CLIENT_ACCEPT_TIMEOUT 30

/* FtpAccess() type codes */
#define FTP_CLIENT_DIR 1
#define FTP_CLIENT_DIR_VERBOSE 2
#define FTP_CLIENT_FILE_READ 3
#define FTP_CLIENT_FILE_WRITE 4
#define FTP_CLIENT_MLSD 5

/* FtpAccess() mode codes */
#define FTP_CLIENT_ASCII 'A'
#define FTP_CLIENT_IMAGE 'I'
#define FTP_CLIENT_TEXT FTP_CLIENT_ASCII
#define FTP_CLIENT_BINARY FTP_CLIENT_IMAGE

/* connection modes */
#define FTP_CLIENT_PASSIVE 1
#define FTP_CLIENT_ACTIVE 2

/* connection option names */
#define FTP_CLIENT_CONNMODE 1
#define FTP_CLIENT_CALLBACK 2
#define FTP_CLIENT_IDLETIME 3
#define FTP_CLIENT_CALLBACKARG 4
#define FTP_CLIENT_CALLBACKBYTES 5

#include <cstdint>

#include "netdb.h"

struct NetBuf;

typedef int (*FtpClientCallback_t)(NetBuf* nControl, uint32_t xfered, void* arg);

struct FtpClientCallbackOptions_t
{
  FtpClientCallback_t cbFunc; /* function to call */
  void* cbArg;                /* argument to pass to function */
  unsigned int bytesXferred;  /* callback if this number of bytes transferred */
  unsigned int idleTime;      /* callback if this many milliseconds have elapsed */
};

struct NetBuf
{
  char* cput;
  char* cget;
  int handle;
  int cavail, cleft;
  char* buf;
  int dir;
  NetBuf* ctrl;
  NetBuf* data;
  int cmode;
  timeval idletime;
  FtpClientCallback_t idlecb;
  void* idlearg;
  unsigned long int xfered;
  unsigned long int cbbytes;
  unsigned long int xfered1;
  char response[1024];
};

class FtpClient
{
public:
  /*Miscellaneous Functions*/
  int ftpClientSite(const char* cmd);
  char* ftpClientGetLastResponse(NetBuf* nControl);
  int ftpClientGetSysType(char* buf, int max);
  int ftpClientGetFileSize(const char* path, unsigned int* size, char mode);
  int ftpClientGetModDate(const char* path, char* dt, int max);
  int ftpClientSetCallback(const FtpClientCallbackOptions_t* opt);
  int ftpClientClearCallback(NetBuf* nControl);

  /*Server connection*/
  int ftpClientConnect(const char* host, uint16_t port);
  int ftpClientLogin(const char* user, const char* pass);
  void ftpClientQuit();
  int ftpClientSetOptions(int opt, long val);

  /*Directory Functions*/
  int ftpClientChangeDir(const char* path);
  int ftpClientMakeDir(const char* path);
  int ftpClientRemoveDir(const char* path);
  int ftpClientDir(const char* outputfile, const char* path);
  int ftpClientNlst(const char* outputfile, const char* path);
  int ftpClientMlsd(const char* outputfile, const char* path);
  int ftpClientChangeDirUp(NetBuf* nControl);
  int ftpClientPwd(char* path, int max);

  /*File to File Transfer*/
  int ftpClientGet(const char* outputfile, const char* path, char mode);
  int ftpClientPut(const char* inputfile, const char* path, char mode);
  int ftpClientDelete(const char* fnm);
  int ftpClientRename(const char* src, const char* dst);

  /*File to Program Transfer*/
  int ftpClientAccess(const char* path, int typ, int mode, NetBuf** nData);
  int ftpClientRead(void* buf, int max, NetBuf* nData);
  int ftpClientWrite(const void* buf, int len, NetBuf* nData);
  int ftpClientClose(NetBuf* nData);

private:
  int socketWait(NetBuf* ctl);
  int readLine(char* buffer, int max, NetBuf* ctl);
  int readResponse(char c, NetBuf* ctl);
  int sendCommand(const char* cmd, char expresp);
  int xfer(const char* localfile, const char* path, int typ, int mode);
  int openPort(NetBuf** nData, int mode, int dir);
  int writeLine(const char* buf, int len, NetBuf* nData);
  int acceptConnection(NetBuf* nData);

private:
  NetBuf m_nControl;
};