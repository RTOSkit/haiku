//------------------------------------------------------------------------------
//	Copyright (c) 2001-2002, Niels S. Reedijk
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.

// Parts of the code: Copyright (c) 1998 Be, Inc. All Rights Reserved

/* ++++++++++
	driver.c
	Implementation of the Realtek 8139 Chipset
+++++ */

#include <KernelExport.h>
#include <Drivers.h>
#include <Errors.h>
#include <PCI.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "ether_driver.h"
#include "util.h"

#define RTL_MAX_CARDS 4

#define RTL_NODEBUG
#ifdef RTL_NODEBUG
#define TRACE no_printf
void no_printf( const char *useless , ... ) {};
#else
#define TRACE TRACE
#endif

/* ----------
	global data
----- */
static pci_info *m_devices[RTL_MAX_CARDS];
static pci_module_info *m_pcimodule = 0;	//To call methods of pci
static char *rtl8139_names[RTL_MAX_CARDS +1];
static int32 m_openmask = 0;					//Is the thing already opened?


int32 api_version = B_CUR_DRIVER_API_VERSION; //Procedure

//Forward declaration
static int32 rtl8139_interrupt(void *data);    /* interrupt handler */

enum registers {
	IDR0 = 0x0 ,
	Command = 0x37 ,
	TxConfig = 0x40 ,
	RxConfig = 0x44 ,
	Config1 = 0x52 , 
	BMCR = 0x62 , 		//BAsic Mode Configuration Register
	BMSR = 0x64 ,
	RBSTART = 0x30 ,
	IMR = 0x3C ,		//Interrupt mask registers
	ISR = 0x3E ,
	_9346CR = 0x50 , 	// 9346 Configuration register
	Config4 = 0x5a ,
	TSD0 = 0x10 ,
	TSD1 = 0x14 ,
	MULINT = 0x5c ,		//Multiple interrupt 
	TSD2 = 0x18 ,
	TSD3 = 0x1C ,
	ESRS = 0x36 ,
	TSAD0 = 0x20 ,
	TSAD1 = 0x24 ,
	TSAD2 = 0x28 ,
	TSAD3 = 0x2C ,
	TSAD = 0x60 ,		//Transmit Status of All Descriptors
	CAPR = 0x38 ,		//Tail pointer for buffer thingie
	CBR = 0x3A ,		//Offset in rxbuffer of next packet
	MPC = 0x4C ,
	MAR0 = 0x8			//Multicast register
	
};


enum Command_actions {
	Reset = 0x10 ,				// 5th bit 
	EnableReceive = 0x08 ,		// 4th bit
	EnableTransmit = 0x04 ,		// 3rd bit
	BUFE = 0x01
};

enum Transmitter_actions {
	MXDMA_2 = 0x400 ,			// 11th bit
	MXDMA_1 = 0x200 ,			// 10th bit
	MXDMA_0 = 0x100	,			// 9th bit
	IFG_1 = 0x2000000 ,			// 26th bit
	IFG_0 = 0x1000000 			// 25th bit
};

enum Receiver_actions {
	RXFTH2 = 0x8000, 			// 16th bit
	RXFTH1 = 0x4000, 			// 15th bit
	RXFTH0 = 0x2000 ,           // 14th bit
	RBLEN_1 = 0x1000 ,			// 13rd bit
	RBLEN_0 = 0x800	,			// 12th bit
	// MXDMA equal to transmitter
	WRAP = 0x100 ,				// 8th bit
	AB = 0x8 ,					// 4rd bit
	AM = 0x4 , 					// 3rd bit
	APM = 0x2					// 2nd bit
};

enum TransmitDescription {
	OWN = 0x2000 ,
	TUN = 0x4000 ,
	TOK = 0x8000 ,
	OWC = 0x20000000 ,
	TABT = 0x40000000 ,
	CRS = 0x80000000
};
	

enum InterruptStatusBits {
	ReceiveOk = 0x01 ,
	ReceiveError = 0x02 ,
	TransmitOk = 0x04 ,
	TransmitError = 0x08 ,
	ReceiveOverflow = 0x10 ,
	ReceiveUnderflow = 0x20 ,
	ReceiveFIFOOverrun = 0x40 ,
	TimeOut = 0x4000 ,
	SystemError = 0x8000
};

enum BMCR_Commands {
	ANE = 0x2000 , 				// Enable auto config
	Duplex_Mode = 0x100 ,
	Restart_Auto_Negotiation = 0x400
};

typedef enum chiptype { RTL_8139_C , RTL_8139_D , RTL_8101_L } chiptype;

/* ----------
	structure that stores internal data
---- */

