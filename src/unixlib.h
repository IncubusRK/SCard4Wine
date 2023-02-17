/*
 * WinSCard Unix library
 *
 * Copyright 2023 Romanov Konstantin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/unixlib.h"

#ifndef __unixlib_h__
#define __unixlib_h__


#ifndef LPCBYTE
    typedef const BYTE *LPCBYTE;
#endif

#define MAX_PCSCLITE_READERNAME            52

#ifdef WINE_UNIX_LIB
    typedef ULONG_PTR SCARDCONTEXT, *PSCARDCONTEXT, *LPSCARDCONTEXT;
    typedef ULONG_PTR SCARDHANDLE,  *PSCARDHANDLE,  *LPSCARDHANDLE;
    typedef unsigned long  DWORD_LITE;
    typedef unsigned long* LPDWORD_LITE;
#else
    #include "winscard.h"
    #ifndef SCARD_READERSTATE
        typedef SCARD_READERSTATEA SCARD_READERSTATE;
    #endif
    #ifdef __x86_64
        typedef unsigned long long  DWORD_LITE;
        typedef unsigned long long* LPDWORD_LITE;
    #else
        typedef unsigned long  DWORD_LITE;
        typedef unsigned long* LPDWORD_LITE;
    #endif    
#endif

#ifdef __APPLE__

#define DWORD_LITE	DWORD
#define LPDWORD_LITE	LPDWORD

#define SCARD_READERSTATE_LITE SCARD_READERSTATEA
#define LPSCARD_READERSTATE_LITE LPSCARD_READERSTATEA


#define SCARD_IO_REQUEST_LITE		SCARD_IO_REQUEST
#define PSCARD_IO_REQUEST_LITE		PSCARD_IO_REQUEST
#define LPSCARD_IO_REQUEST_LITE		LPSCARD_IO_REQUEST
#define LPCSCARD_IO_REQUEST_LITE	LPCSCARD_IO_REQUEST

#else

#undef MAX_ATR_SIZE
#define MAX_ATR_SIZE			33	/**< Maximum ATR size */

typedef struct
{
        const char *szReader;
        void *pvUserData;
        DWORD_LITE dwCurrentState;
        DWORD_LITE dwEventState;
        DWORD_LITE cbAtr;
        unsigned char rgbAtr[MAX_ATR_SIZE];
}
SCARD_READERSTATE_LITE;

typedef SCARD_READERSTATE_LITE *LPSCARD_READERSTATE_LITE;

typedef struct
{
        DWORD_LITE dwProtocol;       /**< Protocol identifier */
        DWORD_LITE cbPciLength;      /**< Protocol Control Inf Length */
}
SCARD_IO_REQUEST_LITE, *PSCARD_IO_REQUEST_LITE, *LPSCARD_IO_REQUEST_LITE;

typedef const SCARD_IO_REQUEST_LITE *LPCSCARD_IO_REQUEST_LITE;

#endif

enum unix_funcs
{
    unix_SCardEstablishContext,
    unix_SCardReleaseContext,
    unix_SCardIsValidContext,
    unix_SCardConnect,
    unix_SCardReconnect,
    unix_SCardDisconnect,
    unix_SCardBeginTransaction,
    unix_SCardEndTransaction,
    unix_SCardStatus,
    unix_SCardGetStatusChange,
    unix_SCardControl,
    unix_SCardTransmit,
    unix_SCardListReaderGroups,
    unix_SCardListReaders,
    unix_SCardFreeMemory,
    unix_SCardCancel,
    unix_SCardGetAttrib,    
    unix_SCardSetAttrib,
    unix_process_attach,
    unix_process_detach,
};

struct SCardEstablishContext_params
{ 
    DWORD_LITE dwScope;
    LPCVOID pvReserved1;
    LPCVOID pvReserved2;
    LPSCARDCONTEXT phContext;
};

struct SCardReleaseContext_params
{ 
    SCARDCONTEXT hContext;
};

struct SCardIsValidContext_params
{ 
    SCARDCONTEXT hContext;
};

struct SCardConnect_params
{
    SCARDCONTEXT hContext;
    LPCSTR szReader;
    DWORD_LITE dwShareMode;
    DWORD_LITE dwPreferredProtocols;
    LPSCARDHANDLE phCard;
    LPDWORD_LITE pdwActiveProtocol;
};

struct SCardReconnect_params
{
    SCARDHANDLE hCard;
    DWORD_LITE dwShareMode;
    DWORD_LITE dwPreferredProtocols;
    DWORD_LITE dwInitialization;
    LPDWORD_LITE pdwActiveProtocol;
};

struct SCardDisconnect_params
{
    SCARDHANDLE hCard;
    DWORD_LITE dwDisposition;
};

struct SCardBeginTransaction_params 
{
    SCARDHANDLE hCard;
};

struct SCardEndTransaction_params
{
    SCARDHANDLE hCard;
    DWORD_LITE dwDisposition;
};

struct SCardStatus_params 
{
    SCARDHANDLE hCard;
    LPSTR mszReaderName;
    LPDWORD_LITE pcchReaderLen;
    LPDWORD_LITE pdwState;
    LPDWORD_LITE pdwProtocol;
    LPBYTE pbAtr;
    LPDWORD_LITE pcbAtrLen;
};

struct SCardGetStatusChange_params 
{
    SCARDCONTEXT hContext;
    DWORD_LITE dwTimeout;
    SCARD_READERSTATE_LITE *rgReaderStates;
    DWORD_LITE cReaders;
};

struct SCardControl_params
{
    SCARDHANDLE hCard;
    DWORD_LITE dwControlCode;
    LPCVOID pbSendBuffer;
    DWORD_LITE cbSendLength;
    LPVOID pbRecvBuffer;
    DWORD_LITE cbRecvLength;
    LPDWORD_LITE lpBytesReturned;
};

struct SCardTransmit_params
{
    SCARDHANDLE hCard;
    const SCARD_IO_REQUEST_LITE *pioSendPci;
    LPCBYTE pbSendBuffer;
    DWORD_LITE cbSendLength;
    SCARD_IO_REQUEST_LITE *pioRecvPci;
    LPBYTE pbRecvBuffer;
    LPDWORD_LITE pcbRecvLength;
};

struct SCardListReaderGroups_params
{
    SCARDCONTEXT hContext;
    LPSTR mszGroups;
    LPDWORD_LITE pcchGroups;
};

struct SCardListReaders_params 
{
    SCARDCONTEXT hContext;
    LPCSTR mszGroups;
    LPSTR mszReaders;
    LPDWORD_LITE pcchReaders;
};

struct SCardFreeMemory_params 
{
    SCARDCONTEXT hContext;
    LPCVOID pvMem;
};

struct SCardCancel_params 
{
    SCARDCONTEXT hContext;
};

struct SCardGetAttrib_params 
{
    SCARDHANDLE hCard;
    DWORD_LITE dwAttrId;
    LPBYTE pbAttr;
    LPDWORD_LITE pcbAttrLen;
};

struct SCardSetAttrib_params 
{
    SCARDHANDLE hCard;
    DWORD_LITE dwAttrId;
    const LPBYTE pbAttr;
    DWORD_LITE cbAttrLen;
};

#endif