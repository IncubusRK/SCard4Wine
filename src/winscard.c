/*
 * Copyright (C) 2007 Mounir IDRASSI  (mounir.idrassi@idrix.fr, for IDRIX)
 * Copyright (C) 2023 Konstantin Romanov
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
 */
#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "ntuser.h"
#include "wine/unixlib.h"
#include "wine/debug.h"

#include "winscard.h"
#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(winscard);

SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, 8 };
SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, 8 };
SCARD_IO_REQUEST g_rgSCardRawPci = { SCARD_PROTOCOL_RAW, 8 };

#define PCSCLITE_SCARD_PROTOCOL_RAW    0x00000004

static DWORD_LITE
ms_proto2lite_proto(DWORD dwProtocol)
{
    if (dwProtocol & SCARD_PROTOCOL_RAW)
    {
        dwProtocol ^= SCARD_PROTOCOL_RAW;
        dwProtocol |= PCSCLITE_SCARD_PROTOCOL_RAW;
    }
    return (DWORD_LITE)dwProtocol;
}

static DWORD
lite_proto2ms_proto(DWORD_LITE dwProtocol)
{
    if (dwProtocol & PCSCLITE_SCARD_PROTOCOL_RAW)
    {
        dwProtocol ^= PCSCLITE_SCARD_PROTOCOL_RAW;
        dwProtocol |= SCARD_PROTOCOL_RAW;
    }
    return (DWORD)dwProtocol;
}

HANDLE g_startedEvent = NULL;


#define WINSCARD_CALL( func, params ) WINE_UNIX_CALL( unix_ ## func, params )

BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    BOOL is_wow64=FALSE;
    TRACE("%p, %#lx, %p\n", hinstDLL, fdwReason, lpvReserved);
    IsWow64Process(GetCurrentProcess(), &is_wow64);
    if (is_wow64)
        WARN("Running in wow64 process. libpcsclite1:i386 must be installed\n");

    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(hinstDLL);
            __wine_init_unix_call();
            if(!WINSCARD_CALL( process_attach, NULL )) 
                WARN("Winscard loading failed.");
            g_startedEvent = CreateEventA(NULL,TRUE,TRUE,NULL);
            break;
        }
        case DLL_PROCESS_DETACH:
        {
            WINSCARD_CALL( process_detach, NULL );
            CloseHandle(g_startedEvent);
            break;
        }
    }
    return TRUE;
}

static LPVOID SCardAllocate(DWORD dwLength)
{
    if(!dwLength)
        return NULL;
    return HeapAlloc(GetProcessHeap(), 0, dwLength);
}

static void SCardFree(LPVOID ptr)
{
    HeapFree(GetProcessHeap(), 0, ptr);
}

/*
 * Convert a wide-char multi-string to an ANSI multi-string
 */
static LONG ConvertListToANSI(LPCWSTR szListW,LPSTR* pszListA,LPDWORD pdwLength)
{
    if(!szListW)
    {
        *pszListA = NULL;
        *pdwLength = 0;
    }
    else if(!*szListW) /* empty multi-string case */
    {
        *pdwLength = 2;
        *pszListA = (LPSTR) SCardAllocate(2);
        if(!*pszListA)
            return SCARD_E_NO_MEMORY;
        *pszListA[0] = '\0';
        *pszListA[1] = '\0';
    }
    else
    {
        LPSTR szStr = NULL;
        int wlen = 0, alen = 0, totallen = 0;    
        LPCWSTR szListWPtr = szListW;
        
        /* compute size of wide-char multi-string */
        while (*szListWPtr)
        {
            wlen = lstrlenW (szListWPtr) + 1;

            totallen += wlen;
            szListWPtr += wlen;
        }
        
        totallen++;
        
        alen = WideCharToMultiByte(CP_ACP, 0, szListW, totallen, NULL, 0, NULL, NULL);
        if (alen == 0)
            return SCARD_F_INTERNAL_ERROR;
        
        /* allocate memory */
        szStr = (LPSTR) SCardAllocate (alen);
        if(!szStr)
            return SCARD_E_NO_MEMORY;
        
        /* perform the conversion */
        alen = WideCharToMultiByte(CP_ACP, 0, szListW, totallen, szStr, alen, NULL, NULL);
        if (alen == 0)
        {
            SCardFree (szStr);
            return SCARD_F_INTERNAL_ERROR;
        }
        
        *pszListA = szStr;
        *pdwLength = alen;
    }
    
    return SCARD_S_SUCCESS;
}

/*
 * Convert a ANSII multi-string to a wide-char multi-string
 */
static LONG ConvertListToWideChar(LPCSTR szListA,LPWSTR* pszListW,LPDWORD pdwLength)
{
    if(!szListA)
    {
        *pszListW = NULL;
        *pdwLength = 0;
    }
    else if(!*szListA) /* empty multi-string case */
    {
        *pdwLength = 2;
        *pszListW = (LPWSTR) SCardAllocate(2*sizeof(WCHAR));
        if(!*pszListW)
            return SCARD_E_NO_MEMORY;
        *pszListW[0] = '\0';
        *pszListW[1] = '\0';
    }
    else
    {
        LPWSTR szStr = NULL;
        int wlen = 0, alen = 0, totallen = 0;    
        LPCSTR szListAPtr = szListA;
        
        /* compute size of ANSI multi-string */
        while (*szListAPtr)
        {
            alen = lstrlenA (szListAPtr) + 1;

            totallen += alen;
            szListAPtr += alen;
        }
        
        totallen++;
        
        wlen = MultiByteToWideChar(CP_ACP, 0, szListA, totallen, NULL, 0);
        if (wlen == 0)
            return SCARD_F_INTERNAL_ERROR;
        
        /* allocate memory */
        szStr = (LPWSTR) SCardAllocate (wlen * sizeof (WCHAR));
        if(!szStr)
            return SCARD_E_NO_MEMORY;
        
        /* perform the conversion */
        wlen = MultiByteToWideChar(CP_ACP, 0, szListA, totallen, szStr, wlen);
        if (wlen == 0)
        {
            SCardFree (szStr);
            return SCARD_F_INTERNAL_ERROR;
        }
        
        *pszListW = szStr;
        *pdwLength = wlen;
    }
    
    return SCARD_S_SUCCESS;
}


/*
 * translate PCSC-lite errors to equivalent MS ones
 * Actually, the only difference is for SCARD_W_INSERTED_CARD(0x8010006A) and
 * SCARD_E_UNSUPPORTED_FEATURE (0x8010001F)
 */

#define PCSCLITE_SCARD_W_INSERTED_CARD                      0x8010006A
#define PCSCLITE_SCARD_E_UNSUPPORTED_FEATURE                0x8010001F

static LONG TranslateToWin32(LONG lRet)
{
    if(lRet == PCSCLITE_SCARD_E_UNSUPPORTED_FEATURE)
        return SCARD_E_UNSUPPORTED_FEATURE;
    else if(lRet == PCSCLITE_SCARD_W_INSERTED_CARD)
        return SCARD_F_UNKNOWN_ERROR; /* FIXME: is there a better WIN32 error code */
    else
        return lRet;
}
    

/*
  * events functions
  */
  
HANDLE WINAPI SCardAccessNewReaderEvent(void)
{
    FIXME("stub\n");
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}

HANDLE WINAPI SCardAccessStartedEvent()
{
    return g_startedEvent;
}

void WINAPI SCardReleaseStartedEvent(HANDLE hStartedEventHandle)
{
    /* do nothing  because we don't implement yet reference couting for the event handle*/
}

LONG WINAPI SCardFreeMemory( SCARDCONTEXT hContext,LPCVOID pvMem)
{
    if(pvMem)
        SCardFree((void*) pvMem);
    return SCARD_S_SUCCESS;
}

/*
 *  smart cards database functions. Almost all of them are stubs
 */

LONG WINAPI SCardListCardsA(
        SCARDCONTEXT hContext,
        const BYTE* pbAtr,
        LPCGUID rgquidInterfaces,
        DWORD cguidInterfaceCount,
        LPSTR mszCards,
        LPDWORD pcchCards)
{
    TRACE("0x%08X, %p, %p, %#lx, %p, %p - stub\n",(unsigned int) hContext,pbAtr,rgquidInterfaces,cguidInterfaceCount,mszCards,pcchCards);
    
    /* we simuate the fact that no cards are enregistred on the system by returning an empty multi-string*/
    if(!pcchCards)
        return SCARD_E_INVALID_PARAMETER;
    else if(!mszCards)
        *pcchCards = 2;    
    else
    {
        DWORD len = *pcchCards;
        if(SCARD_AUTOALLOCATE == len)
        {
            if(!mszCards)
                return SCARD_E_INVALID_PARAMETER;
            else
            {
                /* allocate memory */
                LPSTR* pmszCards = (LPSTR*) mszCards;
                LPSTR szResult = (LPSTR) SCardAllocate(2);
                if(!szResult)
                    return SCARD_E_NO_MEMORY;
                szResult[0] = '\0';
                szResult[1] = '\0';
                *pcchCards = 2;
                *pmszCards = szResult;
            }
        }
        else
        {
            if(len < 2)
            {
                *pcchCards = 2;
                return SCARD_E_INSUFFICIENT_BUFFER;
            }
            else
            {
                /* return a empty multi string */
                mszCards[0] = '\0';
                mszCards[1] = '\0';
                *pcchCards = 2;
            }                
        }
    }
    
    return SCARD_S_SUCCESS;
}

