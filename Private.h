/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Private.h

Abstract:

Environment:

    Kernel mode

--*/


#if !defined(_HDMI_H_)
#define _HDMI_H_
#define ASSOC_WRITE_REQUEST_WITH_DMA_TRANSACTION   1
//
// The device extension for the device object
//
typedef struct _DEVICE_EXTENSION {

    WDFDEVICE               Device;

    // Following fields are specific to the hardware
    // Configuration

    // HW Resources
    PHDMICARD_REG            Regs;             // Registers address
    PULONG                  RegsBase;         // Registers base address
    ULONG                   RegsLength;       // Registers base length

    PULONG                  SRAMBase;         // SRAM base address
    ULONG                   SRAMLength;       // SRAM base length

    PULONG                  SRAM2Base;        // SRAM (alt) base address
    ULONG                   SRAM2Length;      // SRAM (alt) base length

    WDFINTERRUPT            Interrupt;     // Returned by InterruptCreate


    // DmaEnabler
    WDFDMAENABLER           DmaEnabler;
    ULONG                   MaximumTransferLength;

	WDFREQUEST       Request;

    // Write
    WDFQUEUE                WriteQueue;

    WDFDMATRANSACTION       WriteDmaTransaction;

    ULONG                   WriteTransferElements;
    WDFCOMMONBUFFER         WriteCommonBuffer1;
    size_t                  WriteCommonBuffer1Size;
    PULONG                  WriteCommonBuffer1Base;
    PHYSICAL_ADDRESS        WriteCommonBuffer1BaseLA;  // Logical Address
    WDFCOMMONBUFFER         WriteCommonBuffer2;
    size_t                  WriteCommonBuffer2Size;
    PULONG                  WriteCommonBuffer2Base;
    PHYSICAL_ADDRESS        WriteCommonBuffer2BaseLA;  // Logical Address

    WDFQUEUE                IoctrQueue;

    ULONG                   HwErrCount;
		
	ULONG_PTR				 CommonBufferPA;
	ULONG						 DmaNumber;
	ULONG						 DMAcompleted;

}  DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// This will generate the function named HdmiGetDeviceContext to be use for
// retreiving the DEVICE_EXTENSION pointer.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION, HdmiGetDeviceContext)

#if !defined(ASSOC_WRITE_REQUEST_WITH_DMA_TRANSACTION)
//
// The context structure used with WdfDmaTransactionCreate
//
typedef struct _TRANSACTION_CONTEXT_ {

    WDFREQUEST     Request;

} TRANSACTION_CONTEXT, * PTRANSACTION_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(TRANSACTION_CONTEXT, HdmiGetTransactionContext)

#endif

//
// Function prototypes
//
DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD HdmiEvtDeviceAdd;

EVT_WDF_OBJECT_CONTEXT_CLEANUP HdmiEvtDriverContextCleanup;

EVT_WDF_DEVICE_D0_ENTRY HdmiEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT HdmiEvtDeviceD0Exit;
EVT_WDF_DEVICE_PREPARE_HARDWARE HdmiEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE HdmiEvtDeviceReleaseHardware;

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL HdmiEvtIoDeviceCtr;
EVT_WDF_IO_QUEUE_IO_WRITE HdmiEvtIoWrite;

EVT_WDF_INTERRUPT_ISR HdmiEvtInterruptIsr;
EVT_WDF_INTERRUPT_DPC HdmiEvtInterruptDpc;
EVT_WDF_INTERRUPT_ENABLE HdmiEvtInterruptEnable;
EVT_WDF_INTERRUPT_DISABLE HdmiEvtInterruptDisable;

NTSTATUS
HdmiSetIdleAndWakeSettings(
    IN PDEVICE_EXTENSION FdoData
    );

NTSTATUS
HdmiInitializeDeviceExtension(
    IN PDEVICE_EXTENSION DevExt
    );

NTSTATUS
HdmiPrepareHardware(
    IN PDEVICE_EXTENSION DevExt,
    IN WDFCMRESLIST     ResourcesTranslated
    );

NTSTATUS
HdmiInitRead(
    IN PDEVICE_EXTENSION DevExt
    );

NTSTATUS
HdmiInitWrite(
    IN PDEVICE_EXTENSION DevExt
    );

//
// WDFINTERRUPT Support
//
NTSTATUS
HdmiInterruptCreate(
    IN PDEVICE_EXTENSION DevExt
    );

VOID
HdmiReadRequestComplete(
    IN WDFDMATRANSACTION  DmaTransaction,
    IN NTSTATUS           Status
    );

VOID
HdmiWriteRequestComplete(
    IN WDFDMATRANSACTION  DmaTransaction,
    IN NTSTATUS           Status
    );

NTSTATUS
HdmiInitializeHardware(
    IN PDEVICE_EXTENSION DevExt
    );

VOID
HdmiShutdown(
    IN PDEVICE_EXTENSION DevExt
    );

EVT_WDF_PROGRAM_DMA HdmiEvtProgramReadDma;
EVT_WDF_PROGRAM_DMA HdmiEvtProgramWriteDma;

VOID
HdmiHardwareReset(
    IN PDEVICE_EXTENSION    DevExt
    );

NTSTATUS
HdmiInitializeDMA(
    IN PDEVICE_EXTENSION DevExt
    );


NTSTATUS 
CreateAndMapMemory(
    OUT PMDL* PMemMdl, OUT PVOID* UserVa
    );

void 
HdmiEvtRequestCancel(IN WDFREQUEST Request);

#pragma warning(disable:4127) // avoid conditional expression is constant error with W4

#endif  // _PCI9656_H_


