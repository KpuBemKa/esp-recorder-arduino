#include "ftp_client.hpp"

// #include <inttypes.h>
// #include <stdio.h>
#include <cstring>
#include <sys/socket.h>
#include <sys/unistd.h>

#include "esp_log.h"

#ifndef FTP_CLIENT_DEFAULT_MODE
#define FTP_CLIENT_DEFAULT_MODE FTP_CLIENT_PASSIVE
#endif

#define FTP_CLIENT_CONTROL 0
#define FTP_CLIENT_READ 1
#define FTP_CLIENT_WRITE 2

// static bool isInitilized = false;
// static FtpClient ftpClient_;

/*
 * socket_wait - wait for socket to receive or flush data
 *
 * return 1 if no user callback, otherwise, return value returned by
 * user callback
 */
int
FtpClient::socketWait(NetBuf* ctl)
{
  fd_set fd;
  fd_set* rfd = NULL;
  fd_set* wfd = NULL;
  struct timeval tv;
  int rv = 0;

  if ((ctl->dir == FTP_CLIENT_CONTROL) || (ctl->idlecb == NULL))
    return 1;
  if (ctl->dir == FTP_CLIENT_WRITE)
    wfd = &fd;
  else
    rfd = &fd;
  FD_ZERO(&fd);
  do {
    FD_SET(ctl->handle, &fd);
    tv = ctl->idletime;
    rv = select((ctl->handle + 1), rfd, wfd, NULL, &tv);
    if (rv == -1) {
      rv = 0;
      strncpy(ctl->ctrl->response, strerror(errno), sizeof(ctl->ctrl->response));
      break;
    } else if (rv > 0) {
      rv = 1;
      break;
    }
  } while ((rv = ctl->idlecb(ctl, ctl->xfered, ctl->idlearg)));
  return rv;
}

/*
 * read a line of text
 *
 * return -1 on error or bytecount
 */
int
FtpClient::readLine(char* buffer, int max, NetBuf* ctl)
{
  if ((ctl->dir != FTP_CLIENT_CONTROL) && (ctl->dir != FTP_CLIENT_READ))
    return -1;
  if (max == 0)
    return 0;

  int x, retval = 0;
  char *end, *bp = buffer;
  int eof = 0;
  while (1) {
    if (ctl->cavail > 0) {
      x = (max >= ctl->cavail) ? ctl->cavail : (max - 1);
      end = reinterpret_cast<char*>(memccpy(bp, ctl->cget, '\n', x));
      if (end != NULL)
        x = end - bp;
      retval += x;
      bp += x;
      *bp = '\0';
      max -= x;
      ctl->cget += x;
      ctl->cavail -= x;
      if (end != NULL) {
        bp -= 2;
        if (strcmp(bp, "\r\n") == 0) {
          *bp++ = '\n';
          *bp++ = '\0';
          --retval;
        }
        break;
      }
    }
    if (max == 1) {
      *buffer = '\0';
      break;
    }
    if (ctl->cput == ctl->cget) {
      ctl->cput = ctl->cget = ctl->buf;
      ctl->cavail = 0;
      ctl->cleft = FTP_CLIENT_BUFFER_SIZE;
    }
    if (eof) {
      if (retval == 0)
        retval = -1;
      break;
    }
    if (!socketWait(ctl))
      return retval;
    if ((x = recv(ctl->handle, ctl->cput, ctl->cleft, 0)) == -1) {
#if FTP_CLIENT_DEBUG
      perror("FTP Client Error: realLine, read");
#endif
      retval = -1;
      break;
    }
    if (x == 0)
      eof = 1;
    ctl->cleft -= x;
    ctl->cavail += x;
    ctl->cput += x;
  }
  return retval;
}

/*
 * read a response from the server
 *
 * return 0 if first char doesn't match
 * return 1 if first char matches
 */
int
FtpClient::readResponse(char c, NetBuf* ctl)
{
  char match[5];
  if (readLine(ctl->response, FTP_CLIENT_RESPONSE_BUFFER_SIZE, ctl) == -1) {
#if FTP_CLIENT_DEBUG
    perror("FTP Client Error: readResponse, read failed");
#endif
    return 0;
  }
#if FTP_CLIENT_DEBUG == 2
  printf("FTP Client Response: %s\n\r", ctl->response);
#endif
  if (ctl->response[3] == '-') {
    strncpy(match, ctl->response, 3);
    match[3] = ' ';
    match[4] = '\0';
    do {
      if (readLine(ctl->response, FTP_CLIENT_RESPONSE_BUFFER_SIZE, &m_nControl) == -1) {
#if FTP_CLIENT_DEBUG
        perror("FTP Client Error: readResponse, read failed");
#endif
        return 0;
      }
#if FTP_CLIENT_DEBUG == 2
      printf("FTP Client Response: %s\n\r", ctl->response);
#endif
    } while (strncmp(ctl->response, match, 4));
  }
  if (ctl->response[0] == c)
    return 1;
  else
    return 0;
}

