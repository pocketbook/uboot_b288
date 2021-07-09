/*
 * Papyrus epaper power control HAL
 *
 *      Copyright (C) 2009 Dimitar Dimitrov, MM Solutions
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 *
 * TPS6518x power control is facilitated using I2C control and WAKEUP GPIO
 * pin control. The other VCC GPIO Papyrus' signals must be tied to ground.
 *
 * TODO:
 * 	- Instead of polling, use interrupts to signal power up/down
 * 	  acknowledge.
 */

#include <common.h>
#include <i2c.h>


#define PAPYRUS2_1P1_I2C_ADDRESS		0x68
extern void papyrus_set_i2c_address(int address);
#define msleep(a)	udelay(a * 1000)

struct ebc_pwr_ops
{
	int (*power_on)(void);
	int (*power_down)(void);
};
#define TPS65185_I2C_NAME "tps65185"

#define PAPYRUS_VCOM_MAX_MV		0
#define PAPYRUS_VCOM_MIN_MV		-5110

#if 0
#define tps65185_printk(fmt, args...) printf("[tps] " "%s(%d): " fmt, __FUNCTION__, __LINE__, ##args)
#else
#define tps65185_printk(fmt, args...) 
#endif

/* After waking up from sleep, Papyrus
   waits for VN to be discharged and all
   voltage ref to startup before loading
   the default EEPROM settings. So accessing
   registers too early after WAKEUP could
   cause the register to be overridden by
   default values */
#define PAPYRUS_EEPROM_DELAY_MS 50
/* Papyrus WAKEUP pin must stay low for
   a minimum time */
#define PAPYRUS_SLEEP_MINIMUM_MS 110
/* Temp sensor might take a little time to
   settle eventhough the status bit in TMST1
   state conversion is done - if read too early
   0C will be returned instead of the right temp */
#define PAPYRUS_TEMP_READ_TIME_MS 10

/* Powerup sequence takes at least 24 ms - no need to poll too frequently */
#define HW_GET_STATE_INTERVAL_MS 24

#define INVALID_GPIO -1

/* Addresses to scan */
//static const unsigned short normal_i2c[2] = {PAPYRUS2_1P1_I2C_ADDRESS,I2C_CLIENT_END};
/*
struct ctp_config_info config_info = {
	.input_type = CTP_TYPE,
	.name = NULL,
	.int_number = 0,
};
*/

typedef struct
{
	char  gpio_name[32];
	int port;
	int port_num;
	int mul_sel;
	int pull;
	int drv_level;
	int data;
	int gpio;
} disp_gpio_set_t;


struct tps65185_platform_data{
	disp_gpio_set_t wake_up_pin;
	disp_gpio_set_t vcom_ctl_pin;


	
//	int wake_up_pin;
//	int vcom_ctl_pin;
};

struct papyrus_sess {
//	struct i2c_adapter *adap;
//	struct i2c_client *client;
    
	uint8_t enable_reg_shadow;
	uint8_t enable_reg;
	uint8_t vadj;
	uint8_t vcom1;
	uint8_t vcom2;
	uint8_t vcom2off;
	uint8_t int_en1;
	uint8_t int_en2;
	uint8_t upseq0;
	uint8_t upseq1;
	uint8_t dwnseq0;
	uint8_t dwnseq1;
	uint8_t tmst1;
	uint8_t tmst2;

	/* Custom power up/down sequence settings */
	struct {
		/* If options are not valid we will rely on HW defaults. */
		bool valid;
		unsigned int dly[8];
	} seq;
//	struct timeval standby_tv;
	unsigned int v3p3off_time_ms;
//	int wake_up_pin;
//	int vcom_ctl_pin;
	disp_gpio_set_t wake_up_pin;
	int wake_up_pin_hand;
	disp_gpio_set_t vcom_ctl_pin;
	int vcom_ctl_pin_hand;


	/* True if a high WAKEUP brings Papyrus out of reset. */
	int wakeup_active_high;
	int vcomctl_active_high;

	int revision;
	int is_inited;
};

#define tps65185_SPEED	(400*1000)