typedef struct rtl8139_properties
{
	pci_info 	*pcii;				/* Pointer to PCI Info for the device */
	uint32 		reg_base; 			/* Base address for registers */
	area_id		ioarea;				/* PPC: Area where the mmaped registers are */
	chiptype	chip_type;			/* Storage for the chip type */
	uint8		device_id;			/* Which device id this is... */
	
	area_id		receivebuffer;		/* Memoryaddress for the receive buffer */
	void 		*receivebufferlog;	/* Logical address */
	void		*receivebufferphy;	/* Physical address */
	uint16		receivebufferoffset;/* Offset for the next package */
	
	uint8 		writes;				/* Number of writes (0, maximum 4) */
	area_id		transmitbuffer[4];	/* Transmitbuffers */
	void		*transmitbufferlog[4]; /* Logical addresses of the transmit buffer */
	void		*transmitbufferphy[4]; /* Physical addresses of the transmit buffer */
	uint8		transmitstatus[4];	/* Transmitstatus: 0 means empty and 1 means in use */
	uint32		queued_packets;		/* Number of queued tx that have been written */
	uint32		finished_packets;	/* Number of finished transfers */
	
	uint8		multiset;			/* determines if multicast address is set */
	ether_address_t address;		/* holds the MAC address */
	sem_id 		lock;				/* lock this structure: still interrupt */
	sem_id		input_wait;			/* locked until there is a packet to be read */
	uint8		nonblocking;		/* determines if the card blocks on read requests */
} rtl8139_properties_t;

typedef struct packetheader
{
	volatile uint16		bits;				/* Status bits of the packet header */
	volatile uint16		length;				/* Length of the packet including header + CRC */
	volatile uint8		data[1];
} packetheader_t;

static status_t close_hook( void * );
/* -----
	Here all platform dependant code is placed: this keeps the code clean
----- */
#ifdef __INTEL__
	#define WRITE_8(  offset , value)	(m_pcimodule->write_io_8 ((data->reg_base + (offset)), (value) ) )
	#define WRITE_16( offset , value)	(m_pcimodule->write_io_16((data->reg_base + (offset)), (value) ) )
	#define WRITE_32( offset , value)	(m_pcimodule->write_io_32((data->reg_base + (offset)), (value) ) )
	
	#define READ_8(  offset )			(m_pcimodule->read_io_8 ((data->reg_base + offset)))
	#define READ_16( offset )			(m_pcimodule->read_io_16((data->reg_base + offset)))
	#define READ_32( offset )			(m_pcimodule->read_io_32((data->reg_base + offset)))

	void rtl8139_init_registers( rtl8139_properties_t *data )
	{
		data->reg_base = data->pcii->u.h0.base_registers[0];
		//TRACE( "NIELX@@: registers %lu\n" , data->reg_base );
	}
#else /* PPC */
	#include <ByteOrder.h>
	#define WRITE_8(  offset , value)	(*((volatile uint8 *)(data->reg_base + (offset))) = (value))
	#define WRITE_16( offset , value)	(*((volatile uint8 *)(data->reg_base + (offset))) = B_HOST_TO_LENDIAN_INT16(value))
	#define WRITE_32( offset , value)	(*((volatile uint8 *)(data->reg_base + (offset))) = B_HOST_TO_LENDIAN_INT32(value))
	
	#define READ_8(  offset )			(*((volatile uint8*)(data->reg_base + (offset))))
	#define READ_16( offset )			B_LENDIAN_TO_HOST_INT16(*((volatile uint16*)(data->reg_base + (offset))))
	#define READ_32( offset )			B_LENDIAN_TO_HOST_INT32(*((volatile uint32*)(data->reg_base + (offset))))
	
	void rtl8139_init_registers( rtl8139_properties_t *data )
	{
		int32 base, size, offset;
		base = data->pcii->u.h0.base_registers[0];
		size = data->pcii->u.h0.base_register_sizes[0];
		
		/* Round down to nearest page boundary */
		base = base & ~(B_PAGE_SIZE-1);
		
		/* Adjust the size */
		offset = data->pcii->u.h0.base_registers[0] - base;
		size += offset;
		size = (size +(B_PAGE_SIZE-1)) & ~(B_PAGE_SIZE-1);

		TRACE("rtl8139_nielx _open_hook(): PCI base=%lx size=%lx offset=%lx\n", base, size, offset);
		
		data->ioarea = map_physical_memory("rtl8139_regs", (void *)base, size, B_ANY_KERNEL_ADDRESS, B_READ_AREA | B_WRITE_AREA, (void **)&data->reg_base);

		data->reg_base = data->reg_base + offset;
	}
#endif


