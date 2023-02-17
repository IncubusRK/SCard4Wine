/*
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
 */

#include "windows.h"
#include "winscard.h"
#include "winsmcrd.h"
#include "wine/test.h"

#ifndef SCARD_PCI_T0
const SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, 8 };
const SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, 8 };
#define SCARD_PCI_T0	(&g_rgSCardT0Pci) /**< protocol control information (PCI) for T=0 */
#define SCARD_PCI_T1	(&g_rgSCardT1Pci) /**< protocol control information (PCI) for T=1 */
#endif


SCARDCONTEXT hContext;

static void test_winscardA(void)
{
	DWORD dwReaders;
	LONG lRet;
    LPSTR szReaders = NULL;
	LPSTR reader;
	int i, nbReaders = 0;
	char **readers = NULL;
	int reader_nb;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol, dwAtrLen, dwReaderLen, dwState, dwProt;
	LPSCARD_READERSTATEA lpState = NULL;
	BYTE pbAtr[33] = "";
	char pbReader[128] = "";

    const SCARD_IO_REQUEST* pioSendPci;
    SCARD_IO_REQUEST pioRecvPci;
    BYTE pbRecvBuffer[10];
    BYTE pbSendBuffer[] = { 0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };
    DWORD dwSendLength, dwRecvLength;
    
    dwReaders = SCARD_AUTOALLOCATE;
    lRet = SCardListReadersA(hContext, NULL, (LPSTR)&szReaders, &dwReaders);
    if (lRet == SCARD_E_NO_READERS_AVAILABLE) {
		skip("No readers available. Install vsmartcard-vpcd and restart pcscd\n");
		return;
    } else {
		ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);
		
        reader = szReaders;
        while (reader != NULL && *reader != '\0') {
			trace("Found smart-card reader: %s\n", wine_dbgstr_a(reader));
            reader += strlen(reader) + 1;
			nbReaders++;
        }
		
		readers = calloc(nbReaders, sizeof(char *));
		ok(NULL != readers,"Not enough memory");
		
		nbReaders = 0;
		reader = szReaders;
        while (reader != NULL && *reader != '\0') {
			readers[nbReaders] = reader;
            reader += strlen(reader) + 1;
			nbReaders++;
        }

		lpState = (LPSCARD_READERSTATEA)calloc(nbReaders, sizeof(SCARD_READERSTATEA));
        for (i = 0; i < nbReaders; ++i) {
            memset(lpState + i, 0, sizeof(SCARD_READERSTATEA));
            lpState[i].szReader = readers[i];
        }

        lRet = SCardGetStatusChangeA(hContext, 500, lpState, nbReaders);
		/* search first reader with smartcard */
		reader_nb = -1;
		for(i=0; i<nbReaders; i++)
		{
			if(lpState[i].dwEventState & SCARD_STATE_PRESENT)
			{
				reader_nb = i;
				break;
			}
		}
		ok(lRet == SCARD_S_SUCCESS || lRet == SCARD_E_TIMEOUT, "got %#lx\n", lRet);
		if(-1==reader_nb)
		{
			skip("No smartcard available. Install and start virt_cacard\n");
			goto end;
		}

        /* Test SCard connection */
        /* connect to a card */
        dwActiveProtocol = -1;
        lRet = SCardConnectA(hContext, readers[reader_nb], SCARD_SHARE_SHARED,
            SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
		ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);
        
        /* get card status */
        dwAtrLen = sizeof(pbAtr);
        dwReaderLen = sizeof(pbReader);
        lRet = SCardStatusA(hCard, /*NULL*/ pbReader, &dwReaderLen, &dwState, &dwProt,
            pbAtr, &dwAtrLen);
		ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);

		trace(" Reader: %s (length %ld bytes)\n", pbReader, dwReaderLen);
        trace(" State: 0x%lX\n", dwState);
        trace(" Prot: %ld\n", dwProt);
        trace(" ATR (length %ld bytes):", dwAtrLen);
        for (i = 0; i < dwAtrLen; i++)
            trace(" %02X", pbAtr[i]);
        trace("\n");

        switch (dwActiveProtocol)
        {
        case SCARD_PROTOCOL_T0:
            pioSendPci = SCARD_PCI_T0;
            break;
        case SCARD_PROTOCOL_T1:
            pioSendPci = SCARD_PCI_T1;
            break;
        default:
            skip("Unknown active protocol\n");
            goto end;
        }
        pioRecvPci.cbPciLength = sizeof(pioRecvPci);
        pioRecvPci.dwProtocol = dwActiveProtocol;

        /* exchange APDU */
        dwSendLength = sizeof(pbSendBuffer);
        dwRecvLength = sizeof(pbRecvBuffer);
	
		trace("Sending: ");
        for (i = 0; i < dwSendLength; i++)
            trace("%02X ", pbSendBuffer[i]);
        trace("\n");
        lRet = SCardTransmit(hCard, pioSendPci, pbSendBuffer, dwSendLength,
            &pioRecvPci, pbRecvBuffer, &dwRecvLength);
		ok(lRet == SCARD_S_SUCCESS, "got %#lx\t%#lx\n", lRet, GetLastError());
 		trace("Received: ");
        for (i = 0; i < dwRecvLength; i++)
            trace("%02X ", pbRecvBuffer[i]);
        trace("\n");

        /* card reconnect */
        lRet = SCardReconnect(hCard, SCARD_SHARE_SHARED,
                SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, SCARD_LEAVE_CARD,
                &dwActiveProtocol);
        ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);

		/* get card status after reconnect */
		dwAtrLen = sizeof(pbAtr);
        dwReaderLen = sizeof(pbReader);
        lRet = SCardStatusA(hCard, /*NULL*/ pbReader, &dwReaderLen, &dwState, &dwProt,
            pbAtr, &dwAtrLen);
		ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);

        /* begin transaction */
        lRet = SCardBeginTransaction(hCard);
		ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);

        /* exchange APDU */
        dwSendLength = sizeof(pbSendBuffer);
        dwRecvLength = sizeof(pbRecvBuffer);

        lRet = SCardTransmit(hCard, pioSendPci, pbSendBuffer, dwSendLength,
            &pioRecvPci, pbRecvBuffer, &dwRecvLength);

        ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);

            /* end transaction */
        lRet = SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
        ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);

            /* card disconnect */
        lRet = SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
        ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);