#define PAPYRUS_ADDR_TMST_VALUE		0x00
#define PAPYRUS_ADDR_ENABLE		0x01
#define PAPYRUS_ADDR_VADJ		0x02
#define PAPYRUS_ADDR_VCOM1_ADJUST	0x03
#define PAPYRUS_ADDR_VCOM2_ADJUST	0x04
#define PAPYRUS_ADDR_INT_ENABLE1	0x05
#define PAPYRUS_ADDR_INT_ENABLE2	0x06
#define PAPYRUS_ADDR_INT_STATUS1	0x07
#define PAPYRUS_ADDR_INT_STATUS2	0x08
#define PAPYRUS_ADDR_UPSEQ0		0x09
#define PAPYRUS_ADDR_UPSEQ1		0x0a
#define PAPYRUS_ADDR_DWNSEQ0		0x0b
#define PAPYRUS_ADDR_DWNSEQ1		0x0c
#define PAPYRUS_ADDR_TMST1		0x0d
#define PAPYRUS_ADDR_TMST2		0x0e
#define PAPYRUS_ADDR_PG_STATUS		0x0f
#define PAPYRUS_ADDR_REVID		0x10

// INT_ENABLE1
#define PAPYRUS_INT_ENABLE1_ACQC_EN	1
#define PAPYRUS_INT_ENABLE1_PRGC_EN 0

// INT_STATUS1
#define PAPYRUS_INT_STATUS1_ACQC	1
#define PAPYRUS_INT_STATUS1_PRGC	0

// VCOM2_ADJUST
#define PAPYRUS_VCOM2_ACQ	7
#define PAPYRUS_VCOM2_PROG	6
#define PAPYRUS_VCOM2_HIZ	5



#define	SEQ_VDD(index)		((index & 3) << 6)
#define SEQ_VPOS(index)		((index & 3) << 4)
#define SEQ_VEE(index)		((index & 3) << 2)
#define SEQ_VNEG(index)		((index & 3) << 0)

/* power up seq delay time */
#define UDLY_3ms(index)		(0x00 << ((index&4) * 2))
#define UDLY_6ms(index)		(0x01 << ((index&4) * 2))
#define UDLY_9ms(index)		(0x10 << ((index&4) * 2))
#define UDLY_12ms(index)	(0x11 << ((index&4) * 2))

/* power down seq delay time */
#define DDLY_6ms(index)		(0x00 << ((index&4) * 2))
#define DDLY_12ms(index)	(0x01 << ((index&4) * 2))
#define DDLY_24ms(index)	(0x10 << ((index&4) * 2))
#define DDLY_48ms(index)	(0x11 << ((index&4) * 2))



#define PAPYRUS_MV_TO_VCOMREG(MV)	((MV) / 10)

#define V3P3_EN_MASK	0x20
#define PAPYRUS_V3P3OFF_DELAY_MS 10//100

struct papyrus_hw_state {
	uint8_t tmst_value;
	uint8_t int_status1;
	uint8_t int_status2;
	uint8_t pg_status;
};


extern int disp_sys_gpio_request(disp_gpio_set_t *gpio_list, u32 group_count_max);
extern int disp_sys_gpio_set_direction(u32 p_handler, u32 direction, const char *gpio_name);
extern int disp_sys_gpio_release(int p_handler, s32 if_release_to_default_status);
extern int disp_sys_script_get_item(char *main_name, char *sub_name, int value[], int type);



static uint8_t papyrus2_i2c_addr = PAPYRUS2_1P1_I2C_ADDRESS;

struct papyrus_sess sess;


static int papyrus_hw_setreg(struct papyrus_sess *sess, uint8_t regaddr, uint8_t val)
{
	int stat;
	stat =  i2c_write_bus(1,papyrus2_i2c_addr, regaddr, 1, &val, 1);

	return stat;
}


static int papyrus_hw_getreg(struct papyrus_sess *sess, uint8_t regaddr, uint8_t *val)
{
	int stat;

	stat = i2c_read_bus(1,papyrus2_i2c_addr, regaddr,1, val, 1);
	if(!stat)
	{
		tps65185_printk("Papyrus i2c addr %x", papyrus2_i2c_addr);
	}


	return stat;
}


static void papyrus_hw_get_pg(struct papyrus_sess *sess,
							  struct papyrus_hw_state *hwst)
{
	int stat;

	stat = papyrus_hw_getreg(sess,
				PAPYRUS_ADDR_PG_STATUS, &hwst->pg_status);
	if (stat)
		tps65185_printk("papyrus: I2C error: %d\n", stat);
}