/* ----------
	init_hardware - called once the first time the driver is loaded
----- */
status_t
init_hardware (void)
{
	// Nielx: no special requirements here...
	TRACE( "rtl8139_nielx: init_hardware\n" );
	return B_OK;
}


/* ----------
	init_driver - optional function - called every time the driver
	is loaded.
----- */
status_t
init_driver (void)
{
	status_t status; 		//Storage for statuses
	pci_info *item;			//Storage used while looking through pci
	int32 i, found;			//Counter

	TRACE( "rtl8139_nielx: init_driver()\n" );
	
	// Try if the PCI module is loaded (it would be weird if it wouldn't, but alas)
	if( ( status = get_module( B_PCI_MODULE_NAME, (module_info **)&m_pcimodule )) != B_OK) 
	{
		TRACE( "rtl8139_nielx init_driver(): Get PCI module failed! %lu \n", status);
		return status;
	}
	
	// 
	i = 0;
	item = (pci_info *)malloc(sizeof(pci_info));
	for ( i = found = 0 ; m_pcimodule->get_nth_pci_info(i, item) == B_OK ; i++ )
	{
		// Vendorid = 0x10ec and device_id = 0x8139
		if ( ( item->vendor_id == 0x10ec ) && ( item->device_id == 0x8139 ) )
		{
			//Also done in etherpci sample code
			if ((item->u.h0.interrupt_line == 0) || (item->u.h0.interrupt_line == 0xFF)) 
			{
				TRACE( "rtl8139_nielx init_driver(): found with invalid IRQ - check IRQ assignement\n");
				i++; //next
				continue;
			}
			TRACE("rtl8139_nielx init_driver(): found at IRQ %u \n", item->u.h0.interrupt_line);
			m_devices[found] = item;
			item = (pci_info *)malloc(sizeof(pci_info));
			found++;
		}
	}
	
	free( item );
	
	//Check if we have found any devices:
	if ( found == 0 )
	{
		TRACE( "rtl8139_nielx init_driver(): no device found\n" );
		put_module(B_PCI_MODULE_NAME ); //dereference module
		return ENODEV;
	}
	
	//Create the devices list
	{
		char name[32];
	
		for (i = 0; i < found; i++) 
		{
			sprintf(name, "net/rtl8139/%ld", i);
			rtl8139_names[i] = strdup(name);
		}
		rtl8139_names[i] = NULL;
	}		
	return B_OK;
}


/* ----------
	uninit_driver - optional function - called every time the driver
	is unloaded
----- */
void
uninit_driver (void)
{
	int index;
	void *item;
	TRACE( "rtl8139_nielx: uninit_driver()\n" );
	
	for (index = 0; (item = rtl8139_names[index]) != NULL; index++) 
	{
		free(item);
		free(m_devices[index]);
	}

	put_module( B_PCI_MODULE_NAME );
}

	
/* ----------
	open_hook - handle open() calls
----- */

//FWD declaration:
static status_t free_hook( void *cookie );

