/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Init.c

Abstract:

    Contains most of initialization functions

Environment:

    Kernel mode

--*/

#include "precomp.h"

#include "Init.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, HdmiInitializeDeviceExtension)
#pragma alloc_text (PAGE, HdmiPrepareHardware)
#pragma alloc_text (PAGE, HdmiInitializeDMA)
#endif

NTSTATUS
HdmiInitializeDeviceExtension(
    IN PDEVICE_EXTENSION DevExt
    )
/*++
Routine Description:

    This routine is called by EvtDeviceAdd. Here the device context is
    initialized and all the software resources required by the device is
    allocated.

Arguments:

    DevExt     Pointer to the Device Extension

Return Value:

     NTSTATUS

--*/
{
    NTSTATUS    status;
    ULONG       dteCount;
    WDF_IO_QUEUE_CONFIG  queueConfig;

    PAGED_CODE();

    //
    // Set Maximum Transfer Length (which must be less than the SRAM size).
    //
    DevExt->MaximumTransferLength = HDMI_CARD_MAXIMUM_TRANSFER_LENGTH;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                "MaximumTransferLength %d", DevExt->MaximumTransferLength);

    //
    // Calculate the number of DMA_TRANSFER_ELEMENTS + 1 needed to
    // support the MaximumTransferLength.
    //
    dteCount = BYTES_TO_PAGES((ULONG) ROUND_TO_PAGES(
        DevExt->MaximumTransferLength) + PAGE_SIZE) + 16;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Number of DTEs %d", dteCount);

    //
    // Set the number of DMA_TRANSFER_ELEMENTs (DTE) to be available.
    //
    DevExt->WriteTransferElements = dteCount;

    //
    // The PCI9656 has two DMA Channels. This driver will use DMA Channel 0
    // as the "ToDevice" channel (Writes) and DMA Channel 1 as the
    // "From Device" channel (Reads).
    //
    // In order to support "duplex" DMA operation (the ability to have
    // concurrent reads and writes) two Dispatch Queues are created:
    // one for the Write (ToDevice) requests and another for the Read
    // (FromDevice) requests.  While eache Dispatch Queue will operate
    // independently for each other, the requests within a given Dispatch
    // Queue will be serialized. This is hardware can only process one request
    // per DMA Channel at a time.
    //


    //
    // Setup a queue to handle only IRP_MJ_WRITE requests in Sequential
    // dispatch mode. This mode ensures there is only one write request
    // outstanding in the driver at any time. Framework will present the next
    // request only if the current request is completed.
    // Since we have configured the queue to dispatch all the specific requests
    // we care about, we don't need a default queue.  A default queue is
    // used to receive requests that are not preconfigured to goto
    // a specific queue.
    //
    WDF_IO_QUEUE_CONFIG_INIT ( &queueConfig,
                              WdfIoQueueDispatchSequential);

    queueConfig.EvtIoWrite = HdmiEvtIoWrite;
    //queueConfig.EvtIoRead = HdmiEvtIoRead;

    status = WdfIoQueueCreate( DevExt->Device,
                                           &queueConfig,
                                           WDF_NO_OBJECT_ATTRIBUTES,
                                           &DevExt->WriteQueue );

    if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "WdfIoQueueCreate failed: %!STATUS!", status);
        return status;
    }
    //
    // Set the Write Queue forwarding for IRP_MJ_WRITE requests.
    //
    status = WdfDeviceConfigureRequestDispatching( DevExt->Device,
                                       DevExt->WriteQueue,
                                       WdfRequestTypeWrite);



    WDF_IO_QUEUE_CONFIG_INIT ( &queueConfig,
                              WdfIoQueueDispatchSequential);

    queueConfig.EvtIoDeviceControl = HdmiEvtIoDeviceCtr;
    //queueConfig.EvtIoRead = HdmiEvtIoRead;

    status = WdfIoQueueCreate( DevExt->Device,
                                           &queueConfig,
                                           WDF_NO_OBJECT_ATTRIBUTES,
                                           &DevExt->IoctrQueue );

    if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "WdfIoQueueCreate failed: %!STATUS!", status);
        return status;
    }
    //
    // Set the Write Queue forwarding for IRP_MJ_WRITE requests.
    //
    status = WdfDeviceConfigureRequestDispatching( DevExt->Device,
                                       DevExt->IoctrQueue,
                                       WdfRequestTypeDeviceControl);

  DevExt->DMAcompleted = 7;
    //
    // Create a WDFINTERRUPT object.
    //
    status = HdmiInterruptCreate(DevExt);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = HdmiInitializeDMA( DevExt );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    return status;
}


NTSTATUS
HdmiPrepareHardware(
    IN PDEVICE_EXTENSION DevExt,
    IN WDFCMRESLIST     ResourcesTranslated
    )