static void papyrus_hw_send_powerup(struct papyrus_sess *sess)
{
	int stat = 0;

	// set VADJ 
	stat |= papyrus_hw_setreg(sess, PAPYRUS_ADDR_VADJ, sess->vadj);

	// set UPSEQs & DWNSEQs 
	stat |= papyrus_hw_setreg(sess, PAPYRUS_ADDR_UPSEQ0, sess->upseq0);
	stat |= papyrus_hw_setreg(sess, PAPYRUS_ADDR_UPSEQ1, sess->upseq1);
	stat |= papyrus_hw_setreg(sess, PAPYRUS_ADDR_DWNSEQ0, sess->dwnseq0);
	stat |= papyrus_hw_setreg(sess, PAPYRUS_ADDR_DWNSEQ1, sess->dwnseq1);

	// commit it, so that we can adjust vcom through "Rk_ebc_power_control_Release_v1.1" 
	//stat |= papyrus_hw_setreg(sess, PAPYRUS_ADDR_VCOM1_ADJUST, sess->vcom1);
	//stat |= papyrus_hw_setreg(sess, PAPYRUS_ADDR_VCOM2_ADJUST, sess->vcom2);

#if 0
	/* Enable 3.3V switch to the panel */
	sess->enable_reg_shadow |= V3P3_EN_MASK;
	stat |= papyrus_hw_setreg(sess, PAPYRUS_ADDR_ENABLE, sess->enable_reg_shadow);
	msleep(sess->v3p3off_time_ms);
#endif

	/* switch to active mode, keep 3.3V & VEE & VDDH & VPOS & VNEG alive, 
	 * don't enable vcom buffer
	 */
	sess->enable_reg_shadow = (0x80 | 0x20 | 0x0F);
	stat |= papyrus_hw_setreg(sess, PAPYRUS_ADDR_ENABLE, sess->enable_reg_shadow);
	if (stat)
		tps65185_printk("papyrus: I2C error: %d\n", stat);

	return;
}


static int papyrus_hw_send_powerdown(struct papyrus_sess *sess)
{
	int stat;

	/* switch to standby mode, keep 3.3V & VEE & VDDH & VPOS & VNEG alive, 
	 * don't enable vcom buffer
	 */
	sess->enable_reg_shadow = (0x40 | 0x20 | 0x0F);
	stat = papyrus_hw_setreg(sess, PAPYRUS_ADDR_ENABLE, sess->enable_reg_shadow);
	
//	do_gettimeofday(&sess->standby_tv);

#if 0
	/* 3.3V switch must be turned off last */
	msleep(sess->v3p3off_time_ms);
	sess->enable_reg_shadow &= ~V3P3_EN_MASK;
	stat |= papyrus_hw_setreg(sess, PAPYRUS_ADDR_ENABLE, sess->enable_reg_shadow);
	if (stat)
		tps65185_printk("papyrus: I2C error: %d\n", stat);
#endif

	return stat;
}
static int papyrus_hw_read_temperature(int *t)
{
	int stat;
	int ntries = 50;
	uint8_t tb;

	stat = papyrus_hw_setreg(&sess, PAPYRUS_ADDR_TMST1, 0x80);

	do {
		stat = papyrus_hw_getreg(&sess,PAPYRUS_ADDR_TMST1, &tb);
	} while (!stat && ntries-- && (((tb & 0x20) == 0) || (tb & 0x80)));

	if (stat)
		return stat;

	msleep(PAPYRUS_TEMP_READ_TIME_MS);
	stat = papyrus_hw_getreg(&sess, PAPYRUS_ADDR_TMST_VALUE, &tb);
	*t = (int)(int8_t)tb;

	//tps65185_printk("current temperature is %d\n",*t);

	return stat;
}
static int papyrus_hw_get_revid(struct papyrus_sess *sess)
{
	int stat;
	uint8_t revid;

	stat = papyrus_hw_getreg(sess, PAPYRUS_ADDR_REVID, &revid);
	if (stat) {
		tps65185_printk("papyrus: I2C error: %d\n", stat);
		return stat;
	} else
		return revid;
}