/*
 * sendCommand - send a command and wait for expected response
 *
 * return 1 if proper response received, 0 otherwise
 */
int
FtpClient::sendCommand(const char* cmd, char expresp)
{
  char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
  if (m_nControl.dir != FTP_CLIENT_CONTROL)
    return 0;
#if FTP_CLIENT_DEBUG == 2
  printf("FTP Client sendCommand: %s\n\r", cmd);
#endif
  if ((strlen(cmd) + 3) > sizeof(buf))
    return 0;
  sprintf(buf, "%s\r\n", cmd);
  if (send(m_nControl.handle, buf, strlen(buf), 0) <= 0) {
#if FTP_CLIENT_DEBUG
    perror("FTP Client sendCommand: write");
#endif
    return 0;
  }
  return readResponse(expresp, &m_nControl);
}

/*
 * Xfer - issue a command and transfer data
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::xfer(const char* localfile, const char* path, int typ, int mode)
{
  FILE* local = NULL;
  NetBuf* nData;

  if (localfile != NULL) {
    char ac[4];
    memset(ac, 0, sizeof(ac));
    if (typ == FTP_CLIENT_FILE_WRITE)
      ac[0] = 'r';
    else
      ac[0] = 'w';
    if (mode == FTP_CLIENT_IMAGE)
      ac[1] = 'b';
    local = fopen(localfile, ac);
    if (local == NULL) {
      strncpy(m_nControl.response, strerror(errno), sizeof(m_nControl.response));
      return 0;
    }
  }
  if (local == NULL)
    local = (typ == FTP_CLIENT_FILE_WRITE) ? stdin : stdout;
  if (!ftpClientAccess(path, typ, mode, &nData)) {
    if (localfile) {
      fclose(local);
      if (typ == FTP_CLIENT_FILE_READ)
        unlink(localfile);
    }
    return 0;
  }

  int rv = 1;
  int l = 0;
  char* dbuf = reinterpret_cast<char*>(malloc(FTP_CLIENT_BUFFER_SIZE));
  if (typ == FTP_CLIENT_FILE_WRITE) {
    while ((l = fread(dbuf, 1, FTP_CLIENT_BUFFER_SIZE, local)) > 0) {
      int c = ftpClientWrite(dbuf, l, nData);
      if (c < l) {
        printf("Ftp Client xfer short write: passed %d, wrote %d\n", l, c);
        rv = 0;
        break;
      }
    }
  } else {
    while ((l = ftpClientRead(dbuf, FTP_CLIENT_BUFFER_SIZE, nData)) > 0) {
      if (fwrite(dbuf, 1, l, local) == 0) {
#if FTP_CLIENT_DEBUG
        perror("FTP Client xfer localfile write");
#endif
        rv = 0;
        break;
      }
    }
  }
  free(dbuf);
  fflush(local);
  if (localfile != NULL) {
    fclose(local);
    if (rv != 1)
      unlink(localfile);
  }
  ftpClientClose(nData);
  return rv;
}

/*
 * openPort - set up data connection
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::openPort(NetBuf** nData, int mode, int dir)
{
  union
  {
    struct sockaddr sa;
    struct sockaddr_in in;
  } sin;

  if (m_nControl.dir != FTP_CLIENT_CONTROL)
    return -1;
  if ((dir != FTP_CLIENT_READ) && (dir != FTP_CLIENT_WRITE)) {
    sprintf(m_nControl.response, "Invalid direction %d\n", dir);
    return -1;
  }
  if ((mode != FTP_CLIENT_ASCII) && (mode != FTP_CLIENT_IMAGE)) {
    sprintf(m_nControl.response, "Invalid mode %c\n", mode);
    return -1;
  }
  // unsigned int l = sizeof(sin);
  socklen_t l = sizeof(sin);
  if (m_nControl.cmode == FTP_CLIENT_PASSIVE) {
    memset(&sin, 0, l);
    sin.in.sin_family = AF_INET;
    if (!sendCommand("PASV", '2'))
      return -1;
    char* cp = strchr(m_nControl.response, '(');
    if (cp == NULL)
      return -1;
    cp++;
    unsigned int v[6];
    sscanf(cp, "%u,%u,%u,%u,%u,%u", &v[2], &v[3], &v[4], &v[5], &v[0], &v[1]);
    sin.sa.sa_data[2] = v[2];
    sin.sa.sa_data[3] = v[3];
    sin.sa.sa_data[4] = v[4];
    sin.sa.sa_data[5] = v[5];
    sin.sa.sa_data[0] = v[0];
    sin.sa.sa_data[1] = v[1];
  } else {
    if (getsockname(m_nControl.handle, &sin.sa, &l) < 0) {
#if FTP_CLIENT_DEBUG
      perror("FTP Client openPort: getsockname");
#endif
      return -1;
    }
  }
  int sData = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sData == -1) {
#if FTP_CLIENT_DEBUG
    perror("FTP Client openPort: socket");
#endif
    return -1;
  }
  if (m_nControl.cmode == FTP_CLIENT_PASSIVE) {
    if (connect(sData, &sin.sa, sizeof(sin.sa)) == -1) {
#if FTP_CLIENT_DEBUG
      perror("FTP Client openPort: connect");
#endif
      closesocket(sData);
      return -1;
    }
  } else {
    sin.in.sin_port = 0;
    if (bind(sData, &sin.sa, sizeof(sin)) == -1) {
#if FTP_CLIENT_DEBUG
      perror("FTP Client openPort: bind");
#endif
      closesocket(sData);
      return -1;
    }
    if (listen(sData, 1) < 0) {
#if FTP_CLIENT_DEBUG
      perror("FTP Client openPort: listen");
#endif
      closesocket(sData);
      return -1;
    }
    if (getsockname(sData, &sin.sa, &l) < 0)
      return -1;
    char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
    sprintf(buf,
            "PORT %d,%d,%d,%d,%d,%d",
            (unsigned char)sin.sa.sa_data[2],
            (unsigned char)sin.sa.sa_data[3],
            (unsigned char)sin.sa.sa_data[4],
            (unsigned char)sin.sa.sa_data[5],
            (unsigned char)sin.sa.sa_data[0],
            (unsigned char)sin.sa.sa_data[1]);
    if (!sendCommand(buf, '2')) {
      closesocket(sData);
      return -1;
    }
  }
  NetBuf* ctrl = reinterpret_cast<NetBuf*>(calloc(1, sizeof(NetBuf)));
  if (ctrl == NULL) {
#if FTP_CLIENT_DEBUG
    perror("FTP Client openPort: calloc ctrl");
#endif
    closesocket(sData);
    return -1;
  }
  if ((mode == 'A') &&
      ((ctrl->buf = reinterpret_cast<char*>(malloc(FTP_CLIENT_BUFFER_SIZE))) == NULL)) {
#if FTP_CLIENT_DEBUG
    perror("FTP Client openPort: calloc ctrl->buf");
#endif
    closesocket(sData);
    free(ctrl);
    return -1;
  }
  ctrl->handle = sData;
  ctrl->dir = dir;
  ctrl->idletime = m_nControl.idletime;
  ctrl->idlearg = m_nControl.idlearg;
  ctrl->xfered = 0;
  ctrl->xfered1 = 0;
  ctrl->cbbytes = m_nControl.cbbytes;
  ctrl->ctrl = &m_nControl;
  if (ctrl->idletime.tv_sec || ctrl->idletime.tv_usec || ctrl->cbbytes)
    ctrl->idlecb = m_nControl.idlecb;
  else
    ctrl->idlecb = NULL;
  m_nControl.data = ctrl;
  *nData = ctrl;
  return 1;
}

/*
 * write lines of text
 *
 * return -1 on error or bytecount
 */