static status_t
open_hook(const char *name, uint32 flags, void** cookie)
{

	rtl8139_properties_t *data;
	uint8 temp8;
	uint16 temp16;
	uint32 temp32;
	unsigned char cmd;

	TRACE( "rtl8139_nielx open_hook()\n" );
	
	// verify device access
	{
		char *thisName;
		int32 mask;

		// search for device name
		for (temp8 = 0; (thisName = rtl8139_names[temp8]) != NULL; temp8++) {
			if (!strcmp(name, thisName))
				break;
		}
		if (!thisName)
			return EINVAL;
	
		// check if device is already open
		mask = 1L << temp8;
		if (atomic_or(&m_openmask, mask) & mask)
			return B_BUSY;
	}
	
	//Create a structure that contains the internals
	if (!(*cookie = data = (rtl8139_properties_t *)malloc(sizeof(rtl8139_properties_t)))) 
	{
		TRACE( "rtl8139_nielx open_hook(): Out of memory\n" );
		return B_NO_MEMORY;
	}

	//Set status to open:
	m_openmask &= ~( 1L << temp8 );
	
	//Clear memory
	memset( data , 0 , sizeof( rtl8139_properties_t ) );
	
	//Set the ID
	data->device_id = temp8;
	
	// Create lock
	data->lock = create_sem( 1 , "rtl8139_nielx data protect" );
	set_sem_owner( data->lock , B_SYSTEM_TEAM );
	data->input_wait = create_sem( 0 , "rtl8139_nielx read wait" );
	set_sem_owner( data->input_wait , B_SYSTEM_TEAM );
	
	//Set up the cookie
	data->pcii = m_devices[data->device_id];
	
	//Enable the registers
	rtl8139_init_registers( data );
	
	/* enable pci address access */	
	cmd = m_pcimodule->read_pci_config(data->pcii->bus, data->pcii->device, data->pcii->function, PCI_command, 2);
	cmd = cmd | PCI_command_io | PCI_command_master | PCI_command_memory;
	m_pcimodule->write_pci_config(data->pcii->bus, data->pcii->device, data->pcii->function, PCI_command, 2, cmd );
	
	// Check for the chipversion -- The version bits are bits 31-27 and 24-23
	temp32 = READ_32( TxConfig );
	
	if ( temp32 == 0xFFFFFF )
	{
		TRACE( "rtl8139_nielx open_hook(): Faulty chip\n" );
		free_hook( cookie );
		put_module( B_PCI_MODULE_NAME );
		return EIO;
	}
	
	temp32 = temp32 & 0x7cc00000;

	switch( temp32 )
	{
	case 0x74000000:
		TRACE( "rtl8139_nielx open_hook(): Chip is the 8139 C\n" );
		data->chip_type = RTL_8139_C;
		break;
	
	case 0x74400000:
		TRACE( "rtl8139_niels open_hook(): Chip is the 8139 D\n" );
		data->chip_type = RTL_8139_D;
		break;
	
	case 0x74C00000:
		TRACE( "rtl8139_nielx open_hook(): Chip is the 8101L\n" );
		data->chip_type = RTL_8101_L;
		break;
	
	default:
		TRACE( "rtl8139_nielx open_hook(): Unknown chip, assuming 8139 C\n" );
		data->chip_type = RTL_8139_C;
	}

	/* TODO: Linux driver does power management here... */

	/* Reset the chip -- command register;*/
	WRITE_8 ( Command , Reset );
	temp16 = 10000;
	while ( ( READ_8( Command ) & Reset ) && temp16 > 0 )
		temp16--;
	
	if ( temp16 == 0 )
	{
		TRACE( "rtl8139_nielx open_hook(): Reset failed... Bailing out\n" );
		free_hook( cookie );
		return EIO;
	}
	TRACE( "rtl8139_nielx open_hook(): Chip reset: %u \n" , temp16 );
	
	/* Enable writing to the configuration registers */
	WRITE_8( _9346CR , 0xc0 );

	/* Since the reset was succesful, we can immediately open the transmit and receive registers */
	WRITE_8( Command , EnableReceive | EnableTransmit );
			
	/* Reset Config1 register */
	WRITE_8( Config1 , 0 );
	
	// Turn off lan-wake and set the driver-loaded bit
	WRITE_8( Config1, ( READ_8( Config1 )& ~0x30) | 0x20);

	// Enable FIFO auto-clear
	WRITE_8( Config4, READ_8( Config4) | 0x80);

	// Go to normal operation
	WRITE_8( _9346CR , 0 );
	
	/* Reset Rx Missed counter*/
	WRITE_16( MPC , 0 );
	
	/* Configure the Transmit Register */
	//settings: Max DMA burst size per Tx DMA burst is 1024 ( = 110 )
	//settings: Interframe GAP time according to IEEE standard ( = 11 )
	WRITE_32( TxConfig , 
			IFG_1 | IFG_0 | MXDMA_1 );
	
	/* Configure the Receive Register */
	//settings: Early Rx Treshold is 1024 kB ( = 110 ) DISABLED
	//settings: Max DMA burst size per Rx DMA burst is 1024 ( = 110 )
	//settings: The Rx Buffer length is 64k + 16 bytes ( = 11 )
	//settings: continue last packet in memory if it exceeds buffer length.
	WRITE_32( RxConfig , /*RXFTH2 | RXFTH1 | */
		RBLEN_1 | RBLEN_0 | WRAP | MXDMA_2 | MXDMA_1 | APM | AB);
	
	//Disable blocking
	data->nonblocking = 0;
	//Allocate the ring buffer for the receiver. 
	// Size is set above: as 16k + 16 bytes + 1.5 kB--- 16 bytes for last CRC (a
	data->receivebuffer = alloc_mem( &(data->receivebufferlog) , &(data->receivebufferphy) , 1024 * 64 + 16 , "rx buffer" );
	if( data->receivebuffer == B_ERROR )
	{
		TRACE( "rtl8139_nielx open_hook(): memory allocation for ringbuffer failed\n" );
		return B_ERROR;
	}
	WRITE_32( RBSTART , (int32) data->receivebufferphy );
	data->receivebufferoffset = 0;  //First packet starts at 0
	
	//Disable all multi-interrupts
	WRITE_16( MULINT , 0 );

	//Allocate buffers for transmit (There can be two buffers in one page)
	data->transmitbuffer[0] = alloc_mem( &(data->transmitbufferlog[0]) , &(data->transmitbufferphy[0]) , 4096 , "txbuffer01" );
	WRITE_32( TSAD0 , (int32)data->transmitbufferphy[0] );
	data->transmitbuffer[1] = data->transmitbuffer[0];
	data->transmitbufferlog[1] = (void *)( (uint32)data->transmitbufferlog[0] + 2048 );
	data->transmitbufferphy[1] = (void *)( (uint32)data->transmitbufferphy[0] + 2048 );
	WRITE_32( TSAD1 , (int32)data->transmitbufferphy[1] );
	
	data->transmitbuffer[2] = alloc_mem( &(data->transmitbufferlog[2]) , &(data->transmitbufferphy[2]) , 4096 , "txbuffer23" );
	WRITE_32( TSAD2 , (int32)data->transmitbufferphy[2] );
	data->transmitbuffer[3] = data->transmitbuffer[2];
	data->transmitbufferlog[3] = (void *)( (uint32)data->transmitbufferlog[2] + 2048 );
	data->transmitbufferphy[3] = (void *)( (uint32)data->transmitbufferphy[2] + 2048 );
	WRITE_32( TSAD3 , (int32)data->transmitbufferphy[3] );
	
	if( data->transmitbuffer[0] == B_ERROR || data->transmitbuffer[2] == B_ERROR )
	{
		TRACE( "rtl8139_nielx open_hook(): memory allocation for transmitbuffer failed\n" );
		return B_ERROR;
	}

	data->queued_packets = 0;
	data->finished_packets = 0;
		
	// Receive hardware MAC address
	for( temp8 = 0 ; temp8 < 6; temp8++ )
		data->address.ebyte[ temp8 ] = READ_8( IDR0 + temp8  );
		
	TRACE( "rlt8139_nielx open_hook(): MAC address: %x:%x:%x:%x:%x:%x\n",
	data->address.ebyte[0] , data->address.ebyte[1] , data->address.ebyte[2] ,
	data->address.ebyte[3] , data->address.ebyte[4] , data->address.ebyte[5] );
	
	/* Receive physical match packets and broadcast packets */
	WRITE_32( RxConfig ,
			(READ_32( RxConfig )) | APM | AB );
	
	//Clear multicast mask
	WRITE_32( MAR0 , 0 );
	WRITE_32( MAR0 + 4 , 0 );
	

	/* We want interrupts! */
	if ( install_io_interrupt_handler( data->pcii->u.h0.interrupt_line , rtl8139_interrupt , data , 0 ) != B_OK )
	{
		TRACE( "rtl8139_nielx open_hook(): Error installing interrupt handler\n" );
		return B_ERROR;
	}

	WRITE_16( IMR , 
		ReceiveOk | ReceiveError | TransmitOk | TransmitError | 
		ReceiveOverflow | ReceiveUnderflow | ReceiveFIFOOverrun |
		TimeOut | SystemError );
	      
	/* Enable once more */
	WRITE_8( _9346CR , 0 );
	WRITE_8( Command , EnableReceive | EnableTransmit );
	
	//Check if Tx and Rx are enabled
	if( !( READ_8( Command ) & EnableReceive ) || !( READ_8( Command ) & EnableTransmit ) )
		TRACE( "TRANSMIT AND RECEIVE NOT ENABLED!!!\n" );
	else
		TRACE( "TRANSMIT AND RECEIVE ENABLED!!!\n" );
		
	TRACE( "rtl8139_nielx open_hook(): Basic Mode Status Register: 0x%x ESRS: 0x%x\n" ,
		READ_16( BMSR ) , 
		READ_8( ESRS ) );
		
	return B_OK;
}