void papyrus_set_i2c_address(int address)
{
	if (address == PAPYRUS2_1P1_I2C_ADDRESS)
    {
        papyrus2_i2c_addr = PAPYRUS2_1P1_I2C_ADDRESS;
    } else {
        tps65185_printk("papyrus: Invalid i2c address: %d\n", address);
    }
    tps65185_printk("papyrus i2c addr set to %x\n",papyrus2_i2c_addr);
}

static int papyrus_hw_arg_init(struct papyrus_sess *sess)
{
#if 1
	sess->vadj = 0x03;
	
	sess->upseq0 = SEQ_VNEG(0) | SEQ_VEE(1) | SEQ_VPOS(2) | SEQ_VDD(3);
	sess->upseq1 = UDLY_3ms(0) | UDLY_3ms(1) | UDLY_3ms(2) | UDLY_3ms(0);
	
	sess->dwnseq1 = SEQ_VDD(0) | SEQ_VPOS(1) | SEQ_VEE(2) | SEQ_VNEG(3);
	sess->dwnseq1 = DDLY_6ms(0) | DDLY_6ms(1) | DDLY_6ms(2) | DDLY_6ms(0);

	sess->vcom1 = (PAPYRUS_MV_TO_VCOMREG(2500) & 0x00FF);
	sess->vcom2 = ((PAPYRUS_MV_TO_VCOMREG(2500) & 0x0100) >> 8);
#else
	sess->vadj = 0x03;

	sess->upseq0 = 0xE1;
	sess->upseq1 = 0x55;

	sess->dwnseq0 = 0x1B;
	sess->dwnseq1 = 0xC0;
#endif

	return 0;
}


static int papyrus_hw_init(struct papyrus_sess *sess)
{
	int stat = 0;
#if 0	
	if((sess->wake_up_pin!= INVALID_GPIO))
		stat |= gpio_request(sess->wake_up_pin, "papyrus-wake_up");
	if((sess->vcom_ctl_pin!= INVALID_GPIO))
		stat |= gpio_request(sess->vcom_ctl_pin, "papyrus-vcom-ctl");
	if (stat) {
		tps65185_printk("papyrus: cannot reserve GPIOs\n");
		stat = -ENODEV;
		return stat;
	}
	sess->wakeup_active_high = 1;
	sess->vcomctl_active_high = 1;
	if((sess->wake_up_pin != INVALID_GPIO)){
		gpio_direction_output(sess->wake_up_pin, !sess->wakeup_active_high);
		/* wait to reset papyrus */
		msleep(PAPYRUS_SLEEP_MINIMUM_MS);
		gpio_direction_output(sess->wake_up_pin, sess->wakeup_active_high);
		gpio_direction_output(sess->vcom_ctl_pin, sess->vcomctl_active_high);
		msleep(PAPYRUS_EEPROM_DELAY_MS);
	}
#endif

	sess->wakeup_active_high = 1;
	sess->vcomctl_active_high = 1;
	sess->wake_up_pin_hand = disp_sys_gpio_request(&(sess->wake_up_pin), 1);
	sess->vcom_ctl_pin_hand	= disp_sys_gpio_request(&(sess->vcom_ctl_pin), 1);

	disp_sys_gpio_set_direction(sess->wake_up_pin_hand,!sess->wakeup_active_high, "tps65185_wakeup");
	msleep(PAPYRUS_SLEEP_MINIMUM_MS);
	disp_sys_gpio_set_direction(sess->wake_up_pin_hand,sess->wakeup_active_high, "tps65185_wakeup");
	disp_sys_gpio_set_direction(sess->wake_up_pin_hand,sess->wakeup_active_high, "tps65185_vcom");
	msleep(PAPYRUS_EEPROM_DELAY_MS);
	

	stat = papyrus_hw_get_revid(sess);
	if (stat < 0)
		goto free_gpios;
	tps65185_printk("papyrus: detected device with ID=%02x (TPS6518%dr%dp%d)\n",
			stat, stat & 0xF, (stat & 0xC0) >> 6, (stat & 0x30) >> 4);
	stat = 0;

	return stat;
	
free_gpios:
	disp_sys_gpio_release(sess->wake_up_pin_hand, 1);
	disp_sys_gpio_release(sess->vcom_ctl_pin_hand, 1);
	tps65185_printk("papyrus: ERROR: could not initialize I2C papyrus!\n");
	return stat;
}