LONG WINAPI SCardListCardsW(
          SCARDCONTEXT hContext,
          const BYTE* pbAtr,
          LPCGUID rgquidInterfaces,
          DWORD cguidInterfaceCount,
      LPWSTR mszCards,
      LPDWORD pcchCards)
{
    TRACE("0x%08X, %p, %p, %#lx, %p, %p - stub\n",(unsigned int)hContext,pbAtr,rgquidInterfaces,cguidInterfaceCount,mszCards,pcchCards);
    
    /* we simuate the fact that no cards are enregistred on the system by returning an empty multi-string*/    
    if(!pcchCards)
        return SCARD_E_INVALID_PARAMETER;
    else if(!mszCards)
        *pcchCards = 2;
    else
    {
        DWORD len = *pcchCards;
        if(SCARD_AUTOALLOCATE == len)
        {
            if(!mszCards)
                return SCARD_E_INVALID_PARAMETER;
            else
            {
                /* allocate memory */
                LPWSTR* pmszCards = (LPWSTR*) mszCards;
                LPWSTR szResult = (LPWSTR) SCardAllocate(2*sizeof(WCHAR));
                if(!szResult)
                    return SCARD_E_NO_MEMORY;
                szResult[0] = '\0';
                szResult[1] = '\0';
                *pcchCards = 2;
                *pmszCards = szResult;
            }
        }
        else
        {
            if(len < 2)
            {
                *pcchCards = 2;
                return SCARD_E_INSUFFICIENT_BUFFER;
            }
            else
            {
                /* return a empty multi string */
                mszCards[0] = '\0';
                mszCards[1] = '\0';
                *pcchCards = 2;
            }                
        }
    }
    
    return SCARD_S_SUCCESS;    
}
    
LONG WINAPI SCardListInterfacesA(
        SCARDCONTEXT hContext,
        LPCSTR szCard,
        LPGUID pguidInterfaces,
        LPDWORD pcguidInterfaces)
{
    FIXME("0x%08X %s %p %p - stub\n",(unsigned int)hContext,debugstr_a(szCard),pguidInterfaces,pcguidInterfaces);
    
    return SCARD_E_UNKNOWN_CARD;
}

LONG WINAPI SCardListInterfacesW(
        SCARDCONTEXT hContext,
        LPCWSTR szCard,
        LPGUID pguidInterfaces,
        LPDWORD pcguidInterfaces)
{    
    FIXME("0x%08X %s %p %p - stub\n",(unsigned int)hContext,debugstr_w(szCard),pguidInterfaces,pcguidInterfaces);
    
    return SCARD_E_UNKNOWN_CARD;    
}
  
LONG WINAPI SCardGetProviderIdA(
    SCARDCONTEXT hContext,
    LPCSTR szCard,
    LPGUID pguidProviderId)
{
    FIXME("0x%08X %s %p - stub\n",(unsigned int)hContext,debugstr_a(szCard),pguidProviderId);
    
    if(!pguidProviderId)
        return SCARD_E_INVALID_PARAMETER;
    
    return SCARD_E_UNKNOWN_CARD;
}
    
LONG WINAPI SCardGetProviderIdW(
    SCARDCONTEXT hContext,
    LPCWSTR szCard,
    LPGUID pguidProviderId)
{
    FIXME("0x%08X %s %p - stub\n",(unsigned int)hContext,debugstr_w(szCard),pguidProviderId);
    
    if(!pguidProviderId)
        return SCARD_E_INVALID_PARAMETER;
    
    return SCARD_E_UNKNOWN_CARD;    
}
    
LONG WINAPI SCardGetCardTypeProviderNameA(
    SCARDCONTEXT hContext,
    LPCSTR szCardName,
    DWORD dwProviderId,
    LPSTR szProvider,
    LPDWORD pcchProvider)
{
    FIXME("0x%08X %s %#lx %p %p - stub\n",(unsigned int)hContext,debugstr_a(szCardName),dwProviderId,szProvider,pcchProvider);
    
    return SCARD_E_UNKNOWN_CARD;
}
    
LONG WINAPI SCardGetCardTypeProviderNameW(
    SCARDCONTEXT hContext,
    LPCWSTR szCardName,
    DWORD dwProviderId,
    LPWSTR szProvider,
    LPDWORD pcchProvider)
{
    FIXME("0x%08X %s %#lx %p %p - stub\n",(unsigned int)hContext,debugstr_w(szCardName),dwProviderId,szProvider,pcchProvider);
    
    return SCARD_E_UNKNOWN_CARD;    
}


LONG WINAPI SCardIntroduceReaderGroupA(
    SCARDCONTEXT hContext,
    LPCSTR szGroupName)
{
    FIXME("0x%08X %s - stub\n",(unsigned int)hContext,debugstr_a(szGroupName));
    
    return SCARD_F_UNKNOWN_ERROR;
}
    
LONG WINAPI SCardIntroduceReaderGroupW(
    SCARDCONTEXT hContext,
    LPCWSTR szGroupName)
{
    FIXME("0x%08X %s - stub\n",(unsigned int)hContext,debugstr_w(szGroupName));
    
    return SCARD_F_UNKNOWN_ERROR;    
}

LONG WINAPI SCardForgetReaderGroupA(
    SCARDCONTEXT hContext,
    LPCSTR szGroupName)
{
    FIXME("0x%08X %s - stub\n",(unsigned int)hContext,debugstr_a(szGroupName));
    
    return SCARD_S_SUCCESS;    
}
    
LONG WINAPI SCardForgetReaderGroupW(
    SCARDCONTEXT hContext,
    LPCWSTR szGroupName)
{
    FIXME("0x%08X %s - stub\n",(unsigned int)hContext,debugstr_w(szGroupName));
    
    return SCARD_S_SUCCESS;        
}


LONG WINAPI SCardIntroduceReaderA(
    SCARDCONTEXT hContext,
    LPCSTR szReaderName,
    LPCSTR szDeviceName)
{
    FIXME("0x%08X %s %s - stub\n",(unsigned int)hContext,debugstr_a(szReaderName),debugstr_a(szDeviceName));
    
    return SCARD_F_UNKNOWN_ERROR;
}
    
LONG WINAPI SCardIntroduceReaderW(
    SCARDCONTEXT hContext,
    LPCWSTR szReaderName,
    LPCWSTR szDeviceName)
{
    FIXME("0x%08X %s %s - stub\n",(unsigned int)hContext,debugstr_w(szReaderName),debugstr_w(szDeviceName));
    
    return SCARD_F_UNKNOWN_ERROR;    
}

LONG WINAPI SCardForgetReaderA(
    SCARDCONTEXT hContext,
    LPCSTR szReaderName)
{
    FIXME("0x%08X %s - stub\n",(unsigned int)hContext,debugstr_a(szReaderName));
    
    return SCARD_S_SUCCESS;        
}


LONG WINAPI SCardForgetReaderW(
    SCARDCONTEXT hContext,
    LPCWSTR szReaderName)
{
    FIXME("0x%08X %s - stub\n",(unsigned int)hContext,debugstr_w(szReaderName));
    
    return SCARD_S_SUCCESS;    
}

LONG WINAPI SCardAddReaderToGroupA(
    SCARDCONTEXT hContext,
    LPCSTR szReaderName,
    LPCSTR szGroupName)
{
    FIXME("0x%08X %s %s - stub\n",(unsigned int) hContext, debugstr_a( szReaderName), debugstr_a(szGroupName));
    
    return SCARD_F_UNKNOWN_ERROR;
}
    
LONG WINAPI SCardAddReaderToGroupW(
    SCARDCONTEXT hContext,
    LPCWSTR szReaderName,
    LPCWSTR szGroupName)
{
    FIXME("0x%08X %s %s - stub\n",(unsigned int) hContext, debugstr_w( szReaderName), debugstr_w(szGroupName));
    
    return SCARD_F_UNKNOWN_ERROR;    
}

LONG WINAPI SCardRemoveReaderFromGroupA(
    SCARDCONTEXT hContext,
    LPCSTR szReaderName,
    LPCSTR szGroupName)
{
    FIXME("0x%08X %s %s - stub\n",(unsigned int) hContext, debugstr_a( szReaderName), debugstr_a(szGroupName));
    
    return SCARD_S_SUCCESS;    
}
    
