#
# Copyright (c) 2015 Allwinnertech, Inc.
# Tyle <tyle at allwinnertech.com>
#
#

obj-$(CONFIG_SUNXI_DISPLAY) += disp.o
obj-y += disp.o
obj-y += disp_sys_intf.o

disp-objs	:= dev_disp.o
disp-objs	+= de/disp_display.o de/disp_features.o de/disp_device.o de/disp_lcd.o de/disp_manager.o de/disp_private.o \
			   de/disp_smart_backlight.o de/disp_enhance.o de/disp_vdevice.o \
				de/disp_eink_manager.o de/eink_pipeline_manager.o
disp-objs += lcd/panels.o lcd/lcd_source.o lcd/default_eink.o
obj-$(CONFIG_ARCH_SUN8IW10P1) += de/lowlevel_sun8iw10/
