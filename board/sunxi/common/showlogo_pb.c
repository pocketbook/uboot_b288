#include <common.h>
#include <malloc.h>
#include <sys_config.h>
#include <sunxi_display2.h>
#include <bmp_layout.h>
#include <fdt_support.h>

#include <sunxi_bmp.h>
#include <../drivers/video/sunxi/disp2/disp/de/include.h>

DECLARE_GLOBAL_DATA_PTR;

//static __u32 screen_id = 0;
//static __u32 disp_para = 0;
//static __u32 fb_base_addr = SUNXI_DISPLAY_FRAME_BUFFER_ADDR;
extern __s32 disp_delay_ms(__u32 ms);
extern long disp_ioctl(void *hd, unsigned int cmd, void *arg);

static inline void *malloc_aligned(u32 size, u32 alignment)
{
     void *ptr = (void*)malloc(size + alignment);
      if (ptr)
      {
            void * aligned =(void *)(((long)ptr + alignment) & (~(alignment-1)));

            /* Store the original pointer just before aligned pointer*/
            ((void * *) aligned) [-1]  = ptr;
             return aligned;
      }

      return NULL;
}

static inline void free_aligned(void *aligned_ptr)
{
     if (aligned_ptr)
        free (((void * *) aligned_ptr) [-1]);
}

static inline int read4(unsigned char *p, int off)
{
    return p[off] + (p[off+1] << 8) + (p[off+2] << 16) + (p[off+3] << 24);
}

static bool parse_bitmap(unsigned char *data, unsigned char *fbptr, int fbwidth, int fbheight)
{
	int start, hsize, width, height, bpp, comp, scanlen, npal, i, xx, x, yy, y;
	unsigned char *src, *dest, c;
	unsigned char palette[256];

	if (data[0] != 'B' || data[1] != 'M') {
		printf("Not a bitmap file\n");
		return false;
	}
	start = read4(data,0x0a);
	hsize = read4(data,0x0e);
	width = read4(data,0x12);
	height = read4(data,0x16);
	bpp = *((short *)(data+0x1c));
	comp = read4(data,0x1e);
	scanlen = ((width * bpp + 7) / 8 + 3) & ~0x3;
	if ((bpp != 4 && bpp != 8) || comp != 0) {
		printf("Unsupported image format\n");
		return false;
	}
	printf("Image: %dx%d bpp %d\n", width, height, bpp);

	memset(fbptr, 0xff, fbwidth * fbheight);

	src = data + 14 + hsize;
	npal = (bpp == 4) ? 16 : 256;
	for (i=0; i<npal; i++) {
		palette[i] = (src[0] + src[1]*6 + src[2]*3) / 10;
		src += 4;
	}

	xx = (fbwidth - height) / 2;
	yy = (fbheight - width) / 2;
	if (xx < 0) xx = 0;
	if (yy < 0) yy = 0;

	for (y=0; y<height && y<fbwidth; y++) {
		dest = fbptr + xx + y + (width + yy - 1) * fbwidth;
		src = data + start + (height - y - 1) * scanlen;
		for (x=0; x<width && x<fbheight; x++) {
			c = (bpp == 4) ? ((src[x/2] << ((x&1)*4)) >> 4) & 0xf : src[x];
			*dest = palette[c];
			dest -= fbwidth;
		}
	}
	return true;
}

static struct eink_8bpp_image cimage;

int board_display_eink_update(char *name, __u32 update_mode)
{
	char * bmp_argv[6] = { "fatload", "mmc", "3:5", "00000000", name, NULL };
	char bmp_addr[16];

	uint arg[4] = { 0 };
	uint cmd = 0;

	u32 width = 0;
	u32 height = 0;

	unsigned char *bmp_buffer = NULL;
	u32 buf_size = 0;
	int ret;

	char primary_key[25];
	s32 value = 0;
	u32 disp = 0;
	sprintf(primary_key, "lcd%d", disp);

	ret = disp_sys_script_get_item(primary_key, "eink_width", &value, 1);
	if (ret == 1)
	{
		width = value;
	}
	ret = disp_sys_script_get_item(primary_key, "eink_height", &value, 1);
	if (ret == 1)
	{
		height = value;
	}

	buf_size = (width*height)<<2;
	bmp_buffer = (unsigned char*)malloc_aligned(buf_size, ARCH_DMA_MINALIGN);
	if (NULL == bmp_buffer) {
		printf("fail to alloc memory for display bmp.\n");
	}

	sprintf(bmp_addr,"%lx", (ulong)SUNXI_DISPLAY_FRAME_BUFFER_ADDR);
	bmp_argv[3] = bmp_addr;

	do_fat_fsload(0, 0, 5, bmp_argv);

	if(parse_bitmap((unsigned char *)SUNXI_DISPLAY_FRAME_BUFFER_ADDR, bmp_buffer, width, height)) {
	
		cimage.update_mode = update_mode;
		cimage.flash_mode = GLOBAL;
		cimage.state = USED;
		cimage.window_calc_enable = false;
		cimage.size.height = height;
		cimage.size.width = width;
		cimage.size.align = 4;
		cimage.paddr = bmp_buffer;
		cimage.vaddr = bmp_buffer;
		cimage.update_area.x_top = 0;
		cimage.update_area.y_top = 0;
		cimage.update_area.x_bottom = width -1;
		cimage.update_area.y_bottom = height -1;

		arg[0] = (uint)&cimage;
		arg[1] = 0;
		arg[2] = 0;
		arg[3] = 0;

		cmd = DISP_EINK_UPDATE;
		ret = disp_ioctl(NULL, cmd, (void *)arg);
		if(ret != 0)
		{
			printf("update eink image fail\n");
			return -1;
		}

	}

	if(bmp_buffer){
		free_aligned(bmp_buffer);
		bmp_buffer = NULL;
	}

	return 0;
}