LONG WINAPI SCardRemoveReaderFromGroupW(
    SCARDCONTEXT hContext,
    LPCWSTR szReaderName,
    LPCWSTR szGroupName)
{
    FIXME("0x%08X %s %s - stub\n",(unsigned int) hContext, debugstr_w( szReaderName), debugstr_w(szGroupName));
    
    return SCARD_S_SUCCESS;        
}


LONG WINAPI SCardIntroduceCardTypeA(
    SCARDCONTEXT hContext,
    LPCSTR szCardName,
    LPCGUID pguidPrimaryProvider,
    LPCGUID rgguidInterfaces,
    DWORD dwInterfaceCount,
    const BYTE* pbAtr,
    const BYTE* pbAtrMask,
    DWORD cbAtrLen)
{
    FIXME("0x%08X %s %p %p %#lx %p %p %#lx - stub\n",(unsigned int) hContext, debugstr_a(szCardName),pguidPrimaryProvider,rgguidInterfaces,dwInterfaceCount,pbAtr,pbAtrMask,cbAtrLen);
    
    return SCARD_F_UNKNOWN_ERROR;
}
    
LONG WINAPI SCardIntroduceCardTypeW(
    SCARDCONTEXT hContext,
    LPCWSTR szCardName,
    LPCGUID pguidPrimaryProvider,
    LPCGUID rgguidInterfaces,
    DWORD dwInterfaceCount,
    const BYTE* pbAtr,
    const BYTE* pbAtrMask,
    DWORD cbAtrLen)
{
    FIXME("0x%08X %s %p %p %#lx %p %p %#lx - stub\n",(unsigned int) hContext, debugstr_w(szCardName),pguidPrimaryProvider,rgguidInterfaces,dwInterfaceCount,pbAtr,pbAtrMask,cbAtrLen);
    
    return SCARD_F_UNKNOWN_ERROR;    
}
    

LONG WINAPI SCardSetCardTypeProviderNameA(
    SCARDCONTEXT hContext,
    LPCSTR szCardName,
    DWORD dwProviderId,
    LPCSTR szProvider)
{
    FIXME("0x%08X %s %#lx %s - stub\n",(unsigned int) hContext, debugstr_a(szCardName),dwProviderId,debugstr_a(szProvider));
    
    return SCARD_F_UNKNOWN_ERROR;
}
    
LONG WINAPI SCardSetCardTypeProviderNameW(
    SCARDCONTEXT hContext,
    LPCWSTR szCardName,
    DWORD dwProviderId,
    LPCWSTR szProvider)
{
    FIXME("0x%08X %s %#lx %s - stub\n",(unsigned int) hContext, debugstr_w(szCardName),dwProviderId,debugstr_w(szProvider));
    
    return SCARD_F_UNKNOWN_ERROR;    
}

LONG WINAPI SCardForgetCardTypeA(
    SCARDCONTEXT hContext,
    LPCSTR szCardName)
{
    FIXME("0x%08X %s - stub\n",(unsigned int) hContext, debugstr_a(szCardName));
    
    return SCARD_E_UNKNOWN_CARD;
}
    
LONG WINAPI SCardForgetCardTypeW(
    SCARDCONTEXT hContext,
    LPCWSTR szCardName)
{
    FIXME("0x%08X %s - stub\n",(unsigned int) hContext, debugstr_w(szCardName));
    
    return SCARD_E_UNKNOWN_CARD;    
}
    
LONG WINAPI SCardLocateCardsA(
    SCARDCONTEXT hContext,
    LPCSTR mszCards,
    LPSCARD_READERSTATEA rgReaderStates,
    DWORD cReaders)
{
    FIXME("0x%08X %s %p %#lx - stub\n",(unsigned int) hContext, debugstr_a(mszCards),rgReaderStates,cReaders);
    
    return SCARD_E_UNKNOWN_CARD;
}
    
LONG WINAPI SCardLocateCardsW(
    SCARDCONTEXT hContext,
    LPCWSTR mszCards,
    LPSCARD_READERSTATEW rgReaderStates,
    DWORD cReaders)
{
    FIXME("0x%08X %s %p %#lx - stub\n",(unsigned int) hContext, debugstr_w(mszCards),rgReaderStates,cReaders);
    
    return SCARD_E_UNKNOWN_CARD;    
}

LONG WINAPI SCardLocateCardsByATRA(
    SCARDCONTEXT hContext,
    LPSCARD_ATRMASK rgAtrMasks,
    DWORD cAtrs,
    LPSCARD_READERSTATEA rgReaderStates,
    DWORD cReaders)
{
    FIXME("0x%08X %p %#lx %p %#lx - stub\n",(unsigned int) hContext, rgAtrMasks, cAtrs, rgReaderStates,  cReaders);
    
    return SCARD_F_UNKNOWN_ERROR;
}
    
LONG WINAPI SCardLocateCardsByATRW(
    SCARDCONTEXT hContext,
    LPSCARD_ATRMASK rgAtrMasks,
    DWORD cAtrs,
    LPSCARD_READERSTATEW rgReaderStates,
    DWORD cReaders)
{
    FIXME("0x%08X %p %#lx %p %#lx - stub\n",(unsigned int) hContext, rgAtrMasks, cAtrs, rgReaderStates,  cReaders);
    
    return SCARD_F_UNKNOWN_ERROR;    
}

LONG WINAPI SCardListReaderGroupsA(
        SCARDCONTEXT hContext,
        LPSTR mszGroups, 
        LPDWORD pcchGroups)
{    
    LONG lRet = SCARD_F_UNKNOWN_ERROR;
    DWORD_LITE len ;
    struct SCardListReaderGroups_params params;
    
    TRACE(" 0x%08X %s %p\n",(unsigned int) hContext,debugstr_a(mszGroups),pcchGroups);
    
    if(!pcchGroups)
        lRet = SCARD_E_INVALID_PARAMETER;
    else if(mszGroups && *pcchGroups == SCARD_AUTOALLOCATE)
    {
        LPSTR* pmszGroups = (LPSTR*) mszGroups;
        LPSTR szList = NULL;
        len = 0;
        
        /* ask for the length */
        params.hContext = hContext;
        params.mszGroups = NULL;
        params.pcchGroups = &len;
        lRet = WINSCARD_CALL( SCardListReaderGroups, &params );

        if(SCARD_S_SUCCESS == lRet)
        {
            /* allocate memory for the list */
            szList = (LPSTR) SCardAllocate((DWORD) len);
            if(!szList)
                lRet = SCARD_E_NO_MEMORY;
            else
            {
                /* fill the list */
                params.hContext = hContext;
                params.mszGroups = szList;
                params.pcchGroups = &len;
                lRet = WINSCARD_CALL( SCardListReaderGroups, &params );
                if(SCARD_S_SUCCESS != lRet)
                    SCardFree(szList);
                else
                {
                    *pmszGroups = szList;
                    *pcchGroups = (DWORD) len;        
                }
            }
        }  
    }
    else
    {
        params.hContext = hContext;
        params.mszGroups = mszGroups;

        if (pcchGroups)
        {
            len = *pcchGroups;
            params.pcchGroups = &len;
        } 
        else 
            params.pcchGroups = NULL;

        lRet = WINSCARD_CALL( SCardListReaderGroups, &params );
        if (pcchGroups)
            *pcchGroups = len;
    }
    
    return TranslateToWin32(lRet);
}
        
LONG WINAPI SCardListReaderGroupsW(
        SCARDCONTEXT hContext,
        LPWSTR mszGroups, 
        LPDWORD pcchGroups)
{
    LONG lRet = SCARD_F_UNKNOWN_ERROR;
    LPSTR szList = NULL;
    LPWSTR szListW = NULL;
    DWORD alen = 0,wlen = 0;
    
    TRACE(" 0x%08X %s %p\n",(unsigned int) hContext,debugstr_w(mszGroups),pcchGroups);
    
    if(!pcchGroups)
        lRet = SCARD_E_INVALID_PARAMETER;
    else
    {
        if(!mszGroups)
        {
            /* asking for length only
             * get the length in ANSI and multiply by sizeof(WCHAR)
             */
            alen = 0;
            lRet = SCardListReaderGroupsA(hContext,NULL,&alen);
            if(lRet == SCARD_S_SUCCESS || lRet == SCARD_E_INSUFFICIENT_BUFFER)
            {
                *pcchGroups = alen ;
            }
            goto end_label;
        }
        
        /* get the ASCII list from pcsclite */
        alen = SCARD_AUTOALLOCATE;
        lRet = SCardListReaderGroupsA(hContext,(LPSTR) &szList,&alen);
        if(SCARD_S_SUCCESS != lRet)
            goto end_label;
        
        /* now convert the list to a wide char list */
        lRet = ConvertListToWideChar(szList,&szListW,&wlen);
        
        /* free the ASCII list, we don't need it any more */
        SCardFreeMemory(hContext,(LPCVOID) szList);
        
        if(SCARD_S_SUCCESS != lRet)
            goto end_label;
        
        if(SCARD_AUTOALLOCATE == *pcchGroups)
        {
            LPWSTR* pmszGroups = (LPWSTR*) mszGroups;
            *pmszGroups = szListW;
            *pcchGroups = wlen;
            szListW = NULL;
            lRet = SCARD_S_SUCCESS;            
        }
        else
        {
            DWORD cchGroups = *pcchGroups;
            if(cchGroups < wlen)
            {
                *pcchGroups = wlen;
                lRet = SCARD_E_INSUFFICIENT_BUFFER;
            }
            else
            {
                *pcchGroups = wlen;
                memcpy(mszGroups,szListW,wlen*sizeof(WCHAR));
                lRet = SCARD_S_SUCCESS;
            }
        }
    }
    
end_label:    
    if(szListW)
        SCardFree(szListW);
    return TranslateToWin32(lRet);
    
}

