/** \file
 * Autoboot flag control
 */
#include "dryos.h"
#include "bmp.h"

/* CF/SD device structure. we have two types which have different parameter order and little differences in behavior */
struct cf_device
{
    /* type b always reads from raw sectors */
    int (*read_block)(
        struct cf_device * dev,
        void * buf,
        uintptr_t block,
        size_t num_blocks
    );

    int (*write_block)(
        struct cf_device * dev,
        const void * buf,
        uintptr_t block,
        size_t num_blocks
    );
    
    /* is 7D the only one with two null pointers between? */

    void * io_control;
    void * soft_reset;
};

/** Shadow copy of the NVRAM boot flags stored at 0xF8000000 */
#define NVRAM_BOOTFLAGS     ((void*) 0xF8000000)
struct boot_flags
{
    uint32_t        firmware;   // 0x00
    uint32_t        bootdisk;   // 0x04
    uint32_t        ram_exe;    // 0x08
    uint32_t        update;     // 0x0c
    uint32_t        flag_0x10;
    uint32_t        flag_0x14;
    uint32_t        flag_0x18;
    uint32_t        flag_0x1c;
};

static struct boot_flags * const    boot_flags = NVRAM_BOOTFLAGS;;


extern struct cf_device * const cf_device[];
extern struct cf_device * const sd_device[];

struct partition_table 
{
    uint8_t state; // 0x80 = bootable
    uint8_t start_head;
    uint16_t start_cylinder_sector;
    uint8_t type;
    uint8_t end_head;
    uint16_t end_cylinder_sector;
    uint32_t sectors_before_partition;
    uint32_t sectors_in_partition;
}__attribute__((aligned,packed));


/*
 * recompute a exFAT VBR checksum in sector 12
 */

static uint32_t VBRChecksum( unsigned char octets[], int NumberOfBytes) {
   uint32_t Checksum = 0;
   int Index;
   for (Index = 0; Index < NumberOfBytes; Index++) {
     if (Index != 106 && Index != 107 && Index != 112)  // skip 'volume flags' and 'percent in use'
	 Checksum = ((Checksum <<31) | (Checksum>> 1)) + (uint32_t) octets[Index];
   }
   return Checksum;
}

static void exfat_sum(uint32_t* buffer) // size: 12 sectors (0-11)
{
    int i=0;
    uint32_t sum;

    sum = VBRChecksum((unsigned char*)buffer, (512*11));

    //~ NotifyBox(2000, "before: %x %x\nafter: %x ", buffer[(512*11)/4], buffer[(512*11)/4 + 1], sum); msleep(2000);
    
    // fill sector 11 with the checksum, repeated
    for(i=0; i<512; i+=4)
        buffer[(512*11+i)/4] = sum;
}


// http://www.datarescue.com/laboratory/partition.htm
// http://magiclantern.wikia.com/wiki/Bootdisk
int
bootflag_write_bootblock( void )
{
    struct cf_device * const dev = (struct cf_device *) sd_device[1];

    if (!dev)
    {
        return 0;
    }

    uint8_t *block = fio_malloc( 512 );
    int i;
    for(i=0 ; i<0x200 ; i++) block[i] = 0xAA;
    
    dev->read_block( dev, block, 0, 1 );

    struct partition_table p;
    extern void fsuDecodePartitionTable(void* partition_address_on_card, struct partition_table * ptable);
    fsuDecodePartitionTable(block + 446, &p);

    //~ NotifyBox(1000, "decoded => %x,%x,%x", p.type, p.sectors_before_partition, p.sectors_in_partition);

    if (p.type == 6 || p.type == 0xb || p.type == 0xc) // FAT16 or FAT32
    {
        int rc = dev->read_block( dev, block, p.sectors_before_partition, 1 );
        int off1 = p.type == 6 ? 0x2b : 0x47;
        int off2 = p.type == 6 ? 0x40 : 0x5c;
        memcpy( block + off1, (uint8_t*) "EOS_DEVELOP", 0xB );
        memcpy( block + off2, (uint8_t*) "BOOTDISK", 0x8 );
        //~ NotifyBox(1000, "writing");
        rc = dev->write_block( dev, block, p.sectors_before_partition, 1 );
        if (rc != 1)
        {
            NotifyBox(2000, "Bootflag write failed\np.type=%d, p.sec = %d, rc=%d", p.type, p.sectors_before_partition, rc); 
            msleep(2000);
            return 0;
        }
    }
    else if (p.type == 7) // ExFAT
    {
        uint8_t* buffer = fio_malloc(512*24);
        dev->read_block( dev, buffer, p.sectors_before_partition, 24 );

        int off1 = 130;
        int off2 = 122;
        memcpy( buffer + off1, (uint8_t*) "EOS_DEVELOP", 0xB );
        memcpy( buffer + off2, (uint8_t*) "BOOTDISK", 0x8 );
        memcpy( buffer + 512*12 + off1, (uint8_t*) "EOS_DEVELOP", 0xB );
        memcpy( buffer + 512*12 + off2, (uint8_t*) "BOOTDISK", 0x8 );
        exfat_sum((uint32_t*)(buffer));
        exfat_sum((uint32_t*)(buffer+512*12));

        dev->write_block( dev, buffer, p.sectors_before_partition, 24 );
        fio_free( buffer );
    }
    else
    {
        fio_free( block );
        NotifyBox(2000, "Unknown partition: %d", p.type); msleep(2000);
        return 0;
    }
    fio_free( block );
    return 1; // success!
}