/* --------------
	rtl8139_reset - resets the card
----- */

static void rtl8139_reset( rtl8139_properties_t *data )
{
	// Stop everything and disable interrupts
	WRITE_8( Command , Reset );
	WRITE_16( IMR , 0 );
	
	//Reset the buffer pointers
	WRITE_16( CBR , 0 );
	WRITE_16( CAPR , 0 - 16 );
	data->receivebufferoffset = 0;
	
	//Re-enable interrupts
	WRITE_16( IMR , 
		ReceiveOk | ReceiveError | TransmitOk | TransmitError | 
		ReceiveOverflow | ReceiveUnderflow | ReceiveFIFOOverrun |
		TimeOut | SystemError );
		
	//Start rx/tx
	WRITE_8( Command , EnableReceive | EnableTransmit );
}

/* ----------
	read_hook - handle read() calls
----- */

static status_t
read_hook (void* cookie, off_t position, void *buf, size_t* num_bytes)
{
	rtl8139_properties_t *data =/* (rtl8139_properties_t *)*/cookie;
	packetheader_t *packet_header;
	cpu_status former;
	
	TRACE( "rtl8139_nielx: read_hook()\n" );

	//if( !data->nonblocking )
		acquire_sem_etc( data->input_wait , 1 , B_CAN_INTERRUPT , 0 );
	
restart:
	former = lock();
	
	//Next: check in command register if there's actually anything to be read
	if ( READ_8( Command ) & BUFE  )
	{
		TRACE( "rtl8139_nielx read_hook: Nothing to read!!!\n" );
		unlock( former );
		return B_IO_ERROR;
	}
	
	// Retrieve the packet header
	packet_header = (packetheader_t *) ( ( uint8 *)data->receivebufferlog + data->receivebufferoffset );
	
	// Check if the transfer is already done: EarlyRX
	if ( packet_header->length == 0xfff0 )
	{
		TRACE( "rtl8139_nielx read_hook: The transfer is not yet finished!!!\n" );
		unlock( former );
		goto restart;
	}
	
	//Check for an error: if needed: resetrx, length may not be bigger than 1514 + 4 CRC
	if ( !( packet_header->bits & 0x1 ) || packet_header->length > 1518 )
	{
		TRACE( "rtl8139_nielx read_hook: Error in package reception: bits: %u length %u!!!\n" , packet_header->bits , packet_header->length);
		unlock ( former );
		rtl8139_reset( data );
		goto restart;
	}
	
	TRACE( "rtl8139_nielx read_hook(): Packet size: %u Receiveheader: %u Buffer size: %lu\n" , packet_header->length , packet_header->bits , *num_bytes );
	
	//Copy the packet
	*num_bytes = packet_header->length - 4;
	if ( data->receivebufferoffset + *num_bytes > 65536 )
	{
		//Packet wraps around , copy last bits except header ( = +4 )
		memcpy( buf , (void *)( (uint32)data->receivebufferlog + data->receivebufferoffset + 4 ) , 0x10000 - ( data->receivebufferoffset + 4 ) ); 
		//copy remaining bytes from the beginning
		memcpy( (void *) ( (uint32)buf + 0x10000 - ( data->receivebufferoffset + 4 ) ) , data->receivebufferlog , *num_bytes - (0x10000 - ( data->receivebufferoffset + 4 ) ) );
		TRACE( "rtl8139_nielx read_hook: Wrapping around end of buffer\n" );
	}
	else
		memcpy( buf , (void *) ( (uint32)data->receivebufferlog + data->receivebufferoffset + 4 ), packet_header->length - 4);  //length-4 because we don't want to copy the 4 bytes CRC
	
	//Update the buffer -- 4 for the header length, plus 3 for the dword allignment
	data->receivebufferoffset = ( data->receivebufferoffset + packet_header->length + 4 + 3 ) & ~3;
			
	WRITE_16( CAPR , data->receivebufferoffset - 16 ); //-16, avoid overflow
	TRACE( "rtl8139_nielx read_hook(): CBP %u CAPR %u \n" , READ_16( CBR ) , READ_16( CAPR ) );
	
	unlock( former );
	
	return packet_header->length - 4;
}