LONG WINAPI SCardListReadersA(
        SCARDCONTEXT hContext,
        LPCSTR mszGroups,
        LPSTR mszReaders, 
        LPDWORD pcchReaders)
{
    LONG lRet;
    TRACE("0x%p %s %s %ld\n",(void*)hContext, debugstr_a(mszGroups), debugstr_a(mszReaders), (pcchReaders==NULL?0:*pcchReaders));
    if(!pcchReaders)
        lRet = SCARD_E_INVALID_PARAMETER;    
    else if(mszReaders && SCARD_AUTOALLOCATE == *pcchReaders)
    {
        /* get list from pcsc-lite */
        LPSTR* pmszReaders = (LPSTR*) mszReaders;
        LPSTR szList = NULL;
        DWORD_LITE dwListLength = 0;
        struct SCardListReaders_params params = { hContext, mszGroups, NULL, &dwListLength };
        lRet = WINSCARD_CALL( SCardListReaders, &params );

        if(SCARD_S_SUCCESS != lRet && lRet != SCARD_E_INSUFFICIENT_BUFFER)
            goto end_label;
        
        szList = (LPSTR)SCardAllocate((DWORD) dwListLength);
        if(!szList)
        {
            lRet = SCARD_E_NO_MEMORY;
            goto end_label;
        }
        
        params.hContext = hContext;
        params.mszReaders = szList;
        params.pcchReaders = &dwListLength;

        lRet = WINSCARD_CALL( SCardListReaders, &params );

        if(SCARD_S_SUCCESS != lRet)
            SCardFree(szList);
        else
        {
            *pmszReaders = szList;
            *pcchReaders = dwListLength;
        }
    }
    else
    {
        struct SCardListReaders_params params = { hContext, mszGroups, mszReaders, NULL };
        if (pcchReaders)
        {
            DWORD_LITE dwListLength = *pcchReaders;
            params.pcchReaders = &dwListLength;
            lRet = WINSCARD_CALL( SCardListReaders, &params );
            *pcchReaders = dwListLength;
        }
        else
            lRet = WINSCARD_CALL( SCardListReaders, &params );        
    }
    
end_label:
    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);
}

LONG WINAPI SCardListReadersW(
        SCARDCONTEXT hContext,
        LPCWSTR mszGroups,
        LPWSTR mszReaders, 
        LPDWORD pcchReaders)
{
    LONG lRet;
    TRACE("0x%p %s %s %p\n",(void*) hContext,debugstr_w(mszGroups),debugstr_w(mszReaders),pcchReaders);

    if(!pcchReaders)
        lRet = SCARD_E_INVALID_PARAMETER;
    // else if(!liteSCardListReaders)
    //     lRet = SCARD_F_INTERNAL_ERROR;
    else
    {
        /* call the ANSI version */
        LPSTR mszGroupsA = NULL;
        LPSTR mszReadersA = NULL;
        LPWSTR szListW = NULL;
        DWORD dwLength ;
        if(mszGroups)
        {
            lRet = ConvertListToANSI(mszGroups,&mszGroupsA,&dwLength);
            if(SCARD_S_SUCCESS != lRet)
                goto end_label;
        }
        
        if(!mszReaders)
        {
            /* asking for length only
             * Assume that number of wide char is the same
             * as the number f ANSI characters
             */
            dwLength = 0;
            lRet = SCardListReadersA(hContext,mszGroupsA,NULL,&dwLength);
            if(lRet == SCARD_S_SUCCESS || lRet == SCARD_E_INSUFFICIENT_BUFFER)
            {
                *pcchReaders = dwLength;
            }
            if(mszGroupsA)
                SCardFree(mszGroupsA);
            goto end_label;
        }
        
        dwLength = SCARD_AUTOALLOCATE;
        lRet = SCardListReadersA(hContext,mszGroupsA,(LPSTR) &mszReadersA,&dwLength);
        
        /* free the ANSI list of groups : no more needed */
        if(mszGroupsA)
            SCardFree(mszGroupsA);
        
        if(SCARD_S_SUCCESS != lRet)
            goto end_label;
        
        /* we now have the list in ANSI. Covert it to wide-char format*/
        lRet = ConvertListToWideChar(mszReadersA,&szListW,&dwLength);
        
        /* ANSI list of readers no more needed */
        SCardFreeMemory(hContext,(LPCVOID) mszReadersA);
        
        if(SCARD_S_SUCCESS != lRet)
            goto end_label;
        
        if(SCARD_AUTOALLOCATE == *pcchReaders)
        {
            LPWSTR* pmszReaders = (LPWSTR*) mszReaders;
            *pmszReaders = szListW;
            szListW = NULL;
            *pcchReaders = dwLength;
        }
        else
        {
            DWORD inputLength = *pcchReaders;
            if(inputLength < dwLength)
                lRet = SCARD_E_INSUFFICIENT_BUFFER;
            else
                memcpy(mszReaders,szListW,dwLength*sizeof(WCHAR));
            *pcchReaders = dwLength;
        }
        
        if(szListW)
            SCardFree(szListW);
    }
end_label:    
    return TranslateToWin32(lRet);
}

/*
 *  PCS/SC communication functions
 */
LONG WINAPI SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
    LONG lRet;
    struct SCardEstablishContext_params params = { dwScope, pvReserved1, pvReserved2, phContext};
    TRACE("%#lx %p %p %p\n",dwScope,pvReserved1,pvReserved2,phContext);

    if(!phContext)
        lRet = SCARD_E_INVALID_PARAMETER;
    else
        lRet = WINSCARD_CALL( SCardEstablishContext, &params );

    TRACE("returned %#lx  hContext %p\n", lRet, (void*)*params.phContext);
    return TranslateToWin32(lRet);
}

LONG WINAPI SCardReleaseContext(SCARDCONTEXT hContext)
{
    LONG lRet;
    struct SCardReleaseContext_params params = { hContext };
    TRACE("0x%p\n", (void*)hContext);

    lRet = WINSCARD_CALL( SCardReleaseContext, &params );

    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);
}

LONG WINAPI SCardIsValidContext(SCARDCONTEXT hContext)
{
    struct SCardIsValidContext_params params = { hContext };
    TRACE("0x%p\n", (void*)hContext);

    return TranslateToWin32(WINSCARD_CALL( SCardIsValidContext, &params ));
}

LONG WINAPI SCardConnectA(SCARDCONTEXT hContext,
                        LPCSTR szReader,
                        DWORD dwShareMode,
                        DWORD dwPreferredProtocols,
                        LPSCARDHANDLE phCard, 
                        LPDWORD pdwActiveProtocol)
{
    LONG lRet;
    struct SCardConnect_params params = { hContext, szReader, dwShareMode, dwPreferredProtocols, phCard, NULL };
    TRACE(" 0x%08X %s %#lx %#lx %p %p\n",(unsigned int) hContext,debugstr_a(szReader),dwShareMode,dwPreferredProtocols,phCard,pdwActiveProtocol);
    if(!szReader || !phCard || !pdwActiveProtocol)
        lRet = SCARD_E_INVALID_PARAMETER;
    else
    {
        /* the value of SCARD_PROTOCOL_RAW is different between MS implementation and
         * pcsc-lite implementation. We must change its value
         */
        if(dwPreferredProtocols & SCARD_PROTOCOL_RAW)
        {
            dwPreferredProtocols ^= SCARD_PROTOCOL_RAW;
            dwPreferredProtocols |= PCSCLITE_SCARD_PROTOCOL_RAW;
            params.dwPreferredProtocols = dwPreferredProtocols;
        }

        if (pdwActiveProtocol)
        {
            DWORD_LITE dwProtocol = *pdwActiveProtocol;
            params.pdwActiveProtocol = &dwProtocol;
            lRet = WINSCARD_CALL( SCardConnect, &params );
            *pdwActiveProtocol = (DWORD) dwProtocol;
        }
        else
            lRet = WINSCARD_CALL( SCardConnect, &params );
        if(SCARD_S_SUCCESS == lRet)
        {
            /* if PCSCLITE_SCARD_PROTOCOL_RAW is set, put back the MS corresponding value */
            if(*pdwActiveProtocol & PCSCLITE_SCARD_PROTOCOL_RAW)
            {
                *pdwActiveProtocol ^= PCSCLITE_SCARD_PROTOCOL_RAW;
                *pdwActiveProtocol |= SCARD_PROTOCOL_RAW;
            }
            
        }
    }
    
    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);
}
                        