end:		
        if (lpState)
            free(lpState);
		if (readers)
			free(readers);
        // only do this after being done with the strings, or handle the names another way!
        SCardFreeMemory(hContext, szReaders);
    }
}

static void test_winscardW(void)
{
	DWORD dwReaders;
	LONG lRet;
    LPWSTR szReaders = NULL;
	LPWSTR reader;
	int i, nbReaders = 0;
	WCHAR **readers = NULL;
	int reader_nb;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol, dwAtrLen, dwReaderLen, dwState, dwProt;
	LPSCARD_READERSTATEW lpState = NULL;
	BYTE pbAtr[33] = "";
	WCHAR pbReader[128] = L"";

    const SCARD_IO_REQUEST* pioSendPci;
    SCARD_IO_REQUEST pioRecvPci;
    BYTE pbRecvBuffer[10];
    BYTE pbSendBuffer[] = { 0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };
    DWORD dwSendLength, dwRecvLength;
    
    dwReaders = SCARD_AUTOALLOCATE;
    lRet = SCardListReadersW(hContext, NULL, (LPWSTR)&szReaders, &dwReaders);
    if (lRet == SCARD_E_NO_READERS_AVAILABLE) {
		skip("No readers available. Install vsmartcard-vpcd and restart pcscd\n");
		return;
    } else {
		ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);
		
        reader = szReaders;
        while (reader != NULL && *reader != '\0') {
			trace("Found smartcard reader: %s\n", wine_dbgstr_w(reader));
            reader += wcslen(reader) + 1;
			nbReaders++;
        }
		
		readers = calloc(nbReaders, sizeof(WCHAR *));
		ok(NULL != readers,"Not enough memory");
		
		nbReaders = 0;
		reader = szReaders;
        while (reader != NULL && *reader != '\0') {
			readers[nbReaders] = reader;
            reader += wcslen(reader) + 1;
			nbReaders++;
        }

		lpState = (LPSCARD_READERSTATEW)calloc(nbReaders, sizeof(SCARD_READERSTATEW));
        for (i = 0; i < nbReaders; ++i) {
            memset(lpState + i, 0, sizeof(SCARD_READERSTATEW));
            lpState[i].szReader = readers[i];
        }

        lRet = SCardGetStatusChangeW(hContext, 500, lpState, nbReaders);
		/* search first reader with smartcard */
		reader_nb = -1;
		for(i=0; i<nbReaders; i++)
		{
			if(lpState[i].dwEventState & SCARD_STATE_PRESENT)
			{
				reader_nb = i;
				break;
			}
		}
		ok(lRet == SCARD_S_SUCCESS || lRet == SCARD_E_TIMEOUT, "got %#lx\n", lRet);
		if(-1==reader_nb)
		{
			skip("No smartcard available. Install and start virt_cacard\n");
			goto end;
		}

        /* Test SCard connection */
        /* connect to a card */
        dwActiveProtocol = -1;
        lRet = SCardConnectW(hContext, readers[reader_nb], SCARD_SHARE_SHARED,
            SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
		ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);
        
        /* get card status */
        dwAtrLen = sizeof(pbAtr);
        dwReaderLen = sizeof(pbReader);
        lRet = SCardStatusW(hCard, /*NULL*/ pbReader, &dwReaderLen, &dwState, &dwProt,
            pbAtr, &dwAtrLen);
		ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);

		trace(" Reader: %s (length %ld bytes)\n", pbReader, dwReaderLen);
        trace(" State: 0x%lX\n", dwState);
        trace(" Prot: %ld\n", dwProt);
        trace(" ATR (length %ld bytes):", dwAtrLen);
        for (i = 0; i < dwAtrLen; i++)
            trace(" %02X", pbAtr[i]);
        trace("\n");

        switch (dwActiveProtocol)
        {
        case SCARD_PROTOCOL_T0:
            pioSendPci = SCARD_PCI_T0;
            break;
        case SCARD_PROTOCOL_T1:
            pioSendPci = SCARD_PCI_T1;
            break;
        default:
            skip("Unknown active protocol\n");
            goto end;
        }
        pioRecvPci.cbPciLength = sizeof(pioRecvPci);
        pioRecvPci.dwProtocol = dwActiveProtocol;

        /* exchange APDU */
        dwSendLength = sizeof(pbSendBuffer);
        dwRecvLength = sizeof(pbRecvBuffer);
	
		trace("Sending: ");
        for (i = 0; i < dwSendLength; i++)
            trace("%02X ", pbSendBuffer[i]);
        trace("\n");
        lRet = SCardTransmit(hCard, pioSendPci, pbSendBuffer, dwSendLength,
            &pioRecvPci, pbRecvBuffer, &dwRecvLength);
		ok(lRet == SCARD_S_SUCCESS, "got %#lx\t%#lx\n", lRet, GetLastError());
 		trace("Received: ");
        for (i = 0; i < dwRecvLength; i++)
            trace("%02X ", pbRecvBuffer[i]);
        trace("\n");

        /* card reconnect */
        lRet = SCardReconnect(hCard, SCARD_SHARE_SHARED,
                SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, SCARD_LEAVE_CARD,
                &dwActiveProtocol);
        ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);

		/* get card status after reconnect */
		dwAtrLen = sizeof(pbAtr);
        dwReaderLen = sizeof(pbReader);
        lRet = SCardStatusW(hCard, /*NULL*/ pbReader, &dwReaderLen, &dwState, &dwProt,
            pbAtr, &dwAtrLen);
		ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);

        /* begin transaction */
        lRet = SCardBeginTransaction(hCard);
		ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);

        /* exchange APDU */
        dwSendLength = sizeof(pbSendBuffer);
        dwRecvLength = sizeof(pbRecvBuffer);

        lRet = SCardTransmit(hCard, pioSendPci, pbSendBuffer, dwSendLength,
            &pioRecvPci, pbRecvBuffer, &dwRecvLength);

        ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);

            /* end transaction */
        lRet = SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
        ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);

            /* card disconnect */
        lRet = SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
        ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);
end:		
        if (lpState)
            free(lpState);
		if (readers)
			free(readers);
        // only do this after being done with the strings, or handle the names another way!
        SCardFreeMemory(hContext, szReaders);
    }
}

START_TEST(winscard)
{
	//SCARD_SCOPE_SYSTEM
	LONG lRet = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	if(lRet == SCARD_E_NO_SERVICE) 
	{
		skip("pcscd daemon not running\n");
		return;
	}
	ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);
	
    test_winscardA();
    test_winscardW();
	
	lRet = SCardReleaseContext(hContext);
	ok(lRet == SCARD_S_SUCCESS, "got %#lx\n", lRet);
}