static void papyrus_hw_power_req(bool up)
{

	tps65185_printk("papyrus: i2c pwr req: %d\n", up);
	if (up){
		papyrus_hw_send_powerup(&sess);
	} else {
		papyrus_hw_send_powerdown(&sess);
	}
	return;
}


bool papyrus_hw_power_ack(void)
{
	struct papyrus_hw_state hwst;
	int st;
	int retries_left = 10;

	do {
		papyrus_hw_get_pg(&sess, &hwst);

		tps65185_printk("hwst: tmst_val=%d, ist1=%02x, ist2=%02x, pg=%02x\n",
				hwst.tmst_value, hwst.int_status1,
				hwst.int_status2, hwst.pg_status);
		hwst.pg_status &= 0xfa;
		if (hwst.pg_status == 0xfa)
			st = 1;
		else if (hwst.pg_status == 0x00)
			st = 0;
		else {
			st = -1;	/* not settled yet */
			msleep(HW_GET_STATE_INTERVAL_MS);
		}
		retries_left--;
	} while ((st == -1) && retries_left);

	if ((st == -1) && !retries_left)
		tps65185_printk("papyrus: power up/down settle error (PG = %02x)\n", hwst.pg_status);

	return !!st;
}


static void papyrus_hw_cleanup(struct papyrus_sess *sess)
{
	disp_sys_gpio_release(sess->wake_up_pin_hand, 1);
	disp_sys_gpio_release(sess->vcom_ctl_pin_hand, 1);
}


/* -------------------------------------------------------------------------*/

int papyrus_set_enable(int enable)
{
	sess.enable_reg = enable;
	return 0;
}

int papyrus_set_vcom_voltage(int vcom_mv)
{
	sess.vcom1 = (PAPYRUS_MV_TO_VCOMREG(-vcom_mv) & 0x00FF);
	sess.vcom2 = ((PAPYRUS_MV_TO_VCOMREG(-vcom_mv) & 0x0100) >> 8);
	return 0;
}

int papyrus_set_vcom1(uint8_t vcom1)
{
	sess.vcom1 = vcom1;
	return 0;
}

int papyrus_set_vcom2(uint8_t vcom2)
{

	tps65185_printk("papyrus_set_vcom2 vcom2off 0x%02x\n", vcom2);
	sess.vcom2off = vcom2;
	return 0;
}

int papyrus_set_vadj(uint8_t vadj)
{
	sess.vadj = vadj;
	return 0;
}

int papyrus_set_int_en1(uint8_t int_en1)
{
	sess.int_en1 = int_en1;
	return 0;
}

int papyrus_set_int_en2(uint8_t int_en2)
{
	sess.int_en2 = int_en2;
	return 0;
}

int papyrus_set_upseq0(uint8_t upseq0)
{
	sess.upseq0 = upseq0;
	return 0;
}

int papyrus_set_upseq1(uint8_t upseq1)
{
	sess.upseq1 = upseq1;
	return 0;
}

int papyrus_set_dwnseq0(uint8_t dwnseq0)
{
	sess.dwnseq0 = dwnseq0;
	return 0;
}

int papyrus_set_dwnseq1(uint8_t dwnseq1)
{
	sess.dwnseq1 = dwnseq1;
	return 0;
}

int papyrus_set_tmst1(uint8_t tmst1)
{
	sess.tmst1 = tmst1;
	return 0;
}

int papyrus_set_tmst2(uint8_t tmst2)
{
	sess.tmst2 = tmst2;
	return 0;
}

int papyrus_vcom_switch(bool state)
{
	int stat;

	sess.enable_reg_shadow &= ~((1u << 4) | (1u << 6) | (1u << 7));
	sess.enable_reg_shadow |= (state ? 1u : 0) << 4;

	stat = papyrus_hw_setreg(&sess, PAPYRUS_ADDR_ENABLE,
						sess.enable_reg_shadow);

	/* set VCOM off output */
	if (!state && sess.vcom2off != 0) {
		stat = papyrus_hw_setreg(&sess, PAPYRUS_ADDR_VCOM2_ADJUST,
						sess.vcom2off);
	}

	return stat;
}

bool papyrus_standby_dwell_time_ready(void)
{
	return true;
}


static int papyrus_pm_sleep(void)
{
	
	disp_sys_gpio_set_direction(sess.wake_up_pin_hand,!sess.wakeup_active_high, "tps65185_wakeup");
	disp_sys_gpio_set_direction(sess.wake_up_pin_hand,!sess.wakeup_active_high, "tps65185_vcom");
	
	return 0;
}

