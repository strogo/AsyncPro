// ***** BEGIN LICENSE BLOCK *****
// * Version: MPL 1.1
// *
// * The contents of this file are subject to the Mozilla Public License Version
// * 1.1 (the "License"); you may not use this file except in compliance with
// * the License. You may obtain a copy of the License at
// * http://www.mozilla.org/MPL/
// *
// * Software distributed under the License is distributed on an "AS IS" basis,
// * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// * for the specific language governing rights and limitations under the
// * License.
// *
// * The Original Code is TurboPower Async Professional
// *
// * The Initial Developer of the Original Code is
// * TurboPower Software
// *
// * Portions created by the Initial Developer are Copyright (C) 1991-2002
// * the Initial Developer. All Rights Reserved.
// *
// * Contributor(s):
// *
// * ***** END LICENSE BLOCK *****
/
*++
Module Name:

    util.c

Abstract:

    This module provides all the utility functions for apfmon40.


Revision History:

--*/

#include <windows.h>
#include <winspool.h>
#include <spltypes.h>
#include <apfmon40.h>
#include <local.h>
#include <stdarg.h>


//
// These globals are needed so that AddPort can call
// SPOOLSS!EnumPorts to see whether the port to be added
// already exists.
// They will be initialized the first time AddPort is called.
//
// !! LATER !!
//
// This is common code. move PortExists into the router.
//

HMODULE hSpoolssDll = NULL;
FARPROC pfnSpoolssEnumPorts = NULL;


VOID
RemoveColon(
    LPWSTR  pName)
{
    DWORD   Length;

    Length = wcslen(pName);

    if (pName[Length-1] == L':')
        pName[Length-1] = 0;
}


#define NEXTVAL(pch)                    \
    while( *pch && ( *pch != L',' ) )    \
        pch++;                          \
    if( *pch )                          \
        pch++


/* PortExists
 *
 * Calls EnumPorts to check whether the port name already exists.
 * This asks every monitor, rather than just this one.
 * The function will return TRUE if the specified port is in the list.
 * If an error occurs, the return is FALSE and the variable pointed
 * to by pError contains the return from GetLastError().
 * The caller must therefore always check that *pError == NO_ERROR.
 */
BOOL
PortExists(
    LPWSTR pName,
    LPWSTR pPortName,
    PDWORD pError
)
{
    DWORD cbNeeded;
    DWORD cReturned;
    DWORD cbPorts;
    LPPORT_INFO_1 pPorts;
    DWORD i;
    BOOL  Found = TRUE;

    *pError = NO_ERROR;

    if (!hSpoolssDll) {

        hSpoolssDll = LoadLibrary(L"SPOOLSS.DLL");

        if (hSpoolssDll) {
            pfnSpoolssEnumPorts = GetProcAddress(hSpoolssDll,
                                                 "EnumPortsW");
            if (!pfnSpoolssEnumPorts) {

                *pError = GetLastError();
                FreeLibrary(hSpoolssDll);
                hSpoolssDll = NULL;
            }

        } else {

            *pError = GetLastError();
        }
    }

    if (!pfnSpoolssEnumPorts)
        return FALSE;


    if (!(*pfnSpoolssEnumPorts)(pName, 1, NULL, 0, &cbNeeded, &cReturned))
    {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            cbPorts = cbNeeded;

            pPorts = AllocSplMem(cbPorts);

            if (pPorts)
            {
                if ((*pfnSpoolssEnumPorts)(pName, 1, (LPBYTE)pPorts, cbPorts,
                                           &cbNeeded, &cReturned))
                {
                    Found = FALSE;

                    for (i = 0; i < cReturned; i++)
                    {
                        if (!lstrcmpi(pPorts[i].pName, pPortName))
                            Found = TRUE;
                    }
                }
            }

            FreeSplMem(pPorts);
        }
    }

    else
        Found = FALSE;


    return Found;
}


VOID
SplInSem(
   VOID
)
{
    if ((DWORD)SpoolerSection.OwningThread != GetCurrentThreadId()) {
        DBGMSG(DBG_ERROR, ("Not in spooler semaphore\n"));
    }
}

VOID
SplOutSem(
   VOID
)
{
    if ((DWORD)SpoolerSection.OwningThread == GetCurrentThreadId()) {
        DBGMSG(DBG_ERROR, ("Inside spooler semaphore !!\n"));
    }
}

VOID
EnterSplSem(
   VOID
)
{
    EnterCriticalSection(&SpoolerSection);
}

VOID
LeaveSplSem(
   VOID
)
{
#if DBG
    SplInSem();
#endif
    LeaveCriticalSection(&SpoolerSection);
}