/* ----------
	rtl8139_write - handle write() calls
----- */

static status_t
write_hook (void* cookie, off_t position, const void* buffer, size_t* num_bytes)
{
	rtl8139_properties_t *data =/* (rtl8139_properties_t *)*/cookie;
	int buflen = *num_bytes;
	int transmitid = 0;
	uint32 transmitdescription = 0;
	cpu_status former;

	TRACE( "rtl8139_nielx write_hook()\n" );
	
	acquire_sem( data->lock );
	
	//Get spinlock
	WRITE_16( IMR , 0 );
	former = lock();
	
	if ( data->writes == 4 )
	{
		dprintf( "rtl8139_nielx write_hook(): already doing four writes\n" );
		unlock( former );
		release_sem_etc( data->lock , 1 , B_DO_NOT_RESCHEDULE );
		return B_INTERRUPTED;
	}

	if ( buflen > 1792 ) //Maximum of 1792 bytes
	{
		TRACE( "rtl8139_nielx write_hook(): packet is too long\n" );
		unlock( former );
		release_sem_etc( data->lock , 1 , B_DO_NOT_RESCHEDULE );
		return B_IO_ERROR;
	}
		
	// We need to determine a free transmit descriptor
	transmitid = data->queued_packets % 4;
	if ( data->transmitstatus[ transmitid ] == 1 )
	{
		//No free descriptor]
		unlock( former );
		release_sem_etc( data->lock , 1 , B_DO_NOT_RESCHEDULE );
		return B_IO_ERROR;
	}
	
	dprintf( "rtl8139_nielx write_hook(): TransmitID: %u Packagelen: %u Register: %lx\n" , transmitid , buflen , TSD0 + (sizeof(uint32) * transmitid ) );
	
	data->writes++;
	// Set the buffer as used
	data->transmitstatus[transmitid] = 1;
	
	//Copy the packet into the buffer
	memcpy( data->transmitbufferlog[transmitid] , buffer , buflen );
	
	if ( buflen < 60 )
		buflen = 60;
	
	//Clear OWN and start transfer Create transmit description with early Tx FIFO, size
	transmitdescription = ( buflen | 0x80000 | transmitdescription ) ^OWN; //0x80000 = early tx treshold
	TRACE( "rtl8139_nielx write: transmitdescription = %lu\n" , transmitdescription );
	WRITE_32( TSD0 + (sizeof(uint32) * transmitid ) , transmitdescription );

	TRACE( "rtl8139_nielx write: TSAD: %u\n" , READ_16( TSAD ) );
	data->queued_packets++;
	
	unlock( former );
	WRITE_16( IMR ,
		ReceiveOk | ReceiveError | TransmitOk | TransmitError | 
		ReceiveOverflow | ReceiveUnderflow | ReceiveFIFOOverrun |
		TimeOut | SystemError );
	release_sem_etc( data->lock , 1 , B_DO_NOT_RESCHEDULE );

	//Done
	return B_OK;
}