static int papyrus_pm_resume(void)
{
	disp_sys_gpio_set_direction(sess.wake_up_pin_hand,sess.wakeup_active_high, "tps65185_wakeup");
	disp_sys_gpio_set_direction(sess.wake_up_pin_hand,sess.wakeup_active_high, "tps65185_vcom");
	
	return 0;
}






static int papyrus_probe(void)
{
	int stat;
    disp_gpio_set_t  *gpio_info;
	int ret;

	
	//struct device_node *np = NULL;
	//np = of_find_node_by_name(NULL,"tps65185");
	//sess = kzalloc(sizeof(*sess), GFP_KERNEL);
	//if (!sess)
	//	return -ENOMEM;
	//sess->client = client;
	//sess->adap = client->adapter;

	papyrus_hw_arg_init(&sess);

	sess.v3p3off_time_ms = PAPYRUS_V3P3OFF_DELAY_MS;


	gpio_info  = &sess.wake_up_pin;
	ret = disp_sys_script_get_item("tps65185", "tps65185_wakeup", (int*) gpio_info, 3);
	if(ret != 3)
	{
		tps65185_printk("%s: tps65185_wakeup io is invalid. \n",__func__ );
	}
	
	gpio_info  = &sess.vcom_ctl_pin;
	ret = disp_sys_script_get_item("tps65185", "tps65185_vcom", (int*) gpio_info, 3);
	if(ret != 3)
	{
		tps65185_printk("%s: tps65185_vcom io is invalid. \n",__func__ );
	}

	stat = papyrus_hw_init(&sess);
	if (stat)
	{
		return stat;
	}

	sess.enable_reg_shadow = 0;
	stat = papyrus_hw_setreg(&sess, PAPYRUS_ADDR_ENABLE,
						sess.enable_reg_shadow);

	sess.revision = papyrus_hw_get_revid(&sess);

	return stat;
}



/*
const struct pmic_driver pmic_driver_tps65185_i2c = {
	.id = "tps65185-i2c",

	.vcom_min = PAPYRUS_VCOM_MIN_MV,
	.vcom_max = PAPYRUS_VCOM_MAX_MV,
	.vcom_step = 10,

	.hw_read_temperature = papyrus_hw_read_temperature,
	.hw_power_ack = papyrus_hw_power_ack,
	.hw_power_req = papyrus_hw_power_req,

	.set_enable = papyrus_set_enable,
	.set_vcom_voltage = papyrus_set_vcom_voltage,
	.set_vcom1 = papyrus_set_vcom1,
	.set_vcom2 = papyrus_set_vcom2,
	.set_vadj = papyrus_set_vadj,
	.set_int_en1 = papyrus_set_int_en1,
	.set_int_en2 = papyrus_set_int_en2,
	.set_upseq0 = papyrus_set_upseq0,
	.set_upseq1 = papyrus_set_upseq1,
	.set_dwnseq0 = papyrus_set_dwnseq0,
	.set_dwnseq1 = papyrus_set_dwnseq1,
	.set_tmst1 = papyrus_set_tmst1,
	.set_tmst2 = papyrus_set_tmst2,

	.hw_vcom_switch = papyrus_vcom_switch,

	.hw_init = papyrus_probe,
	.hw_cleanup = papyrus_remove,

	.hw_standby_dwell_time_ready = papyrus_standby_dwell_time_ready,
	.hw_pm_sleep = papyrus_pm_sleep,
	.hw_pm_resume = papyrus_pm_resume,
};
*/


int tps65185_probe(void)
{
	int t;
	if(0 != papyrus_probe())
	{
		tps65185_printk("pmic_driver_tps65185_i2c hw_init failed.");
		return -1;
	}
	//pmic_driver_tps65185_i2c.hw_power_req((struct pmic_sess *)&pmic_sess_data,1);

	sess.is_inited = 1;
	
	tps65185_printk("tps65185_probe ok.\n");

	
	papyrus_hw_read_temperature(&t);
	tps65185_printk("----------t =%d\n", t);
#if 0
	{
		papyrus_hw_power_req(1);
		msleep(1000);
		papyrus_hw_power_req(0);
		msleep(1000);
		papyrus_hw_power_req(1);
		msleep(1000);
		papyrus_hw_power_req(0);
		
	}
#endif	

	return 0;
}

