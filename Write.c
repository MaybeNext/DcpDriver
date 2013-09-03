/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Write.c

Abstract:


Environment:

    Kernel mode

--*/

#include "precomp.h"

#include "Write.tmh"


ULONG_PTR pa_buf;
ULONG     dma_num_buf;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
VOID
HdmiEvtIoWrite(
    IN WDFQUEUE         Queue,
    IN WDFREQUEST       Request,
    IN size_t            Length
    )
/*++

Routine Description:

    Called by the framework as soon as it receives a write request.
    If the device is not ready, fail the request.
    Otherwise get scatter-gather list for this request and send the
    packet to the hardware for DMA.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    Length - Length of the IO operation
                 The default property of the queue is to not dispatch
                 zero lenght read & write requests to the driver and
                 complete is with status success. So we will never get
                 a zero length request.

Return Value:

--*/
{
    NTSTATUS          status = STATUS_UNSUCCESSFUL;
    PDEVICE_EXTENSION devExt = NULL;


    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                "--> HdmiEvtIoWrite: Request %p", Request);

    //
    // Get the DevExt from the Queue handle
    //

	
    devExt = HdmiGetDeviceContext(WdfIoQueueGetDevice(Queue));
		devExt->Request = Request;
    //
    // Validate the Length parameter.
    //
    if (Length > HDMI_SRAM_SIZE)  {
        status = STATUS_INVALID_BUFFER_SIZE;
        goto CleanUp;
    }
			if (devExt->DMAcompleted == 7)
			{
					devExt->DMAcompleted = 1;
			}
			else if (devExt->DMAcompleted == 1)
			{
					WdfDmaTransactionDmaCompleted( devExt->WriteDmaTransaction,
                                                       &status );
			}    
    //
    // Following code illustrates two different ways of initializing a DMA
    // transaction object. If ASSOC_WRITE_REQUEST_WITH_DMA_TRANSACTION is
    // defined in the sources file, the first section will be used.
    //
#ifdef ASSOC_WRITE_REQUEST_WITH_DMA_TRANSACTION

    //
    // This section illustrates how to create and initialize
    // a DmaTransaction using a WDF Request.
    // This type of coding pattern would probably be the most commonly used
    // for handling client Requests.
    //
    status = WdfDmaTransactionInitializeUsingRequest(
                                           devExt->WriteDmaTransaction,
                                           Request,
                                           HdmiEvtProgramWriteDma,
                                           WdfDmaDirectionWriteToDevice );

    if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                    "WdfDmaTransactionInitializeUsingRequest failed: "
                    "%!STATUS!", status);
        goto CleanUp;
    }
#else
    //
    // This section illustrates how to create and initialize
    // a DmaTransaction via direct parameters (e.g. not using a WDF Request).
    // This type of coding pattern might be used for driver-initiated DMA
    // operations (e.g. DMA operations not based on driver client requests.)
    //
    // NOTE: This example unpacks the WDF Request in order to get a set of
    //       parameters for the call to WdfDmaTransactionInitialize. While
    //       this is completely legimate, the represenative usage pattern
    //       for WdfDmaTransactionIniitalize would have the driver create/
    //       initialized a DmaTransactin without a WDF Request. A simple
    //       example might be where the driver needs to DMA the devices's
    //       firmware to it during device initialization. There would be
    //       no WDF Request; the driver would supply the parameters for
    //       WdfDmaTransactionInitialize directly.
    //
    {
        PTRANSACTION_CONTEXT  transContext;
        PMDL                  mdl;
        PVOID                 virtualAddress;
        ULONG                 length;
				
				
				
        //
        // Initialize this new DmaTransaction with direct parameters.
        //
        status = WdfRequestRetrieveInputWdmMdl(Request, &mdl);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                        "WdfRequestRetrieveInputWdmMdl failed: %!STATUS!", status);
            goto CleanUp;
        }

        virtualAddress = MmGetMdlVirtualAddress(mdl);
        length = MmGetMdlByteCount(mdl);

        status = WdfDmaTransactionInitialize( devExt->WriteDmaTransaction,
                                              HdmiEvtProgramWriteDma,
                                              WdfDmaDirectionWriteToDevice,
                                              mdl,
                                              virtualAddress,
                                              length );

        if(!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                        "WdfDmaTransactionInitialize failed: %!STATUS!", status);
              goto CleanUp;
        }

        //
        // Retreive this DmaTransaction's context ptr (aka TRANSACTION_CONTEXT)
        // and fill it in with info.
        //
        transContext = HdmiGetTransactionContext( devExt->WriteDmaTransaction );
        transContext->Request = Request;
    }
