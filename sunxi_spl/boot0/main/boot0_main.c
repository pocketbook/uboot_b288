/*
**********************************************************************************************************************
*
*						           the Embedded Secure Bootloader System
*
*
*						       Copyright(C), 2006-2014, Allwinnertech Co., Ltd.
*                                           All Rights Reserved
*
* File    :
*
* By      :
*
* Version : V2.00
*
* Date	  :
*
* Descript:
**********************************************************************************************************************
*/
#include <common.h>
#include <private_boot0.h>
#include <private_uboot.h>
#include <asm/arch/clock.h>
#include <asm/arch/timer.h>
#include <asm/arch/uart.h>
#include <asm/arch/dram.h>
#include <asm/arch/rtc_region.h>
#include <private_toc.h>
#include <boot0_helper.h>
#include <asm/arch/key.h>

#ifdef CONFIG_SUNXI_MULITCORE_BOOT
#include <asm/arch/platsmp.h>
#include <asm/arch/base_pmu.h>
extern void sunxi_set_cpu1_wfi(void);
extern uint sunxi_probe_key_input(void);
extern uint sunxi_probe_uart_input(void);
#endif
static int uart_input_value, lradc_input_value;
extern const boot0_file_head_t  BT0_head;
static int boot0_clear_env(void);
static void update_uboot_info(__u32 dram_size);

//static int check_update_key(int key_value);

int axp22x_check_voltage(void);
int set_ddr_voltage(int set_vol);

#ifdef	SUNXI_OTA_TEST
static void print_ota_test(void);
#endif


extern char boot0_hash_value[64];
extern int debug_enable;

int mmc_config_addr = (int)(BT0_head.prvt_head.storage_data);

//phoenixcard will set this bit
#define BOOT0_BURN_SECURE_BIT (1<<31)

/*******************************************************
we should implement below interfaces if platform support
handler standby flag in boot0
*******************************************************/
int __attribute__((weak)) probe_super_standby_flag (void)
{
	return 0;
}

void __attribute__((weak)) handler_super_standby(void)
{
	return;
}

void __attribute__((weak)) sid_set_security_mode(void)
{
	return;
}

void __attribute__((weak)) reboot(void)
{
	return;
}

//void __attribute__((weak)) set_gpio_gate(void)
//{
//	return;
//}

int __attribute__((weak)) probe_power_key(void)
{
	return 0;
}

