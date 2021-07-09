#include <common.h>
//#include <asm/arch/drv_display.h>
#include <sys_config.h>
#include <asm/arch/timer.h>
#include <power/sunxi/pmu.h>
#include <power/sunxi/power.h>
#include <sunxi_board.h>
#include <fdt_support.h>
#include <sys_config_old.h>

DECLARE_GLOBAL_DATA_PTR;

typedef enum __BOOT_POWER_STATE
{
	STATE_SHUTDOWN_DIRECTLY = 0,
	STATE_SHUTDOWN_CHARGE,
	STATE_ANDROID_CHARGE,
	STATE_NORMAL_BOOT
}BOOT_POWER_STATE_E;
extern int board_display_eink_update(char *name, __u32 update_mode);

//power_start
//0:  not allow boot by insert dcin: press power key in long time & pre state is system state(Battery ratio shouled enough)
//1: allow boot directly  by insert dcin:( Battery ratio shouled enough)
//2: not allow boot by insert dcin:press power key in long time & pre state is system state(do not check battery ratio)
//3: allow boot directly  by insert dcin:( do not check battery ratio)
uint32_t PowerStart = 0;

int __sunxi_bmp_display(char* name)
{
	return 0;
}


int sunxi_bmp_display(char * name)
	__attribute__((weak, alias("__sunxi_bmp_display")));


#if 0
static void UpdateChargeVariable(void)
{
	#if 0
	int ChargeMode = 0;
	if(0 == script_parser_fetch("charging_type", "charging_type", &ChargeMode, 1))
	{
		gd->chargemode = 1;
	}
	#endif
	//the default mode is use Android charge
	gd->chargemode = 1;
}
#endif

static void EnterNormalShutDownMode(void)
{
	sunxi_board_shutdown();
	for(;;);
}

static void EnterLowPowerShutDownMode(void)
{
	sunxi_board_shutdown();
	for(;;);
}

#if 0
static void EnterShutDownWithChargeMode(void)
{
}
#endif

static void EnterAndroidChargeMode(void)
{
}

static void EnterNormalBootMode(void)
{
}



int ProbePreSystemMode(void)
{
    int  PreSysMode = 0;

    PreSysMode = axp_probe_pre_sys_mode();
    if(PreSysMode == PMU_PRE_SYS_MODE )
    {
        printf("pre sys mode\n");
        return PMU_PRE_SYS_MODE;
    }
    else if(PreSysMode == PMU_PRE_CHARGE_MODE)
    {
        printf("pre charge mode\n");
        return PMU_PRE_CHARGE_MODE;
    }
    else if(PreSysMode == PMU_PRE_FASTBOOT_MODE)
    {
        printf("pre fastboot mode\n");
        return PMU_PRE_FASTBOOT_MODE;
    }

    return 0;
}

#if 0
static int ProbeStartupCause(void)
{
	uint PowerOnCause = 0;
	PowerOnCause = axp_probe_startup_cause();
	if(PowerOnCause == AXP_POWER_ON_BY_POWER_KEY)
	{
		printf("key trigger\n");
	}
	else if(PowerOnCause == AXP_POWER_ON_BY_POWER_TRIGGER)
	{
		printf("power trigger\n");
	}
	return PowerOnCause;
}
#endif

//check battery and voltage
static int GetBatteryRatio( void)
{
	int  Ratio ;

	Ratio = axp_probe_rest_battery_capacity();
	if(Ratio < 1)
	{
		//some board coulombmeter value is not precise whit low capacity, so open it again here
		//note :in this case ,you should wait at least 1s berfore you read battery ratio again 
		axp_set_coulombmeter_onoff(0);
		axp_set_coulombmeter_onoff(1);
	}
	return Ratio;
}

#if 0
/*
*function : GetStateOnLowBatteryRatio
*@PowerBus           :   0: power  not exist   other:  power  exist
*@LowVoltageFlag :   0:high voltage  1: low voltage
*@PowerOnCause  :   Power is inserted OR   power_key is pressed
*note:  Decide which state should enter when battery ratio is low
*/
static BOOT_POWER_STATE_E GetStateOnLowBatteryRatio(int PowerBus,int LowVoltageFlag,int PowerOnCause)
{
	BOOT_POWER_STATE_E BootPowerState;

	do {
		//power  not exist,shutdown directly
		if(PowerBus == 0)
		{
			BootPowerState = STATE_SHUTDOWN_DIRECTLY;
			break;
		}

		//----------------power  exist: dcin or vbus------------------
		//user config is 3, allow boot directly  by insert dcin, not check battery ratio
		if(PowerStart == 3)
		{
			BootPowerState = STATE_NORMAL_BOOT;
			break;
		}

		//low voltage
		if(LowVoltageFlag)
		{
			BootPowerState = STATE_SHUTDOWN_CHARGE;
		}
		//high voltage
		else
		{
			BootPowerState = (PowerOnCause == AXP_POWER_ON_BY_POWER_TRIGGER) ? \
				STATE_ANDROID_CHARGE:STATE_SHUTDOWN_CHARGE;
		}
	}while(0);
	return BootPowerState;
}