int
FtpClient::writeLine(const char* buf, int len, NetBuf* nData)
{
  if (nData->dir != FTP_CLIENT_WRITE)
    return -1;
  char* nbp = nData->buf;
  int nb = 0;
  int w = 0;
  const char* ubp = buf;
  char lc = 0;
  int x = 0;

  for (x = 0; x < len; x++) {
    if ((*ubp == '\n') && (lc != '\r')) {
      if (nb == FTP_CLIENT_BUFFER_SIZE) {
        if (!socketWait(nData))
          return x;
        w = send(nData->handle, nbp, FTP_CLIENT_BUFFER_SIZE, 0);
        if (w != FTP_CLIENT_BUFFER_SIZE) {
#if FTP_CLIENT_DEBUG
          printf("Ftp client write line: net_write(1) returned %d, errno = %d\n", w, errno);
#endif
          return (-1);
        }
        nb = 0;
      }
      nbp[nb++] = '\r';
    }
    if (nb == FTP_CLIENT_BUFFER_SIZE) {
      if (!socketWait(nData))
        return x;
      w = send(nData->handle, nbp, FTP_CLIENT_BUFFER_SIZE, 0);
      if (w != FTP_CLIENT_BUFFER_SIZE) {
#if FTP_CLIENT_DEBUG
        printf("Ftp client write line: net_write(2) returned %d, errno = %d\n", w, errno);
#endif
        return (-1);
      }
      nb = 0;
    }
    nbp[nb++] = lc = *ubp++;
  }
  if (nb) {
    if (!socketWait(nData))
      return x;
    w = send(nData->handle, nbp, nb, 0);
    if (w != nb) {
#if FTP_CLIENT_DEBUG
      printf("Ftp client write line: net_write(3) returned %d, errno = %d\n", w, errno);
#endif
      return (-1);
    }
  }
  return len;
}

