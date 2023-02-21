/*
 * WinScard Unix library
 *
 * Copyright 2023 Konstantin Romanov
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>

#define __user
#include "unixlib.h"

LONG SCardEstablishContext(DWORD_LITE dwScope, LPCVOID pvReserved1, LPCVOID pvReserved2, SCARDCONTEXT *phContext);
LONG SCardReleaseContext(SCARDCONTEXT hContext);
LONG SCardIsValidContext(SCARDCONTEXT hContext);
LONG SCardConnect(SCARDCONTEXT hContext, LPCSTR szReader, DWORD_LITE dwShareMode, DWORD_LITE dwPreferredProtocols, SCARDHANDLE *phCard, DWORD_LITE *pdwActiveProtocol);
LONG SCardReconnect(SCARDHANDLE hCard, DWORD_LITE dwShareMode, DWORD_LITE dwPreferredProtocols, DWORD_LITE dwInitialization, DWORD_LITE *pdwActiveProtocol);
LONG SCardDisconnect(SCARDHANDLE hCard, DWORD_LITE dwDisposition);
LONG SCardBeginTransaction(SCARDHANDLE hCard);
LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD_LITE dwDisposition);
LONG SCardStatus(SCARDHANDLE hCard, LPSTR mszReaderName, DWORD_LITE *pcchReaderLen, DWORD_LITE *pdwState, DWORD_LITE *pdwProtocol, LPBYTE pbAtr, DWORD_LITE *pcbAtrLen);
LONG SCardGetStatusChange(SCARDCONTEXT hContext, DWORD_LITE dwTimeout, SCARD_READERSTATE_LITE *rgReaderStates, DWORD_LITE cReaders);
LONG SCardControl(SCARDHANDLE hCard, DWORD_LITE dwControlCode, LPCVOID pbSendBuffer, DWORD_LITE cbSendLength, LPVOID pbRecvBuffer, DWORD_LITE cbRecvLength, DWORD_LITE *lpBytesReturned);
LONG SCardTransmit(SCARDHANDLE hCard, const SCARD_IO_REQUEST_LITE *pioSendPci, LPCBYTE pbSendBuffer, DWORD_LITE cbSendLength, SCARD_IO_REQUEST_LITE *pioRecvPci, LPBYTE pbRecvBuffer, DWORD_LITE *pcbRecvLength);
LONG SCardListReaderGroups(SCARDCONTEXT hContext, LPSTR mszGroups, DWORD_LITE *pcchGroups);
LONG SCardListReaders(SCARDCONTEXT hContext, LPCSTR mszGroups, LPSTR mszReaders, DWORD_LITE *pcchReaders);
LONG SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem);
LONG SCardCancel(SCARDCONTEXT hContext);
LONG SCardGetAttrib(SCARDHANDLE hCard, DWORD_LITE dwAttrId, LPBYTE pbAttr, DWORD_LITE *pcbAttrLen);
LONG SCardSetAttrib(SCARDHANDLE hCard, DWORD_LITE dwAttrId, LPCBYTE pbAttr, DWORD_LITE cbAttrLen);

#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(winscard);
WINE_DECLARE_DEBUG_CHANNEL(winediag);

static void* g_pcscliteHandle = NULL;

#define MAKE_FUNCPTR(f) static typeof(f) * p##f
   MAKE_FUNCPTR(SCardEstablishContext);
   MAKE_FUNCPTR(SCardReleaseContext);
   MAKE_FUNCPTR(SCardIsValidContext);
   MAKE_FUNCPTR(SCardConnect);
   MAKE_FUNCPTR(SCardReconnect);
   MAKE_FUNCPTR(SCardDisconnect);
   MAKE_FUNCPTR(SCardBeginTransaction);
   MAKE_FUNCPTR(SCardEndTransaction);
   MAKE_FUNCPTR(SCardStatus);
   MAKE_FUNCPTR(SCardGetStatusChange);
   MAKE_FUNCPTR(SCardControl);
   MAKE_FUNCPTR(SCardTransmit);
   MAKE_FUNCPTR(SCardListReaderGroups);
   MAKE_FUNCPTR(SCardListReaders);
   MAKE_FUNCPTR(SCardFreeMemory);
   MAKE_FUNCPTR(SCardCancel);
   MAKE_FUNCPTR(SCardGetAttrib);    
   MAKE_FUNCPTR(SCardSetAttrib);
#undef MAKE_FUNCPTR

static BOOL load_pcsclite(void);

static LONG pcsclite_process_attach( void *args )
{
   if (!load_pcsclite()) return SCARD_F_INTERNAL_ERROR;
   return SCARD_S_SUCCESS;
}

static LONG pcsclite_process_detach( void *args )
{
    if (g_pcscliteHandle) dlclose( g_pcscliteHandle );
    g_pcscliteHandle = NULL;
    return SCARD_S_SUCCESS;
}

static LONG pcsclite_SCardEstablishContext( void *args )
{
    struct SCardEstablishContext_params *params = args;
    if (!pSCardEstablishContext) return SCARD_F_INTERNAL_ERROR;
    return pSCardEstablishContext( params->dwScope, params->pvReserved1, params->pvReserved2, params->phContext );
}

static LONG pcsclite_SCardReleaseContext( void *args )
{
    struct SCardReleaseContext_params *params = args;
    if (!pSCardReleaseContext) return SCARD_F_INTERNAL_ERROR;
    return pSCardReleaseContext( params->hContext );
}

static LONG pcsclite_SCardIsValidContext( void *args )
{
    struct SCardIsValidContext_params *params = args;
    if (!pSCardIsValidContext) return SCARD_F_INTERNAL_ERROR;
    return pSCardIsValidContext( params->hContext );
}

static LONG pcsclite_SCardConnect( void *args )
{
    struct SCardConnect_params *params = args;
    if (!pSCardConnect) return SCARD_F_INTERNAL_ERROR;
    return pSCardConnect( params->hContext, params->szReader, params->dwShareMode, params->dwPreferredProtocols,
        params->phCard, params->pdwActiveProtocol );
}

static LONG pcsclite_SCardReconnect( void *args )
{
   struct SCardReconnect_params *params = args;
   if (!pSCardReconnect) return SCARD_F_INTERNAL_ERROR;
   return pSCardReconnect( params->hCard, params->dwShareMode, params->dwPreferredProtocols, params->dwInitialization, params->pdwActiveProtocol );
}

static LONG pcsclite_SCardDisconnect( void *args )
{
   struct SCardDisconnect_params *params = args;
   if (!pSCardDisconnect) return SCARD_F_INTERNAL_ERROR;
   return pSCardDisconnect( params->hCard, params->dwDisposition );
}

static LONG pcsclite_SCardBeginTransaction( void *args )
{
   struct SCardBeginTransaction_params *params = args;
   if (!pSCardBeginTransaction) return SCARD_F_INTERNAL_ERROR;
   return pSCardBeginTransaction( params->hCard );
}

static LONG pcsclite_SCardEndTransaction( void *args )
{
   struct SCardEndTransaction_params *params = args;
   if (!pSCardEndTransaction) return SCARD_F_INTERNAL_ERROR;
   return pSCardEndTransaction( params->hCard, params->dwDisposition );
}

static LONG pcsclite_SCardStatus( void *args )
{
   struct SCardStatus_params *params = args;
   if (!pSCardStatus) return SCARD_F_INTERNAL_ERROR;
   return pSCardStatus( params->hCard, params->mszReaderName, params->pcchReaderLen, params->pdwState, params->pdwProtocol, 
    params->pbAtr, params->pcbAtrLen );
}

static LONG pcsclite_SCardGetStatusChange( void *args )
{
   struct SCardGetStatusChange_params *params = args;
   if (!pSCardGetStatusChange) return SCARD_F_INTERNAL_ERROR;
   return pSCardGetStatusChange( params->hContext, params->dwTimeout, params->rgReaderStates, params->cReaders );
}

static LONG pcsclite_SCardControl( void *args )
{
   struct SCardControl_params *params = args;
   if (!pSCardControl) return SCARD_F_INTERNAL_ERROR;
   return pSCardControl( params->hCard, params->dwControlCode, params->pbSendBuffer, params->cbSendLength,
    params->pbRecvBuffer, params->cbRecvLength, params->lpBytesReturned );
}

static LONG pcsclite_SCardTransmit( void *args )
{
   struct SCardTransmit_params *params = args;
   if (!pSCardTransmit) return SCARD_F_INTERNAL_ERROR;
   return pSCardTransmit( params->hCard, params->pioSendPci, params->pbSendBuffer, params->cbSendLength,
    params->pioRecvPci, params->pbRecvBuffer, params->pcbRecvLength );
}

static LONG pcsclite_SCardListReaderGroups( void *args )
{
   struct SCardListReaderGroups_params *params = args;
   if (!pSCardListReaderGroups) return SCARD_F_INTERNAL_ERROR;
   return pSCardListReaderGroups( params->hContext, params->mszGroups, params->pcchGroups );
}

static LONG pcsclite_SCardListReaders( void *args )
{
   struct SCardListReaders_params *params = args;
   if (!pSCardListReaders) return SCARD_F_INTERNAL_ERROR;
   return pSCardListReaders( params->hContext, params->mszGroups, params->mszReaders, params->pcchReaders );
}

static LONG pcsclite_SCardFreeMemory( void *args )
{
   struct SCardFreeMemory_params *params = args;
   if (!pSCardFreeMemory) return SCARD_F_INTERNAL_ERROR;
   return pSCardFreeMemory( params->hContext, params->pvMem );
}

static LONG pcsclite_SCardCancel( void *args )
{
   struct SCardCancel_params *params = args;
   if (!pSCardCancel) return SCARD_F_INTERNAL_ERROR;
   return pSCardCancel( params->hContext );
}

static LONG pcsclite_SCardGetAttrib( void *args )
{
   struct SCardGetAttrib_params *params = args;
   if (!pSCardGetAttrib) return SCARD_F_INTERNAL_ERROR;
   return pSCardGetAttrib( params->hCard, params->dwAttrId, params->pbAttr, params->pcbAttrLen );
}

static LONG pcsclite_SCardSetAttrib( void *args )
{
   struct SCardSetAttrib_params *params = args;
   if (!pSCardSetAttrib) return SCARD_F_INTERNAL_ERROR;
   return pSCardSetAttrib( params->hCard, params->dwAttrId, params->pbAttr, params->cbAttrLen );
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
   pcsclite_SCardEstablishContext,
   pcsclite_SCardReleaseContext,
   pcsclite_SCardIsValidContext,
   pcsclite_SCardConnect,
   pcsclite_SCardReconnect,
   pcsclite_SCardDisconnect,
   pcsclite_SCardBeginTransaction,
   pcsclite_SCardEndTransaction,
   pcsclite_SCardStatus,
   pcsclite_SCardGetStatusChange,
   pcsclite_SCardControl,
   pcsclite_SCardTransmit,
   pcsclite_SCardListReaderGroups,
   pcsclite_SCardListReaders,
   pcsclite_SCardFreeMemory,
   pcsclite_SCardCancel,
   pcsclite_SCardGetAttrib,    
   pcsclite_SCardSetAttrib,
   pcsclite_process_attach,
   pcsclite_process_detach,
};

static BOOL load_pcsclite(void)
{
   if(!g_pcscliteHandle)
   {
        /* try to load pcsc-lite */