PINIENTRY
FindName(
   PINIENTRY pIniKey,
   LPWSTR pName
)
{
    if (pName) {
        while (pIniKey) {

            if (!lstrcmpi(pIniKey->pName, pName)) {
                return pIniKey;
            }

            pIniKey=pIniKey->pNext;
        }
    }

    return FALSE;
}

PINIENTRY
FindIniKey(
   PINIENTRY pIniEntry,
   LPWSTR pName
)
{
   if (!pName)
      return NULL;

   SplInSem();

   while (pIniEntry && lstrcmpi(pName, pIniEntry->pName))
      pIniEntry = pIniEntry->pNext;

   return pIniEntry;
}

LPBYTE
PackStrings(
   LPWSTR *pSource,
   LPBYTE pDest,
   DWORD *DestOffsets,
   LPBYTE pEnd
)
{
   while (*DestOffsets != -1) {
      if (*pSource) {
         pEnd-=wcslen(*pSource)*sizeof(WCHAR) + sizeof(WCHAR);
         *(LPWSTR *)(pDest+*DestOffsets)=wcscpy((LPWSTR)pEnd, *pSource);
      } else
         *(LPWSTR *)(pDest+*DestOffsets)=0;
      pSource++;
      DestOffsets++;
   }

   return pEnd;
}


/* Message
 *
 * Displays a message by loading the strings whose IDs are passed into
 * the function, and substituting the supplied variable argument list
 * using the varargs macros.
 *
 */
int Message(HWND hwnd, DWORD Type, int CaptionID, int TextID, ...)
{
    WCHAR   MsgText[256];
    WCHAR   MsgFormat[256];
    WCHAR   MsgCaption[40];
    va_list vargs;

    if( ( LoadString( hInst, TextID, MsgFormat,
                      sizeof MsgFormat / sizeof *MsgFormat ) > 0 )
     && ( LoadString( hInst, CaptionID, MsgCaption,
                      sizeof MsgCaption / sizeof *MsgCaption ) > 0 ) )
    {
        va_start( vargs, TextID );
        wvsprintf( MsgText, MsgFormat, vargs );
        va_end( vargs );

        return MessageBox(hwnd, MsgText, MsgCaption, Type);
    }
    else
        return 0;
}


/*
 *
 */
LPTSTR
GetErrorString(
    DWORD   Error
)
{
    TCHAR   Buffer[1024];
    LPTSTR  pErrorString = NULL;

    if( FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL, Error, 0, Buffer,
                       sizeof(Buffer), NULL )
      == 0 )

        LoadString( hInst, IDS_UNKNOWN_ERROR, Buffer,
                    sizeof(Buffer) / sizeof(*Buffer) );

    pErrorString = AllocSplStr(Buffer);

    return pErrorString;
}




DWORD ReportError( HWND  hwndParent,
                   DWORD idTitle,
                   DWORD idDefaultError )
{
    DWORD  ErrorID;
    DWORD  MsgType;
    LPTSTR pErrorString;

    ErrorID = GetLastError( );

    if( ErrorID == ERROR_ACCESS_DENIED )
        MsgType = MSG_INFORMATION;
    else
        MsgType = MSG_ERROR;


    pErrorString = GetErrorString( ErrorID );

    Message( hwndParent, MsgType, idTitle,
             idDefaultError, pErrorString );

    FreeSplStr( pErrorString );


    return ErrorID;
}



LPWSTR
AllocSplStr(
    LPWSTR pStr
    )

/*++

Routine Description:

    This function will allocate enough local memory to store the specified
    string, and copy that string to the allocated memory

Arguments:

    pStr - Pointer to the string that needs to be allocated and stored

Return Value:

    NON-NULL - A pointer to the allocated memory containing the string

    FALSE/NULL - The operation failed. Extended error status is available
    using GetLastError.

--*/

{
    LPWSTR pMem;
    DWORD  cbStr;

    if (!pStr) {
        return NULL;
    }

    cbStr = wcslen(pStr)*sizeof(WCHAR) + sizeof(WCHAR);

    if (pMem = AllocSplMem( cbStr )) {
        CopyMemory( pMem, pStr, cbStr );
    }
    return pMem;
}


LPVOID
AllocSplMem(
    DWORD cbAlloc
    )

{
    PVOID pvMemory;

    pvMemory = GlobalAlloc(GMEM_FIXED, cbAlloc);

    if( pvMemory ){
        ZeroMemory( pvMemory, cbAlloc );
    }

    return pvMemory;
}