/*
 * acceptConnection - accept connection from server
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::acceptConnection(NetBuf* nData)
{
  int rv = 0;
  fd_set mask;
  FD_ZERO(&mask);
  FD_SET(m_nControl.handle, &mask);
  FD_SET(nData->handle, &mask);
  struct timeval tv;
  tv.tv_usec = 0;
  tv.tv_sec = FTP_CLIENT_ACCEPT_TIMEOUT;
  int i = m_nControl.handle;
  if (i < nData->handle)
    i = nData->handle;
  i = select(i + 1, &mask, NULL, NULL, &tv);
  if (i == -1) {
    strncpy(m_nControl.response, strerror(errno), sizeof(m_nControl.response));
    closesocket(nData->handle);
    nData->handle = 0;
    rv = 0;
  } else if (i == 0) {
    strcpy(m_nControl.response,
           "FTP Client accept connection "
           "timed out waiting for connection");
    closesocket(nData->handle);
    nData->handle = 0;
    rv = 0;
  } else {
    if (FD_ISSET(nData->handle, &mask)) {
      struct sockaddr addr;
      // unsigned int l = sizeof(addr);
      socklen_t l = sizeof(addr);
      int sData = accept(nData->handle, &addr, &l);
      i = errno;
      closesocket(nData->handle);
      if (sData > 0) {
        rv = 1;
        nData->handle = sData;
      } else {
        strncpy(m_nControl.response, strerror(i), sizeof(m_nControl.response));
        nData->handle = 0;
        rv = 0;
      }
    } else if (FD_ISSET(m_nControl.handle, &mask)) {
      closesocket(nData->handle);
      nData->handle = 0;
      readResponse('2', &m_nControl);
      rv = 0;
    }
  }
  return rv;
}

/*
 * ftpClientSite - send a SITE command
 *
 * return 1 if command successful, 0 otherwise
 */
int
FtpClient::ftpClientSite(const char* cmd)
{
  char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
  if ((strlen(cmd) + 7) > sizeof(buf))
    return 0;
  sprintf(buf, "SITE %s", cmd);
  if (!sendCommand(buf, '2'))
    return 0;
  else
    return 1;
}

/*
 * ftpClientGetLastResponse - return a pointer to the last response received
 */
char*
FtpClient::ftpClientGetLastResponse(NetBuf* nControl)
{
  if ((nControl) && (nControl->dir == FTP_CLIENT_CONTROL))
    return nControl->response;
  else
    return NULL;
}

/*
 * ftpClientGetSysType - send a SYST command
 *
 * Fills in the user buffer with the remote system type.  If more
 * information from the response is required, the user can parse
 * it out of the response buffer returned by FtpLastResponse().
 *
 * return 1 if command successful, 0 otherwise
 */
int
FtpClient::ftpClientGetSysType(char* buf, int max)
{
  if (!sendCommand("SYST", '2'))
    return 0;
  char* s = &m_nControl.response[4];
  int l = max;
  char* b = buf;
  while ((--l) && (*s != ' '))
    *b++ = *s++;
  *b++ = '\0';
  return 1;
}

