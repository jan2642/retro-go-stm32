/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.  To obtain a
** copy of the GNU Library General Public License, write to the Free
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** nes_mmc.c
**
** NES Memory Management Controller (mapper) emulation
** $Id: nes_mmc.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include <string.h>
#include <nofrendo.h>
#include <mappers.h>
#include "nes_ppu.h"
#include "nes_mmc.h"
#include "nes_rom.h"

#define  MMC_8KPRG         (mmc.prg_banks * 2)
#define  MMC_16KPRG        (mmc.prg_banks)
#define  MMC_32KPRG        (mmc.prg_banks / 2)
#define  MMC_8KCHR         (mmc.chr_banks)
#define  MMC_4KCHR         (mmc.chr_banks * 2)
#define  MMC_2KCHR         (mmc.chr_banks * 4)
#define  MMC_1KCHR         (mmc.chr_banks * 8)

static mmc_t mmc;


rominfo_t *mmc_getinfo(void)
{
   return mmc.cart;
}

void mmc_setcontext(mmc_t *src_mmc)
{
   ASSERT(src_mmc);
   mmc = *src_mmc;
}

void mmc_getcontext(mmc_t *dest_mmc)
{
   ASSERT(dst_mmc);
   *dest_mmc = mmc;
}

/* Map a pointer into the address space */
void mmc_bankptr(int size, uint32 address, int bank, uint8 *ptr)
{
   int page = address >> MEM_PAGESHIFT;
   uint8 *base = ptr;

   if (ptr == NULL)
   {
      MESSAGE_ERROR("MMC: Invalid pointer! Addr: $%04X Bank: %d Size: %d\n", address, bank, size);
      abort();
   }

   switch (size)
   {
   case 8:
      base += ((bank >= 0 ? bank : MMC_8KPRG + bank) % MMC_8KPRG) << 13;
      break;

   case 16:
      base += ((bank >= 0 ? bank : MMC_16KPRG + bank) % MMC_16KPRG) << 14;
      break;

   case 32:
      base += ((bank >= 0 ? bank : MMC_32KPRG + bank) % MMC_32KPRG) << 15;
      break;

   default:
      MESSAGE_ERROR("MMC: Invalid bank size! Addr: $%04X Bank: %d Size: %d\n", address, bank, size);
      abort();
   }

   for (int i = 0; i < (size * 0x400 / MEM_PAGESIZE); i++)
   {
      mem_setpage(page + i, base + i * MEM_PAGESIZE);
   }
}

/* PRG-ROM bankswitching */
void mmc_bankrom(int size, uint32 address, int bank)
{
   mmc_bankptr(size, address, bank, mmc.prg);
}

/* PRG-RAM bankswitching */
void mmc_bankwram(int size, uint32 address, int bank)
{
   mmc_bankptr(size, address, bank, mmc.cart->sram);
}

/* CHR-ROM bankswitching */
void mmc_bankvrom(int size, uint32 address, int bank)
{
   switch (size)
   {
   case 1:
      bank = (bank >= 0 ? bank : MMC_1KCHR + bank) % MMC_1KCHR;
      ppu_setpage(1, address >> 10, &mmc.chr[bank << 10] - address);
      break;

   case 2:
      bank = (bank >= 0 ? bank : MMC_2KCHR + bank) % MMC_2KCHR;
      ppu_setpage(2, address >> 10, &mmc.chr[bank << 11] - address);
      break;

   case 4:
      bank = (bank >= 0 ? bank : MMC_4KCHR + bank) % MMC_4KCHR;
      ppu_setpage(4, address >> 10, &mmc.chr[bank << 12] - address);
      break;

   case 8:
      bank = (bank >= 0 ? bank : MMC_8KCHR + bank) % MMC_8KCHR;
      ppu_setpage(8, 0, &mmc.chr[bank << 13]);
      break;

   default:
      MESSAGE_ERROR("MMC: Invalid CHR bank size %d\n", size);
      abort();
   }
}

/* Check to see if this mapper is supported */
mapintf_t *mmc_peek(int map_num)
{
   mapintf_t **map_ptr = (mapintf_t **)mappers;

   while (NULL != *map_ptr)
   {
      if ((*map_ptr)->number == map_num)
         return *map_ptr;
      map_ptr++;
   }

   return NULL;
}

static void mmc_setpages(void)
{
   /* Switch Save RAM into CPU space */
   mmc_bankwram(8, 0x6000, 0);

   /* Switch PRG ROM into CPU space */
   mmc_bankrom(16, 0x8000, 0);
   mmc_bankrom(16, 0xC000, MMC_LASTBANK);

   /* Switch CHR ROM/RAM into CPU space */
   mmc_bankvrom(8, 0x0000, 0);

   if (mmc.cart->flags & ROM_FLAG_FOURSCREEN)
      ppu_mirror(0, 1, 2, 3);
   else if (MIRROR_VERT == mmc.cart->mirror)
      ppu_mirror(0, 1, 0, 1);
   else
      ppu_mirror(0, 0, 1, 1);

   // ppu_mirrorhipages();
}

/* Mapper initialization routine */
void mmc_reset(void)
{
   mmc_setpages();

   ppu_setlatchfunc(NULL);

   if (mmc.intf->init)
      mmc.intf->init();

   MESSAGE_INFO("MMC: Mapper %s (%d) ready!\n", mmc.intf->name, mmc.intf->number);
}

void mmc_shutdown()
{

}

mmc_t *mmc_init(rominfo_t *rominfo)
{
   memset(&mmc, 0, sizeof(mmc_t));

   mmc.intf = mmc_peek(rominfo->mapper_number);
   if (NULL == mmc.intf)
   {
      MESSAGE_ERROR("MMC: Unsupported mapper %d\n", rominfo->mapper_number);
      return NULL;
   }

   mmc.cart = rominfo;
   mmc.prg = rominfo->rom;
   mmc.prg_banks = rominfo->rom_banks;

   if (rominfo->vrom_banks)
   {
      mmc.chr = rominfo->vrom;
      mmc.chr_banks = rominfo->vrom_banks;
      MESSAGE_INFO("MMC: Using CHR-ROM/VROM (%d banks)\n", rominfo->vrom_banks);
   }
   else
   {
      mmc.chr = rominfo->vram;
      mmc.chr_banks = rominfo->vram_banks;
      MESSAGE_INFO("MMC: Using CHR-RAM/VRAM (%d banks)\n", rominfo->vram_banks);
   }

   return &mmc;
}