LONG WINAPI SCardConnectW(SCARDCONTEXT hContext,
                        LPCWSTR szReader,
                        DWORD dwShareMode,
                        DWORD dwPreferredProtocols,
                        LPSCARDHANDLE phCard, 
                        LPDWORD pdwActiveProtocol)
{
    LONG lRet;
    struct SCardConnect_params params = { hContext, NULL, dwShareMode, dwPreferredProtocols, phCard, NULL};
    TRACE(" 0x%08X %s %#lx %#lx %p %p\n",(unsigned int) hContext,debugstr_w(szReader),dwShareMode,dwPreferredProtocols,phCard,pdwActiveProtocol);    
    if(!szReader || !phCard || !pdwActiveProtocol)
        lRet = SCARD_E_INVALID_PARAMETER;
    else
    {
        LPSTR szReaderA = NULL;
        int dwLen = WideCharToMultiByte(CP_ACP,0,szReader,-1,NULL,0,NULL,NULL);
        if(!dwLen)
        {
            lRet = SCARD_F_UNKNOWN_ERROR;
            goto end_label;
        }
        
        szReaderA = (LPSTR) SCardAllocate(dwLen);
        if(!szReaderA)
        {
            lRet = SCARD_E_NO_MEMORY;
            goto end_label;
        }
        
        dwLen = WideCharToMultiByte(CP_ACP,0,szReader,-1,szReaderA,dwLen,NULL,NULL);
        if(!dwLen)
        {
            SCardFree(szReaderA);
            lRet = SCARD_F_UNKNOWN_ERROR;
            goto end_label;
        }
        
        /* the value of SCARD_PROTOCOL_RAW is different between MS implementation and
         * pcsc-lite implementation. We must change its value
         */
        if(dwPreferredProtocols & SCARD_PROTOCOL_RAW)
        {
            dwPreferredProtocols ^= SCARD_PROTOCOL_RAW;
            dwPreferredProtocols |= PCSCLITE_SCARD_PROTOCOL_RAW;
            params.dwPreferredProtocols = dwPreferredProtocols;
        }

        params.szReader = szReaderA;
        if (pdwActiveProtocol)
        {
            DWORD_LITE dwProtocol = *pdwActiveProtocol;
            params.pdwActiveProtocol = &dwProtocol;
            lRet = WINSCARD_CALL( SCardConnect, &params );
            *pdwActiveProtocol = (DWORD) dwProtocol;
        }
        else
            lRet = WINSCARD_CALL( SCardConnect, &params );
        if(SCARD_S_SUCCESS == lRet)
        {
            /* if PCSCLITE_SCARD_PROTOCOL_RAW is set, put back the MS corresponding value */
            if(*pdwActiveProtocol & PCSCLITE_SCARD_PROTOCOL_RAW)
            {
                *pdwActiveProtocol ^= PCSCLITE_SCARD_PROTOCOL_RAW;
                *pdwActiveProtocol |= SCARD_PROTOCOL_RAW;
            }            
        }
        
        /* free the allocate ANSI string */
        SCardFree(szReaderA);
    }        
end_label:    
    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);    
}

LONG WINAPI SCardReconnect(SCARDHANDLE hCard,
                        DWORD dwShareMode,
                        DWORD dwPreferredProtocols,
                        DWORD dwInitialization, 
                        LPDWORD pdwActiveProtocol)
{
    LONG lRet;
    struct SCardReconnect_params params = { hCard, dwShareMode, dwPreferredProtocols, dwInitialization, NULL};
    TRACE(" 0x%08X %#lx %#lx %#lx %p\n",(unsigned int) hCard,dwShareMode,dwPreferredProtocols,dwInitialization,pdwActiveProtocol);
    if(!pdwActiveProtocol)
        lRet = SCARD_E_INVALID_PARAMETER;
    else
    {
        /* the value of SCARD_PROTOCOL_RAW is different between MS implementation and
         * pcsc-lite implementation. We must change its value
         */
        if(dwPreferredProtocols & SCARD_PROTOCOL_RAW)
        {
            dwPreferredProtocols ^= SCARD_PROTOCOL_RAW;
            dwPreferredProtocols |= PCSCLITE_SCARD_PROTOCOL_RAW;
            params.dwPreferredProtocols = dwPreferredProtocols;
        }

        if (pdwActiveProtocol)
        {
            DWORD_LITE dwProtocol = *pdwActiveProtocol;
            params.pdwActiveProtocol = &dwProtocol;
            lRet = WINSCARD_CALL( SCardReconnect, &params );
            *pdwActiveProtocol = (DWORD) dwProtocol;
        }
        else
            lRet = WINSCARD_CALL( SCardReconnect, &params );

        if(SCARD_S_SUCCESS == lRet)
        {
            /* if PCSCLITE_SCARD_PROTOCOL_RAW is set, put back the MS corresponding value */
            if(*pdwActiveProtocol & PCSCLITE_SCARD_PROTOCOL_RAW)
            {
                *pdwActiveProtocol ^= PCSCLITE_SCARD_PROTOCOL_RAW;
                *pdwActiveProtocol |= SCARD_PROTOCOL_RAW;
            }            
        }        
    }
    
    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);
}

LONG WINAPI SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
    LONG lRet;
    struct SCardDisconnect_params params = { hCard, dwDisposition};
    TRACE(" 0x%08X %#lx\n",(unsigned int) hCard,dwDisposition);

    lRet = WINSCARD_CALL( SCardDisconnect, &params );

    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);
}

LONG WINAPI SCardBeginTransaction(SCARDHANDLE hCard)
{
    LONG lRet;
    struct SCardBeginTransaction_params params = { hCard };
    TRACE(" 0x%08X\n",(unsigned int) hCard);

    lRet = WINSCARD_CALL( SCardBeginTransaction, &params );
    
    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);
}

LONG WINAPI SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{    
    LONG lRet;
    struct SCardEndTransaction_params params = { hCard, dwDisposition };
    TRACE(" 0x%08X %#lx\n",(unsigned int) hCard,dwDisposition);
    
    lRet = WINSCARD_CALL( SCardEndTransaction, &params );
    
    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);
}

LONG WINAPI SCardState(
    SCARDHANDLE hCard,
    LPDWORD pdwState,
    LPDWORD pdwProtocol,
    LPBYTE pbAtr,
    LPDWORD pcbAtrLen)
{
    LONG lRet ;
    LPSTR szName = NULL;
    DWORD cchReaderLen = SCARD_AUTOALLOCATE;
    TRACE(" 0x%08X %p %p %p %p\n",(unsigned int) hCard,pdwState,pdwProtocol,pbAtr,pcbAtrLen);
    lRet = SCardStatusA(hCard,(LPSTR) &szName,&cchReaderLen,pdwState,pdwProtocol,pbAtr,pcbAtrLen);
    
    /* free szName is allocated by SCardStatusA */
    if(szName)
        SCardFree((void*) szName);
    
    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);    
}