/*
 * ftpClientGetFileSize - determine the size of a remote file
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientGetFileSize(const char* path, unsigned int* size, char mode)
{
  char cmd[FTP_CLIENT_TEMP_BUFFER_SIZE];
  if ((strlen(path) + 7) > sizeof(cmd))
    return 0;
  sprintf(cmd, "TYPE %c", mode);
  if (!sendCommand(cmd, '2'))
    return 0;
  int rv = 1;
  sprintf(cmd, "SIZE %s", path);
  if (!sendCommand(cmd, '2'))
    rv = 0;
  else {
    int resp;
    unsigned int sz;
    if (sscanf(m_nControl.response, "%d %u", &resp, &sz) == 2)
      *size = sz;
    else
      rv = 0;
  }
  return rv;
}

/*
 * ftpClientGetModDate - determine the modification date of a remote file
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientGetModDate(const char* path, char* dt, int max)
{
  char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
  if ((strlen(path) + 7) > sizeof(buf))
    return 0;
  sprintf(buf, "MDTM %s", path);
  int rv = 1;
  if (!sendCommand(buf, '2'))
    rv = 0;
  else
    strncpy(dt, &m_nControl.response[4], max);
  return rv;
}

int
FtpClient::ftpClientSetCallback(const FtpClientCallbackOptions_t* opt)
{
  m_nControl.idlecb = opt->cbFunc;
  m_nControl.idlearg = opt->cbArg;
  m_nControl.idletime.tv_sec = opt->idleTime / 1000;
  m_nControl.idletime.tv_usec = (opt->idleTime % 1000) * 1000;
  m_nControl.cbbytes = opt->bytesXferred;
  return 1;
}

int
FtpClient::ftpClientClearCallback(NetBuf* nControl)
{
  nControl->idlecb = NULL;
  nControl->idlearg = NULL;
  nControl->idletime.tv_sec = 0;
  nControl->idletime.tv_usec = 0;
  nControl->cbbytes = 0;

  return 1;
}

/*
 * connect - connect to remote server
 *
 * return 1 if connected, 0 if not
 */