#ifdef __APPLE__
        g_pcscliteHandle = dlopen("/System/Library/Frameworks/PCSC.framework/PCSC",RTLD_LAZY | RTLD_GLOBAL);
#else
        g_pcscliteHandle = dlopen("libpcsclite.so",RTLD_LAZY | RTLD_GLOBAL);
        if(!g_pcscliteHandle)
        {
            /* error occured. Trying libpcsclite.so.1*/
            g_pcscliteHandle = dlopen("libpcsclite.so.1",RTLD_LAZY | RTLD_GLOBAL);
        }
        if(!g_pcscliteHandle)
        {
#ifdef __LP64__
            g_pcscliteHandle = dlopen("/lib/x86_64-linux-gnu/libpcsclite.so.1",RTLD_LAZY | RTLD_GLOBAL);
#else
            g_pcscliteHandle = dlopen("/lib/i386-linux-gnu/libpcsclite.so.1",RTLD_LAZY | RTLD_GLOBAL);
#endif    
        }
         if(!g_pcscliteHandle)
        {
#ifdef __LP64__
            g_pcscliteHandle = dlopen("/usr/lib/x86_64-linux-gnu/libpcsclite.so.1",RTLD_LAZY | RTLD_GLOBAL);
#else
            g_pcscliteHandle = dlopen("/usr/lib/i386-linux-gnu/libpcsclite.so.1",RTLD_LAZY | RTLD_GLOBAL);
#endif    
        }