int tps65185_remove(void)
{
	papyrus_hw_cleanup(&sess);
	return 0;
}
int tps65185_suspend(void)
{
	return papyrus_pm_sleep();
}
int tps65185_resume(void)
{
	return papyrus_pm_resume();
}


/*
static struct i2c_driver tps65185_driver = {
	.class = I2C_CLASS_HWMON,
	.probe	= tps65185_probe,
	.remove 	= tps65185_remove,
	.suspend = tps65185_suspend,
	.resume  = tps65185_resume,
	.id_table	= tps65185_id,
	.detect   = tps65185_i2c_detect,
	.driver = {
		.name	  = TPS65185_I2C_NAME,
		.owner	  = THIS_MODULE,
	},
	.address_list   = normal_i2c,
};
*/


int tps65185_vcom_set(int vcom_mv)
{

	uint8_t rev_val = 0;
	int stat = 0;
	int read_vcom_mv = 0;

	tps65185_printk("tps65185_vcom_set enter.\n");
	if(!sess.is_inited)
		return -1;
//	gpio_direction_output(sess->wake_up_pin, 1);
	disp_sys_gpio_set_direction(sess.wake_up_pin_hand,sess.wakeup_active_high, "tps65185_wakeup");
	msleep(10);
	// Set vcom voltage
	papyrus_set_vcom_voltage(vcom_mv);
	stat |= papyrus_hw_setreg(&sess, PAPYRUS_ADDR_VCOM1_ADJUST,sess.vcom1);
	stat |= papyrus_hw_setreg(&sess, PAPYRUS_ADDR_VCOM2_ADJUST,sess.vcom2);
	tps65185_printk("sess->vcom1 = 0x%x sess->vcom2 = 0x%x\n",sess.vcom1,sess.vcom2);

	// PROGRAMMING
	sess.vcom2 |= 1<<PAPYRUS_VCOM2_PROG;
	stat |= papyrus_hw_setreg(&sess, PAPYRUS_ADDR_VCOM2_ADJUST,sess.vcom2);
	rev_val = 0;
	while(!(rev_val & (1<<PAPYRUS_INT_STATUS1_PRGC)))
	{
		stat |= papyrus_hw_getreg(&sess, PAPYRUS_ADDR_INT_STATUS1, &rev_val);
		tps65185_printk("PAPYRUS_ADDR_INT_STATUS1 = 0x%x\n",rev_val);
		msleep(50);
	}
	
	// VERIFICATION
	tps65185_printk("sess->vcom1 = 0x%x sess->vcom2 = 0x%x\n",sess.vcom1,sess.vcom2);
//	gpio_direction_output(sess->wake_up_pin, 0);
	disp_sys_gpio_set_direction(sess.wake_up_pin_hand,!sess.wakeup_active_high, "tps65185_wakeup");
	msleep(10);
//	gpio_direction_output(sess->wake_up_pin, 1);
	disp_sys_gpio_set_direction(sess.wake_up_pin_hand,sess.wakeup_active_high, "tps65185_wakeup");
	msleep(10);
	read_vcom_mv = 0;
	stat |= papyrus_hw_getreg(&sess, PAPYRUS_ADDR_VCOM1_ADJUST, &rev_val);
	tps65185_printk("rev_val = 0x%x\n",rev_val);
	read_vcom_mv += rev_val;
	stat |= papyrus_hw_getreg(&sess, PAPYRUS_ADDR_VCOM2_ADJUST, &rev_val);
	tps65185_printk("rev_val = 0x%x\n",rev_val);
	read_vcom_mv += ((rev_val & 0x0001)<<8);
	tps65185_printk("read_vcom_mv = %d\n",read_vcom_mv);

	if (stat)
		tps65185_printk("papyrus: I2C error: %d\n", stat);

	return 0;
}


int tps65185_power_on(void)
{
	if(sess.is_inited)
		papyrus_hw_power_req(1);
	return 0;
}
int tps65185_power_down(void)
{
	if(sess.is_inited)
		papyrus_hw_power_req(0);
	return 0;
}
int tps65185_temperature_get(int *temp)
{
	if(sess.is_inited)
		return papyrus_hw_read_temperature(temp);
	else
		return 0;
}




