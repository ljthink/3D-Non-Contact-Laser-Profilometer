
MEMORY
{
	/* Define each memory region */
	FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 32k
	RAM (rwx)  : ORIGIN = 0x10000100, LENGTH = 8k-256
}

INCLUDE "memory_sections.ld"