LONG WINAPI SCardStatusA(
        SCARDHANDLE hCard,
        LPSTR mszReaderNames, 
        LPDWORD pcchReaderLen,
        LPDWORD pdwState,
        LPDWORD pdwProtocol,
        LPBYTE pbAtr, 
        LPDWORD pcbAtrLen)
{
    LONG lRet;
    struct SCardStatus_params params;
    TRACE(" 0x%08X %p %p %p %p %p %p\n",(unsigned int) hCard,mszReaderNames,pcchReaderLen,pdwState,pdwProtocol,pbAtr,pcbAtrLen);
    if(!pcchReaderLen || !pdwState || !pdwProtocol || !pcbAtrLen)
        lRet = SCARD_E_INVALID_PARAMETER;
    else
    {
        DWORD_LITE dwNameLen = 0,dwAtrLen=MAX_ATR_SIZE, dwState, dwProtocol = 0;
        LPDWORD_LITE pdwStateLite = NULL, pdwProtocolLite = NULL, pdwNameLenLite = NULL, pdwAtrLenLite = NULL;
        BYTE atr[MAX_ATR_SIZE];
        if (pdwState)
        {
            dwState = *pdwState;
            pdwStateLite = &dwState;
        }
        if (pdwProtocol)
        {
            pdwProtocolLite = &dwProtocol;
        }

        if(!mszReaderNames || !pbAtr 
            || (*pcchReaderLen == SCARD_AUTOALLOCATE) || (*pcbAtrLen == SCARD_AUTOALLOCATE))
        {
            /* retreive the information from pcsc-lite */
            BOOL bHasAutoAllocated = FALSE;            
            LPSTR szNames = NULL;
            
            params.hCard = hCard;
            params.mszReaderName = NULL;
            params.pcchReaderLen = &dwNameLen;
            params.pdwState = pdwStateLite;
            params.pdwProtocol = pdwProtocolLite;
            params.pbAtr = atr;
            params.pcbAtrLen = &dwAtrLen;

            lRet = WINSCARD_CALL( SCardStatus, &params );
            if (pdwState)
            {
                *pdwState = (DWORD) dwState;
            }
            if (pdwProtocol)
            {
                *pdwProtocol = lite_proto2ms_proto(dwProtocol);
            }
            if(lRet != SCARD_S_SUCCESS && lRet != SCARD_E_INSUFFICIENT_BUFFER)
                goto end_label;
            
            /* case 1: asking for reader names length */
            if(!mszReaderNames)
            {
                lRet = SCARD_S_SUCCESS;
                *pcchReaderLen = dwNameLen;
                if(!pbAtr)
                    *pcbAtrLen = (DWORD) dwAtrLen;
                else if(*pcbAtrLen == SCARD_AUTOALLOCATE)
                {
                    LPBYTE* ppbAtr = (LPBYTE*) pbAtr;
                    *ppbAtr = (LPBYTE) SCardAllocate(dwAtrLen);
                    *pcbAtrLen = (DWORD) dwAtrLen;
                    memcpy(*ppbAtr,atr,dwAtrLen);
                }
                else if(*pcbAtrLen < dwAtrLen)
                {
                    *pcbAtrLen = dwAtrLen;    
                    lRet = SCARD_E_INSUFFICIENT_BUFFER;
                }
                else
                {
                    *pcbAtrLen = dwAtrLen;    
                    memcpy(pbAtr,atr,dwAtrLen);
                }
                
                goto end_label;
            }
            
            /* case 2: reader names pointer provided but its length is unsufficient */
            if(*pcchReaderLen < dwNameLen)
            {
                *pcchReaderLen = (DWORD) dwNameLen;
                lRet = SCARD_E_INSUFFICIENT_BUFFER;
                goto end_label;
            }
            
            bHasAutoAllocated = (*pcchReaderLen == SCARD_AUTOALLOCATE)? TRUE : FALSE;
            if(bHasAutoAllocated)            
                szNames = (LPSTR) SCardAllocate(dwNameLen);
            else
                szNames = mszReaderNames;
            
            params.hCard = hCard;
            params.mszReaderName = szNames;
            params.pcchReaderLen = &dwNameLen;
            params.pdwState = pdwStateLite;
            params.pdwProtocol = pdwProtocolLite;
            params.pbAtr = atr;
            params.pcbAtrLen = &dwAtrLen;

            lRet = WINSCARD_CALL( SCardStatus, &params );
            if(lRet != SCARD_S_SUCCESS)
            {
                if(bHasAutoAllocated)
                    SCardFree(szNames);
                goto end_label;
            }
            
            *pcchReaderLen = (DWORD) dwNameLen;
            if(bHasAutoAllocated)
            {
                LPSTR* pmszReaderNames = (LPSTR*) mszReaderNames;
                *pmszReaderNames = szNames;
                szNames = NULL;
            }
                
            
            /* now fill the ATR  parameter */
            if(!pbAtr)
                *pcbAtrLen = (DWORD) dwAtrLen;
            else if(*pcbAtrLen == SCARD_AUTOALLOCATE)
            {
                LPBYTE* ppbAtr = (LPBYTE*) pbAtr;
                *ppbAtr = (LPBYTE) SCardAllocate(dwAtrLen);
                *pcbAtrLen = (DWORD) dwAtrLen;
                memcpy(*ppbAtr,atr,dwAtrLen);
            }
            else if(*pcbAtrLen < dwAtrLen)
            {
                *pcbAtrLen = (DWORD) dwAtrLen;    
                lRet = SCARD_E_INSUFFICIENT_BUFFER;
            }
            else
            {
                *pcbAtrLen = (DWORD) dwAtrLen;    
                memcpy(pbAtr,atr,dwAtrLen);
            }
                
            if(bHasAutoAllocated && szNames)
                SCardFree(szNames);
        }
        else
        {
            if (pcchReaderLen)
            {
                dwNameLen = *pcchReaderLen;
                pdwNameLenLite = &dwNameLen;
            }
            else
                pdwNameLenLite = NULL;
            if (pcbAtrLen)
            {
                dwAtrLen = *pcbAtrLen;
                pdwAtrLenLite = &dwAtrLen;
            }
            else
                pdwAtrLenLite = NULL;

            params.hCard = hCard;
            params.mszReaderName = mszReaderNames;
            params.pcchReaderLen = pdwNameLenLite;
            params.pdwState = pdwStateLite;
            params.pdwProtocol = pdwProtocolLite;
            params.pbAtr = pbAtr;
            params.pcbAtrLen = pdwAtrLenLite;

            lRet = WINSCARD_CALL( SCardStatus, &params );
            if (pdwState)
            {
                *pdwState = (DWORD) dwState;
            }
            if (pdwProtocol)
            {
                *pdwProtocol = lite_proto2ms_proto(dwProtocol);
            }            
            if (pcchReaderLen)
            {
                *pcchReaderLen = dwNameLen;
            }
            if (pcbAtrLen)
            {
                *pcbAtrLen = dwAtrLen;
            }
        }
    }
    
end_label:    
    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);
}
        
LONG WINAPI SCardStatusW(
        SCARDHANDLE hCard,
        LPWSTR mszReaderNames, 
        LPDWORD pcchReaderLen,
        LPDWORD pdwState,
        LPDWORD pdwProtocol,
        LPBYTE pbAtr, 
        LPDWORD pcbAtrLen)
{
    LONG lRet;
    TRACE(" 0x%08X %p %p %p %p %p %p\n",(unsigned int) hCard,mszReaderNames,pcchReaderLen,pdwState,pdwProtocol,pbAtr,pcbAtrLen);
    if(!pcchReaderLen || !pdwState || !pdwProtocol || !pcbAtrLen)
        lRet = SCARD_E_INVALID_PARAMETER;
    else
    {    
        /* call the ANSI version with SCARD_AUTOALLOCATE */
        LPSTR mszReaderNamesA = NULL;
        DWORD dwAnsiNamesLength = SCARD_AUTOALLOCATE;
        lRet = SCardStatusA(hCard,(LPSTR) &mszReaderNamesA,&dwAnsiNamesLength,pdwState,pdwProtocol,pbAtr,pcbAtrLen);
        if(lRet == SCARD_S_SUCCESS)
        {
            /* convert mszReaderNamesA to a wide char multi-string */
            LPWSTR mszWideNamesList = NULL;
            DWORD dwWideNamesLength = 0;
            lRet = ConvertListToWideChar(mszReaderNamesA,&mszWideNamesList,&dwWideNamesLength);
            
            /* no more needed */
            if(mszReaderNamesA)
                    SCardFree(mszReaderNamesA);
            
            if(lRet == SCARD_S_SUCCESS)
            {
                if(!mszReaderNames)
                    *pcchReaderLen = dwWideNamesLength;
                else if(*pcchReaderLen == SCARD_AUTOALLOCATE)
                {
                    LPWSTR* pmszReaderNames = (LPWSTR*) mszReaderNames;
                    *pmszReaderNames = mszWideNamesList;
                    *pcchReaderLen = dwWideNamesLength;
                    mszWideNamesList = NULL;
                }
                else if(*pcchReaderLen < dwWideNamesLength)
                {
                    *pcchReaderLen = dwWideNamesLength;
                    lRet = SCARD_E_INSUFFICIENT_BUFFER;
                }
                else
                {
                    *pcchReaderLen = dwWideNamesLength;
                    memcpy(mszReaderNames,mszWideNamesList,dwWideNamesLength);
                }
            }
            
            if(mszWideNamesList)
                SCardFree(mszWideNamesList);    
        }
    }
    
    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);
}