/* ----------
	rtl8139_control - handle ioctl calls
----- */

static status_t
control_hook (void* cookie, uint32 op, void* arg, size_t len)
{
	rtl8139_properties_t *data = (rtl8139_properties_t *)cookie;
	ether_address_t address;
	TRACE( "rtl8139_nielx control_hook()\n" );
	
	
	switch ( op )
	{	
	case ETHER_INIT:
		TRACE( "rtl8139_nielx control_hook(): Wants us to init... ;-)\n" );
		return B_NO_ERROR;
		
	case ETHER_GETADDR:
		if ( data == NULL )
			return B_ERROR;
			
		TRACE( "rtl8139_nielx control_hook(): Wants our address...\n" );
		memcpy( arg , (void *) &(data->address) , sizeof( ether_address_t ) );
		return B_OK;
	
	case ETHER_ADDMULTI:
		if (data == NULL )
			return B_ERROR;
		//Check if the maximum of multicast addresses isn't reached
		if ( data->multiset == 8 )
			return B_ERROR;
			
		TRACE( "rtl8139_nielx control_hook(): Add multicast...\n" );
		memcpy( &address , arg , sizeof( address ) );
		TRACE( "Multicast address: %i %i %i %i %i %i \n" , address.ebyte[0] ,
		address.ebyte[1] , address.ebyte[2] , address.ebyte[3] , address.ebyte[4] ,
		address.ebyte[5] );
		return B_OK;

	case ETHER_NONBLOCK:
		if ( data == NULL )
			return B_ERROR;
		
		TRACE( "rtl8139_nielx control_hook(): Wants to set block/nonblock\n" );
		memcpy( &data->nonblocking , arg , sizeof( data->nonblocking ) );
		return B_NO_ERROR;
		
	case ETHER_REMMULTI:
		TRACE( "rtl8139_nielx control_hook(): Wants REMMULTI\n" );
		return B_OK;
		
	case ETHER_SETPROMISC:
		TRACE("rtl8139_nielx control_hook(): Wants PROMISC\n" );
		return B_OK;
	
	case ETHER_GETFRAMESIZE:
		TRACE("rtl8139_nielx control_hook(): Wants GETFRAMESIZE\n" ) ;
		*( (unsigned int *)arg ) = 1514;
		return B_OK;
	}
	return B_BAD_VALUE;
}

/* ----------
	interrupt_handler - handle issued interrupts
----- */