int
FtpClient::ftpClientConnect(const char* host, uint16_t port)
{
  ESP_LOGD(__FUNCTION__, "host=%s", host);
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = inet_addr(host);
  ESP_LOGD(__FUNCTION__, "sin.sin_addr.s_addr=%" PRIx32, sin.sin_addr.s_addr);
  if (sin.sin_addr.s_addr == 0xffffffff) {
    struct hostent* hp;
    hp = gethostbyname(host);
    if (hp == NULL) {
#if FTP_CLIENT_DEBUG
      perror("FTP Client Error: Connect, gethostbyname");
#endif
      return 0;
    }
    struct ip4_addr* ip4_addr;
    ip4_addr = (struct ip4_addr*)hp->h_addr;
    sin.sin_addr.s_addr = ip4_addr->addr;
    ESP_LOGD(__FUNCTION__, "sin.sin_addr.s_addr=%" PRIx32, sin.sin_addr.s_addr);
  }

  int sControl = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  ESP_LOGD(__FUNCTION__, "sControl=%d", sControl);
  if (sControl == -1) {
#if FTP_CLIENT_DEBUG
    perror("FTP Client Error: Connect, socket");
#endif
    return 0;
  }
  if (connect(sControl, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
#if FTP_CLIENT_DEBUG
    perror("FTP Client Error: Connect, connect");
#endif
    closesocket(sControl);
    return 0;
  }
  NetBuf* ctrl = reinterpret_cast<NetBuf*>(calloc(1, sizeof(NetBuf)));
  if (ctrl == NULL) {
#if FTP_CLIENT_DEBUG
    perror("FTP Client Error: Connect, calloc ctrl");
#endif
    closesocket(sControl);
    return 0;
  }
  ctrl->buf = reinterpret_cast<char*>(malloc(FTP_CLIENT_BUFFER_SIZE));
  if (ctrl->buf == NULL) {
#if FTP_CLIENT_DEBUG
    perror("FTP Client Error: Connect, calloc ctrl->buf");
#endif
    closesocket(sControl);
    free(ctrl);
    return 0;
  }
  ctrl->handle = sControl;
  ctrl->dir = FTP_CLIENT_CONTROL;
  ctrl->ctrl = NULL;
  ctrl->data = NULL;
  ctrl->cmode = FTP_CLIENT_DEFAULT_MODE;
  ctrl->idlecb = NULL;
  ctrl->idletime.tv_sec = ctrl->idletime.tv_usec = 0;
  ctrl->idlearg = NULL;
  ctrl->xfered = 0;
  ctrl->xfered1 = 0;
  ctrl->cbbytes = 0;
  if (readResponse('2', ctrl) == 0) {
    closesocket(sControl);
    free(ctrl->buf);
    free(ctrl);
    return 0;
  }
  m_nControl = *ctrl;
  return 1;
}

/*
 * login - log in to remote server
 *
 * return 1 if logged in, 0 otherwise
 */
int
FtpClient::ftpClientLogin(const char* user, const char* pass)
{
  char tempbuf[64];
  if (((strlen(user) + 7) > sizeof(tempbuf)) || ((strlen(pass) + 7) > sizeof(tempbuf)))
    return 0;
  sprintf(tempbuf, "USER %s", user);
  if (!sendCommand(tempbuf, '3')) {
    if (m_nControl.response[0] == '2')
      return 1;
    return 0;
  }
  sprintf(tempbuf, "PASS %s", pass);
  return sendCommand(tempbuf, '2');
}

/*
 * ftpClientQuit - disconnect from remote
 *
 * return 1 if successful, 0 otherwise
 */
void
FtpClient::ftpClientQuit()
{
  if (m_nControl.dir != FTP_CLIENT_CONTROL)
    return;
  sendCommand("QUIT", '2');
  closesocket(m_nControl.handle);
  free(m_nControl.buf);
}

/*
 * ftpClientSetOptions - change connection options
 *
 * returns 1 if successful, 0 on error
 */
int
FtpClient::ftpClientSetOptions(int opt, long val)
{
  int v, rv = 0;
  switch (opt) {
    case FTP_CLIENT_CONNMODE: {
      v = (int)val;
      if ((v == FTP_CLIENT_PASSIVE) || (v == FTP_CLIENT_ACTIVE)) {
        m_nControl.cmode = v;
        rv = 1;
      }
    } break;

    case FTP_CLIENT_CALLBACK: {
      m_nControl.idlecb = (FtpClientCallback_t)val;
      rv = 1;
    } break;

    case FTP_CLIENT_IDLETIME: {
      v = (int)val;
      rv = 1;
      m_nControl.idletime.tv_sec = v / 1000;
      m_nControl.idletime.tv_usec = (v % 1000) * 1000;
    } break;

    case FTP_CLIENT_CALLBACKARG: {
      rv = 1;
      m_nControl.idlearg = (void*)val;
    } break;

    case FTP_CLIENT_CALLBACKBYTES: {
      rv = 1;
      m_nControl.cbbytes = (int)val;
    } break;
  }
  return rv;
}

/*
 * ftpClientChangeDir - change path at remote
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientChangeDir(const char* path)
{
  char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
  if ((strlen(path) + 6) > sizeof(buf))
    return 0;
  sprintf(buf, "CWD %s", path);
  if (!sendCommand(buf, '2'))
    return 0;
  else
    return 1;
}

/*
 * ftpClientMakeDir - create a directory at server
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientMakeDir(const char* path)
{
  char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
  if ((strlen(path) + 6) > sizeof(buf))
    return 0;
  sprintf(buf, "MKD %s", path);
  if (!sendCommand(buf, '2'))
    return 0;
  else
    return 1;
}

/*
 * ftpClientRemoveDir - remove directory at remote
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientRemoveDir(const char* path)
{
  char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
  if ((strlen(path) + 6) > sizeof(buf))
    return 0;
  sprintf(buf, "RMD %s", path);
  if (!sendCommand(buf, '2'))
    return 0;
  else
    return 1;
}

/*
 * ftpClientDir - issue a LIST command and write response to output
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientDir(const char* outputfile, const char* path)
{
  return xfer(outputfile, path, FTP_CLIENT_DIR_VERBOSE, FTP_CLIENT_ASCII);
}

/*
 * ftpClientNlst - issue an NLST command and write response to output
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientNlst(const char* outputfile, const char* path)
{
  return xfer(outputfile, path, FTP_CLIENT_DIR, FTP_CLIENT_ASCII);
}

/*
 * ftpClientNlst - issue an MLSD command and write response to output
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientMlsd(const char* outputfile, const char* path)
{
  return xfer(outputfile, path, FTP_CLIENT_MLSD, FTP_CLIENT_ASCII);
}

/*
 * ftpClientChangeDirUp - move to parent directory at remote
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientChangeDirUp(NetBuf* nControl)
{
  if (!sendCommand("CDUP", '2'))
    return 0;
  else
    return 1;
}

/*
 * ftpClientPwd - get working directory at remote
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientPwd(char* path, int max)
{
  if (!sendCommand("PWD", '2'))
    return 0;
  char* s = strchr(m_nControl.response, '"');
  if (s == NULL)
    return 0;
  s++;
  int l = max;
  char* b = path;
  while ((--l) && (*s) && (*s != '"'))
    *b++ = *s++;
  *b++ = '\0';
  return 1;
}

/*
 * ftpClientGet - issue a GET command and write received data to output
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientGet(const char* outputfile, const char* path, char mode)
{
  return xfer(outputfile, path, FTP_CLIENT_FILE_READ, mode);
}

/*
 * ftpClientPut - issue a PUT command and send data from input
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientPut(const char* inputfile, const char* path, char mode)
{
  return xfer(inputfile, path, FTP_CLIENT_FILE_WRITE, mode);
}

/*
 * deleteFtpClient - delete a file at remote
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientDelete(const char* fnm)
{
  char cmd[FTP_CLIENT_TEMP_BUFFER_SIZE];
  if ((strlen(fnm) + 7) > sizeof(cmd))
    return 0;
  sprintf(cmd, "DELE %s", fnm);
  if (!sendCommand(cmd, '2'))
    return 0;
  else
    return 1;
}

/*
 * ftpClientRename - rename a file at remote
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientRename(const char* src, const char* dst)
{
  char cmd[FTP_CLIENT_TEMP_BUFFER_SIZE];
  if (((strlen(src) + 7) > sizeof(cmd)) || ((strlen(dst) + 7) > sizeof(cmd)))
    return 0;
  sprintf(cmd, "RNFR %s", src);
  if (!sendCommand(cmd, '3'))
    return 0;
  sprintf(cmd, "RNTO %s", dst);
  if (!sendCommand(cmd, '2'))
    return 0;
  else
    return 1;
}

/*
 * ftpClientAccess - return a handle for a data stream
 *
 * return 1 if successful, 0 otherwise
 */
int
FtpClient::ftpClientAccess(const char* path,
                           int typ,
                           int mode,
                           NetBuf** nData)
{
  if ((path == NULL) && ((typ == FTP_CLIENT_FILE_WRITE) || (typ == FTP_CLIENT_FILE_READ))) {
    sprintf(m_nControl.response, "Missing path argument for file transfer\n");
    return 0;
  }
  char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
  sprintf(buf, "TYPE %c", mode);
  if (!sendCommand(buf, '2'))
    return 0;
  int dir;
  switch (typ) {
    case FTP_CLIENT_DIR: {
      strcpy(buf, "NLST");
      dir = FTP_CLIENT_READ;
    } break;

    case FTP_CLIENT_DIR_VERBOSE: {
      strcpy(buf, "LIST");
      dir = FTP_CLIENT_READ;
    } break;

    case FTP_CLIENT_FILE_READ: {
      strcpy(buf, "RETR");
      dir = FTP_CLIENT_READ;
    } break;

    case FTP_CLIENT_FILE_WRITE: {
      strcpy(buf, "STOR");
      dir = FTP_CLIENT_WRITE;
    } break;

    case FTP_CLIENT_MLSD: {
      strcpy(buf, "MLSD");
      dir = FTP_CLIENT_READ;
    } break;

    default: {
      sprintf(m_nControl.response, "Invalid open type %d\n", typ);
      return 0;
    }
  }

  if (path != NULL) {
    int i = strlen(buf);
    buf[i++] = ' ';
    if ((strlen(path) + i + 1) >= sizeof(buf))
      return 0;
    strcpy(&buf[i], path);
  }

  if (openPort(nData, mode, dir) == -1)
    return 0;
  if (!sendCommand(buf, '1')) {
    ftpClientClose(*nData);
    *nData = NULL;
    return 0;
  }
  if (m_nControl.cmode == FTP_CLIENT_ACTIVE) {
    if (!acceptConnection(*nData)) {
      ftpClientClose(*nData);
      *nData = NULL;
      m_nControl.data = NULL;
      return 0;
    }
  }
  return 1;
}

/*
 * ftpClientRead - read from a data connection
 */
int
FtpClient::ftpClientRead(void* buf, int max, NetBuf* nData)
{
  if (nData->dir != FTP_CLIENT_READ)
    return 0;
  int i = 0;
  if (nData->buf) {
    i = readLine(reinterpret_cast<char*>(buf), max, nData);
  } else {
    i = socketWait(nData);
    if (i != 1)
      return 0;
    i = recv(nData->handle, buf, max, 0);
  }
  if (i == -1)
    return 0;
  nData->xfered += i;
  if (nData->idlecb && nData->cbbytes) {
    nData->xfered1 += i;
    if (nData->xfered1 > nData->cbbytes) {
      if (nData->idlecb(nData, nData->xfered, nData->idlearg) == 0)
        return 0;
      nData->xfered1 = 0;
    }
  }
  return i;
}

/*
 * ftpClientWrite - write to a data connection
 */
int
FtpClient::ftpClientWrite(const void* buf, int len, NetBuf* nData)
{
  int i = 0;
  if (nData->dir != FTP_CLIENT_WRITE)
    return 0;
  if (nData->buf)
    i = writeLine(reinterpret_cast<const char*>(buf), len, nData);
  else {
    socketWait(nData);
    i = send(nData->handle, buf, len, 0);
  }
  if (i == -1)
    return 0;
  nData->xfered += i;
  if (nData->idlecb && nData->cbbytes) {
    nData->xfered1 += i;
    if (nData->xfered1 > nData->cbbytes) {
      nData->idlecb(nData, nData->xfered, nData->idlearg);
      nData->xfered1 = 0;
    }
  }
  return i;
}

/*
 * ftpClientClose - close a data connection
 */
int
FtpClient::ftpClientClose(NetBuf* nData)
{
  switch (nData->dir) {
    case FTP_CLIENT_CONTROL:
      if (nData->data) {
        nData->ctrl = NULL;
        ftpClientClose(nData->data);
      }

      closesocket(nData->handle);
      free(nData);

      return 0;

    case FTP_CLIENT_READ:
    case FTP_CLIENT_WRITE:
      if (nData->buf) {
        free(nData->buf);
      }

      shutdown(nData->handle, 2);
      closesocket(nData->handle);
      NetBuf* ctrl = nData->ctrl;
      free(nData);
      ctrl->data = NULL;

      if (ctrl && ctrl->response[0] != '4' && ctrl->response[0] != '5') {
        return (readResponse('2', ctrl));
      }

      return 1;
  }

  return 1;
}

// FtpClient*
// getFtpClient(void)
// {
//   if (!isInitilized) {
    // ftpClient_.ftpClientSite = ftpClientSite;
    // ftpClient_.ftpClientGetLastResponse = ftpClientGetLastResponse;
    // ftpClient_.ftpClientGetSysType = ftpClientGetSysType;
    // ftpClient_.ftpClientGetFileSize = ftpClientGetFileSize;
    // ftpClient_.ftpClientGetModDate = ftpClientGetModDate;
    // ftpClient_.ftpClientSetCallback = ftpClientSetCallback;
    // ftpClient_.ftpClientClearCallback = ftpClientClearCallback;
    // ftpClient_.ftpClientConnect = ftpClientConnect;
    // ftpClient_.ftpClientLogin = ftpClientLogin;
    // ftpClient_.ftpClientQuit = ftpClientQuit;
    // ftpClient_.ftpClientSetOptions = ftpClientSetOptions;
    // ftpClient_.ftpClientChangeDir = ftpClientChangeDir;
    // ftpClient_.ftpClientMakeDir = ftpClientMakeDir;
    // ftpClient_.ftpClientRemoveDir = ftpClientRemoveDir;
    // ftpClient_.ftpClientDir = ftpClientDir;
    // ftpClient_.ftpClientNlst = ftpClientNlst;
    // ftpClient_.ftpClientMlsd = ftpClientMlsd;
    // ftpClient_.ftpClientChangeDirUp = ftpClientChangeDirUp;
    // ftpClient_.ftpClientPwd = ftpClientPwd;
    // ftpClient_.ftpClientGet = ftpClientGet;
    // ftpClient_.ftpClientPut = ftpClientPut;
    // ftpClient_.ftpClientDelete = ftpClientDelete;
    // ftpClient_.ftpClientRename = ftpClientRename;
    // ftpClient_.ftpClientAccess = ftpClientAccess;
    // ftpClient_.ftpClientRead = ftpClientRead;
    // ftpClient_.ftpClientWrite = ftpClientWrite;
    // ftpClient_.ftpClientClose = ftpClientClose;
//     isInitilized = true;
//   }

//   return &ftpClient_;
// }