LONG WINAPI SCardGetStatusChangeA(
        SCARDCONTEXT hContext,
        DWORD dwTimeout,
        LPSCARD_READERSTATEA rgReaderStates, 
        DWORD cReaders)
{
    LONG lRet;
    struct SCardGetStatusChange_params params; 
    TRACE(" 0x%08X %#lx %p %#lx\n",(unsigned int) hContext, dwTimeout,rgReaderStates,cReaders);
    if(!rgReaderStates && cReaders)
        lRet =  SCARD_E_INVALID_PARAMETER;
    else if(!cReaders)
    {
        lRet =  SCardIsValidContext(hContext); 
    }
    else
    {
        DWORD i;
        LPSCARD_READERSTATE_LITE pStates = (LPSCARD_READERSTATE_LITE) SCardAllocate(cReaders * sizeof(SCARD_READERSTATE_LITE));
        memset(pStates,0,cReaders * sizeof(SCARD_READERSTATE_LITE));
        for(i=0;i<cReaders;i++)
        {
            pStates[i].szReader = rgReaderStates[i].szReader;
            pStates[i].pvUserData = rgReaderStates[i].pvUserData;
        }

        /* in pcsclite, dwTimeout = 0 is equivalent to dwTimeout = INFINITE
         * In Windows, dwTimeout = 0 means return immediately
         * We will simulate an immediate return
         */
        if(dwTimeout == 0)
        {
            /* get the current state of readers and compare it with input
             * Return SCARD_S_SUCCESS if a change is detected and
             * SCARD_E_TIMEOUT otherwide
             */
            params.hContext = hContext;
            params.dwTimeout = 0;
            params.rgReaderStates = pStates;
            params.cReaders = cReaders;
            lRet = WINSCARD_CALL( SCardGetStatusChange, &params );
            if(lRet == SCARD_S_SUCCESS)
            {
                BOOL bStateChanges = FALSE;
                for(i=0;i<cReaders;i++)
                {
                    DWORD dwState = pStates[i].dwEventState & (~SCARD_STATE_CHANGED);
                    rgReaderStates[i].cbAtr = pStates[i].cbAtr;
                    memcpy(rgReaderStates[i].rgbAtr,pStates[i].rgbAtr,min (rgReaderStates[i].cbAtr, sizeof (rgReaderStates[i].rgbAtr)));                    
                    
                    if(dwState != rgReaderStates[i].dwCurrentState)
                    {
                        rgReaderStates[i].dwEventState = pStates[i].dwEventState;
                        bStateChanges = TRUE;
                    }
                    else
                        rgReaderStates[i].dwEventState = dwState;
                }
                
                if(!bStateChanges)
                    lRet = SCARD_E_TIMEOUT;
            }
        }
        else
        {
            for(i=0;i<cReaders;i++)
            {
                pStates[i].dwCurrentState = rgReaderStates[i].dwCurrentState;
                pStates[i].dwEventState = rgReaderStates[i].dwEventState;
                pStates[i].cbAtr = min (MAX_ATR_SIZE, rgReaderStates[i].cbAtr);
                memcpy(pStates[i].rgbAtr,rgReaderStates[i].rgbAtr,pStates[i].cbAtr);
            }

            params.hContext = hContext;
            params.dwTimeout = dwTimeout;
            params.rgReaderStates = pStates;
            params.cReaders = cReaders;
            lRet = WINSCARD_CALL( SCardGetStatusChange, &params );

            for(i=0;i<cReaders;i++)
            {
                rgReaderStates[i].dwCurrentState = pStates[i].dwCurrentState;
                rgReaderStates[i].dwEventState = pStates[i].dwEventState;
                rgReaderStates[i].cbAtr = pStates[i].cbAtr;
                memcpy(rgReaderStates[i].rgbAtr,pStates[i].rgbAtr, MAX_ATR_SIZE);
            }            
        }
        
        SCardFree(pStates);
    }
    
    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);    
}
        
LONG WINAPI SCardGetStatusChangeW(
        SCARDCONTEXT hContext,
        DWORD dwTimeout,
        LPSCARD_READERSTATEW rgReaderStates, 
        DWORD cReaders)
{
    LONG lRet;
    TRACE(" 0x%08X %#lx %p %#lx\n",(unsigned int) hContext, dwTimeout,rgReaderStates,cReaders);    
    if(!rgReaderStates && cReaders)
        lRet = SCARD_E_INVALID_PARAMETER;
    else if(!cReaders)
    {
        lRet =  SCardIsValidContext(hContext);     
    }    
    else
    {
        /* create an ANSI array of readers states* */
        DWORD i;
        LPSCARD_READERSTATEA rgReaderStatesAnsi = (LPSCARD_READERSTATEA) SCardAllocate(cReaders * sizeof(SCARD_READERSTATEA));
        if(!rgReaderStatesAnsi)
            return SCARD_E_NO_MEMORY;
        memset(rgReaderStatesAnsi,0,cReaders * sizeof(SCARD_READERSTATEA));
        for(i=0;i<cReaders;i++)
        {
            int alen = WideCharToMultiByte(CP_ACP,0,rgReaderStates[i].szReader,-1,NULL,0,NULL,NULL);
            if(!alen)
                break;
            rgReaderStatesAnsi[i].szReader = (LPSTR) SCardAllocate(alen);
            WideCharToMultiByte(CP_ACP,0,rgReaderStates[i].szReader,-1,(LPSTR) rgReaderStatesAnsi[i].szReader,alen,NULL,NULL);
            rgReaderStatesAnsi[i].pvUserData = rgReaderStates[i].pvUserData;
            rgReaderStatesAnsi[i].dwCurrentState = rgReaderStates[i].dwCurrentState;
            rgReaderStatesAnsi[i].dwEventState = rgReaderStates[i].dwEventState;
            rgReaderStatesAnsi[i].cbAtr = rgReaderStates[i].cbAtr;        
            memcpy(rgReaderStatesAnsi[i].rgbAtr,rgReaderStates[i].rgbAtr, sizeof (rgReaderStatesAnsi[i].rgbAtr));
        }
        
        if(i < cReaders)
            lRet = SCARD_F_UNKNOWN_ERROR;
        else
        {
            lRet = SCardGetStatusChangeA(hContext,dwTimeout,rgReaderStatesAnsi,cReaders);
            /* copy back the information */
            for(i=0;i<cReaders;i++)
            {
                rgReaderStates[i].dwEventState = rgReaderStatesAnsi[i].dwEventState;
                rgReaderStates[i].cbAtr = rgReaderStatesAnsi[i].cbAtr;        
                memcpy(rgReaderStates[i].rgbAtr,rgReaderStatesAnsi[i].rgbAtr, sizeof (rgReaderStates[i].rgbAtr));                        
            }               
        }
        /* free memory */
        for(i=0;i<cReaders;i++)
        {
            if(rgReaderStatesAnsi[i].szReader)
                SCardFree((void*) rgReaderStatesAnsi[i].szReader);
        }
        SCardFree(rgReaderStatesAnsi);    
    }
    
    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);
}

LONG WINAPI SCardControl(
            SCARDHANDLE hCard, 
            DWORD dwControlCode,
            LPCVOID pbSendBuffer, 
            DWORD cbSendLength,
            LPVOID pbRecvBuffer, 
            DWORD cbRecvLength, 
            LPDWORD lpBytesReturned)
{
        struct SCardControl_params params = { hCard, dwControlCode, pbSendBuffer, cbSendLength, pbRecvBuffer, cbRecvLength, NULL };
        DWORD_LITE dwBytesReturned = 0;
        LONG lRet;
        if (lpBytesReturned)
        {
            dwBytesReturned = *lpBytesReturned;
            params.lpBytesReturned = &dwBytesReturned;
        }  
        lRet = WINSCARD_CALL( SCardControl, &params );
        if (lpBytesReturned)
            *lpBytesReturned = dwBytesReturned;
        return TranslateToWin32(lRet);
}