#endif

#if 0 //FYI
        //
        // Modify the MaximumLength for this DmaTransaction only.
        //
        // Note: The new length must be less than or equal to that set when
        //       the DmaEnabler was created.
        //
        {
            ULONG length =  devExt->MaximumTransferLength / 2;

            //TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
            //            "Setting a new MaxLen %d", length);

            WdfDmaTransactionSetMaximumLength( devExt->WriteDmaTransaction, length );
        }
#endif

	

    //
    // Execute this DmaTransaction transaction.
    //
    status = WdfDmaTransactionExecute( devExt->WriteDmaTransaction, 
                                       WDF_NO_CONTEXT);


    if(!NT_SUCCESS(status)) {

        //
        // Couldn't execute this DmaTransaction, so fail Request.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                    "WdfDmaTransactionExecute failed: %!STATUS!", status);
        goto CleanUp;
    }

    //
    // Indicate that Dma transaction has been started successfully. The request
    // will be complete by the Dpc routine when the DMA transaction completes.
    //
    status = STATUS_SUCCESS;

CleanUp:

    //
    // If there are errors, then clean up and complete the Request.
    //
    if (!NT_SUCCESS(status)) {
        WdfDmaTransactionRelease(devExt->WriteDmaTransaction);        
        WdfRequestComplete(Request, status);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                "<-- HdmiEvtIoWrite: %!STATUS!", status);

    return;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
BOOLEAN
HdmiEvtProgramWriteDma(
    IN  WDFDMATRANSACTION       Transaction,
    IN  WDFDEVICE               Device,
    IN  PVOID                   Context,
    IN  WDF_DMA_DIRECTION       Direction,
    IN  PSCATTER_GATHER_LIST    SgList
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    PDEVICE_EXTENSION        devExt;
    size_t                   offset = 0;
    size_t                   bytecount = 0;
    unsigned int             dmacount = 0;
    PDMA_TRANSFER_ELEMENT    dteVA;
    ULONG_PTR                dteLA;
    BOOLEAN                  errors;
    ULONG                    i;

    UNREFERENCED_PARAMETER( Context );
    UNREFERENCED_PARAMETER( Direction );

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                "--> HdmiEvtProgramWriteDma");

    //
    // Initialize locals
    //
    devExt = HdmiGetDeviceContext(Device);
    errors = FALSE;

    //
    // Get the number of bytes as the offset to the beginning of this
    // Dma operations transfer location in the buffer.
    //
     offset = WdfDmaTransactionGetBytesTransferred(Transaction);



        //
        // Setup the pointer to the next DMA_TRANSFER_ELEMENT
        // for both virtual and physical address references.
        //
        dteVA = (PDMA_TRANSFER_ELEMENT) devExt->WriteCommonBuffer1Base + 16;
        dteLA = (((ULONG_PTR)devExt->WriteCommonBuffer1BaseLA.HighPart << 32) | devExt->WriteCommonBuffer1BaseLA.LowPart);
		 devExt->CommonBufferPA = dteLA;
		 devExt->DmaNumber = SgList->NumberOfElements;
		 
		pa_buf = dteLA;
		dma_num_buf = SgList->NumberOfElements;

        for (i=0; i < SgList->NumberOfElements; i++) 
        {

                if (i == (SgList->NumberOfElements-1))
                {  
                    dteVA->DescPtr = 0x40000000 | (SgList->Elements[i].Length >> 2);
                }
                else
                {
                    dteVA->DescPtr = (SgList->Elements[i].Length >> 2);
                }


            //
            // Construct this DTE.
            //
            // NOTE: The LocalAddress is the offset into the SRAM from
            //       where this Write will start.
            //
            dteVA->HostAddressLow  = SgList->Elements[i].Address.LowPart;
            dteVA->HostAddressHigh = SgList->Elements[i].Address.HighPart;

            dteVA->DeviceAddress   = (ULONG) offset;


            //
            // Increment the DmaTransaction length by this element length
            //
            offset += SgList->Elements[i].Length;

            //
            // Adjust the next DMA_TRANSFER_ELEMEMT
            //
            dteVA++;
           // dteLA += sizeof(DMA_TRANSFER_ELEMENT);
        }

	//WdfRequestMarkCancelable(devExt->Request, HdmiEvtRequestCancel);
	
		WdfInterruptAcquireLock( devExt->Interrupt );
		
    WRITE_REGISTER_ULONG( (PULONG)&devExt->Regs->WriteCtr.CtrBit,
            0xffffffff );

    WRITE_REGISTER_ULONG( (PULONG)&devExt->Regs->WriteCtr.CtrBit,
                            0x00000 | SgList->NumberOfElements );
  
    WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->WriteCtr.DescTableAddressHigh,
                            dteLA >> 32 );
 
    WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->WriteCtr.DescTableAddressLow,
                            dteLA & 0xffffffff );
 
    WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->WriteCtr.LastDesc,
                            SgList->NumberOfElements - 1 );
	
		WdfInterruptReleaseLock( devExt->Interrupt );
    //
    // NOTE: This shows how to process errors which occur in the
    //       PFN_WDF_PROGRAM_DMA function in general.
    //       Basically the DmaTransaction must be deleted and
    //       the Request must be completed.
    //
    if (errors) {
        //
        // Must abort the transaction before deleting it.
        //
        NTSTATUS status;

        (VOID) WdfDmaTransactionDmaCompletedFinal(Transaction, 0, &status);
        ASSERT(NT_SUCCESS(status));
        HdmiWriteRequestComplete( Transaction, STATUS_INVALID_DEVICE_STATE );
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                    "<-- HdmiEvtProgramWriteDma: error ****");
        return FALSE;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                "<-- HdmiEvtProgramWriteDma");

    return TRUE;
}