static BOOT_POWER_STATE_E GetStateOnHighBatteryRatio(int PowerBus,int LowVoltageFlag,int PowerOnCause)
{
	BOOT_POWER_STATE_E BootPowerState;
	//battery ratio enougth
	//note : 
	//enter android charge:  power is  inserted and PowerStart not allow boot directly  by insert dcin
	//enter normal  boot:  other way

	BootPowerState = STATE_NORMAL_BOOT;
	if(PowerOnCause == AXP_POWER_ON_BY_POWER_TRIGGER)
	{
		//user config is 3 or 1, allow boot directly  by insert dcin,
		//if(PowerStart == 3 || PowerStart == 1)
		//{
			BootPowerState = STATE_NORMAL_BOOT;
		//}
		//else
		//{
		//	BootPowerState = STATE_ANDROID_CHARGE;
		//}
	}

	return BootPowerState;
}
#endif


#define   LED_GPIO                 6     // PG
#define   LED_PIN                  9

static void set_led(int status)
{
  volatile u32 *gpio_base = (volatile u32 *)(SUNXI_PIO_BASE + LED_GPIO * 0x24);
  volatile u32 *cfg = &gpio_base[LED_PIN / 8];
  volatile u32 *data = &gpio_base[4];

  *cfg = ((*cfg) & ~(0xf << ((LED_PIN & 7) * 4))) | (0x1 << ((LED_PIN & 7) * 4)); // output
  if (status) {
    *data = (*data) | (1 << LED_PIN);
  } else {
    *data = (*data) & ~(1 << LED_PIN);
  }
}

//function : PowerCheck
//para: null
//note:  Decide whether to boot
int PowerCheck(void)
{
	int BatExist = 0;
	int PowerBus=0;
	int BatVol=0;
	int BatRatio=0;
	int SafeVol =0;
	int Ret = 0;
	int nodeoffset;
	uint pmu_bat_unused = 0;

	int ledon = 1;
	int delaycnt = 1;

	if(get_boot_work_mode() != WORK_MODE_BOOT)
	{
		return 0;
	}

	nodeoffset =  fdt_path_offset(working_fdt,PMU_SCRIPT_NAME);
	if(nodeoffset >0)
	{
		script_parser_fetch(PMU_SCRIPT_NAME, "power_start", (int *)&PowerStart, 1);
		script_parser_fetch(PMU_SCRIPT_NAME, "pmu_bat_unused", (int *)&pmu_bat_unused, 1);
	}
	//clear  power key 
	axp_probe_key();

	//check battery
	BatExist = pmu_bat_unused?0:axp_probe_battery_exist();

	//check power bus
	PowerBus = axp_probe_power_source();

	power_limit_for_vbus(BatExist,PowerBus);

	if(BatExist <= 0)
	{
		printf("no battery exist\n");
		EnterNormalBootMode();
		return 0;
	}

	//if android call shutdown when  charing , then boot should enter android charge mode
	if((PMU_PRE_CHARGE_MODE == ProbePreSystemMode()))
	{
		if(PowerBus)
		{
			EnterAndroidChargeMode();
		}
		else
		{
			printf("pre system is charge mode,but without dc or ac, should be ShowDown\n");
			EnterNormalShutDownMode();
		}
		return 0;
	}

	//PMU_SUPPLY_DCDC2 is for cpua
	Ret = script_parser_fetch(PMU_SCRIPT_NAME, "pmu_safe_vol", &SafeVol, 1);
	if((Ret) || (SafeVol < 3000))
	{
		SafeVol = 3500;
		//SafeVol = 4000;
	}

	while (1) {

		set_led(ledon);

		//check battery ratio
		PowerBus = axp_probe_power_source();
		BatRatio = GetBatteryRatio();
		BatVol = axp_probe_battery_vol();
		printf("Bus:%d VBat:%d Ratio:%d\n", PowerBus, BatVol, BatRatio);

		if (BatVol >= SafeVol && BatRatio >= 1) {
			if (--delaycnt <= 0) {
				set_led(1);
				EnterNormalBootMode();
				return 0;
			}
		} else {
			delaycnt = 60;
		}

		if (! PowerBus) {
			__msdelay(100);
			EnterLowPowerShutDownMode();
			return 0;
		}
		__msdelay(250);
		ledon = 1 - ledon;

	}
}