LONG WINAPI SCardTransmit(
        SCARDHANDLE hCard,
        LPCSCARD_IO_REQUEST pioSendPci,
        const BYTE* pbSendBuffer, 
        DWORD cbSendLength,
        LPSCARD_IO_REQUEST pioRecvPci,
        LPBYTE pbRecvBuffer, 
        LPDWORD pcbRecvLength)
{
    LONG lRet;
    struct SCardTransmit_params params;
    DWORD_LITE dwRecvLength = 0;
    LPDWORD_LITE pdwRecvLengthLite = NULL;
    SCARD_IO_REQUEST_LITE ioSendPci, ioRecvPci;
    ioSendPci.cbPciLength = sizeof(ioSendPci);
    ioRecvPci.cbPciLength = sizeof(ioRecvPci);
    
    if (pcbRecvLength)
    {
        dwRecvLength = *pcbRecvLength;
        pdwRecvLengthLite = &dwRecvLength;
    }
    
    if (pioRecvPci) {
        ioRecvPci.dwProtocol = ms_proto2lite_proto(pioRecvPci->dwProtocol);
    }
    
    if(pioSendPci)
    {
        ioSendPci.dwProtocol = ms_proto2lite_proto(pioSendPci->dwProtocol);
    }
    else
    {
        /* In MS PC/SC, pioSendPci can be NULL. But not in pcsc-lite */
        /* Get the protocol and set the correct value for pioSendPci*/
        DWORD protocol,dwState;
        DWORD dwAtrLen,dwNameLen;
        lRet = SCardStatusA(hCard,NULL,&dwNameLen,&dwState,&protocol,NULL,&dwAtrLen);
        if(lRet == SCARD_S_SUCCESS)
        {
            ioSendPci.dwProtocol = ms_proto2lite_proto(protocol);
        }
        else
            goto transmit_end;
    }

    params.hCard = hCard;
    params.pioSendPci = &ioSendPci;
    params.pbSendBuffer = pbSendBuffer;
    params.cbSendLength = cbSendLength;
    params.pioRecvPci = pioRecvPci? &ioRecvPci : NULL;
    params.pbRecvBuffer = pbRecvBuffer;
    params.pcbRecvLength = pdwRecvLengthLite;
    lRet = WINSCARD_CALL( SCardTransmit, &params );

    if (pcbRecvLength)
        *pcbRecvLength = dwRecvLength;
    
transmit_end:
    return TranslateToWin32(lRet);
}
        
LONG WINAPI SCardCancel(SCARDCONTEXT hContext)
{
    LONG lRet;
    struct SCardCancel_params params = { hContext };
    TRACE(" 0x%08X \n",(unsigned int) hContext);
    lRet = WINSCARD_CALL( SCardCancel, &params );

    TRACE(" returned %#lx\n",lRet);
    return TranslateToWin32(lRet);
}

LONG WINAPI SCardGetAttrib(
            SCARDHANDLE hCard, 
            DWORD dwAttrId,
            LPBYTE pbAttr, 
            LPDWORD pcbAttrLen)
{
    LONG lRet;
    TRACE(" 0x%08X %#lx %p %p \n",(unsigned int) hCard, dwAttrId,pbAttr,pcbAttrLen);
    if(!pcbAttrLen)
        lRet = SCARD_E_INVALID_PARAMETER;
    else
    {
        LPBYTE ptr = NULL;
        DWORD_LITE dwLength = 0;
        struct SCardGetAttrib_params params = {hCard, dwAttrId, NULL, &dwLength}; 
        lRet = WINSCARD_CALL( SCardGetAttrib, &params );
        if(lRet == SCARD_S_SUCCESS || lRet == SCARD_E_INSUFFICIENT_BUFFER)
        {
            if(!pbAttr)
                *pcbAttrLen = (DWORD) dwLength;
            else if(*pcbAttrLen < (DWORD) dwLength)
            {
                *pcbAttrLen = (DWORD) dwLength;
                lRet = SCARD_E_INSUFFICIENT_BUFFER;
            }
            else
            {        
                BOOL bHasAutoAllocate = (*pcbAttrLen == SCARD_AUTOALLOCATE)? TRUE : FALSE;
                if(bHasAutoAllocate)
                    ptr = (LPBYTE) SCardAllocate((DWORD) dwLength);
                else
                    ptr = pbAttr;

                params.hCard = hCard;
                params.dwAttrId = dwAttrId;
                params.pbAttr = ptr;
                params.pcbAttrLen = &dwLength;
                lRet = WINSCARD_CALL( SCardGetAttrib, &params );

                if(lRet == SCARD_S_SUCCESS)
                {
                    *pcbAttrLen = (DWORD) dwLength;
                    if(bHasAutoAllocate)
                    {
                        LPBYTE *ppbAttr = (LPBYTE*) pbAttr;
                        *ppbAttr = ptr;
                        ptr = NULL;
                    }    
                }
            
                if(bHasAutoAllocate && ptr)
                    SCardFree(ptr);
            }
        }
        
        if(SCARD_E_UNSUPPORTED_FEATURE == TranslateToWin32(lRet))
        {
            /* in case of one of these attributes , do it our selves */
            if(SCARD_ATTR_ICC_PRESENCE == dwAttrId || SCARD_ATTR_CURRENT_PROTOCOL_TYPE == dwAttrId
                || SCARD_ATTR_ATR_STRING == dwAttrId || SCARD_ATTR_DEVICE_FRIENDLY_NAME_A == dwAttrId
                || SCARD_ATTR_DEVICE_FRIENDLY_NAME_W == dwAttrId 
                || SCARD_ATTR_DEVICE_SYSTEM_NAME_A == dwAttrId
                || SCARD_ATTR_DEVICE_SYSTEM_NAME_W == dwAttrId)
            {
                DWORD dwState;
                DWORD dwProtocol;
                BYTE pbAtr[MAX_ATR_SIZE];
                DWORD dwAtrLen =MAX_ATR_SIZE;
                LPVOID pszReaderNames = NULL;
                DWORD dwNameLength = SCARD_AUTOALLOCATE;
                LONG status;
                
                if(SCARD_ATTR_DEVICE_SYSTEM_NAME_W == dwAttrId 
                    || SCARD_ATTR_DEVICE_FRIENDLY_NAME_W == dwAttrId)                    
                    status = SCardStatusW(hCard,(LPWSTR) &pszReaderNames, &dwNameLength, &dwState,&dwProtocol, pbAtr,&dwAtrLen);
                else
                    status = SCardStatusA(hCard,(LPSTR) &pszReaderNames, &dwNameLength, &dwState,&dwProtocol, pbAtr,&dwAtrLen);
                if(SCARD_S_SUCCESS == status)
                {
                    BYTE pbValue[MAX_ATR_SIZE];
                    DWORD dwValueLen = 0;
                    LPBYTE pValuePtr = &pbValue[0];
                    if(SCARD_ATTR_ICC_PRESENCE == dwAttrId)
                    {
                        dwValueLen = 1;
                        pbValue[0] = 1; /* present by default */
                        if(dwState == SCARD_ABSENT)
                            pbValue[0] = 0;
                        else if(dwState == SCARD_SWALLOWED)
                            pbValue[0] = 2;
                    }
                    else if(SCARD_ATTR_CURRENT_PROTOCOL_TYPE == dwAttrId)
                    {
                        dwValueLen = 4;
                        memcpy(pbValue,&dwProtocol,4);
                    }
                    else if(SCARD_ATTR_DEVICE_FRIENDLY_NAME_A == dwAttrId ||
                        SCARD_ATTR_DEVICE_SYSTEM_NAME_A == dwAttrId)
                    {
                        pValuePtr = (LPBYTE) pszReaderNames;
                        dwValueLen = dwNameLength ;                        
                    }
                    else if(SCARD_ATTR_DEVICE_FRIENDLY_NAME_W == dwAttrId ||
                        SCARD_ATTR_DEVICE_SYSTEM_NAME_W == dwAttrId)
                    {
                        pValuePtr = (LPBYTE) pszReaderNames;
                        dwValueLen = dwNameLength * sizeof(WCHAR);
                    }
                    else /* ATR case */
                    {
                        dwValueLen = dwAtrLen;
                        memcpy(pbValue,pbAtr,dwAtrLen);
                    }
                    
                    lRet = SCARD_S_SUCCESS;
                    if(!pbAttr)
                        *pcbAttrLen =dwValueLen;
                    else if(*pcbAttrLen == SCARD_AUTOALLOCATE)
                    {
                        LPBYTE *ppbAttr = (LPBYTE*) pbAttr;
                        *ppbAttr = (LPBYTE) SCardAllocate(dwValueLen);
                        memcpy(*ppbAttr,pValuePtr,dwValueLen);
                        *pcbAttrLen = dwValueLen;
                    }
                    else if(*pcbAttrLen < dwValueLen)
                    {
                        *pcbAttrLen = dwValueLen;
                        lRet = SCARD_E_INSUFFICIENT_BUFFER;
                    }
                    else
                    {
                        *pcbAttrLen = dwValueLen;
                        memcpy(pbAttr,pValuePtr,dwValueLen);
                    }
                    
                    SCardFree(pszReaderNames);
                }
            }
        }
    }
    TRACE(" returned %#lx \n",lRet);
    return TranslateToWin32(lRet);
}

LONG WINAPI SCardSetAttrib(
            SCARDHANDLE hCard, 
            DWORD dwAttrId,
            const BYTE* pbAttr, 
            DWORD cbAttrLen)
{
    LONG lRet;
    struct SCardSetAttrib_params params = {hCard, dwAttrId, pbAttr, cbAttrLen}; 
    TRACE(" 0x%08X %#lx %p %#lx \n",(unsigned int) hCard,dwAttrId,pbAttr,cbAttrLen);
    lRet = WINSCARD_CALL( SCardGetAttrib, &params );
    TRACE(" returned %#lx \n",lRet);    
    return TranslateToWin32(lRet);
}