/*++
Routine Description:

    Gets the HW resources assigned by the bus driver from the start-irp
    and maps it to system address space.

Arguments:

    DevExt      Pointer to our DEVICE_EXTENSION

Return Value:

     None

--*/
{
    ULONG               i;
    NTSTATUS            status = STATUS_SUCCESS;
    CHAR              * bar;

    BOOLEAN             foundRegs      = FALSE;
    PHYSICAL_ADDRESS    regsBasePA     = {0};
    ULONG               regsLength     = 0;

    BOOLEAN             foundSRAM      = FALSE;
    PHYSICAL_ADDRESS    SRAMBasePA     = {0};
    ULONG               SRAMLength     = 0;

    BOOLEAN             foundSRAM2     = FALSE;
    PHYSICAL_ADDRESS    SRAM2BasePA    = {0};
    ULONG               SRAM2Length    = 0;


    PCM_PARTIAL_RESOURCE_DESCRIPTOR  desc;

    PAGED_CODE();

    //
    // Parse the resource list and save the resource information.
    //
    for (i=0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {

        desc = WdfCmResourceListGetDescriptor( ResourcesTranslated, i );

        if(!desc) {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                        "WdfResourceCmGetDescriptor failed");
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

        switch (desc->Type) {

            case CmResourceTypeMemory:

                bar = NULL;

                if (foundSRAM2 && !foundRegs &&
                    desc->u.Memory.Length == HDMI_SRAM_3_SIZE) {

                    regsBasePA = desc->u.Memory.Start;
                    regsLength = desc->u.Memory.Length;
                    foundRegs = TRUE;
                    bar = "BAR2";
                }

                if (foundSRAM && !foundSRAM2 &&
                    desc->u.Memory.Length == HDMI_SRAM_2_SIZE) {

                    SRAM2BasePA = desc->u.Memory.Start;
                    SRAM2Length = desc->u.Memory.Length;
                    foundSRAM2 = TRUE;
                    bar = "BAR1";
                }

                if (!foundSRAM &&
                    desc->u.Memory.Length == HDMI_SRAM_1_SIZE) {

                    SRAMBasePA = desc->u.Memory.Start;
                    SRAMLength = desc->u.Memory.Length;
                    foundSRAM = TRUE;
                    bar = "BAR0";
                }

                TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                            " - Memory Resource [%I64X-%I64X] %s",
                            desc->u.Memory.Start.QuadPart,
                            desc->u.Memory.Start.QuadPart +
                            desc->u.Memory.Length,
                            (bar) ? bar : "<unrecognized>" );
                break;

            case CmResourceTypePort:

                break;

            default:
                //
                // Ignore all other descriptors
                //
                break;
        }
    }  

    if (!(foundRegs && foundSRAM)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "HdmiMapResources: Missing resources");
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    //
    // Map in the Registers Memory resource: BAR2
    //
    DevExt->RegsBase = (PULONG) MmMapIoSpace( regsBasePA,
                                              regsLength,
                                              MmNonCached );

    if (!DevExt->RegsBase) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    " - Unable to map Registers memory %08I64X, length %d",
                    regsBasePA.QuadPart, regsLength);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DevExt->RegsLength = regsLength;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                " - Registers %p, length %d",
                DevExt->RegsBase, DevExt->RegsLength );

    //
    // Set seperated pointer to PCI9656_REGS structure.
    //
    DevExt->Regs = (PHDMICARD_REG) DevExt->RegsBase;

    //
    // Map in the SRAM Memory Space resource: BAR2
    //
   /* DevExt->SRAMBase = (PUCHAR) MmMapIoSpace( SRAMBasePA,
                                              SRAMLength,
                                              MmNonCached );

    if (!DevExt->SRAMBase) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    " - Unable to map SRAM memory %08I64X, length %d",
                    SRAMBasePA.QuadPart,  SRAMLength);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DevExt->SRAMLength = SRAMLength;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                " - SRAM      %p, length %d",
                DevExt->SRAMBase, DevExt->SRAMLength );*/

    return status;
}

WDFDMATRANSACTION   dmaTransactionbuf;

NTSTATUS
HdmiInitializeDMA(
    IN PDEVICE_EXTENSION DevExt
    )
