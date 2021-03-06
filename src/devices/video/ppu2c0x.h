// license:BSD-3-Clause
// copyright-holders:Ernesto Corvi,Brad Oliver
/******************************************************************************

    Nintendo 2C0x PPU emulation.

    Written by Ernesto Corvi.
    This code is heavily based on Brad Oliver's MESS implementation.

******************************************************************************/

#ifndef MAME_VIDEO_PPU2C0X_H
#define MAME_VIDEO_PPU2C0X_H

#pragma once

///*************************************************************************
//  MACROS / CONSTANTS
///*************************************************************************

// mirroring types
#define PPU_MIRROR_NONE       0
#define PPU_MIRROR_VERT       1
#define PPU_MIRROR_HORZ       2
#define PPU_MIRROR_HIGH       3
#define PPU_MIRROR_LOW        4
#define PPU_MIRROR_4SCREEN    5 // Same effect as NONE, but signals that we should never mirror

#define PPU_DRAW_BG       0
#define PPU_DRAW_OAM      1


///*************************************************************************
//  TYPE DEFINITIONS
///*************************************************************************

// ======================> ppu2c0x_device

class ppu2c0x_device :  public device_t,
						public device_memory_interface,
						public device_video_interface,
						public device_palette_interface
{
public:
	typedef device_delegate<void (int scanline, int vblank, int blanked)> scanline_delegate;
	typedef device_delegate<void (int scanline, int vblank, int blanked)> hblank_delegate;
	typedef device_delegate<void (int *ppu_regs)> nmi_delegate;
	typedef device_delegate<int (int address, int data)> vidaccess_delegate;
	typedef device_delegate<void (offs_t offset)> latch_delegate;

	enum
	{
		NTSC_SCANLINES_PER_FRAME   = 262,
		PAL_SCANLINES_PER_FRAME    = 312,

		BOTTOM_VISIBLE_SCANLINE    = 239,
		VBLANK_FIRST_SCANLINE      = 241,
		VBLANK_FIRST_SCANLINE_PALC = 291,
		VBLANK_LAST_SCANLINE_NTSC  = 260,
		VBLANK_LAST_SCANLINE_PAL   = 310

		// Both the scanline immediately before and immediately after VBLANK
		// are non-rendering and non-vblank.
	};

	virtual uint8_t read(offs_t offset);
	virtual void write(offs_t offset, uint8_t data);
	virtual uint8_t palette_read(offs_t offset);
	virtual void palette_write(offs_t offset, uint8_t data);

	template <typename T> void set_cpu_tag(T &&tag) { m_cpu.set_tag(std::forward<T>(tag)); }
	auto int_callback() { return m_int_callback.bind(); }

	/* routines */
	rgb_t nespal_to_RGB(int color_intensity, int color_num);
	virtual void init_palette();
	void init_palette(bool indirect);
	virtual uint32_t palette_entries() const override { return 4*16*8; }

	virtual void read_tile_plane_data(int address, int color);
	virtual void shift_tile_plane_data(uint8_t &pix);
	virtual void draw_tile_pixel(uint8_t pix, int color, pen_t back_pen, uint32_t *&dest, const pen_t *color_table);
	virtual void draw_tile(uint8_t *line_priority, int color_byte, int color_bits, int address, int start_x, pen_t back_pen, uint32_t *&dest, const pen_t *color_table);
	void draw_background( uint8_t *line_priority );

	virtual void read_sprite_plane_data(int address);
	virtual void make_sprite_pixel_data(uint8_t &pixel_data, int flipx);
	virtual void draw_sprite_pixel(int sprite_xpos, int color, int pixel, uint8_t pixel_data, bitmap_rgb32 &bitmap);
	virtual void read_extra_sprite_bits(int sprite_index);

	void draw_sprites(uint8_t *line_priority);
	void render_scanline();
	void update_scanline();

	void spriteram_dma(address_space &space, const uint8_t page);
	void render(bitmap_rgb32 &bitmap, int flipx, int flipy, int sx, int sy, const rectangle &cliprect);
	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	int get_current_scanline() { return m_scanline; }
	template <typename... T> void set_scanline_callback(T &&... args) { m_scanline_callback_proc.set(std::forward<T>(args)...); m_scanline_callback_proc.resolve(); /* FIXME: if this is supposed to be set at config time, it should be resolved on start */ }
	template <typename... T> void set_hblank_callback(T &&... args) { m_hblank_callback_proc.set(std::forward<T>(args)...); m_hblank_callback_proc.resolve(); /* FIXME: if this is supposed to be set at config time, it should be resolved on start */ }
	template <typename... T> void set_vidaccess_callback(T &&... args) { m_vidaccess_callback_proc.set(std::forward<T>(args)...); m_vidaccess_callback_proc.resolve(); /* FIXME: if this is supposed to be set at config time, it should be resolved on start */ }
	void set_scanlines_per_frame(int scanlines) { m_scanlines_per_frame = scanlines; }

	// MMC5 has to be able to check this
	int is_sprite_8x16() { return m_regs[PPU_CONTROL0] & PPU_CONTROL0_SPRITE_SIZE; }
	int get_draw_phase() { return m_draw_phase; }
	int get_tilenum() { return m_tilecount; }

	//27/12/2002 (HACK!)
	template <typename... T> void set_latch(T &&... args) { m_latch.set(std::forward<T>(args)...); m_latch.resolve(); /* FIXME: if this is supposed to be set at config time, it should be resolved on start */ }

	//  void update_screen(bitmap_t &bitmap, const rectangle &cliprect);

	// some bootleg / clone hardware appears to ignore this
	void use_sprite_write_limitation_disable() { m_use_sprite_write_limitation = false; }
	uint16_t get_vram_dest();
	void set_vram_dest(uint16_t dest);

	void ppu2c0x(address_map &map);
protected:
	// registers definition
	enum
	{
		PPU_CONTROL0 = 0,
		PPU_CONTROL1,
		PPU_STATUS,
		PPU_SPRITE_ADDRESS,
		PPU_SPRITE_DATA,
		PPU_SCROLL,
		PPU_ADDRESS,
		PPU_DATA,
		PPU_MAX_REG
	};

	// bit definitions for (some of) the registers
	enum
	{
		PPU_CONTROL0_INC               = 0x04,
		PPU_CONTROL0_SPR_SELECT        = 0x08,
		PPU_CONTROL0_CHR_SELECT        = 0x10,
		PPU_CONTROL0_SPRITE_SIZE       = 0x20,
		PPU_CONTROL0_NMI               = 0x80,

		PPU_CONTROL1_DISPLAY_MONO      = 0x01,
		PPU_CONTROL1_BACKGROUND_L8     = 0x02,
		PPU_CONTROL1_SPRITES_L8        = 0x04,
		PPU_CONTROL1_BACKGROUND        = 0x08,
		PPU_CONTROL1_SPRITES           = 0x10,
		PPU_CONTROL1_COLOR_EMPHASIS    = 0xe0,

		PPU_STATUS_8SPRITES            = 0x20,
		PPU_STATUS_SPRITE0_HIT         = 0x40,
		PPU_STATUS_VBLANK              = 0x80
	};

	// construction/destruction
	ppu2c0x_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock = 0);

	virtual void device_start() override;
	virtual void device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr) override;
	virtual void device_config_complete() override;

	// device_config_memory_interface overrides
	virtual space_config_vector memory_space_config() const override;

	// address space configurations
	const address_space_config      m_space_config;

	required_device<cpu_device> m_cpu;

	int                         m_scanlines_per_frame;  /* number of scanlines per frame */
	int                         m_security_value;       /* 2C05 protection */
	int                         m_vblank_first_scanline;  /* the very first scanline where VBLANK occurs */

	// used in rendering
	uint8_t m_planebuf[2];
	int                         m_scanline;         /* scanline count */
	std::unique_ptr<uint8_t[]>  m_spriteram;           /* sprite ram */