/*******************************************************************************
main:   body for c runtime
*******************************************************************************/
void main( void )
{
	__u32 status;
	__s32 dram_size;
	__u32 fel_flag;
	__u32 boot_cpu=0;
	int use_monitor = 0;
	__maybe_unused int pmu_type = 0;
	__maybe_unused int cpu1_power_on = 0;
	__maybe_unused struct spare_boot_head_t  *bfh = NULL;

	timer_init();
	sunxi_serial_init( BT0_head.prvt_head.uart_port, (void *)BT0_head.prvt_head.uart_ctrl, 6 );

	int sstandby = probe_super_standby_flag();

	if (! sstandby)  dprintf("HELLO ;) BOOT0 is starting!\n");
	debug_enable = 1;
	//dprintf("boot0 commit : %s \n",boot0_hash_value);
	//uart_input_value = set_debugmode_flag();
	sunxi_key_init();
#ifdef	SUNXI_OTA_TEST
	print_ota_test();
#endif

#if 0
	if (BT0_head.prvt_head.enable_jtag&BOOT0_BURN_SECURE_BIT)
	{
		printf("ready to burn secure bit\n");
		sid_set_security_mode();
		reboot();
	}

	if( BT0_head.prvt_head.enable_jtag )
	{
		boot_set_gpio((normal_gpio_cfg *)BT0_head.prvt_head.jtag_gpio, 6, 1);
	}
#endif

#ifdef CONFIG_SUNXI_MULITCORE_BOOT
	pmu_type = pmu_init(BT0_head.prvt_head.power_mode);
	set_pll_voltage(CONFIG_SUNXI_CORE_VOL);
#endif

	set_pll();
	set_gpio_gate();

	axp22x_check_voltage();

	//set_ddr_voltage(1350);

	//detect step1: rtc
	fel_flag = rtc_region_probe_fel_flag();
	if(fel_flag == SUNXI_RUN_EFEX_FLAG)
	{
		rtc_region_clear_fel_flag();
		printf("eraly jump fel\n");
		goto __boot0_entry_err0;
	}

#if 0

	//detect ste2: uart input
	if (uart_input_value == '2') {
		printf("detected user input 2\n");
		goto __boot0_entry_err0;
	}

	//detect step3: power unit
	lradc_input_value = sunxi_key_read();
	if (check_update_key(lradc_input_value) ) {
		printf("detected power key unit\n");
		goto __boot0_entry_err0;
	}

#endif

	debug_enable = 0;
#ifdef FPGA_PLATFORM
	dram_size = mctl_init((void *)BT0_head.prvt_head.dram_para);
#else
	dram_size = init_DRAM(0, (void *)BT0_head.prvt_head.dram_para);
#endif
	debug_enable = 1;

	if(! dram_size)
	{
		printf("initializing SDRAM Fail.\n");
		goto  __boot0_entry_err0;
	}
	//on some platform, boot0 should handler standby flag.
	if (sstandby) handler_super_standby();

	printf("DRAM: %d\n", dram_size);

	mmu_setup(dram_size);

	//load boot1
	status = load_boot1();
	if(status == 0 )
	{
		use_monitor = 0;
		status = load_fip(&use_monitor);
	}

	//printf("Ready to disable icache.\n");

    // disable instruction cache
	mmu_turn_off( );

	if( status == 0 )
	{
		//update bootpackage size for uboot
		update_uboot_info(dram_size);
		//update flash para
		update_flash_para();
		//update dram para before jmp to boot1
		set_dram_para((void *)&BT0_head.prvt_head.dram_para, dram_size, boot_cpu);

		bfh = (struct spare_boot_head_t *) CONFIG_SYS_TEXT_BASE;
		bfh->boot_ext[0].data[0] = pmu_type;
		bfh->boot_ext[0].data[1] = uart_input_value;
		bfh->boot_ext[0].data[2] = lradc_input_value;
		bfh->boot_ext[0].data[3] = debug_enable;

#if 0
		if (uart_input_value == 'k') {
			printf_all();
		}
#endif

		printf("Jump to u-boot\n");
		if(use_monitor)
		{
			boot0_jmp_monitor();
		}
		else
		{
			boot0_jmp_boot1(CONFIG_SYS_TEXT_BASE);
		}
	}

__boot0_entry_err0:
	printf_all();
	boot0_clear_env();

	boot0_jmp_other(FEL_BASE);
}

/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static int boot0_clear_env(void)
{

	reset_pll();
	mmu_turn_off();

	__msdelay(10);

	return 0;
}

//
static void update_uboot_info(__u32 dram_size)
{
	struct spare_boot_head_t  *bfh = (struct spare_boot_head_t *) CONFIG_SYS_TEXT_BASE;
	struct sbrom_toc1_head_info *toc1_head = (struct sbrom_toc1_head_info *)CONFIG_BOOTPKG_STORE_IN_DRAM_BASE;
	bfh->boot_data.boot_package_size = toc1_head->valid_len;
	bfh->boot_data.dram_scan_size = dram_size;
	//printf("boot package size: 0x%x\n",bfh->boot_data.boot_package_size);
}

#define   KEY_DELAY_MAX          (8)
#define   KEY_DELAY_EACH_TIME    (40)
#define   KEY_MAX_COUNT_GO_ON    ((KEY_DELAY_MAX * 1000)/(KEY_DELAY_EACH_TIME))

#if 0
static int check_update_key(int key_value)
{
	int time_tick = 0;
	int count = 0;
	int power_key;

	if(key_value <= 0)
		return 0;

	probe_power_key();			//clear power key status
	while(sunxi_key_read() > 0) //press key and not loosen
	{
		time_tick++;
		__msdelay(KEY_DELAY_EACH_TIME);
		//detect power key status for fel mode
		power_key = probe_power_key();
		if (power_key > 0)
		{
			count ++;
		}
		if(count == 3)
		{
			dprintf("you can loosen the key to update now\n");
			//jump to fel
			return -1;
		}

		if(time_tick > KEY_MAX_COUNT_GO_ON)
		{
			dprintf("time out\n");

			return 0;
		}
	}
	dprintf("key not pressed anymore\n");
	return 0;
}
#endif

#ifdef	SUNXI_OTA_TEST
static void print_ota_test(void)
{
	printf("*********************************************\n");
	printf("*********************************************\n");
	printf("*********************************************\n");
	printf("*********************************************\n");
	printf("********[OTA TEST]:update boot0 sucess*******\n");
	printf("*********************************************\n");
	printf("*********************************************\n");
	printf("*********************************************\n");
	printf("*********************************************\n");
	return;
}
#endif