VOID
HdmiWriteRequestComplete(
    IN WDFDMATRANSACTION  DmaTransaction,
    IN NTSTATUS           Status
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    WDFDEVICE          device;
    WDFREQUEST         request;
    PDEVICE_EXTENSION  devExt;
    size_t             bytesTransferred;

    //
    // Initialize locals
    //
    device  = WdfDmaTransactionGetDevice(DmaTransaction);
    devExt  = HdmiGetDeviceContext(device);

#ifdef ASSOC_WRITE_REQUEST_WITH_DMA_TRANSACTION

    request = WdfDmaTransactionGetRequest(DmaTransaction);

#else
    //
    // If CreateDirect was used then there will be no assoc. Request.
    //
    {
        PTRANSACTION_CONTEXT transContext = HdmiGetTransactionContext(DmaTransaction);

        request = transContext->Request;
        transContext->Request = NULL;
        
    }
#endif

    //
    // Get the final bytes transferred count.
    //
    bytesTransferred =  WdfDmaTransactionGetBytesTransferred( DmaTransaction );

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_DPC,
                "HdmiWriteRequestComplete:  Request %p, Status %!STATUS!, "
                "bytes transferred %d\n",
                 request, Status, (int) bytesTransferred );

    WdfDmaTransactionRelease(DmaTransaction);        

    WdfRequestCompleteWithInformation( request, Status, bytesTransferred);

}


/*void HdmiEvtRequestCancel(IN WDFREQUEST Request)
{
	WDFDEVICE	device;
    NTSTATUS            status;
    PDEVICE_EXTENSION   devExt;
    WDFDMATRANSACTION   dmaTransaction;
	BOOLEAN transactionComplete;

	device = WdfIoQueueGetDevice(WdfRequestGetIoQueue(Request));
	devExt = HdmiGetDeviceContext(device);

	WdfRequestUnmarkCancelable(Request);

	transactionComplete = WdfDmaTransactionDmaCompleted( devExt->WriteDmaTransaction,
                                                         &status );	
	if (transactionComplete)
	{
    	WdfDmaTransactionRelease(devExt->WriteDmaTransaction);        

	    WdfRequestCompleteWithInformation( Request, STATUS_CANCELLED, 0);
	}
}
*/