static int32
rtl8139_interrupt( void *cookie )
{
	rtl8139_properties_t *data = (rtl8139_properties_t *)cookie;
	uint8 temp8;
	uint16 isr_contents;
	int32 retval = B_UNHANDLED_INTERRUPT; 
	cpu_status status;
	
	status = lock();
	WRITE_16( IMR , 0 ); //Disable interrupts
	
	for (;;)
	{
		isr_contents = READ_16( ISR );
		if ( isr_contents == 0 )
			break;
		else
			WRITE_16( ISR , isr_contents );
			
		TRACE( "NIELX INTERRUPT: %u \n" , isr_contents );
		if( isr_contents & ReceiveOk )
		{
			TRACE( "rtl8139_nielx interrupt ReceiveOk\n" );
			release_sem_etc( data->input_wait , 1 , B_DO_NOT_RESCHEDULE );
			retval = B_INVOKE_SCHEDULER;
		}
	
		if (isr_contents & 	ReceiveError )
		{
			//Do something
			;
		}
	
		if (isr_contents & TransmitOk )
		{
			uint32 checks = data->queued_packets - data->finished_packets;
			uint32 txstatus;
			// Check each status descriptor
			while( checks > 0 )
			{
				// If a register isn't used, continue next run
				temp8 = data->finished_packets % 4 ;
				txstatus = READ_32( TSD0 + temp8 * sizeof( int32 ) );
				dprintf( "run: %u txstatus: %lu Register: %lx\n" , temp8 , txstatus , TSD0 + temp8 * sizeof( int32 ) );
				
				if ( !( txstatus & ( TOK | TUN | TABT ) ) )
				{
					dprintf( "NOT FINISHED\n" );
					break;
				}
				
				if ( txstatus & ( TABT | OWC ) )
				{
					dprintf( "MAJOR ERROR\n" );
					continue;
				} 
				
				if ( txstatus &( TUN ) ) 
				{
					dprintf( "TRANSMIT UNDERRUN\n" );
					continue;
				}
				
				if ( ( txstatus & TOK ) )
				{
					//this one is the one!
					dprintf( "NIELX INTERRUPT: TXOK, clearing register %u\n" , temp8 );
					data->transmitstatus[temp8] = 0; //That's all there is to it
					data->writes--;
					data->finished_packets++;
					checks--;
					//update next transmitid
					continue;
				}

			}
			retval = B_HANDLED_INTERRUPT;
		}
	
		if( isr_contents & TransmitError )
		{
			//
			;
		}
	
		if( isr_contents & ReceiveOverflow )
		{
			// Discard all the current packages to be processed -- newos driver
			WRITE_16( CAPR , ( READ_16( CBR ) + 16 ) % 0x1000 );
			retval = B_HANDLED_INTERRUPT;
		}
	
		if( isr_contents & ReceiveUnderflow )
		{
			// Most probably a link change -> TODO CHECK!
			TRACE( "rtl8139_nielx interrupt(): BMCR: 0x%x BMSR: 0x%x\n" ,
				READ_16( BMCR ) ,
				READ_16( BMSR ) );
			retval = B_HANDLED_INTERRUPT;
		}
	
		if ( isr_contents & ReceiveFIFOOverrun )
		{
			//
			;
		}
	
		if ( isr_contents & TimeOut )
		{
			//
			;
		}
	
		if ( isr_contents & SystemError )
		{
			//
			;
		}
	}
	
	//Re-enable interrupts
	WRITE_16( IMR , 
		ReceiveOk | ReceiveError | TransmitOk | TransmitError | 
		ReceiveOverflow | ReceiveUnderflow | ReceiveFIFOOverrun |
		TimeOut | SystemError );	
	
	unlock( status );
	
	return retval;	
}

/* ----------
	close_hook - handle close() calls
----- */

static status_t
close_hook (void* cookie)
{
	rtl8139_properties_t * data = (rtl8139_properties_t *) cookie;
	//Stop Rx and Tx process
	WRITE_8( Command , 0 );
	WRITE_16( IMR , 0 );
	return B_OK;
}


/* -----
	free_hook - called after the last device is closed, and after
	all i/o is complete.
----- */
static status_t
free_hook (void* cookie)
{
	rtl8139_properties_t *data = (rtl8139_properties_t *) cookie;

	TRACE( "rtl8139_nielx free_hook()\n" );
	
	//Remove interrupt handler
	remove_io_interrupt_handler( data->pcii->u.h0.interrupt_line , 
	                             rtl8139_interrupt , cookie );
	
	//Free Rx and Tx buffers
	delete_area( data->receivebuffer );
	delete_area( data->transmitbuffer[0] );
	delete_area( data->transmitbuffer[2] );
	delete_area( data->ioarea ); //Only does something on ppc
	
	m_openmask &= ~(1L << data->device_id);
	
	//Finally, free the cookie
	free( data );
	
	//Put the pci module
	put_module( B_PCI_MODULE_NAME );
	
	return B_OK;
}


/* -----
	function pointers for the device hooks entry points
----- */

device_hooks rtl8139_hooks = {
	open_hook,	 			/* -> open entry point */
	close_hook, 			/* -> close entry point */
	free_hook,			/* -> free cookie */
	control_hook, 			/* -> control entry point */
	read_hook,			/* -> read entry point */
	write_hook				/* -> write entry point */
};

/* ----------
	publish_devices - return a null-terminated array of devices
	supported by this driver.
----- */

const char**
publish_devices()
{
	return (const char **)rtl8139_names;
}

/* ----------
	find_device - return ptr to device hooks structure for a
	given device name
----- */

device_hooks*
find_device(const char* name)
{
	return &rtl8139_hooks;
}