private:
	static constexpr device_timer_id TIMER_HBLANK = 0;
	static constexpr device_timer_id TIMER_NMI = 1;
	static constexpr device_timer_id TIMER_SCANLINE = 2;

	inline uint8_t readbyte(offs_t address);
	inline void writebyte(offs_t address, uint8_t data);

	std::unique_ptr<bitmap_rgb32>                m_bitmap;          /* target bitmap */
	std::unique_ptr<pen_t[]>    m_colortable;          /* color table modified at run time */
	std::unique_ptr<pen_t[]>    m_colortable_mono;     /* monochromatic color table modified at run time */

	scanline_delegate           m_scanline_callback_proc;   /* optional scanline callback */
	hblank_delegate             m_hblank_callback_proc; /* optional hblank callback */
	vidaccess_delegate          m_vidaccess_callback_proc;  /* optional video access callback */
	devcb_write_line            m_int_callback;         /* nmi access callback from interface */

	int                         m_regs[PPU_MAX_REG];        /* registers */
	int                         m_refresh_data;         /* refresh-related */
	int                         m_refresh_latch;        /* refresh-related */
	int                         m_x_fine;               /* refresh-related */
	int                         m_toggle;               /* used to latch hi-lo scroll */
	int                         m_add;              /* vram increment amount */
	int                         m_videomem_addr;        /* videomem address pointer */
	int                         m_data_latch;           /* latched videomem data */
	int                         m_buffered_data;
	int                         m_tile_page;            /* current tile page */
	int                         m_sprite_page;          /* current sprite page */
	int                         m_back_color;           /* background color */
	uint8_t                     m_palette_ram[0x20];        /* shouldn't be in main memory! */
	int                         m_scan_scale;           /* scan scale */
	int                         m_tilecount;            /* MMC5 can change attributes to subsets of the 34 visible tiles */
	int                         m_draw_phase;           /* MMC5 uses different regs for BG and OAM */
	latch_delegate              m_latch;

	// timers
	emu_timer                   *m_hblank_timer;        /* hblank period at end of each scanline */
	emu_timer                   *m_nmi_timer;           /* NMI timer */
	emu_timer                   *m_scanline_timer;      /* scanline timer */

	bool m_use_sprite_write_limitation;
};

