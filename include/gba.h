#ifndef GBA_H
#define GBA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Basic Integer Types */
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;

typedef volatile uint8_t   vu8;
typedef volatile uint16_t  vu16;
typedef volatile uint32_t  vu32;
typedef volatile int8_t    vs8;
typedef volatile int16_t   vs16;
typedef volatile int32_t   vs32;

/* Display Dimensions */
#define SCREEN_WIDTH   240
#define SCREEN_HEIGHT  160

/* Memory Map Base Addresses */
#define MEM_EWRAM      0x02000000
#define MEM_IWRAM      0x03000000
#define MEM_IO         0x04000000
#define MEM_PALETTE    0x05000000
#define MEM_VRAM       0x06000000
#define MEM_OAM        0x07000000
#define MEM_ROM        0x08000000
#define MEM_SRAM       0x0E000000

/* Display Registers */
#define REG_DISPCNT    (*(vu16*)(MEM_IO + 0x0000))
#define REG_DISPSTAT   (*(vu16*)(MEM_IO + 0x0004))
#define REG_VCOUNT     (*(vu16*)(MEM_IO + 0x0006))

/* Display Control Bits */
#define MODE_0         0x0000
#define MODE_1         0x0001
#define MODE_2         0x0002
#define MODE_3         0x0003
#define MODE_4         0x0004
#define MODE_5         0x0005
#define BG0_ENABLE     0x0100
#define BG1_ENABLE     0x0200
#define BG2_ENABLE     0x0400
#define BG3_ENABLE     0x0800
#define OBJ_ENABLE     0x1000
#define BACKBUFFER     0x0010

/* Input Register: 0 = Pressed, 1 = Released */
#define REG_KEYINPUT   (*(vu16*)(MEM_IO + 0x0130))
#define REG_KEYCNT     (*(vu16*)(MEM_IO + 0x0132))

/* Button Masks */
#define KEY_A          (1 << 0)
#define KEY_B          (1 << 1)
#define KEY_SELECT     (1 << 2)
#define KEY_START      (1 << 3)
#define KEY_RIGHT      (1 << 4)
#define KEY_LEFT       (1 << 5)
#define KEY_UP         (1 << 6)
#define KEY_DOWN       (1 << 7)
#define KEY_R          (1 << 8)
#define KEY_L          (1 << 9)
#define KEY_ANY        0x03FF

/* System & Waitstate Control */
#define REG_WAITCNT    (*(vu16*)(MEM_IO + 0x0204))
#define REG_WRAMCNT    (*(vu8 *)(MEM_IO + 0x0800))

/* Interrupt Control */
#define REG_IE         (*(vu16*)(MEM_IO + 0x0200))
#define REG_IF         (*(vu16*)(MEM_IO + 0x0202))
#define REG_IME        (*(vu16*)(MEM_IO + 0x0208))

/* Sound Registers */
#define REG_SOUND1CNT_L (*(vu16*)(MEM_IO + 0x0060))
#define REG_SOUND1CNT_H (*(vu16*)(MEM_IO + 0x0062))
#define REG_SOUND1CNT_X (*(vu16*)(MEM_IO + 0x0064))
#define REG_SOUND2CNT_L (*(vu16*)(MEM_IO + 0x0068))
#define REG_SOUND2CNT_H (*(vu16*)(MEM_IO + 0x006C))
#define REG_SOUND3CNT_L (*(vu16*)(MEM_IO + 0x0070))
#define REG_SOUND3CNT_H (*(vu16*)(MEM_IO + 0x0072))
#define REG_SOUND3CNT_X (*(vu16*)(MEM_IO + 0x0074))
#define REG_SOUND4CNT_L (*(vu16*)(MEM_IO + 0x0078))
#define REG_SOUND4CNT_H (*(vu16*)(MEM_IO + 0x007C))
#define REG_SOUNDCNT_L  (*(vu16*)(MEM_IO + 0x0080))
#define REG_SOUNDCNT_H  (*(vu16*)(MEM_IO + 0x0082))
#define REG_SOUNDCNT_X  (*(vu16*)(MEM_IO + 0x0084))
#define REG_WAVE_RAM    ((vu32*)(MEM_IO + 0x0090))
#define REG_WAVE_RAM0   (*(vu32*)(MEM_IO + 0x0090))
#define REG_WAVE_RAM1   (*(vu32*)(MEM_IO + 0x0094))
#define REG_WAVE_RAM2   (*(vu32*)(MEM_IO + 0x0098))
#define REG_WAVE_RAM3   (*(vu32*)(MEM_IO + 0x009C))

/* DirectSound FIFOs */
#define REG_FIFO_A    (*(vu32*)(MEM_IO + 0x00A0))
#define REG_FIFO_B    (*(vu32*)(MEM_IO + 0x00A4))