#endif
      TRACE("g_pcscliteHandle: %p\n", g_pcscliteHandle);
        if(!g_pcscliteHandle)
        {
            /* error occured*/
#ifdef __APPLE__
         ERR_(winediag)( "loading PCSC framework failed, scard support will be disabled. Error: %s\n", debugstr_a(dlerror()) );
#else
         ERR_(winediag)( "failed to load libpcsclite.so, scard support will be disabled. Error: %s\n", debugstr_a(dlerror()) );
#endif
         return FALSE;
        }
   }

#define LOAD_FUNCPTR(f) \
   if (!(p##f = dlsym( g_pcscliteHandle, #f ))) \
   { \
      ERR( "failed to load %s\n", #f ); \
      goto fail; \
   }
   LOAD_FUNCPTR( SCardEstablishContext)
   LOAD_FUNCPTR( SCardReleaseContext)
   LOAD_FUNCPTR( SCardIsValidContext)
   LOAD_FUNCPTR( SCardConnect)
   LOAD_FUNCPTR( SCardReconnect)
   LOAD_FUNCPTR( SCardDisconnect)
   LOAD_FUNCPTR( SCardBeginTransaction)
   LOAD_FUNCPTR( SCardEndTransaction)
   LOAD_FUNCPTR( SCardStatus)
   LOAD_FUNCPTR( SCardGetStatusChange)
   LOAD_FUNCPTR( SCardControl)
   LOAD_FUNCPTR( SCardTransmit)
   LOAD_FUNCPTR( SCardListReaderGroups)
   LOAD_FUNCPTR( SCardListReaders)
   LOAD_FUNCPTR( SCardFreeMemory)
   LOAD_FUNCPTR( SCardCancel)
   LOAD_FUNCPTR( SCardGetAttrib)    
   LOAD_FUNCPTR( SCardSetAttrib)
#undef LOAD_FUNCPTR
   return TRUE;
fail:
   dlclose( g_pcscliteHandle );
   g_pcscliteHandle = NULL;
   return FALSE; 
}
