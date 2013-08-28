/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    DeviceCtr.c

Abstract:


Environment:

    Kernel mode

--*/

#include "precomp.h"

#include "DeviceCtr.tmh"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
VOID HdmiEvtIoDeviceCtr(
  __in  WDFQUEUE Queue,
  __in  WDFREQUEST Request,
  __in  size_t OutputBufferLength,
  __in  size_t InputBufferLength,
  __in  ULONG IoControlCode
)
{ 
    NTSTATUS          status = STATUS_UNSUCCESSFUL;
    PDEVICE_EXTENSION DevExt = NULL;
    PVOID  UserModeVirtualAddress = NULL;
    PVOID  RecieveBuf;
    PMDL BufferMdl;
    ULONG i;
    KIRQL  IrqlOld;
    KIRQL  Irql;	
    //
    // Get the DevExt from the Queue handle
    //
    DevExt = HdmiGetDeviceContext(WdfIoQueueGetDevice(Queue));

    switch (IoControlCode)
    {
		case IOCTL_GET_BUFFERADDRESS1:
			//CreateAndMapMemory(&BufferMdl, &UserModeVirtualAddress);


			if (!UserModeVirtualAddress)
			{
				WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 0); 
				break;
			}
			
			WdfRequestRetrieveOutputBuffer(Request, 8, &RecieveBuf, NULL);
			if (!NT_SUCCESS(status)) 
			{
				break;
			}
			(USHORT *)RecieveBuf = UserModeVirtualAddress;
			WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 8); 
			break;
		default:
			break;
    }

}




