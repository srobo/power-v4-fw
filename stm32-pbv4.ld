/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2012 Karl Palsson <karlp@tweak.net.au>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Linker script for STM32F100x6, 32K flash, 4K RAM. */

/* Define memory regions. */
MEMORY
{
	rom (rx) : ORIGIN = 0x08000000, LENGTH = 32K
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 10K
}

/* Include the common ld script. */
INCLUDE libopencm3_stm32f1.ld

/* Enforce emmition of the vector table. */
EXTERN (vector_table)

/* Define the entry point of the output file. */
ENTRY(reset_handler)

EXTERN(powerboard_main);

/* Define sections. */
SECTIONS
{
        .text : {
                *(.vectors)     /* Vector table */
		dfu-bootloader/usbdfu.o(.text*)
		usb*(.text*)
	} >rom
	. = 0x08002000;
	.text : {
                *(.text*)       /* Program code */
                . = ALIGN(4);
                *(.fwver)
                . = ALIGN(4);
                *(.rodata*)     /* Read-only data */
                . = ALIGN(4);
        } >rom

	 /*
         * Another section used by C++ stuff, appears when using newlib with
         * 64bit (long long) printf support
         */
        .ARM.extab : {
                *(.ARM.extab*)
        } >rom
        .ARM.exidx : {
                __exidx_start = .;
                *(.ARM.exidx*)
                __exidx_end = .;
        } >rom

	 . = ALIGN(4);
        _etext = ALIGN(8);

        .data : {
                _data = .;
                *(.data*)       /* Read-write initialized data */
                . = ALIGN(4);
                _edata = .;
        } >ram AT >rom
        _data_loadaddr = LOADADDR(.data);

        .bss : {
                *(.bss*)        /* Read-write zero initialized data */
                *(COMMON)
                . = ALIGN(4);
                _ebss = .;
        } >ram

	/*
         * The .eh_frame section appears to be used for C++ exception handling.
         * You may need to fix this if you're using C++.
         */
        /DISCARD/ : { *(.eh_frame) }

        . = ALIGN(4);
        end = .;
}

PROVIDE(_stack = ORIGIN(ram) + LENGTH(ram));