/* DMA Registers */
#define REG_DMA0SAD   (*(vu32*)(MEM_IO + 0x00B0))
#define REG_DMA0DAD   (*(vu32*)(MEM_IO + 0x00B4))
#define REG_DMA0CNT_L (*(vu16*)(MEM_IO + 0x00B8))
#define REG_DMA0CNT_H (*(vu16*)(MEM_IO + 0x00BA))

#define REG_DMA1SAD   (*(vu32*)(MEM_IO + 0x00BC))
#define REG_DMA1DAD   (*(vu32*)(MEM_IO + 0x00C0))
#define REG_DMA1CNT_L (*(vu16*)(MEM_IO + 0x00C4))
#define REG_DMA1CNT_H (*(vu16*)(MEM_IO + 0x00C6))

#define REG_DMA2SAD   (*(vu32*)(MEM_IO + 0x00C8))
#define REG_DMA2DAD   (*(vu32*)(MEM_IO + 0x00CC))
#define REG_DMA2CNT_L (*(vu16*)(MEM_IO + 0x00D0))
#define REG_DMA2CNT_H (*(vu16*)(MEM_IO + 0x00D2))

#define REG_DMA3SAD   (*(vu32*)(MEM_IO + 0x00D4))
#define REG_DMA3DAD   (*(vu32*)(MEM_IO + 0x00D8))
#define REG_DMA3CNT_L (*(vu16*)(MEM_IO + 0x00DC))
#define REG_DMA3CNT_H (*(vu16*)(MEM_IO + 0x00DE))

/* Timer Registers */
#define REG_TM0CNT_L   (*(vu16*)(MEM_IO + 0x0100))
#define REG_TM0CNT_H   (*(vu16*)(MEM_IO + 0x0102))
#define REG_TM1CNT_L   (*(vu16*)(MEM_IO + 0x0104))
#define REG_TM1CNT_H   (*(vu16*)(MEM_IO + 0x0106))
#define REG_TM2CNT_L   (*(vu16*)(MEM_IO + 0x0108))
#define REG_TM2CNT_H   (*(vu16*)(MEM_IO + 0x010A))
#define REG_TM3CNT_L   (*(vu16*)(MEM_IO + 0x010C))
#define REG_TM3CNT_H   (*(vu16*)(MEM_IO + 0x010E))

/* Serial Communication (SIO / EXT Port) */
#define REG_SIODATA32   (*(vu32*)(MEM_IO + 0x0120))
#define REG_SIOMULTI0   (*(vu16*)(MEM_IO + 0x0120))
#define REG_SIOMULTI1   (*(vu16*)(MEM_IO + 0x0122))
#define REG_SIOMULTI2   (*(vu16*)(MEM_IO + 0x0124))
#define REG_SIOMULTI3   (*(vu16*)(MEM_IO + 0x0126))
#define REG_SIOCNT      (*(vu16*)(MEM_IO + 0x0128))
#define REG_SIOMLT_SEND (*(vu16*)(MEM_IO + 0x012A))
#define REG_SIODATA8    (*(vu8 *)(MEM_IO + 0x012A))
#define REG_RCNT        (*(vu16*)(MEM_IO + 0x0134))

/* VRAM access */
#define VRAM           ((u16*)MEM_VRAM)
#define VRAM_PAGE2     ((u16*)(MEM_VRAM + 0xA000))

/* Color Definition: 15-bit BGR555 format */
#define RGB555(r, g, b)  ((u16)(((r) & 0x1F) | (((g) & 0x1F) << 5) | (((b) & 0x1F) << 10)))

/* Standard Color Palette */
#define COLOR_BLACK       RGB555(0, 0, 0)
#define COLOR_WHITE       RGB555(31, 31, 31)
#define COLOR_RED         RGB555(31, 0, 0)
#define COLOR_GREEN       RGB555(0, 31, 0)
#define COLOR_BLUE        RGB555(0, 0, 31)
#define COLOR_YELLOW      RGB555(31, 31, 0)
#define COLOR_CYAN        RGB555(0, 31, 31)
#define COLOR_MAGENTA     RGB555(31, 0, 31)
#define COLOR_GRAY        RGB555(16, 16, 16)
#define COLOR_DARK_GRAY   RGB555(6, 7, 9)
#define COLOR_LIGHT_GRAY  RGB555(24, 24, 25)
#define COLOR_NAVY        RGB555(2, 4, 12)
#define COLOR_GOLD        RGB555(31, 25, 4)
#define COLOR_EMERALD     RGB555(2, 28, 14)
#define COLOR_CORAL       RGB555(31, 10, 8)
#define COLOR_AMBER       RGB555(31, 18, 0)
#define COLOR_CYAN_GLOW   RGB555(6, 26, 31)
#define COLOR_HEADER_BG   RGB555(3, 5, 14)
#define COLOR_CARD_BG     RGB555(4, 8, 18)
#define COLOR_CARD_BORDER RGB555(8, 14, 28)

/* Hardware sync functions: wait for VBlank using DISPSTAT */
static inline void vsync(void) {
    while (REG_DISPSTAT & 1);
    while (!(REG_DISPSTAT & 1));
}

#endif /* GBA_H */
