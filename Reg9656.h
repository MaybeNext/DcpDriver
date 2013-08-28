#ifndef __REG_H_
#define __REG_H_

//*****************************************************************************
//  
//  File Name: Reg9656.h
// 
//  Description:  This file defines all the PLX 9656 chip Registers.
// 
//  NOTE: These definitions are for memory-mapped register access only.
//
//*****************************************************************************

//-----------------------------------------------------------------------------   
// PCI Device/Vendor Ids.
//-----------------------------------------------------------------------------   
#define HDMI_CARD_VENDOR_ID               0x1172
#define HDMI_CARD_DEVICE_ID               0xE001
                                        
//-----------------------------------------------------------------------------   
// Expected size of the PCI9656RDK-Lite on-board SRAM
//-----------------------------------------------------------------------------   
#define HDMI_SRAM_1_SIZE                  (0x10000000)
#define HDMI_SRAM_2_SIZE                  (0x40000)
#define HDMI_SRAM_3_SIZE                  (0x40000)
                                                   
//-----------------------------------------------------------------------------   
// Maximum DMA transfer size (in bytes).
//
// NOTE: This value is rather abritrary for this drive, 
//       but must be between [0 - PCI9656_SRAM_SIZE] in value.
//       You can play with the value to see how requests are sequenced as a
//       set of one or more DMA operations.
//-----------------------------------------------------------------------------   
#define HDMI_CARD_MAXIMUM_TRANSFER_LENGTH    (16*1024*1024)

//-----------------------------------------------------------------------------   
// The DMA_TRANSFER_ELEMENTS (the 9656's hardware scatter/gather list element)
// must be aligned on a 16-byte boundry.  This is because the lower 4 bits of
// the DESC_PTR.Address contain bit fields not included in the "next" address.
//-----------------------------------------------------------------------------   
#define HDMI_CARD_DTE_ALIGNMENT_16      FILE_OCTA_ALIGNMENT 

//-----------------------------------------------------------------------------   
// DMA Transfer Element (DTE)
// 
// NOTE: This structure is modeled after the DMAPADRx, DMALADRx, DMASIZx and 
//       DMAADPRx registers. See DataBook Registers description: 11-74 to 11-77.
//-----------------------------------------------------------------------------   
typedef struct _DESC_PTR_ { 

    unsigned int     DmaLength  : 16 ;  
    unsigned int     MsiEna     : 1  ;  
    unsigned int     EplastEna  : 1  ;  
    unsigned int     Reserved   : 14 ; 
    
} DESC_PTR;


typedef struct _DMA_TRANSFER_ELEMENT {

    unsigned int       DescPtr         ;
	unsigned int       DeviceAddress   ;
    unsigned int       HostAddressHigh ;
    unsigned int       HostAddressLow  ;

} DMA_TRANSFER_ELEMENT, * PDMA_TRANSFER_ELEMENT;


typedef struct _DMA_CTR_ { 

    unsigned int     NumOfDescTable  : 16  ;  
    unsigned int     Reserved	     : 1  ;  
    unsigned int     MsiEna		     : 1  ;  
    unsigned int     EplastEna	     : 1  ;  
    unsigned int     MsiNum          : 5 ;
    unsigned int     Reserved1	     : 3  ;
    unsigned int     MsiTrafficClass : 3 ;
    unsigned int     DmaLoop         : 1 ;

} DMA_CTR;

typedef struct _DMA_TRANSFER_CTR_ {

    unsigned int	   CtrBit         ;
    unsigned int       DescTableAddressHigh ;
	unsigned int       DescTableAddressLow  ;
    unsigned int       LastDesc         ;

} DMA_TRANSFER_CTR, * PDMA_TRANSFER_CTR;

typedef struct _HDMICARD_REG_  {

	DMA_TRANSFER_CTR   ReadCtr;
	DMA_TRANSFER_CTR   WriteCtr;


}HDMICARD_REG, *PHDMICARD_REG;


typedef struct _PER_DMA_TRANSFER_ {

    unsigned int      DescTableAddressHigh;
    unsigned int      DescTableAddressLow;
    unsigned int      DescNum;
    unsigned int      ByteCnt;

}PER_DMA_TRANSFER, * PPER_DMA_TRANSFER;
    
#endif  // __REG_H_