class ppu2c0x_rgb_device : public ppu2c0x_device {
protected:
	ppu2c0x_rgb_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock = 0);

	virtual void init_palette() override;

private:
	required_region_ptr<uint8_t> m_palette_data;
};

class ppu2c02_device : public ppu2c0x_device {
public:
	ppu2c02_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
};

class ppu2c03b_device : public ppu2c0x_rgb_device {
public:
	ppu2c03b_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
};

class ppu2c04_device : public ppu2c0x_rgb_device {
public:
	ppu2c04_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
};

class ppu2c07_device : public ppu2c0x_device {
public:
	ppu2c07_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
};

class ppupalc_device : public ppu2c0x_device {
public:
	ppupalc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
};

class ppu2c05_01_device : public ppu2c0x_rgb_device {
public:
	ppu2c05_01_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
};

class ppu2c05_02_device : public ppu2c0x_rgb_device {
public:
	ppu2c05_02_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
};

class ppu2c05_03_device : public ppu2c0x_rgb_device {
public:
	ppu2c05_03_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
};

class ppu2c05_04_device : public ppu2c0x_rgb_device {
public:
	ppu2c05_04_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
};


// device type definition
//extern const device_type PPU_2C0X;
DECLARE_DEVICE_TYPE(PPU_2C02,    ppu2c02_device)    // NTSC NES
DECLARE_DEVICE_TYPE(PPU_2C03B,   ppu2c03b_device)   // Playchoice 10
DECLARE_DEVICE_TYPE(PPU_2C04,    ppu2c04_device)    // Vs. Unisystem
DECLARE_DEVICE_TYPE(PPU_2C07,    ppu2c07_device)    // PAL NES
DECLARE_DEVICE_TYPE(PPU_PALC,    ppupalc_device)    // PAL Clones
DECLARE_DEVICE_TYPE(PPU_2C05_01, ppu2c05_01_device) // Vs. Unisystem (Ninja Jajamaru Kun)
DECLARE_DEVICE_TYPE(PPU_2C05_02, ppu2c05_02_device) // Vs. Unisystem (Mighty Bomb Jack)
DECLARE_DEVICE_TYPE(PPU_2C05_03, ppu2c05_03_device) // Vs. Unisystem (Gumshoe)
DECLARE_DEVICE_TYPE(PPU_2C05_04, ppu2c05_04_device) // Vs. Unisystem (Top Gun)

#endif // MAME_VIDEO_PPU2C0X_H