/*++
Routine Description:

    Initializes the DMA adapter.

Arguments:

    DevExt      Pointer to our DEVICE_EXTENSION

Return Value:

     None

--*/
{
    NTSTATUS    status;
    WDF_OBJECT_ATTRIBUTES attributes;

    PAGED_CODE();

    //
    // Hdmi DMA_TRANSFER_ELEMENTS must be 16-byte aligned
    //
    WdfDeviceSetAlignmentRequirement( DevExt->Device,
                                      HDMI_CARD_DTE_ALIGNMENT_16 );

    //
    // Create a new DMA Enabler instance.
    // Use Scatter/Gather, 64-bit Addresses, Duplex-type profile.
    //
    {
        WDF_DMA_ENABLER_CONFIG   dmaConfig;

        WDF_DMA_ENABLER_CONFIG_INIT( &dmaConfig,
                                     WdfDmaProfileScatterGather64,
                                     DevExt->MaximumTransferLength);

        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                    " - The DMA Profile is WdfDmaProfileScatterGather64Duplex");

        status = WdfDmaEnablerCreate( DevExt->Device,
                                      &dmaConfig,
                                      WDF_NO_OBJECT_ATTRIBUTES,
                                      &DevExt->DmaEnabler );

        if (!NT_SUCCESS (status)) {

            TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                        "WdfDmaEnablerCreate failed: %!STATUS!", status);
            return status;
        }
    }

    //
    // Allocate common buffer for building writes
    //
    // NOTE: This common buffer will not be cached.
    //       Perhaps in some future revision, cached option could
    //       be used. This would have faster access, but requires
    //       flushing before starting the DMA in HdmiStartWriteDma.
    //
    DevExt->WriteCommonBuffer1Size =
        sizeof(DMA_TRANSFER_ELEMENT) * DevExt->WriteTransferElements;

    status = WdfCommonBufferCreate( DevExt->DmaEnabler,
                                    DevExt->WriteCommonBuffer1Size,
                                    WDF_NO_OBJECT_ATTRIBUTES,
                                    &DevExt->WriteCommonBuffer1 );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "WdfCommonBufferCreate (write) failed: %!STATUS!", status);
        return status;
    }


    DevExt->WriteCommonBuffer1Base =
        WdfCommonBufferGetAlignedVirtualAddress(DevExt->WriteCommonBuffer1);

    DevExt->WriteCommonBuffer1BaseLA =
        WdfCommonBufferGetAlignedLogicalAddress(DevExt->WriteCommonBuffer1);

    RtlZeroMemory( DevExt->WriteCommonBuffer1Base,
                   DevExt->WriteCommonBuffer1Size);

    status = WdfCommonBufferCreate( DevExt->DmaEnabler,
                                    DevExt->WriteCommonBuffer1Size,
                                    WDF_NO_OBJECT_ATTRIBUTES,
                                    &DevExt->WriteCommonBuffer2 );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "WdfCommonBufferCreate (write) failed: %!STATUS!", status);
        return status;
    }


    DevExt->WriteCommonBuffer2Base =
        WdfCommonBufferGetAlignedVirtualAddress(DevExt->WriteCommonBuffer2);

    DevExt->WriteCommonBuffer2BaseLA =
        WdfCommonBufferGetAlignedLogicalAddress(DevExt->WriteCommonBuffer2);

    RtlZeroMemory( DevExt->WriteCommonBuffer2Base,
                   DevExt->WriteCommonBuffer2Size);	
	

    /*TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                "WriteCommonBuffer 0x%p  (#0x%I64X), length %I64d",
                DevExt->WriteCommonBuffer1Base,
                DevExt->WriteCommonBuffer1BaseLA.LowPart,
                WdfCommonBufferGetLength(DevExt->WriteCommonBuffer1) );*/

    //
    // Since we are using sequential queue and processing one request
    // at a time, we will create transaction objects upfront and reuse
    // them to do DMA transfer. Transactions objects are parented to
    // DMA enabler object by default. They will be deleted along with
    // along with the DMA enabler object. So need to delete them
    // explicitly.
    //

    // WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, TRANSACTION_CONTEXT);
    //
    // Create a new DmaTransaction.
    //
    status = WdfDmaTransactionCreate( DevExt->DmaEnabler,
                                      WDF_NO_OBJECT_ATTRIBUTES,
                                      &DevExt->WriteDmaTransaction );

    if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                    "WdfDmaTransactionCreate(write) failed: %!STATUS!", status);
        return status;
    }
		dmaTransactionbuf = DevExt->WriteDmaTransaction;
    return status;
}


NTSTATUS
HdmiInitWrite(
    IN PDEVICE_EXTENSION DevExt
    )
/*++
Routine Description:

    Initialize write data structures

Arguments:

    DevExt     Pointer to Device Extension

Return Value:

    None

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> HdmiInitWrite");

    //
    // Make sure the Dma0 DAC (Dual Address Cycle) register is set to 0.
    //
    //WRITE_REGISTER_ULONG( (PULONG) &DevExt->Regs->Dma0_PCI_DAC, 0 );

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- HdmiInitWrite");

    return STATUS_SUCCESS;
}


NTSTATUS
HdmiInitRead(
    IN PDEVICE_EXTENSION DevExt
    )
/*++
Routine Description:

    Initialize read data structures

Arguments:

    DevExt     Pointer to Device Extension

Return Value:

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> HdmiInitRead");

    //
    // Make sure the DMA Chan 1 DAC (Dual Address Cycle) is set to 0.
    //
   // WRITE_REGISTER_ULONG( (PULONG) &DevExt->Regs->Dma1_PCI_DAC, 0 );

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- HdmiInitRead");

    return STATUS_SUCCESS;
}

VOID
HdmiShutdown(
    IN PDEVICE_EXTENSION DevExt
    )
/*++

Routine Description:

    Reset the device to put the device in a known initial state.
    This is called from D0Exit when the device is torn down or
    when the system is shutdown. Note that Wdf has already
    called out EvtDisable callback to disable the interrupt.

Arguments:

    DevExt -  Pointer to our adapter

Return Value:

    None

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "---> HdmiShutdown");

    //
    // WdfInterrupt is already disabled so issue a full reset
    //
   /* if (DevExt->Regs) {

        HdmiHardwareReset(DevExt);
    }*/

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<--- HdmiShutdown");
}





