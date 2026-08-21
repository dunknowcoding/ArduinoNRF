ENTRY(Reset_Handler)

__nrf_reserved_flash_bytes = DEFINED(__nrf_reserved_flash_bytes) ? __nrf_reserved_flash_bytes : 0;
__nrf_storage_region_bytes = DEFINED(__nrf_storage_region_bytes) ? __nrf_storage_region_bytes : __nrf_reserved_flash_bytes;
__nrf_program_flash_bytes = DEFINED(__nrf_program_flash_bytes) ? __nrf_program_flash_bytes : (1024K - __nrf_app_start - __nrf_reserved_flash_bytes);
__nrf_app_start = DEFINED(__nrf_app_start) ? __nrf_app_start : 0;

MEMORY
{
  FLASH (rx)  : ORIGIN = __nrf_app_start, LENGTH = __nrf_program_flash_bytes
  RAM   (rwx) : ORIGIN = 0x20006000, LENGTH = 256K - 0x6000
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
__nrf_storage_region_start = ORIGIN(FLASH) + LENGTH(FLASH);
__nrf_storage_region_length = __nrf_storage_region_bytes;

SECTIONS
{
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
    KEEP(*(.init))
    KEEP(*(.fini))
  } > FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } > FLASH

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } > FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } > FLASH

  .data :
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } > RAM AT > FLASH

  .svc_data :
  {
    PROVIDE(__start_svc_data = .);
    KEEP(*(.svc_data))
    PROVIDE(__stop_svc_data = .);
  } > RAM

  .fs_data :
  {
    PROVIDE(__start_fs_data = .);
    KEEP(*(.fs_data))
    PROVIDE(__stop_fs_data = .);
  } > RAM

  _sidata = LOADADDR(.data);

  .noinit (NOLOAD) :
  {
    . = ALIGN(4);
    __noinit_start = .;
    *(.noinit*)
    . = ALIGN(4);
    __noinit_end = .;
  } > RAM

  .bss :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > RAM

  .heap (COPY) :
  {
    _end = .;
    end = .;
    . = . + 0x1000;
  } > RAM
}
