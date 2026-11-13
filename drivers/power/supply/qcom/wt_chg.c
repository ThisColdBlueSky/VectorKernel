#define pr_fmt(fmt) "batt_chg %s: " fmt, __func__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/soc/qcom/pmic_glink.h>
#include <linux/soc/qcom/battery_charger.h>
#include <linux/pm_wakeup.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/consumer.h>
#include <linux/thermal.h>
#include <linux/thermal_override.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include "wt_chg_iio.h"

#define MSG_OWNER_BC			32778
#define MSG_TYPE_REQ_RESP		1
#define MSG_TYPE_NOTIFY			2

#define MAX_STR_LEN                    128

/* opcode for battery charger */
#define BC_XM_STATUS_GET		0x50
#define BC_XM_STATUS_SET		0x51
#define BC_SET_NOTIFY_REQ		0x04
#define BC_NOTIFY_IND			0x07
#define BC_BATTERY_STATUS_GET		0x30
#define BC_BATTERY_STATUS_SET		0x31
#define BC_USB_STATUS_GET		0x32
#define BC_USB_STATUS_SET		0x33
#define BC_WLS_STATUS_GET		0x34
#define BC_WLS_STATUS_SET		0x35
#define BC_SHIP_MODE_REQ_SET		0x36
#define BC_SHUTDOWN_REQ_SET		0x37
#define BC_WLS_FW_CHECK_UPDATE		0x40
#define BC_WLS_FW_PUSH_BUF_REQ		0x41
#define BC_WLS_FW_UPDATE_STATUS_RESP	0x42
#define BC_WLS_FW_PUSH_BUF_RESP		0x43
#define BC_WLS_FW_GET_VERSION		0x44
#define BC_SHUTDOWN_NOTIFY		0x47
#define BC_GENERIC_NOTIFY		0x80

#if defined(CONFIG_BQ_FG_1S)
#define BATTERY_DIGEST_LEN 32
#else
#define BATTERY_DIGEST_LEN 20
#endif
#define BATTERY_SS_AUTH_DATA_LEN 4

#define USBPD_UVDM_SS_LEN		4
#define USBPD_UVDM_VERIFIED_LEN		1

#define MAX_THERMAL_LEVEL		20

#define BATTERY_VOLTAGET_PRE     3500000
#define BATTERY_VOLTAGET_OFFSET  20000

#define WT_VBUS_UNPLUG_MV 4000

enum uvdm_state {
	USBPD_UVDM_DISCONNECT,
	USBPD_UVDM_CHARGER_VERSION,
	USBPD_UVDM_CHARGER_VOLTAGE,
	USBPD_UVDM_CHARGER_TEMP,
	USBPD_UVDM_SESSION_SEED,
	USBPD_UVDM_AUTHENTICATION,
	USBPD_UVDM_VERIFIED,
	USBPD_UVDM_REMOVE_COMPENSATION,
	USBPD_UVDM_REVERSE_AUTHEN,
	USBPD_UVDM_CONNECT,
};

enum usb_connector_type {
	USB_CONNECTOR_TYPE_TYPEC,
	USB_CONNECTOR_TYPE_MICRO_USB,
};

enum psy_type {
	PSY_TYPE_BATTERY,
	PSY_TYPE_USB,
	PSY_TYPE_XM,
	PSY_TYPE_MAX,
};

/* property ids */
enum battery_property_id {
	BATT_STATUS,
	BATT_HEALTH,
	BATT_PRESENT,
	BATT_CHG_TYPE,
	BATT_CAPACITY,
	BATT_SOH,
	BATT_VOLT_OCV,
	BATT_VOLT_NOW,
	BATT_VOLT_MAX,
	BATT_CURR_NOW,
	BATT_CHG_CTRL_LIM,
	BATT_CHG_CTRL_LIM_MAX,
	BATT_CONSTANT_CURRENT,
	BATT_TEMP,
	BATT_TECHNOLOGY,
	BATT_CHG_COUNTER,
	BATT_CYCLE_COUNT,
	BATT_CHG_FULL_DESIGN,
	BATT_CHG_FULL,
	BATT_MODEL_NAME,
	BATT_TTF_AVG,
	BATT_TTE_AVG,
	BATT_RESISTANCE,
	BATT_POWER_NOW,
	BATT_POWER_AVG,
	BATT_PROP_MAX,
};

enum usb_property_id {
	USB_ONLINE,
	USB_VOLT_NOW,
	USB_VOLT_MAX,
	USB_CURR_NOW,
	USB_CURR_MAX,
	USB_INPUT_CURR_LIMIT,
	USB_TYPE,
	USB_ADAP_TYPE,
	USB_MOISTURE_DET_EN,
	USB_MOISTURE_DET_STS,
	USB_TEMP,
	USB_REAL_TYPE,
	USB_TYPEC_COMPLIANT,
	USB_SCOPE,
	USB_CONNECTOR_TYPE,
	USB_PROP_MAX,
};

enum xm_property_id {
	XM_PROP_RESISTANCE_ID,
	XM_PROP_VERIFY_DIGEST,
	XM_PROP_CONNECTOR_TEMP,
	XM_PROP_AUTHENTIC,
	XM_PROP_CHIP_OK,
	XM_PROP_SOC_DECIMAL,
	XM_PROP_SOC_DECIMAL_RATE,
	XM_PROP_SHUTDOWN_DELAY,
	XM_PROP_VBUS_DISABLE,
	XM_PROP_CC_ORIENTATION,
	XM_PROP_SLAVE_BATT_PRESENT,
#if defined(CONFIG_BQ2597X)
	XM_PROP_BQ2597X_CHIP_OK,
	XM_PROP_BQ2597X_SLAVE_CHIP_OK,
	XM_PROP_BQ2597X_BUS_CURRENT,
	XM_PROP_BQ2597X_SLAVE_BUS_CURRENT,
	XM_PROP_BQ2597X_BUS_DELTA,
	XM_PROP_BQ2597X_BUS_VOLTAGE,
	XM_PROP_BQ2597X_BATTERY_PRESENT,
	XM_PROP_BQ2597X_SLAVE_BATTERY_PRESENT,
	XM_PROP_BQ2597X_BATTERY_VOLTAGE,
	XM_PROP_COOL_MODE,
#endif
	XM_PROP_BT_TRANSFER_START,
	XM_PROP_MASTER_SMB1396_ONLINE,
	XM_PROP_MASTER_SMB1396_IIN,
	XM_PROP_SLAVE_SMB1396_ONLINE,
	XM_PROP_SLAVE_SMB1396_IIN,
	XM_PROP_SMB_IIN_DIFF,
	XM_PROP_INPUT_SUSPEND,
	XM_PROP_REAL_TYPE,
	/*used for pd authentic*/
	XM_PROP_VERIFY_PROCESS,
	XM_PROP_VDM_CMD_CHARGER_VERSION,
	XM_PROP_VDM_CMD_CHARGER_VOLTAGE,
	XM_PROP_VDM_CMD_CHARGER_TEMP,
	XM_PROP_VDM_CMD_SESSION_SEED,
	XM_PROP_VDM_CMD_AUTHENTICATION,
	XM_PROP_VDM_CMD_VERIFIED,
	XM_PROP_VDM_CMD_REMOVE_COMPENSATION,
	XM_PROP_VDM_CMD_REVERSE_AUTHEN,
	XM_PROP_CURRENT_STATE,
	XM_PROP_ADAPTER_ID,
	XM_PROP_ADAPTER_SVID,
	XM_PROP_PD_VERIFED,
	XM_PROP_PDO2,
	XM_PROP_UVDM_STATE,
	/*****************/
	XM_PROP_FASTCHGMODE,
	XM_PROP_APDO_MAX,
	XM_PROP_THERMAL_REMOVE,
	XM_PROP_VOTER_DEBUG,
	XM_PROP_FG_RM,
	XM_PROP_MTBF_CURRENT,
	XM_PROP_FAKE_TEMP,
	XM_PROP_QBG_VBAT,
	XM_PROP_QBG_VPH_PWR,
	XM_PROP_QBG_TEMP,
	XM_PROP_FB_BLANK_STATE,
	XM_PROP_THERMAL_TEMP,
	XM_PROP_TYPEC_MODE,
	XM_PROP_NIGHT_CHARGING,
	XM_PROP_SMART_BATT,
	XM_PROP_FG1_QMAX,
	XM_PROP_FG1_RM,
	XM_PROP_FG1_FCC,
	XM_PROP_FG1_SOH,
	XM_PROP_FG1_FCC_SOH,
	XM_PROP_FG1_CYCLE,
	XM_PROP_FG1_FAST_CHARGE,
	XM_PROP_FG1_CURRENT_MAX,
	XM_PROP_FG1_VOL_MAX,
	XM_PROP_FG1_TSIM,
	XM_PROP_FG1_TAMBIENT,
	XM_PROP_FG1_TREMQ,
	XM_PROP_FG1_TFULLQ,
	XM_PROP_SHIPMODE_COUNT_RESET,
	XM_PROP_SPORT_MODE,
	XM_PROP_MAX,
};
enum {
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP = 0x80,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5,
	QTI_POWER_SUPPLY_USB_TYPE_USB_FLOAT,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3_CLASSB,
};

struct psy_state {
	struct power_supply	*psy;
	char			*model;
	char			*version;
	const int		*map;
	u32			*prop;
	u32			prop_count;
	u32			opcode_get;
	u32			opcode_set;
};

static const int battery_prop_map[BATT_PROP_MAX] = {
	[BATT_STATUS]		= POWER_SUPPLY_PROP_STATUS,
	[BATT_HEALTH]		= POWER_SUPPLY_PROP_HEALTH,
	[BATT_PRESENT]		= POWER_SUPPLY_PROP_PRESENT,
	[BATT_CHG_TYPE]		= POWER_SUPPLY_PROP_CHARGE_TYPE,
	[BATT_CAPACITY]		= POWER_SUPPLY_PROP_CAPACITY,
	[BATT_VOLT_OCV]		= POWER_SUPPLY_PROP_VOLTAGE_OCV,
	[BATT_VOLT_NOW]		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
	[BATT_VOLT_MAX]		= POWER_SUPPLY_PROP_VOLTAGE_MAX,
	[BATT_CURR_NOW]		= POWER_SUPPLY_PROP_CURRENT_NOW,
	[BATT_CHG_CTRL_LIM]	= POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	[BATT_CHG_CTRL_LIM_MAX]	= POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	[BATT_CONSTANT_CURRENT]	= POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	[BATT_TEMP]		= POWER_SUPPLY_PROP_TEMP,
	[BATT_TECHNOLOGY]	= POWER_SUPPLY_PROP_TECHNOLOGY,
	[BATT_CHG_COUNTER]	= POWER_SUPPLY_PROP_CHARGE_COUNTER,
	[BATT_CYCLE_COUNT]	= POWER_SUPPLY_PROP_CYCLE_COUNT,
	[BATT_CHG_FULL_DESIGN]	= POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	[BATT_CHG_FULL]		= POWER_SUPPLY_PROP_CHARGE_FULL,
	[BATT_MODEL_NAME]	= POWER_SUPPLY_PROP_MODEL_NAME,
	[BATT_TTF_AVG]		= POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	[BATT_TTE_AVG]		= POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	[BATT_POWER_NOW]	= POWER_SUPPLY_PROP_POWER_NOW,
	[BATT_POWER_AVG]	= POWER_SUPPLY_PROP_POWER_AVG,
};

static const int usb_prop_map[USB_PROP_MAX] = {
	[USB_ONLINE]		= POWER_SUPPLY_PROP_ONLINE,
	[USB_VOLT_NOW]		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
	[USB_VOLT_MAX]		= POWER_SUPPLY_PROP_VOLTAGE_MAX,
	[USB_CURR_NOW]		= POWER_SUPPLY_PROP_CURRENT_NOW,
	[USB_CURR_MAX]		= POWER_SUPPLY_PROP_CURRENT_MAX,
	[USB_INPUT_CURR_LIMIT]	= POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	[USB_ADAP_TYPE]		= POWER_SUPPLY_PROP_USB_TYPE,
	[USB_TEMP]		= POWER_SUPPLY_PROP_TEMP,
	[USB_SCOPE]		= POWER_SUPPLY_PROP_SCOPE,
};

static const int xm_prop_map[XM_PROP_MAX] = {

};

/* Standard usb_type definitions similar to power_supply_sysfs.c */
static const char * const power_supply_usb_type_text[] = {
	"Unknown", "USB", "USB_DCP", "USB_CDP", "USB_ACA", "USB_C",
	"USB_PD", "PD_DRP", "PD_PPS", "BrickID", "USB_HVDCP",
	"USB_HVDCP3","USB_HVDCP3P5", "USB_FLOAT"
};

static const char * const power_supply_usbc_text[] = {
	"Nothing attached",
	"Source attached (default current)",
	"Source attached (medium current)",
	"Source attached (high current)",
	"Non compliant",
	"Sink attached",
	"Powered cable w/ sink",
	"Debug Accessory",
	"Audio Adapter",
	"Powered cable w/o sink",
};

static const char * const qc_power_supply_usb_type_text[] = {
	"HVDCP", "HVDCP_3", "HVDCP_3P5","USB_FLOAT","HVDCP_3"
};

static int thermal_mitigation[] = {
	12400000, 12000000, 11500000, 11000000, 10500000,
	10000000, 9500000, 9000000, 8500000, 8000000,
	7500000, 7000000, 6500000, 6000000, 5500000,
	5000000, 4500000, 4000000, 3500000, 3000000,
	2500000, 2000000, 1500000, 1000000, 500000,
};

enum batt_iio_type {
	MAIN,
	BMS,
	CP_MASTER,
	CP_SLAVE,
};

struct batt_dt_props {
	int usb_icl_ua;
	bool no_battery;
	bool hvdcp_disable;
	int sec_charger_config;
	int auto_recharge_vbat_mv;
	int batt_profile_fcc_ua;
	int batt_profile_fv_uv;
	int batt_iterm;
	int jeita_temp_step0;
	int jeita_temp_step1;
	int jeita_temp_step2;
	int jeita_temp_step3;
	int jeita_temp_step4;
	int jeita_temp_step5;
	int jeita_temp_step6;
};

struct batt_chg {
	struct platform_device *pdev;
	struct device *dev;

	struct iio_channel *cp_master_therm;
	struct iio_channel *cp_slave_therm;
	struct iio_channel *connector_therm;

	struct power_supply *batt_psy;
	struct power_supply *usb_psy;

	struct iio_dev          *indio_dev;
	struct iio_chan_spec    *iio_chan;
	struct iio_channel	*int_iio_chans;

	struct iio_channel	**gq_ext_iio_chans;
	struct iio_channel	**cp_ext_iio_chans;
	struct iio_channel	**cp_psy_ext_iio_chans;
	struct iio_channel	**main_chg_ext_iio_chans;

	struct class battery_class;
	struct device batt_device;
	struct psy_state psy_list[PSY_TYPE_MAX];

	struct wakeup_source *wt_ws;
	struct batt_dt_props dt;
	struct delayed_work batt_chg_work;
	struct delayed_work usb_therm_work;
	struct delayed_work usb_type_work;

	struct delayed_work	xm_prop_change_work;
	struct delayed_work	charger_debug_info_print_work;

	int pd_active;
	int real_type;
	int apdo_max_volt;
	int apdo_max_curr;
	int pd_min_vol;
	int pd_max_vol;
	int pd_cur_max;
	int pd_usb_suspend;
	int pd_in_hard_reset;
	int pd_type_cnt;
	int wakeup_flag;
	int cp_master_temp;
	int cp_slave_temp;
	int ui_soc;
	int fastcharge_mode;

	bool node_flag;
	bool is_chg_control;
	bool is_battery_on;
	bool otg_enable;
	bool shutdown_delay_en;

	int therm_step;
	int pd_auth;
	int batt_auth;
	int is_pps_on;
	int batt_id;
	int lcd_on;
	int charging_call_state;
	int polarity_state;
	int mtbf_current;
	int connector_temp;
	int usb_temp;
	int usb_temp_flag;
	int vbus_cnt;
	int cp_master_adc;
	int cp_slave_adc;
	int start_cnt;
	int low_temp_flag;

	int batt_current_max;
	int batt_current_now;
	int batt_voltage_now;
	int batt_done_curr;
	int charge_voltage_max;
	int charge_design_voltage_max;
	int input_batt_current_max;
	int input_batt_current_now;
	int input_suspend;
	int battery_cv;
	int cv_flag;
	int battery_temp;
	int cycle_count;
	int cell_voltage;
	int high_temp_flag;
	int vbus_voltage;
	int input_limit_flag;

	int fake_batt_status;
	int thermal_levels;
	int system_temp_level;
	int batt_iterm;
	int shutdown_flag;
	int typec_mode;
	int mishow_flag;
	int charge_done;
	int update_cont;
	int old_capacity;
	int old_charge_type;
	int usb_float;
	int power_supply_count;
	bool isln8000flg;
	int bypass_mode;
	struct mutex bypass_lock;
	int usb_icl_override_ma;
	struct mutex icl_lock;
	bool thermal_throttle_active;
	int charge_stop_pct;
	int charge_resume_pct;
	bool limit_hold;
	int charge_limit_state;
	int limit_debounce_cnt;
	int limit_invalid_soc_cnt;
	struct mutex limit_lock;
	struct delayed_work charge_limit_work;
	int limit_raw_cc_soc;
	int limit_raw_cap;
	ktime_t last_debounce_sample_time;
};

struct batt_chg *g_batt_chg;
static struct kobject *vector_kobj;

bool wt_chg_is_bypass_active(void)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);

	if (!chg)
		return false;
	return !!READ_ONCE(chg->bypass_mode) || !!READ_ONCE(chg->limit_hold);
}
EXPORT_SYMBOL(wt_chg_is_bypass_active);

int get_iio_channel(struct batt_chg *chg, const char *propname,
                                        struct iio_channel **chan)
{
        int rc = 0;

        rc = of_property_match_string(chg->dev->of_node,
                                        "io-channel-names", propname);
        if (rc < 0)
                return 0;

        *chan = iio_channel_get(chg->dev, propname);
        if (IS_ERR(*chan)) {
                rc = PTR_ERR(*chan);
                if (rc != -EPROBE_DEFER)
                        pr_err("%s channel unavailable, %d\n", propname, rc);
                *chan = NULL;
        }

        return rc;
}

static bool is_bms_chan_valid(struct batt_chg *chip,
		enum batt_qg_exit_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->gq_ext_iio_chans[chan]))
		return false;

	if (!chip->gq_ext_iio_chans[chan]) {
		chip->gq_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					qg_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->gq_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->gq_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->gq_ext_iio_chans[chan] = NULL;
			else
				pr_err("Failed to get IIO channel %s, rc=%d\n",
				       qg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

static bool is_cp_chan_valid(struct batt_chg *chip,
		enum cp_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->cp_ext_iio_chans[chan]))
		return false;

	if (!chip->cp_ext_iio_chans[chan]) {
		chip->cp_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					cp_iio_chan_name[chan]);
		if (IS_ERR(chip->cp_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->cp_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->cp_ext_iio_chans[chan] = NULL;
			else
				pr_err("Failed to get IIO channel %s, rc=%d\n",
				       cp_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

static bool is_cp_psy_chan_valid(struct batt_chg *chip,
		enum cp_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->cp_psy_ext_iio_chans[chan]))
		return false;

	if (!chip->cp_psy_ext_iio_chans[chan]) {
		chip->cp_psy_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					cp_sec_iio_chan_name[chan]);
		if (IS_ERR(chip->cp_psy_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->cp_psy_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->cp_psy_ext_iio_chans[chan] = NULL;
			else
				pr_err("Failed to get IIO channel %s, rc=%d\n",
				       cp_sec_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

static bool is_main_chg_chan_valid(struct batt_chg *chip,
		enum cp_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->main_chg_ext_iio_chans[chan]))
		return false;

	if (!chip->main_chg_ext_iio_chans[chan]) {
		chip->main_chg_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					main_iio_chan_name[chan]);
		if (IS_ERR(chip->main_chg_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->main_chg_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->main_chg_ext_iio_chans[chan] = NULL;
			else
				pr_err("Failed to get IIO channel %s, rc=%d\n",
				       main_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

int batt_get_iio_channel(struct batt_chg *chg,
			enum batt_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list;
	int rc;
	static bool firstflg = true;

	if(firstflg){
		if(is_cp_chan_valid(chg, 0)) {
			chg->isln8000flg = false;
			firstflg = false;
			pr_err("batt_get_iio_channel SC8551\n");
		} else if(is_cp_chan_valid(chg, IIO_WT_SECOND_OFFSET)) {
			chg->isln8000flg = true;
			firstflg = false;
			pr_err("batt_get_iio_channel LN8000\n");
		}
	}

	if(chg->shutdown_flag)
		return -ENODEV;

	switch (type) {
	case CP_MASTER:
		if (!chg->isln8000flg) {
			if (!is_cp_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_ext_iio_chans[channel];
		} else {
			if (!is_cp_chan_valid(chg, (channel + IIO_WT_SECOND_OFFSET)))
				return -ENODEV;
			iio_chan_list = chg->cp_ext_iio_chans[channel + IIO_WT_SECOND_OFFSET];
		}
		break;
	case CP_SLAVE:
		if (!chg->isln8000flg) {
			if (!is_cp_psy_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_psy_ext_iio_chans[channel];
		} else {
			if (!is_cp_psy_chan_valid(chg, (channel + IIO_WT_SECOND_OFFSET)))
				return -ENODEV;
			iio_chan_list = chg->cp_psy_ext_iio_chans[channel + IIO_WT_SECOND_OFFSET];
		}
		break;
	case BMS:
		if (!is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->gq_ext_iio_chans[channel];
		break;
	case MAIN:
		if (!is_main_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}

int batt_set_iio_channel(struct batt_chg *chg,
			enum batt_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list;
	int rc;

	if(chg->shutdown_flag)
		return -ENODEV;

	switch (type) {
	case CP_MASTER:
		if (!chg->isln8000flg) {
			if (!is_cp_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_ext_iio_chans[channel];
		} else {
			if (!is_cp_chan_valid(chg, (channel + IIO_WT_SECOND_OFFSET)))
				return -ENODEV;
			iio_chan_list = chg->cp_ext_iio_chans[channel + IIO_WT_SECOND_OFFSET];
		}
		break;
	case CP_SLAVE:
		if (!chg->isln8000flg) {
			if (!is_cp_psy_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_psy_ext_iio_chans[channel];
		} else {
			if (!is_cp_psy_chan_valid(chg, (channel + IIO_WT_SECOND_OFFSET)))
				return -ENODEV;
			iio_chan_list = chg->cp_psy_ext_iio_chans[channel + IIO_WT_SECOND_OFFSET];
		}
		break;
	case BMS:
		if (!is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->gq_ext_iio_chans[channel];
		break;
	case MAIN:
		if (!is_main_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_write_channel_raw(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}

static int get_boot_mode(void)
{
#ifdef CONFIG_WT_QGKI
	char *bootmode_string= NULL;
	char bootmode_start[32] = " ";
	int rc;

	bootmode_string = strstr(saved_command_line,"androidboot.mode=");
	if(bootmode_string != NULL){
		strncpy(bootmode_start, bootmode_string+17, 7);
		rc = strncmp(bootmode_start, "charger", 7);
		if(rc == 0){
	//		pr_err("Offcharger mode!\n");
			return 1;
		}
	}
#endif
	return 0;
}

int get_charger_pump_master_temp(struct batt_chg *chg,
				 int *val)
{
	int rc, temp;

	if (chg->cp_master_therm) {
		rc = iio_read_channel_processed(chg->cp_master_therm,
				&temp);
		if (rc < 0) {
			pr_err("Error in reading temp channel, rc=%d\n", rc);
			return rc;
		}
		*val = temp / 100;
	} else {
		return -ENODATA;
	}

	return rc;
}

int get_charger_pump_slave_temp(struct batt_chg *chg,
				 int *val)
{
	int rc, temp;

	if (chg->cp_master_therm) {
		rc = iio_read_channel_processed(chg->cp_slave_therm,
				&temp);
		if (rc < 0) {
			pr_err("Error in reading temp channel, rc=%d\n", rc);
			return rc;
		}
		*val = temp / 100;
	} else {
		return -ENODATA;
	}

	return rc;
}

int get_charger_connector_temp(struct batt_chg *chg,
				 int *val)
{
	int rc, temp;

	if (chg->connector_therm) {
		rc = iio_read_channel_processed(chg->connector_therm,
				&temp);
		if (rc < 0) {
			pr_err("Error in reading temp channel, rc=%d\n", rc);
			return rc;
		}
		*val = temp / 100;
	} else {
		return -ENODATA;
	}

	return rc;
}

int get_prop_batt_health(struct batt_chg *chg, union power_supply_propval *val)
{
	int  rc = -EINVAL;
	if (!chg)
		return -EINVAL;

	rc = batt_get_iio_channel(chg, BMS, BATT_QG_TEMP, &val->intval);
	if (rc < 0)
		return -EINVAL;

	if (val->intval >= 600)
		val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (val->intval >= 450 && val->intval < 600)
		val->intval = POWER_SUPPLY_HEALTH_WARM;
	else if (val->intval >= 150 && val->intval < 450)
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
	else if (val->intval >= 0 && val->intval < 150)
		val->intval = POWER_SUPPLY_HEALTH_COOL;
	else if (val->intval < 0)
		val->intval = POWER_SUPPLY_HEALTH_COLD;

	return 0;
}

/*
 * Runtime switches for vendor thermal throttle control:
 * ignore_vendor_charging_thermal:
 *   1 = ignore vendor thermal daemon charging throttle when safe
 *   0 = stock behavior (always enforce vendor thermal throttle; default)
 * vendor_thermal_max_level:
 *   Max effective thermal mitigation level allowed when battery is safe (default: 18 = 3500mA)
 * ignore_vendor_cpu_thermal:
 *   1 = ignore vendor thermal CPU frequency caps when safe
 *   0 = stock behavior (always enforce vendor CPU frequency caps; default)
 * cpu_little_override_max_state:
 *   Max cooling state floor for Little cores when override active (default: 0 -> no cap / 1.80GHz)
 * cpu_big_override_max_state:
 *   Max cooling state floor for Big cores when override active (default: 0 -> no cap / 2.21GHz)
 * skin_override_max_mc:
 *   Chassis skin temp backstop in millidegC (default: 0 = disabled; clamp 45000-60000 with 2000mC hysteresis)
 */
static int ignore_vendor_charging_thermal = 0;
static int vendor_thermal_max_level = 18;
static int ignore_vendor_cpu_thermal = 0;
static unsigned int cpu_little_override_max_state = 0;
static unsigned int cpu_big_override_max_state = 0;
static int ignore_vendor_gpu_thermal = 0;
static unsigned int gpu_override_max_state = 0;
static int ignore_vendor_backlight_thermal = 0;
static int skin_override_max_mc = 0;
static unsigned int battery_trip_decic = 450;
static unsigned int battery_clear_decic = 420;

static DEFINE_SPINLOCK(guard_lock);
static bool guard_battery_enforced;
static bool guard_skin_enforced;
static bool cached_override_safe = false;
static bool thermal_override_initialized = false;

static struct thermal_zone_device *skin_tz;
static struct delayed_work thermal_override_poll_work;
static struct work_struct thermal_override_reapply_work;

static void (*cpu_reapply_cb)(void);
static void (*gpu_reapply_cb)(void);
static void (*backlight_reapply_cb)(void);
static DEFINE_MUTEX(reapply_cb_lock);

static inline bool thermal_override_poll_needed(void)
{
	return READ_ONCE(ignore_vendor_cpu_thermal) ||
	       READ_ONCE(ignore_vendor_gpu_thermal) ||
	       READ_ONCE(ignore_vendor_backlight_thermal) ||
	       (READ_ONCE(skin_override_max_mc) > 0);
}

static void thermal_override_reapply_work_fn(struct work_struct *work)
{
	void (*cpu_cb)(void);
	void (*gpu_cb)(void);
	void (*backlight_cb)(void);

	mutex_lock(&reapply_cb_lock);
	cpu_cb = cpu_reapply_cb;
	gpu_cb = gpu_reapply_cb;
	backlight_cb = backlight_reapply_cb;
	mutex_unlock(&reapply_cb_lock);

	if (cpu_cb)
		cpu_cb();
	if (gpu_cb)
		gpu_cb();
	if (backlight_cb)
		backlight_cb();
}

void vendor_thermal_reapply(void)
{
	if (!smp_load_acquire(&thermal_override_initialized))
		return;

	schedule_work(&thermal_override_reapply_work);
}
EXPORT_SYMBOL_GPL(vendor_thermal_reapply);

static void set_ignore_vendor_charging_thermal_val(int val)
{
	struct batt_chg *chg;

	WRITE_ONCE(ignore_vendor_charging_thermal, !!val);
	pr_info("wt_chg: ignore_vendor_charging_thermal set to %d\n", !!val);

	if (smp_load_acquire(&thermal_override_initialized)) {
		if (thermal_override_poll_needed())
			schedule_delayed_work(&thermal_override_poll_work, 0);

		vendor_thermal_reapply();

		chg = READ_ONCE(g_batt_chg);
		if (chg && !READ_ONCE(chg->shutdown_flag)) {
			cancel_delayed_work(&chg->batt_chg_work);
			schedule_delayed_work(&chg->batt_chg_work, 0);
		}
	}
}

static int param_set_ignore_vendor_charging_thermal(const char *val, const struct kernel_param *kp)
{
	int ret, num;

	ret = kstrtoint(val, 10, &num);
	if (ret)
		return ret;

	set_ignore_vendor_charging_thermal_val(num);
	return 0;
}

static const struct kernel_param_ops ignore_vendor_charging_thermal_param_ops = {
	.set = param_set_ignore_vendor_charging_thermal,
	.get = param_get_int,
};
module_param_cb(ignore_vendor_charging_thermal, &ignore_vendor_charging_thermal_param_ops, &ignore_vendor_charging_thermal, 0644);
MODULE_PARM_DESC(ignore_vendor_charging_thermal, "Ignore vendor charging thermal mitigation when battery is safe (removes fast-charging caps; skin backstop disabled by default)");

static void set_vendor_thermal_max_level_val(int val)
{
	struct batt_chg *chg;

	if (val < 0)
		val = 0;
	else if (val > 24)
		val = 24;

	WRITE_ONCE(vendor_thermal_max_level, val);
	pr_info("wt_chg: vendor_thermal_max_level set to %d\n", val);

	if (smp_load_acquire(&thermal_override_initialized)) {
		chg = READ_ONCE(g_batt_chg);
		if (chg && !READ_ONCE(chg->shutdown_flag)) {
			cancel_delayed_work(&chg->batt_chg_work);
			schedule_delayed_work(&chg->batt_chg_work, 0);
		}
	}
}

static int param_set_vendor_thermal_max_level(const char *val, const struct kernel_param *kp)
{
	int ret, num;

	ret = kstrtoint(val, 10, &num);
	if (ret)
		return ret;

	set_vendor_thermal_max_level_val(num);
	return 0;
}

static const struct kernel_param_ops vendor_thermal_max_level_param_ops = {
	.set = param_set_vendor_thermal_max_level,
	.get = param_get_int,
};
module_param_cb(vendor_thermal_max_level, &vendor_thermal_max_level_param_ops, &vendor_thermal_max_level, 0644);
MODULE_PARM_DESC(vendor_thermal_max_level, "Max allowed vendor thermal throttling level when battery is safe (default: 18)");

static void set_ignore_vendor_cpu_thermal_val(int val)
{
	WRITE_ONCE(ignore_vendor_cpu_thermal, !!val);
	pr_info("wt_chg: ignore_vendor_cpu_thermal set to %d\n", !!val);

	if (smp_load_acquire(&thermal_override_initialized)) {
		if (thermal_override_poll_needed())
			schedule_delayed_work(&thermal_override_poll_work, 0);

		vendor_thermal_reapply();
	}
}

static int param_set_ignore_vendor_cpu_thermal(const char *val, const struct kernel_param *kp)
{
	int ret, num;

	ret = kstrtoint(val, 10, &num);
	if (ret)
		return ret;

	set_ignore_vendor_cpu_thermal_val(num);
	return 0;
}

static const struct kernel_param_ops ignore_vendor_cpu_thermal_param_ops = {
	.set = param_set_ignore_vendor_cpu_thermal,
	.get = param_get_int,
};
module_param_cb(ignore_vendor_cpu_thermal, &ignore_vendor_cpu_thermal_param_ops, &ignore_vendor_cpu_thermal, 0644);
MODULE_PARM_DESC(ignore_vendor_cpu_thermal, "Ignore vendor thermal CPU frequency caps when safe (removes CPU caps; skin backstop disabled by default)");

static void set_cpu_little_override_max_state_val(unsigned int val)
{
	WRITE_ONCE(cpu_little_override_max_state, val);
	pr_info("wt_chg: cpu_little_override_max_state set to %u\n", val);

	if (smp_load_acquire(&thermal_override_initialized))
		vendor_thermal_reapply();
}

static int param_set_cpu_little_override_max_state(const char *val, const struct kernel_param *kp)
{
	int ret;
	unsigned int num;

	ret = kstrtouint(val, 10, &num);
	if (ret)
		return ret;

	set_cpu_little_override_max_state_val(num);
	return 0;
}

static const struct kernel_param_ops cpu_little_override_max_state_param_ops = {
	.set = param_set_cpu_little_override_max_state,
	.get = param_get_uint,
};
module_param_cb(cpu_little_override_max_state, &cpu_little_override_max_state_param_ops, &cpu_little_override_max_state, 0644);
MODULE_PARM_DESC(cpu_little_override_max_state, "Max cooling state floor for CPU Little cores when override active (default: 0 -> no cap / 1.80GHz)");

static void set_cpu_big_override_max_state_val(unsigned int val)
{
	WRITE_ONCE(cpu_big_override_max_state, val);
	pr_info("wt_chg: cpu_big_override_max_state set to %u\n", val);

	if (smp_load_acquire(&thermal_override_initialized))
		vendor_thermal_reapply();
}

static int param_set_cpu_big_override_max_state(const char *val, const struct kernel_param *kp)
{
	int ret;
	unsigned int num;

	ret = kstrtouint(val, 10, &num);
	if (ret)
		return ret;

	set_cpu_big_override_max_state_val(num);
	return 0;
}

static const struct kernel_param_ops cpu_big_override_max_state_param_ops = {
	.set = param_set_cpu_big_override_max_state,
	.get = param_get_uint,
};
module_param_cb(cpu_big_override_max_state, &cpu_big_override_max_state_param_ops, &cpu_big_override_max_state, 0644);
MODULE_PARM_DESC(cpu_big_override_max_state, "Max cooling state floor for CPU Big cores when override active (default: 0 -> no cap / 2.21GHz)");

static void set_ignore_vendor_gpu_thermal_val(int val)
{
	WRITE_ONCE(ignore_vendor_gpu_thermal, !!val);
	pr_info("wt_chg: ignore_vendor_gpu_thermal set to %d\n", !!val);

	if (smp_load_acquire(&thermal_override_initialized)) {
		if (thermal_override_poll_needed())
			schedule_delayed_work(&thermal_override_poll_work, 0);

		vendor_thermal_reapply();
	}
}

static int param_set_ignore_vendor_gpu_thermal(const char *val, const struct kernel_param *kp)
{
	int ret, num;

	ret = kstrtoint(val, 10, &num);
	if (ret)
		return ret;

	set_ignore_vendor_gpu_thermal_val(num);
	return 0;
}

static const struct kernel_param_ops ignore_vendor_gpu_thermal_param_ops = {
	.set = param_set_ignore_vendor_gpu_thermal,
	.get = param_get_int,
};
module_param_cb(ignore_vendor_gpu_thermal, &ignore_vendor_gpu_thermal_param_ops, &ignore_vendor_gpu_thermal, 0644);
MODULE_PARM_DESC(ignore_vendor_gpu_thermal, "Ignore vendor thermal GPU frequency caps when safe (removes GPU caps; skin backstop disabled by default)");

static void set_gpu_override_max_state_val(unsigned int val)
{
	WRITE_ONCE(gpu_override_max_state, val);
	pr_info("wt_chg: gpu_override_max_state set to %u\n", val);

	if (smp_load_acquire(&thermal_override_initialized))
		vendor_thermal_reapply();
}

static int param_set_gpu_override_max_state(const char *val, const struct kernel_param *kp)
{
	int ret;
	unsigned int num;

	ret = kstrtouint(val, 10, &num);
	if (ret)
		return ret;

	set_gpu_override_max_state_val(num);
	return 0;
}

static const struct kernel_param_ops gpu_override_max_state_param_ops = {
	.set = param_set_gpu_override_max_state,
	.get = param_get_uint,
};
module_param_cb(gpu_override_max_state, &gpu_override_max_state_param_ops, &gpu_override_max_state, 0644);
MODULE_PARM_DESC(gpu_override_max_state, "Max cooling state floor for GPU when override active (default: 0 -> no cap / 840MHz)");

static void set_ignore_vendor_backlight_thermal_val(int val)
{
	WRITE_ONCE(ignore_vendor_backlight_thermal, !!val);
	pr_info("wt_chg: ignore_vendor_backlight_thermal set to %d\n", !!val);

	if (smp_load_acquire(&thermal_override_initialized)) {
		if (thermal_override_poll_needed())
			schedule_delayed_work(&thermal_override_poll_work, 0);

		vendor_thermal_reapply();
	}
}

static int param_set_ignore_vendor_backlight_thermal(const char *val, const struct kernel_param *kp)
{
	int ret, num;

	ret = kstrtoint(val, 10, &num);
	if (ret)
		return ret;

	set_ignore_vendor_backlight_thermal_val(num);
	return 0;
}

static const struct kernel_param_ops ignore_vendor_backlight_thermal_param_ops = {
	.set = param_set_ignore_vendor_backlight_thermal,
	.get = param_get_int,
};
module_param_cb(ignore_vendor_backlight_thermal, &ignore_vendor_backlight_thermal_param_ops, &ignore_vendor_backlight_thermal, 0644);
MODULE_PARM_DESC(ignore_vendor_backlight_thermal, "Ignore vendor thermal backlight dimming when safe (removes backlight caps; skin backstop disabled by default)");

static void set_skin_override_max_mc_val(int val)
{
	if (val > 0)
		val = clamp(val, 45000, 60000);
	else
		val = 0;

	WRITE_ONCE(skin_override_max_mc, val);
	pr_info("wt_chg: skin_override_max_mc set to %d\n", val);

	if (smp_load_acquire(&thermal_override_initialized)) {
		if (thermal_override_poll_needed())
			schedule_delayed_work(&thermal_override_poll_work, 0);

		vendor_thermal_reapply();
	}
}

static int param_set_skin_override_max_mc(const char *val, const struct kernel_param *kp)
{
	int ret, num;

	ret = kstrtoint(val, 10, &num);
	if (ret)
		return ret;

	set_skin_override_max_mc_val(num);
	return 0;
}

static const struct kernel_param_ops skin_override_max_mc_param_ops = {
	.set = param_set_skin_override_max_mc,
	.get = param_get_int,
};
module_param_cb(skin_override_max_mc, &skin_override_max_mc_param_ops, &skin_override_max_mc, 0644);
MODULE_PARM_DESC(skin_override_max_mc, "Chassis skin temp backstop in millidegC (0 = disabled by default; clamp 45000-60000 with 2000mC hysteresis)");

static void set_battery_trip_decic_val(unsigned int val)
{
	val = clamp_val(val, 400U, 470U);
	WRITE_ONCE(battery_trip_decic, val);
	if (READ_ONCE(battery_clear_decic) > val - 20)
		WRITE_ONCE(battery_clear_decic, val - 20);

	pr_info("wt_chg: battery_trip_decic set to %u (clear: %u)\n",
		val, READ_ONCE(battery_clear_decic));

	if (smp_load_acquire(&thermal_override_initialized)) {
		if (thermal_override_poll_needed())
			schedule_delayed_work(&thermal_override_poll_work, 0);

		vendor_thermal_reapply();
	}
}

static int param_set_battery_trip_decic(const char *val, const struct kernel_param *kp)
{
	int ret;
	unsigned int num;

	ret = kstrtouint(val, 10, &num);
	if (ret)
		return ret;

	set_battery_trip_decic_val(num);
	return 0;
}

static const struct kernel_param_ops battery_trip_decic_param_ops = {
	.set = param_set_battery_trip_decic,
	.get = param_get_uint,
};
module_param_cb(battery_trip_decic, &battery_trip_decic_param_ops, &battery_trip_decic, 0644);
MODULE_PARM_DESC(battery_trip_decic, "Battery throttle trip temp in decidegC (default: 450 = 45.0C; clamp 400-470)");

static void set_battery_clear_decic_val(unsigned int val)
{
	unsigned int trip = READ_ONCE(battery_trip_decic);
	unsigned int max_clear = (trip > 20) ? trip - 20 : 0;

	val = clamp_val(val, 300U, max_clear);
	WRITE_ONCE(battery_clear_decic, val);

	pr_info("wt_chg: battery_clear_decic set to %u (trip: %u)\n",
		val, trip);

	if (smp_load_acquire(&thermal_override_initialized)) {
		if (thermal_override_poll_needed())
			schedule_delayed_work(&thermal_override_poll_work, 0);

		vendor_thermal_reapply();
	}
}

static int param_set_battery_clear_decic(const char *val, const struct kernel_param *kp)
{
	int ret;
	unsigned int num;

	ret = kstrtouint(val, 10, &num);
	if (ret)
		return ret;

	set_battery_clear_decic_val(num);
	return 0;
}

static const struct kernel_param_ops battery_clear_decic_param_ops = {
	.set = param_set_battery_clear_decic,
	.get = param_get_uint,
};
module_param_cb(battery_clear_decic, &battery_clear_decic_param_ops, &battery_clear_decic, 0644);
MODULE_PARM_DESC(battery_clear_decic, "Battery throttle clear temp in decidegC (default: 420 = 42.0C; max trip-20)");

void vendor_thermal_register_cpu_reapply(void (*fn)(void))
{
	mutex_lock(&reapply_cb_lock);
	cpu_reapply_cb = fn;
	mutex_unlock(&reapply_cb_lock);
}
EXPORT_SYMBOL_GPL(vendor_thermal_register_cpu_reapply);

void vendor_thermal_unregister_cpu_reapply(void)
{
	mutex_lock(&reapply_cb_lock);
	cpu_reapply_cb = NULL;
	mutex_unlock(&reapply_cb_lock);
}
EXPORT_SYMBOL_GPL(vendor_thermal_unregister_cpu_reapply);

void vendor_thermal_register_gpu_reapply(void (*fn)(void))
{
	mutex_lock(&reapply_cb_lock);
	gpu_reapply_cb = fn;
	mutex_unlock(&reapply_cb_lock);
}
EXPORT_SYMBOL_GPL(vendor_thermal_register_gpu_reapply);

void vendor_thermal_unregister_gpu_reapply(void)
{
	mutex_lock(&reapply_cb_lock);
	gpu_reapply_cb = NULL;
	mutex_unlock(&reapply_cb_lock);
}
EXPORT_SYMBOL_GPL(vendor_thermal_unregister_gpu_reapply);

void vendor_thermal_register_backlight_reapply(void (*fn)(void))
{
	mutex_lock(&reapply_cb_lock);
	backlight_reapply_cb = fn;
	mutex_unlock(&reapply_cb_lock);
}
EXPORT_SYMBOL_GPL(vendor_thermal_register_backlight_reapply);

void vendor_thermal_unregister_backlight_reapply(void)
{
	mutex_lock(&reapply_cb_lock);
	backlight_reapply_cb = NULL;
	mutex_unlock(&reapply_cb_lock);
}
EXPORT_SYMBOL_GPL(vendor_thermal_unregister_backlight_reapply);

static void thermal_override_poll_work_fn(struct work_struct *work)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);
	int bms_temp = 0, skin_temp = 0;
	int skin_max, ret;
	bool bms_read_ok = false, skin_read_ok = false;
	bool prev_safe, new_safe;
	bool state_changed = false;
	unsigned long flags;
	unsigned int trip, clear;

	if (!smp_load_acquire(&thermal_override_initialized))
		return;

	if (!chg || READ_ONCE(chg->shutdown_flag))
		return;

	/* 1. Poll battery temperature from BMS */
	ret = batt_get_iio_channel(chg, BMS, BATT_QG_TEMP, &bms_temp);
	if (ret >= 0)
		bms_read_ok = true;

	/* 2. Poll skin temperature if skin backstop enabled */
	skin_max = READ_ONCE(skin_override_max_mc);
	if (skin_max > 0) {
		if (!skin_tz || IS_ERR(skin_tz))
			skin_tz = thermal_zone_get_zone_by_name("sdm-skin-therm-usr");

		if (!IS_ERR_OR_NULL(skin_tz)) {
			ret = thermal_zone_get_temp(skin_tz, &skin_temp);
			if (ret >= 0)
				skin_read_ok = true;
		}
	}

	/* 3. Evaluate guard state under spinlock */
	spin_lock_irqsave(&guard_lock, flags);
	prev_safe = cached_override_safe;

	trip = READ_ONCE(battery_trip_decic);
	clear = READ_ONCE(battery_clear_decic);

	if (bms_read_ok) {
		/* Battery hysteresis: trip >= trip, release < clear */
		if (!chg->thermal_throttle_active) {
			if (bms_temp >= (int)trip)
				chg->thermal_throttle_active = true;
		} else {
			if (bms_temp < (int)clear)
				chg->thermal_throttle_active = false;
		}

		/* JEITA hot check */
		if (bms_temp >= chg->dt.jeita_temp_step5 || chg->high_temp_flag)
			guard_battery_enforced = true;
		else
			guard_battery_enforced = chg->thermal_throttle_active;
	} else {
		/* Fail-safe on BMS read error */
		chg->thermal_throttle_active = true;
		guard_battery_enforced = true;
	}

	if (skin_max > 0) {
		int thresh = clamp(skin_max, 45000, 60000);
		int release = thresh - 2000;

		if (skin_read_ok) {
			if (!guard_skin_enforced) {
				if (skin_temp >= thresh)
					guard_skin_enforced = true;
			} else {
				if (skin_temp < release)
					guard_skin_enforced = false;
			}
		} else {
			/* Fail-safe on skin read error if enabled */
			guard_skin_enforced = true;
		}
	} else {
		guard_skin_enforced = false;
	}

	new_safe = bms_read_ok && !guard_battery_enforced && !guard_skin_enforced;
	if (new_safe != prev_safe) {
		cached_override_safe = new_safe;
		state_changed = true;
	}
	spin_unlock_irqrestore(&guard_lock, flags);

	if (state_changed)
		vendor_thermal_reapply();

	if (smp_load_acquire(&thermal_override_initialized) && thermal_override_poll_needed())
		schedule_delayed_work(&thermal_override_poll_work, msecs_to_jiffies(2000));
}

static bool thermal_throttle_enforced(struct batt_chg *chg)
{
	int bms_temp = 0;
	int rc = 0;
	unsigned long flags;
	bool enforced;
	bool prev_safe, new_safe;
	bool state_changed = false;
	unsigned int trip, clear;

	if (!chg || !smp_load_acquire(&thermal_override_initialized))
		return true;

	/* If disabled at runtime, behave 100% like stock (enforce throttle) */
	if (!READ_ONCE(ignore_vendor_charging_thermal))
		return true;

	/* Read true battery temperature from BMS fuel gauge in decidegC */
	rc = batt_get_iio_channel(chg, BMS, BATT_QG_TEMP, &bms_temp);

	spin_lock_irqsave(&guard_lock, flags);
	prev_safe = cached_override_safe;

	trip = READ_ONCE(battery_trip_decic);
	clear = READ_ONCE(battery_clear_decic);

	if (rc < 0) {
		/* Fail-safe: if BMS temperature read fails, immediately enforce thermal limit */
		chg->thermal_throttle_active = true;
		guard_battery_enforced = true;
		enforced = true;
	} else {
		/*
		 * Temperature hysteresis guardrail:
		 * Start enforcing thermal request at >= trip (default 450 = 45.0C).
		 * Resume overriding thermal request only when cooled below clear (default 420 = 42.0C).
		 */
		if (!chg->thermal_throttle_active) {
			if (bms_temp >= (int)trip)
				chg->thermal_throttle_active = true;
		} else {
			if (bms_temp < (int)clear)
				chg->thermal_throttle_active = false;
		}

		/* Also enforce thermal request if JEITA hot state is active (>= 48.0C or high_temp_flag) */
		if (bms_temp >= chg->dt.jeita_temp_step5 || chg->high_temp_flag)
			guard_battery_enforced = true;
		else
			guard_battery_enforced = chg->thermal_throttle_active;

		enforced = guard_battery_enforced;
	}

	new_safe = (rc >= 0) && !guard_battery_enforced && !guard_skin_enforced;
	if (new_safe != prev_safe) {
		cached_override_safe = new_safe;
		state_changed = true;
	}
	spin_unlock_irqrestore(&guard_lock, flags);

	if (state_changed)
		vendor_thermal_reapply();

	return enforced;
}

bool vendor_thermal_override_safe(void)
{
	unsigned long flags;
	bool safe;

	if (!smp_load_acquire(&thermal_override_initialized))
		return false;

	spin_lock_irqsave(&guard_lock, flags);
	safe = cached_override_safe;
	spin_unlock_irqrestore(&guard_lock, flags);

	return safe;
}
EXPORT_SYMBOL_GPL(vendor_thermal_override_safe);

bool vendor_thermal_cpu_override_active(unsigned long *max_state, struct cpufreq_policy *policy)
{
	bool is_big = false;

	if (!READ_ONCE(ignore_vendor_cpu_thermal))
		return false;

	if (!vendor_thermal_override_safe())
		return false;

	if (policy && policy->cpu >= 6)
		is_big = true;

	if (max_state) {
		if (is_big)
			*max_state = READ_ONCE(cpu_big_override_max_state);
		else
			*max_state = READ_ONCE(cpu_little_override_max_state);
	}

	return true;
}
EXPORT_SYMBOL_GPL(vendor_thermal_cpu_override_active);

bool vendor_thermal_gpu_override_active(unsigned long *max_state)
{
	if (!READ_ONCE(ignore_vendor_gpu_thermal))
		return false;

	if (!vendor_thermal_override_safe())
		return false;

	if (max_state)
		*max_state = READ_ONCE(gpu_override_max_state);

	return true;
}
EXPORT_SYMBOL_GPL(vendor_thermal_gpu_override_active);

bool vendor_thermal_backlight_override_active(void)
{
	if (!READ_ONCE(ignore_vendor_backlight_thermal))
		return false;

	return vendor_thermal_override_safe();
}
EXPORT_SYMBOL_GPL(vendor_thermal_backlight_override_active);

static int sw_battery_set_cv(struct batt_chg *chg)
{
	int cv = 0;
	int charger_term = 0;
	int temp, batt_id;
	int cur_now, cur = 0;
	int charge_full;
	static int cnt = 0;

	if (!chg)
		return -EINVAL;

	batt_get_iio_channel(chg, BMS, BATT_QG_BATTERY_ID, &batt_id);
	batt_get_iio_channel(chg, BMS, BATT_QG_TEMP, &temp);
	batt_get_iio_channel(chg, BMS, BATT_QG_CURRENT_NOW, &cur_now);
	batt_get_iio_channel(chg, MAIN, MAIN_CHARGER_DONE, &chg->charge_done);
	batt_get_iio_channel(chg, BMS, BATT_QG_STATUS, &charge_full);
	if(charge_full != POWER_SUPPLY_STATUS_FULL)
		charge_full = 0;
	if (cur_now < 0)
		cur = 0 - (cur_now / 1000);
	chg->batt_id = batt_id;
	if (chg->batt_auth && chg->pd_auth &&
	    (!thermal_throttle_enforced(chg) || chg->system_temp_level != 24)) {
		if (temp >= 150 && temp < 350 && batt_id) {
			charger_term = 896;
			if(cur < charger_term - 40)
				cnt++;
			else
				cnt = 0;
			if (chg->is_pps_on) {
				cv = 4608;
				chg->cv_flag = 0;
			} else {
				if(chg->cell_voltage >= 4475000) {
					if (cur > charger_term){
						chg->batt_done_curr = cur - 64;
						batt_set_iio_channel(chg, MAIN, MAIN_CHAGER_CURRENT, chg->batt_done_curr);
						pr_err("cur %d\n", chg->batt_done_curr);
					} else {
						chg->cv_flag = 1;
					}
				}
				if(cnt > 3 && cur)
					cv = 4440;
				else if (chg->cv_flag)
					cv = 4480;
				else
					cv = 4496;
			}
		} else if (temp >= 350 && temp < 480 && batt_id) {
			if (batt_id == 1)
				charger_term = 960;
			else if(batt_id == 2 || batt_id == 4)
				charger_term = 980 + 30;
			if (cur < charger_term)
				cnt++;
			else
				cnt = 0;
			if (chg->is_pps_on) {
				cv = 4608;
				chg->cv_flag = 0;
			} else {
				if(chg->cell_voltage >= 4475000){
					if (cur > charger_term){
						chg->batt_done_curr = cur - 64;
						batt_set_iio_channel(chg, MAIN, MAIN_CHAGER_CURRENT, chg->batt_done_curr);
						pr_err("cur %d\n", chg->batt_done_curr);
					} else {
						chg->cv_flag = 1;
					}
				}
				if(cnt > 3 && cur)
					cv = 4440;
				else if(chg->cv_flag)
					cv = 4480;
				else
					cv = 4496;
			}
		} else if(temp >= 480) {
			charger_term = 256;
			if(chg->high_temp_flag)
				cv = 4450;
			else
				cv = 4100;
		} else if(temp >= 50 && temp < 150) {
			charger_term = 256;
			if (chg->is_pps_on)
				cv = 4608;
			else
				cv = 4450;
		} else {
			charger_term = 256;
			cv = 4450;
		}
	} else {
		charger_term = 256;
		if (chg->is_pps_on) {
			cv = 4608;
		} else if (temp >= 480 && !chg->high_temp_flag) {
			cv = 4100;
		} else {
			cv = 4450;
		}

	}

	pr_err("cur_now %d, chg->batt_auth %d, chg->pd_auth %d, batt_id %d temp %d, cnt %d,charger_term %d, cv %d,cell_voltage %d,charge_done %d\n",
		cur, chg->batt_auth, chg->pd_auth, batt_id, temp, cnt, charger_term, cv, chg->cell_voltage, chg->charge_done);

	chg->battery_cv = cv;
//	if(chg->ui_soc != 100) {
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_VOLTAGE_TERM, cv);
		batt_set_iio_channel(chg, MAIN, MAIN_CHAGER_TERM, charger_term);
//	}

	return 0;
}

static int sw_battery_recharge(struct batt_chg *chg)
{
	int r_soc;
	int en = 0;

	if (READ_ONCE(chg->bypass_mode) || READ_ONCE(chg->limit_hold))
		return 0;

	batt_get_iio_channel(chg, BMS, BATT_QG_CC_SOC, &r_soc);
	if(r_soc <= 9750 && chg->charge_done && r_soc > 9000){
		en = 0;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, en);
		pr_err("r_soc %d,en %d\n", r_soc, en);
		msleep(100);
		en = 1;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, en);
	}
	pr_err("r_soc %d,en %d\n", r_soc, en);
	return 0;
}

#define CHG_BAT_TEMP_0	340
#define CHG_BAT_TEMP_1	370
#define CHG_BAT_TEMP_2	380
#define CHG_BAT_TEMP_3	390
#define CHG_BAT_TEMP_4	400
#define CHG_BAT_TEMP_5	410
#define CHG_BAT_TEMP_6	420
#define CHG_BAT_TEMP_7	430
#define CHG_BAT_TEMP_8	440
#define CHG_BAT_TEMP_9	450
#define CHG_BAT_TEMP_10 460
#define CHG_BAT_TEMP_11 470

int power_off_charge_them(struct batt_chg *chg, int temp)
{
	int cur;

	if (temp < CHG_BAT_TEMP_1) {
		if (chg->therm_step == 0) {
			chg->therm_step = 0;
			cur = 12200;
		} else {
			if (temp < CHG_BAT_TEMP_0) {
				cur = 12200;
				chg->therm_step = 0;
			} else {
				cur = 11000;
				chg->therm_step = 1;
			}
		}
	} else if (temp >= CHG_BAT_TEMP_1 && temp < CHG_BAT_TEMP_2) {
		if (chg->therm_step <= 1) {
			cur = 11000;
			chg->therm_step = 1;
		} else if (chg->therm_step == 2) {
			if(temp < CHG_BAT_TEMP_1) {
				cur = 11000;
				chg->therm_step = 1;
			} else {
				cur = 10000;
				chg->therm_step = 2;
			}
		} else {
			cur = 10000;
			chg->therm_step = 2;
		}
	} else if (temp >= CHG_BAT_TEMP_2 && temp < CHG_BAT_TEMP_3) {
		if (chg->therm_step <= 2) {
			cur = 10000;
			chg->therm_step = 2;
		} else if(chg->therm_step == 3) {
			if(temp < CHG_BAT_TEMP_2) {
				cur = 10000;
				chg->therm_step = 2;
			} else {
				cur = 9000;
				chg->therm_step = 3;
			}
		} else {
			cur = 9000;
			chg->therm_step = 3;
		}
	} else if (temp >= CHG_BAT_TEMP_3 && temp < CHG_BAT_TEMP_4) {
		if (chg->therm_step <= 3) {
			cur = 9000;
			chg->therm_step = 3;
		} else if(chg->therm_step == 4) {
			if(temp < CHG_BAT_TEMP_3) {
				cur = 9000;
				chg->therm_step = 3;
			} else {
				cur = 8000;
				chg->therm_step = 4;
			}
		} else {
			cur = 8000;
			chg->therm_step = 4;
		}
	} else if (temp >= CHG_BAT_TEMP_4 && temp < CHG_BAT_TEMP_5) {
		if (chg->therm_step <= 4) {
			cur = 8000;
			chg->therm_step = 4;
		} else if(chg->therm_step == 5) {
			if(temp < CHG_BAT_TEMP_4) {
				cur = 8000;
				chg->therm_step = 4;
			} else {
				cur = 7000;
				chg->therm_step = 5;
			}
		} else {
			cur = 7000;
			chg->therm_step = 5;
		}
	} else if (temp >= CHG_BAT_TEMP_5 && temp < CHG_BAT_TEMP_6) {
		if (chg->therm_step <= 5) {
			cur = 7000;
			chg->therm_step = 5;
		} else if (chg->therm_step == 6) {
			if(temp < CHG_BAT_TEMP_5) {
				cur = 7000;
				chg->therm_step = 5;
			} else {
				cur = 7000;
				chg->therm_step = 6;
			}
		} else {
			cur = 6000;
			chg->therm_step = 6;
		}
	} else if (temp >= CHG_BAT_TEMP_6 && temp < CHG_BAT_TEMP_7) {
		if (chg->therm_step <= 6) {
			cur = 6000;
			chg->therm_step = 6;
		} else if (chg->therm_step == 7) {
			if(temp < CHG_BAT_TEMP_6) {
				cur = 6000;
				chg->therm_step = 6;
			} else {
				cur = 5000;
				chg->therm_step = 7;
			}
		} else {
			cur = 5000;
			chg->therm_step = 7;
		}
	} else if (temp >= CHG_BAT_TEMP_7 && temp < CHG_BAT_TEMP_8) {
		if (chg->therm_step <= 7) {
			cur = 5000;
			chg->therm_step = 7;
		} else if (chg->therm_step == 8) {
			if (temp < CHG_BAT_TEMP_7) {
				cur = 5000;
				chg->therm_step = 7;
			} else {
				cur = 4000;
				chg->therm_step = 8;
			}
		} else {
			cur = 4000;
			chg->therm_step = 8;
		}
	} else if (temp >= CHG_BAT_TEMP_8 && temp < CHG_BAT_TEMP_9) {
		if (chg->therm_step <= 8) {
			cur = 4000;
			chg->therm_step = 8;
		} else if (chg->therm_step == 9) {
			if (temp < CHG_BAT_TEMP_8) {
				cur = 4000;
				chg->therm_step = 8;
			} else {
				cur = 3000;
				chg->therm_step = 9;
			}
		} else {
			cur = 3000;
			chg->therm_step = 9;
		}
	} else if (temp >= CHG_BAT_TEMP_9 && temp < CHG_BAT_TEMP_11) {
		if (chg->therm_step <= 9) {
			cur = 3000;
			chg->therm_step = 9;
		} else if (chg->therm_step == 10) {
			if(temp < CHG_BAT_TEMP_10) {
				cur = 3000;
				chg->therm_step = 9;
			} else {
				cur = 2000;
				chg->therm_step = 10;
			}
		} else {
			cur = 2000;
			chg->therm_step = 10;
		}
	} else if (temp >= CHG_BAT_TEMP_11 && temp < 480) {
		cur = 2000;
		chg->therm_step = 10;
	} else {
		cur = 2000;
	}
	return cur;
}

int set_jeita_lcd_on_off(bool lcdon)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);

	if (!chg)
		return -ENODEV;

	chg->lcd_on = lcdon;
	pr_err("%s, lcd_on %d\n", __func__, lcdon);
	return 0;
}
EXPORT_SYMBOL(set_jeita_lcd_on_off);

int get_jeita_lcd_on_off(void)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);

	return chg ? chg->lcd_on : 0;
}

static int sw_battery_jeita(struct batt_chg *chg)
{
	int en = 0;
	int cur = 0, them_curr;
	int temp;
	int type = 0;
	int type_cur = 0;
	int input_cur = 0;

	if(!chg)
		return -EINVAL;

	batt_get_iio_channel(chg, BMS, BATT_QG_TEMP, &temp);
	if(temp < chg->dt.jeita_temp_step0){
		en = 0;
	} else if(temp >= chg->dt.jeita_temp_step0 && temp < chg->dt.jeita_temp_step1) {
		en = 1;
		if(chg->batt_voltage_now <= 4200000 && chg->low_temp_flag != 1)
			cur = 1000;
		else {
			cur = 710;
			chg->low_temp_flag = 1;
		}
	} else if(temp >= chg->dt.jeita_temp_step1 && temp < chg->dt.jeita_temp_step2) {
		en = 1;
		cur = 2450;
	} else if(temp >= chg->dt.jeita_temp_step2 && temp < chg->dt.jeita_temp_step3) {
		en = 1;
		cur = 3500;
	} else if(temp >= chg->dt.jeita_temp_step3 && temp < chg->dt.jeita_temp_step4) {
		en = 1;
		cur = 5700;
	} else if(temp >= chg->dt.jeita_temp_step4 && temp < chg->dt.jeita_temp_step5) {
		en = 1;
		cur = 5700;
	} else if(temp >= chg->dt.jeita_temp_step5 && temp < chg->dt.jeita_temp_step6) {
		en = 1;
		cur = 2450;
	} else if(temp >= chg->dt.jeita_temp_step6){
		en = 0;
	}

	batt_get_iio_channel(chg, BMS, BATT_QG_VOLTAGE_NOW, &chg->cell_voltage);

	if (chg->batt_auth && chg->pd_auth) {
		if (temp >= chg->dt.jeita_temp_step4 && temp < chg->dt.jeita_temp_step5)
			cur = 12200;
		if (get_boot_mode()) {
			them_curr = power_off_charge_them(chg, temp);
			cur = 12050;
		} else {
			int lvl;
			if (thermal_throttle_enforced(chg)) {
				lvl = chg->system_temp_level;
			} else {
				int max_lvl = clamp(READ_ONCE(vendor_thermal_max_level), 0, chg->thermal_levels);
				lvl = min(chg->system_temp_level, max_lvl);
			}
			them_curr = thermal_mitigation[lvl] / 1000;
		}
		cur = min(cur, them_curr);
		batt_set_iio_channel(chg, BMS, BATT_QG_FCC_MAX, cur);
		if (chg->is_pps_on && !chg->fastcharge_mode && chg->pd_auth) {
			batt_set_iio_channel(chg, BMS, BATT_QG_FASTCHARGE_MODE, true);
			chg->fastcharge_mode = 1;
		}
	} else {
		if (get_boot_mode()) {
			them_curr = power_off_charge_them(chg, temp);
		} else {
			int lvl;
			if (thermal_throttle_enforced(chg)) {
				lvl = chg->system_temp_level;
			} else {
				int max_lvl = clamp(READ_ONCE(vendor_thermal_max_level), 0, chg->thermal_levels);
				lvl = min(chg->system_temp_level, max_lvl);
			}
			them_curr = thermal_mitigation[lvl] / 1000;
		}
		cur = min(cur, them_curr);
		batt_set_iio_channel(chg, BMS, BATT_QG_FCC_MAX, cur);
	}

	batt_get_iio_channel(chg, MAIN, MAIN_CHARGER_TYPE, &type);
	if (chg->real_type == POWER_SUPPLY_TYPE_USB_PD) {
		if(type == POWER_SUPPLY_TYPE_USB) {
			input_cur = 1500;
			type_cur = 1500;
		} else if (type == POWER_SUPPLY_TYPE_USB_CDP){
			if(chg->ui_soc == 100)
				input_cur = 1200;
			else
				input_cur = 1500;
			type_cur = 1500;
		} else if (type == POWER_SUPPLY_TYPE_USB_FLOAT){
			input_cur = 1500;
			type_cur = 3010;
		} else if(type == POWER_SUPPLY_TYPE_USB_DCP){
			input_cur = 2000;
			if(get_boot_mode())
				type_cur = 3000;
			else
				type_cur = 3010;
		} else {
			input_cur = 2000;
			if(get_boot_mode())
				type_cur = 3000;
			else
				type_cur = 3010;
		}
		if (chg->pd_auth){
			input_cur = 3000;
		}
	} else {
		if(type == POWER_SUPPLY_TYPE_USB) {
			if(get_boot_mode())
				input_cur = 500;
			else
				input_cur = 500;
			type_cur = 512;
			if (chg->real_type == POWER_SUPPLY_TYPE_USB) {
				int override_ma = READ_ONCE(chg->usb_icl_override_ma);
				if (override_ma > 0) {
					input_cur = max(input_cur, override_ma);
					type_cur = max(type_cur, override_ma);
				}
			}
		} else if (type == POWER_SUPPLY_TYPE_USB_CDP) {
			if(chg->ui_soc == 100)
				input_cur = 1200;
			else
				input_cur = 1500;
			type_cur = 1500;
		} else if (type == POWER_SUPPLY_TYPE_USB_DCP) {
			type_cur = 2000;
			if(get_boot_mode()) {
				input_cur = 1950;
			} else
				input_cur = 2000;
		} else if (type == POWER_SUPPLY_TYPE_USB_HVDCP) {
			if(get_boot_mode())
				type_cur = 3000;
			else
				type_cur = 3010;
			if((chg->vbus_voltage > 5600 && chg->vbus_voltage < 8600) || chg->input_limit_flag){
				input_cur = 1200;
				chg->input_limit_flag = 1;
			} else {
				input_cur = 2000;
			}
			if(get_boot_mode())
				input_cur = 2000;	
		} else if (type == POWER_SUPPLY_TYPE_USB_FLOAT) {
			type_cur = 1000;
			input_cur = 1000;
		} else {
			type_cur = 500;
			input_cur = 500;
		}
	}

	cur = min(cur, type_cur);

	if(chg->mtbf_current > 500 && chg->mtbf_current <= 4000)
		batt_set_iio_channel(chg, MAIN, MAIN_INPUT_CURRENT_SETTLED, chg->mtbf_current);

	if (chg->charging_call_state)
		cur = min(cur, 1000);
	if (chg->batt_auth && chg->pd_auth && chg->ui_soc >= 98 && chg->batt_done_curr)
		cur = min(chg->batt_done_curr, cur);
	if(!chg->batt_id)
		cur = min(1999, cur);
	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_CHARGING_ENABLED, &chg->is_pps_on);
	if(chg->is_pps_on) {
		if(chg->pd_auth)
			chg->charge_voltage_max = 4480000;
		else
			chg->charge_voltage_max = 4450000;
		chg->batt_done_curr = 0;
		if(chg->ui_soc <= 50)
			input_cur = 100;
		else
			input_cur = 1500;
		batt_set_iio_channel(chg, MAIN, MAIN_CHAGER_CURRENT, 100);
	} else {
		chg->charge_voltage_max = 4450000;
		if(chg->battery_temp > 480 && chg->cell_voltage > 4110000) {
			en = 0;
			chg->high_temp_flag = 1;
		} else {
			chg->high_temp_flag = 0;
		}
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, en);
		batt_set_iio_channel(chg, MAIN, MAIN_CHAGER_CURRENT, cur);
	}
	batt_set_iio_channel(chg, MAIN, MAIN_INPUT_CURRENT_SETTLED, input_cur);
	if (READ_ONCE(chg->bypass_mode) || READ_ONCE(chg->limit_hold))
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, 0);
	pr_err("temp = %d,input_cur %d curr=%d, en %d, is_pps %d,mtbf_current %d, type %d cv_flag %d , input_limit_flag %d\n",
		temp, input_cur, cur, en, chg->is_pps_on, chg->mtbf_current, type, chg->cv_flag, chg->input_limit_flag);

	return 0;
}

static int get_real_type(struct batt_chg *chg)
{
	int type, real_type;
	if(!chg)
		return -EINVAL;

	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BUS_VOLTAGE, &chg->vbus_voltage);
	batt_get_iio_channel(chg, MAIN, MAIN_CHARGER_TYPE, &type);
	real_type = type;

	if (chg->pd_active == QTI_POWER_SUPPLY_PD_ACTIVE
		|| chg->pd_active == QTI_POWER_SUPPLY_PD_PPS_ACTIVE) {
		if(type || chg->vbus_voltage > 8500)
			real_type = POWER_SUPPLY_TYPE_USB_PD;
		else
			real_type = POWER_SUPPLY_TYPE_USB;
	}

	if(chg->old_charge_type == POWER_SUPPLY_TYPE_USB_FLOAT && real_type == POWER_SUPPLY_TYPE_UNKNOWN && !chg->usb_float) {
		chg->usb_float = 1;
		cancel_delayed_work(&chg->usb_type_work);
		schedule_delayed_work(&chg->usb_type_work, msecs_to_jiffies(1500));
	}

	if(type)
		chg->pd_type_cnt = 0;

	if(chg->otg_enable || chg->mishow_flag)
		real_type = 0;

	if (chg->vbus_cnt > 3) {
		chg->vbus_cnt = 0;
		chg->pd_active = 0;
	}

	if(chg->pd_type_cnt > 50 && type == POWER_SUPPLY_TYPE_UNKNOWN){
			real_type = POWER_SUPPLY_TYPE_USB;
	}

	if(!real_type) {
		chg->pd_auth = 0;
		chg->therm_step = 0;
		chg->batt_done_curr = 0;
		chg->apdo_max_curr = 0;
		if (chg->fastcharge_mode) {
			batt_set_iio_channel(chg, BMS, BATT_QG_FASTCHARGE_MODE, false);
			chg->fastcharge_mode = 0;
		}
	}

	if(chg->real_type == POWER_SUPPLY_TYPE_UNKNOWN) {
		chg->charge_done = 0;
		chg->input_limit_flag = 0;
		if(chg->battery_cv < 4450) {
			chg->battery_cv = 4450;
			batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_VOLTAGE_TERM, chg->battery_cv);
		}
	}

	if(chg->usb_float){
		real_type = POWER_SUPPLY_TYPE_USB_FLOAT;
	}

	chg->real_type = real_type;
	if(chg->old_charge_type != real_type && !READ_ONCE(chg->shutdown_flag)) {
		chg->update_cont = 15;
		chg->pd_type_cnt = 0;
		power_supply_changed(chg->batt_psy);
		cancel_delayed_work(&chg->xm_prop_change_work);
		schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(10));
		cancel_delayed_work(&chg->batt_chg_work);
		schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(1000));
	}
	chg->old_charge_type = real_type;
	return real_type;
}

static int get_usb_real_type(struct batt_chg *chg)
{
	int type, real_type;
	if(!chg)
		return -EINVAL;

	batt_get_iio_channel(chg, MAIN, MAIN_CHARGER_TYPE, &type);
	real_type = type;

	if(type)
		chg->pd_type_cnt = 0;

	if(chg->usb_float){
		real_type = POWER_SUPPLY_TYPE_USB_FLOAT;
	}

	if (chg->pd_active == QTI_POWER_SUPPLY_PD_ACTIVE
		|| chg->pd_active == QTI_POWER_SUPPLY_PD_PPS_ACTIVE) {
		if(type)
			real_type = POWER_SUPPLY_TYPE_USB_PD;
		else
			real_type = POWER_SUPPLY_TYPE_USB_PD;
	}
	if(chg->otg_enable || chg->mishow_flag || (chg->vbus_cnt > 3))
		real_type = 0;
	if(!real_type) {
		chg->pd_auth = 0;
		chg->therm_step = 0;
		chg->batt_done_curr = 0;
		chg->charge_done = 0;
	}

	if(chg->pd_type_cnt > 50 && type == POWER_SUPPLY_TYPE_UNKNOWN){
			real_type = POWER_SUPPLY_TYPE_USB_FLOAT;
	}

	if(chg->real_type == POWER_SUPPLY_TYPE_UNKNOWN)
		chg->charge_done = 0;

	chg->real_type = real_type;

	return real_type;
}

struct quick_charge adapter_cap[10] = {
	{ POWER_SUPPLY_TYPE_USB,        QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_DCP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_CDP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_ACA,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_FLOAT,  QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_PD,       QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP,    QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP_3,  QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_WIRELESS,     QUICK_CHARGE_FAST },
	{0, 0},
};

int get_quick_charge_type(struct batt_chg *chg)
{
	int i = 0;

	if (!chg)
		return 0;

	chg->real_type = get_real_type(chg);
	if(chg->real_type == POWER_SUPPLY_TYPE_USB_PD){
		if(((chg->apdo_max_volt * chg->apdo_max_curr)/1000000 > 20) && ((chg->apdo_max_volt * chg->apdo_max_curr)/1000000 < 27))
			return QUICK_CHARGE_FLASH;
		else if(((chg->apdo_max_volt * chg->apdo_max_curr)/1000000 >= 27) && ((chg->apdo_max_volt * chg->apdo_max_curr)/1000000 <= 50))
			return QUICK_CHARGE_TURBE;
		else if((chg->apdo_max_volt * chg->apdo_max_curr)/1000000 > 50)
			return QUICK_CHARGE_TURBE;
		else
			return QUICK_CHARGE_FAST;
		//	return QUICK_CHARGE_SUPER;
	} else {
		while (adapter_cap[i].adap_type != 0) {
			if (chg->real_type == adapter_cap[i].adap_type) {
				pr_err("get_quick_charge_type is i=%d\n", i);
				return adapter_cap[i].adap_cap;
			}
			i++;
		}
	}
	pr_err("get_quick_charge_type is not supported in usb\n");

	return 0;
}

int get_apdo_max(struct batt_chg *chg)
{
	if (!chg)
		return 0;

	chg->real_type = get_real_type(chg);
	if(chg->real_type == POWER_SUPPLY_TYPE_USB_PD){
		return ((chg->apdo_max_volt * chg->apdo_max_curr)/1000000);
	}
	pr_err("get_apdo_max is not supported in usb\n");

	return 0;
}

static int batt_get_prop_batt_charge_type(struct batt_chg *chip,
		union power_supply_propval *val)
{
	int  rc = -EINVAL;
	if (!chip)
		return -EINVAL;

	if(get_real_type(chip)){
		if(chip->batt_voltage_now <= BATTERY_VOLTAGET_PRE)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		else if(chip->batt_voltage_now >= chip->dt.batt_profile_fv_uv - BATTERY_VOLTAGET_OFFSET) {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		}  else {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE;
		}
	} else {
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	return rc;
}

int batt_set_prop_system_temp_level(struct batt_chg *chg,
				const union power_supply_propval *val)
{
	int cur;
	if (val->intval < 0)
		return -EINVAL;

	if (chg->thermal_levels <= 0)
		return -EINVAL;

	if (val->intval > chg->thermal_levels)
		return -EINVAL;

	chg->system_temp_level = val->intval;
	cur = thermal_mitigation[val->intval] / 1000;
	sw_battery_jeita(chg);
	pr_err("batt_set_prop_system_temp_levels %d, cur = %d\n", chg->system_temp_level, cur);
	return 0;
}

static enum power_supply_property batt_psy_props[] = {
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
};

static int batt_psy_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	struct batt_chg *chg = power_supply_get_drvdata(psy);
	int rc = 0;
	int current_cur = 0;
	int voltage_now = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_STATUS, &pval->intval);
		rc = get_real_type(chg);
		if (pval->intval == POWER_SUPPLY_STATUS_DISCHARGING && rc > 0)
			pval->intval = POWER_SUPPLY_STATUS_CHARGING;
		if(pval->intval == POWER_SUPPLY_STATUS_FULL) {
			if(chg->real_type)
				pval->intval = POWER_SUPPLY_STATUS_CHARGING;
			else
				pval->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		if (chg->charge_done && chg->ui_soc == 100)
			pval->intval = POWER_SUPPLY_STATUS_FULL;
		if(chg->usb_temp > 680 && pval->intval == POWER_SUPPLY_STATUS_CHARGING)
			pval->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		if (READ_ONCE(chg->bypass_mode) || READ_ONCE(chg->limit_hold))
			pval->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		rc = get_prop_batt_health(chg, pval);
		if(rc != 0)
			pval->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_PRESENT, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		pval->intval = chg->input_batt_current_max;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		rc = batt_get_prop_batt_charge_type(chg, pval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CAPACITY, &pval->intval);
		if(pval->intval == -107)
			chg->is_battery_on = false;
		else
			chg->is_battery_on = true;
		if (!chg->is_battery_on) {
			pr_err("no battery,batt capacity is 15\n");
			pval->intval = 15;
			rc = batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BATTERY_VOLTAGE, &voltage_now);
			if(voltage_now < 3500)
				pval->intval = 1;
		}
//		pr_err("read batt capacity is %d\n", pval->intval);
		chg->ui_soc = pval->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		pval->intval = chg->system_temp_level;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		pval->intval = chg->thermal_levels;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if(chg->real_type && chg->battery_temp > 0 && chg->battery_temp < 150) {
			rc = batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BATTERY_VOLTAGE, &voltage_now);
			pval->intval = (voltage_now + 5) * 1000;
		} if (chg->real_type && chg->battery_temp >= 480) {
			rc = batt_get_iio_channel(chg, MAIN, MAIN_VBAT_VOLTAGE, &voltage_now);
			pval->intval = (voltage_now + 5) * 1000;
		} else {
			batt_get_iio_channel(chg, BMS, BATT_QG_VOLTAGE_NOW, &pval->intval);
		}
		if (!chg->is_battery_on)
			pval->intval = 3800000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pval->intval = chg->charge_voltage_max;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		pval->intval = 0;
		if (!chg->is_battery_on)
			break;
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CURRENT_NOW, &current_cur);
		pval->intval = -current_cur;
		if(chg->ui_soc == 100 && current_cur < 0 && current_cur > -10000 && chg->pd_auth) {
			pval->intval = 0;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		pval->intval = chg->batt_current_max;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		pval->intval = chg->batt_current_now ;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		pval->intval = chg->batt_iterm ;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_TEMP, &pval->intval);
		if (!chg->is_battery_on)
			pval->intval = 250;
#ifdef CONFIG_DISABLE_TEMP_PROTECT
		pval->intval = 250;
#endif
		if(get_boot_mode() && pval->intval >= 590)
			pval->intval = 590;
		chg->battery_temp = pval->intval;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		pval->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CHARGE_FULL, &current_cur);
		pval->intval = (current_cur * chg->ui_soc) / 100;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CYCLE_COUNT, &pval->intval);
		chg->cycle_count = pval->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CHARGE_FULL, &pval->intval);
		if(chg->battery_temp >=10 && chg->battery_temp <= 480 && pval->intval < 4500000 && chg->cycle_count < 100)
			pval->intval += 100000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CHARGE_FULL_DESIGN, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_TIME_TO_FULL_NOW, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_TIME_TO_EMPTY_NOW, &pval->intval);
		break;
	default:
		pr_err("batt power supply prop %d not supported\n", psp);
		return -EINVAL;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int batt_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int rc = 0;
	struct batt_chg *chg = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == POWER_SUPPLY_STATUS_FULL)
			chg->fake_batt_status = val->intval;
		else
			chg->fake_batt_status = -EINVAL;
		power_supply_changed(chg->batt_psy);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		chg->input_batt_current_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		rc = batt_set_prop_system_temp_level(chg, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		batt_set_iio_channel(chg, BMS, BATT_QG_CAPACITY, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		chg->charge_voltage_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		chg->batt_current_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = batt_set_iio_channel(chg, BMS, BATT_QG_TEMP, val->intval);
	default:
		rc = -EINVAL;
	}

	return rc;
}

static int batt_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
	case POWER_SUPPLY_PROP_TEMP:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = batt_psy_props,
	.num_properties = ARRAY_SIZE(batt_psy_props),
	.get_property = batt_psy_get_prop,
	.set_property = batt_psy_set_prop,
	.property_is_writeable = batt_psy_prop_is_writeable,
};

static int wt_init_batt_psy(struct batt_chg *chg)
{
	struct power_supply_config batt_cfg = {};
	int rc = 0;

	if(!chg) {
		pr_err("chg is NULL\n");
		return rc;
	}

	batt_cfg.drv_data = chg;
	batt_cfg.of_node = chg->dev->of_node;
	chg->batt_psy = devm_power_supply_register(chg->dev,
					   &batt_psy_desc,
					   &batt_cfg);
	if (IS_ERR(chg->batt_psy)) {
		pr_err("Couldn't register battery power supply\n");
		return PTR_ERR(chg->batt_psy);
	}

	return rc;
}

/************************
 * USB PSY REGISTRATION *
 ************************/
static enum power_supply_property usb_psy_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE,
};

static int usb_psy_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int rc = 0, type = 0;
	struct batt_chg *chg = power_supply_get_drvdata(psy);
	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_PRESENT, &val->intval);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if(get_real_type(chg) > 0)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = chg->charge_design_voltage_max;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = chg->charge_voltage_max;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_VOLTAGE_NOW, &chg->batt_voltage_now);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CURRENT_NOW, &chg->batt_current_now);
		chg->batt_current_now = abs(chg->batt_current_now);
		val->intval = chg->batt_current_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chg->batt_current_max;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		type = get_real_type(chg);
		if (type == POWER_SUPPLY_TYPE_USB || type == POWER_SUPPLY_TYPE_USB_CDP)
			val->intval = POWER_SUPPLY_TYPE_USB;
		else
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = POWER_SUPPLY_SCOPE_SYSTEM;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = chg->input_batt_current_max;
		break;
	case POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE:
		val->intval = get_quick_charge_type(chg);
		//pr_err("test get_quick_charge_type %d\n", val->intval);
		if (chg->battery_temp >= 580)
			val->intval = QUICK_CHARGE_NORMAL;
		break;
	default:
		pr_err("get prop %d is not supported in usb\n", psp);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int usb_psy_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct batt_chg *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		chip->batt_current_now = val->intval;
		break;
	default:
		pr_err("Set prop %d is not supported in usb psy\n",
				psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int usb_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_POWER_NOW:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB_PD,
	.properties = usb_psy_props,
	.num_properties = ARRAY_SIZE(usb_psy_props),
	.get_property = usb_psy_get_prop,
	.set_property = usb_psy_set_prop,
	.property_is_writeable = usb_psy_prop_is_writeable,
};

static int wt_init_usb_psy(struct batt_chg *chg)
{
	struct power_supply_config usb_cfg = {};

	usb_cfg.drv_data = chg;
	usb_cfg.of_node = chg->dev->of_node;
	chg->usb_psy = devm_power_supply_register(chg->dev,
						  &usb_psy_desc,
						  &usb_cfg);
	if (IS_ERR(chg->usb_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->usb_psy);
	}

	return 0;
}

static int batt_init_config(struct batt_chg *chg)
{
	chg->input_batt_current_max = 6100000;
	chg->batt_current_max = 12200000;
	chg->mishow_flag = 0;
	chg->charge_voltage_max = 4450000;
	chg->charge_design_voltage_max = 11000000;
	chg->system_temp_level = 0;
	chg->thermal_throttle_active = false;
	chg->batt_iterm = 250;
	chg->thermal_levels = 24;
	chg->shutdown_flag = false;
	chg->dt.jeita_temp_step0 = -100;
	chg->dt.jeita_temp_step1 = 0;
	chg->dt.jeita_temp_step2 = 50;
	chg->dt.jeita_temp_step3 = 100;
	chg->dt.jeita_temp_step4 = 150;
	chg->dt.jeita_temp_step5 = 480;
	chg->dt.jeita_temp_step6 = 580;
	chg->mtbf_current = 0;
	chg->node_flag = false;
	chg->is_chg_control = false;
	chg->therm_step = 0;
	chg->update_cont = 0;
	chg->fastcharge_mode= 0;
	chg->cp_master_adc = 1;
	chg->cp_slave_adc = 1;
	chg->start_cnt = 0;
	chg->high_temp_flag = 0;
	chg->connector_temp = 0;
	chg->usb_temp = 0;
	chg->input_limit_flag = 0;
	chg->pd_type_cnt = 0;
	chg->low_temp_flag = 0;

	return 0;
}

static int batt_parse_dt(struct batt_chg *chg)
{
	struct device_node *node = chg->dev->of_node;
	int rc = 0;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(node,
				"qcom,fv-max-uv", &chg->dt.batt_profile_fv_uv);
	if (rc < 0)
		chg->dt.batt_profile_fv_uv = 4450000;
	else
		pr_err("test %d\n",chg->dt.batt_profile_fv_uv);

	rc = of_property_read_u32(node,
				"qcom,fcc-max-ua", &chg->dt.batt_profile_fcc_ua);
	if (rc < 0)
		chg->dt.batt_profile_fcc_ua = 10000000;
	else
		pr_err("test %d\n",chg->dt.batt_profile_fcc_ua);

	rc = of_property_read_u32(node,
				"qcom,batt_iterm", &chg->dt.batt_iterm);
	if (rc < 0)
		chg->dt.batt_iterm = 300000;
	else
		pr_err("test %d\n",chg->dt.batt_iterm);

	rc = get_iio_channel(chg, "main_therm", &chg->cp_master_therm);
	if (rc < 0)
		return rc;

	rc = get_iio_channel(chg, "slave_therm", &chg->cp_slave_therm);
	if (rc < 0)
		return rc;

	rc = get_iio_channel(chg, "usb_therm", &chg->connector_therm);
	if (rc < 0)
		return rc;

	return 0;
};

/*
 * Notify userspace listeners (such as vectord) via poll/epoll on
 * /sys/kernel/vector/charge_limit_state when state changes.
 */
static void wt_chg_notify_charge_limit_state(void)
{
	if (vector_kobj)
		sysfs_notify(vector_kobj, NULL, "charge_limit_state");
}

static void wt_chg_set_limit_state_locked(struct batt_chg *chg, int new_state)
{
	int old_state = READ_ONCE(chg->charge_limit_state);

	if (old_state != new_state) {
		WRITE_ONCE(chg->charge_limit_state, new_state);
		wt_chg_notify_charge_limit_state();
	}
}

/*
 * Checks whether the charging cable is physically connected.
 * Hardware bus voltage under WT_VBUS_UNPLUG_MV indicates physical disconnect
 * even if chg->real_type has not yet updated.
 */
static bool wt_chg_is_cable_plugged(struct batt_chg *chg)
{
	int bq_vbus = 0;
	int rc;

	if (!chg)
		return false;

	if (!chg->real_type)
		return false;

	rc = batt_get_iio_channel(chg, MAIN, MAIN_BUS_VOLTAGE, &bq_vbus);
	if (rc >= 0 && bq_vbus < WT_VBUS_UNPLUG_MV)
		return false;

	return true;
}

/*
 * Reads SoC from the fuel gauge:
 * Prioritizes ui_soc to match the Android status bar, with BMS capacity and CC_SOC fallback.
 * Stores raw readings in chg->limit_raw_cc_soc and chg->limit_raw_cap for logging.
 * Returns true if valid SoC (1..100) was obtained, false otherwise.
 */
static bool wt_chg_get_limit_soc(struct batt_chg *chg, int *soc_pct, int *soc_bp)
{
	int r_soc = -1, cap = -1;
	int rc_rsoc, rc_cap;
	int pct = -1, bp = -1;
	bool valid = false;

	if (!chg)
		return false;

	rc_rsoc = batt_get_iio_channel(chg, BMS, BATT_QG_CC_SOC, &r_soc);
	rc_cap = batt_get_iio_channel(chg, BMS, BATT_QG_CAPACITY, &cap);

	chg->limit_raw_cc_soc = (rc_rsoc == 0) ? r_soc : -1;
	chg->limit_raw_cap = (rc_cap == 0) ? cap : -1;

	if (chg->ui_soc > 0 && chg->ui_soc <= 100) {
		pct = chg->ui_soc;
		if (rc_rsoc == 0 && r_soc > 0 && r_soc <= 10000 && (r_soc / 100) == chg->ui_soc)
			bp = r_soc;
		else
			bp = chg->ui_soc * 100;
		valid = true;
	} else if (rc_cap == 0 && cap > 0 && cap <= 100) {
		pct = cap;
		bp = (rc_rsoc == 0 && r_soc > 0 && r_soc <= 10000) ? r_soc : (cap * 100);
		valid = true;
	} else if (rc_rsoc == 0 && r_soc > 0 && r_soc <= 10000) {
		bp = r_soc;
		pct = r_soc / 100;
		valid = true;
	}

	if (valid) {
		if (soc_pct)
			*soc_pct = pct;
		if (soc_bp)
			*soc_bp = bp;
	}

	return valid;
}

/*
 * Charge limit hysteresis and safety evaluation.
 * Must be called with chg->limit_lock held.
 */
static void wt_chg_evaluate_charge_limit_locked(struct batt_chg *chg)
{
	int stop, resume, state;
	int soc_pct = -1, soc_bp = -1;
	bool valid;

	if (READ_ONCE(chg->shutdown_flag))
		return;

	stop = READ_ONCE(chg->charge_stop_pct);
	resume = READ_ONCE(chg->charge_resume_pct);
	state = READ_ONCE(chg->charge_limit_state);

	/* Feature disabled (stop == 0 or 100): release hold immediately and reset resume */
	if (stop == 0 || stop == 100) {
		wt_chg_set_limit_state_locked(chg, 0);
		WRITE_ONCE(chg->charge_stop_pct, 0);
		WRITE_ONCE(chg->charge_resume_pct, 0);
		chg->limit_debounce_cnt = 0;
		chg->limit_invalid_soc_cnt = 0;
		if (chg->limit_hold) {
			chg->limit_hold = false;
			if (!READ_ONCE(chg->bypass_mode) && chg->real_type && !chg->high_temp_flag)
				sw_battery_jeita(chg);
			power_supply_changed(chg->batt_psy);
			pr_info("wt_chg: charge limit disabled, hold released (ui_soc=%d%%, cap=%d%%, cc_soc=%d.%02d%%)\n",
				chg->ui_soc, chg->limit_raw_cap,
				chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc / 100 : 0,
				chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc % 100 : 0);
		}
		return;
	}

	/* Case 4: Unplug: clear limit_hold and reset state to charging */
	if (!wt_chg_is_cable_plugged(chg)) {
		wt_chg_set_limit_state_locked(chg, 1);
		chg->limit_debounce_cnt = 0;
		chg->limit_invalid_soc_cnt = 0;
		if (chg->limit_hold) {
			chg->limit_hold = false;
			power_supply_changed(chg->batt_psy);
			pr_info("wt_chg: unplugged, charge limit hold cleared (ui_soc=%d%%, cap=%d%%, cc_soc=%d.%02d%%)\n",
				chg->ui_soc, chg->limit_raw_cap,
				chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc / 100 : 0,
				chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc % 100 : 0);
		}
		return;
	}

	/* Read capacity via unified helper */
	valid = wt_chg_get_limit_soc(chg, &soc_pct, &soc_bp);

	/*
	 * Case 6: Invalid, zero or out-of-range capacity read: never change state
	 * on that sample. If capacity is invalid for N (3) consecutive evaluations
	 * while holding, release the hold (fail safe toward charging).
	 */
	if (!valid) {
		chg->limit_debounce_cnt = 0;
		if (state == 2 /* HOLDING */) {
			chg->limit_invalid_soc_cnt++;
			if (chg->limit_invalid_soc_cnt >= 3) {
				pr_warn("wt_chg: invalid capacity read %d consecutive times while holding; failing safe to charging (ui_soc=%d%%, cap=%d%%, cc_soc=%d.%02d%%)\n",
					chg->limit_invalid_soc_cnt, chg->ui_soc, chg->limit_raw_cap,
					chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc / 100 : 0,
					chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc % 100 : 0);
				wt_chg_set_limit_state_locked(chg, 1);
				chg->limit_hold = false;
				chg->limit_invalid_soc_cnt = 0;
				if (!READ_ONCE(chg->bypass_mode) && chg->real_type && !chg->high_temp_flag)
					sw_battery_jeita(chg);
				power_supply_changed(chg->batt_psy);
			}
		}
		return;
	}

	/* Valid sample: clear invalid read counter */
	chg->limit_invalid_soc_cnt = 0;

	/*
	 * Case 7: soc <= 15: force-release limit_hold regardless of config,
	 * and never enter a hold below that.
	 */
	if (soc_pct <= 15) {
		chg->limit_debounce_cnt = 0;
		if (state == 2 || chg->limit_hold) {
			pr_warn("wt_chg: soc %d%% <= 15%%: force-releasing charge limit hold (ui_soc=%d%%, cap=%d%%, cc_soc=%d.%02d%%)\n",
				soc_pct, chg->ui_soc, chg->limit_raw_cap,
				chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc / 100 : 0,
				chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc % 100 : 0);
			wt_chg_set_limit_state_locked(chg, 1);
			chg->limit_hold = false;
			if (!READ_ONCE(chg->bypass_mode) && chg->real_type && !chg->high_temp_flag)
				sw_battery_jeita(chg);
			power_supply_changed(chg->batt_psy);
		}
		return;
	}

	/*
	 * Hysteresis and 2-sample debounce state machine:
	 * Compare whole percents on displayed value (soc_pct).
	 * Enforces >= 400ms between sample 1 and sample 2.
	 * State 1 = CHARGING, State 2 = HOLDING.
	 */
	if (state == 2 /* HOLDING */) {
		/* While holding: soc_pct <= resume -> resume charging after 2 samples */
		if (soc_pct <= resume) {
			ktime_t now = ktime_get();
			if (chg->limit_debounce_cnt == 0) {
				chg->limit_debounce_cnt = 1;
				chg->last_debounce_sample_time = now;
				pr_info("wt_chg: charge limit resume condition met (soc=%d%% [%d.%02d%%] <= %d%%, count=1/2)\n",
					soc_pct, soc_bp / 100, soc_bp % 100, resume);
			} else if (ktime_ms_delta(now, chg->last_debounce_sample_time) >= 400) {
				chg->limit_debounce_cnt = 2;
				chg->last_debounce_sample_time = now;
				wt_chg_set_limit_state_locked(chg, 1);
				chg->limit_hold = false;
				chg->limit_debounce_cnt = 0;
				if (!READ_ONCE(chg->bypass_mode) && chg->real_type && !chg->high_temp_flag)
					sw_battery_jeita(chg);
				power_supply_changed(chg->batt_psy);
				pr_info("wt_chg: charge limit resumed charging (ui_soc=%d%%, cap=%d%%, cc_soc=%d.%02d%%, resume=%d%%)\n",
					chg->ui_soc, chg->limit_raw_cap,
					chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc / 100 : 0,
					chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc % 100 : 0,
					resume);
			}
		} else {
			chg->limit_debounce_cnt = 0;
		}
	} else {
		/* In CHARGING state (or start): soc_pct >= stop -> hold after 2 samples */
		if (state != 1)
			wt_chg_set_limit_state_locked(chg, 1);

		if (soc_pct >= stop) {
			ktime_t now = ktime_get();
			if (chg->limit_debounce_cnt == 0) {
				chg->limit_debounce_cnt = 1;
				chg->last_debounce_sample_time = now;
				pr_info("wt_chg: charge limit stop condition met (soc=%d%% [%d.%02d%%] >= %d%%, count=1/2)\n",
					soc_pct, soc_bp / 100, soc_bp % 100, stop);
			} else if (ktime_ms_delta(now, chg->last_debounce_sample_time) >= 400) {
				chg->limit_debounce_cnt = 2;
				chg->last_debounce_sample_time = now;
				wt_chg_set_limit_state_locked(chg, 2);
				chg->limit_hold = true;
				chg->limit_debounce_cnt = 0;
				batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, 0);
				power_supply_changed(chg->batt_psy);
				pr_info("wt_chg: charge limit engaged hold (ui_soc=%d%%, cap=%d%%, cc_soc=%d.%02d%%, stop=%d%%)\n",
					chg->ui_soc, chg->limit_raw_cap,
					chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc / 100 : 0,
					chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc % 100 : 0,
					stop);
			}
		} else {
			chg->limit_debounce_cnt = 0;
		}
	}
}

/*
 * Dedicated non-deferrable work for charge limit polling and debouncing.
 * Ensures ~2s regular polling and 500ms debounce confirmation even in suspend/idle.
 */
static void wt_chg_charge_limit_work_fn(struct work_struct *work)
{
	struct batt_chg *chg = container_of(work, struct batt_chg, charge_limit_work.work);
	int stop, interval_ms;

	if (!chg || READ_ONCE(chg->shutdown_flag))
		return;

	mutex_lock(&chg->limit_lock);
	if (READ_ONCE(chg->shutdown_flag)) {
		mutex_unlock(&chg->limit_lock);
		return;
	}

	wt_chg_evaluate_charge_limit_locked(chg);

	stop = READ_ONCE(chg->charge_stop_pct);
	if (stop > 0 && stop < 100 && !READ_ONCE(chg->shutdown_flag) && wt_chg_is_cable_plugged(chg)) {
		interval_ms = (chg->limit_debounce_cnt > 0) ? 500 : 2000;
		schedule_delayed_work(&chg->charge_limit_work, msecs_to_jiffies(interval_ms));
	}
	mutex_unlock(&chg->limit_lock);
}

static void batt_chg_main(struct work_struct *work)
{
	int val = 0;
	int temp;
	int soc;
	int time = 0;

	struct batt_chg *chg = container_of(work,
				struct batt_chg, batt_chg_work.work);

	if (!chg || READ_ONCE(chg->shutdown_flag))
		return;

	chg->start_cnt++;
	if(chg->start_cnt > 1000000)
		chg->start_cnt = 100;

	val = get_charger_pump_master_temp(chg, &chg->cp_master_temp);
	val = get_charger_pump_slave_temp(chg, &chg->cp_slave_temp);
	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BUS_VOLTAGE, &chg->vbus_voltage);
	batt_get_iio_channel(chg, BMS, BATT_QG_TEMP, &temp);
	batt_get_iio_channel(chg, BMS, BATT_QG_CAPACITY, &soc);
	pr_err("wt workup cp_master_temp %d, cp_slave_temp %d, get_boot_mode = %d, lcdon %d, vbus %d, count %d, count %d, soc %d fastcharge_mode %d\n",
		chg->cp_master_temp, chg->cp_slave_temp, get_boot_mode(), chg->lcd_on, chg->vbus_voltage, chg->update_cont, chg->power_supply_count, soc, chg->fastcharge_mode);

	if(chg->mishow_flag) {
		if(chg->real_type)
			val = 1;
		else
			val = 0;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
		power_supply_changed(chg->usb_psy);
		pr_err("mishow capacity %d,mishow_flag %d\n", chg->ui_soc, chg->mishow_flag);
	}

	if(chg->mtbf_current == 1500 && chg->ui_soc <= 40){
		val = 0;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
		pr_err("mtbf %d\n", chg->ui_soc, val);
	}

	if(chg->ui_soc <= 1) {
		cancel_delayed_work(&chg->xm_prop_change_work);
		chg->update_cont = 15;
		schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(300));
	}

	if(chg->real_type) {
		if(!chg->wakeup_flag) {
			__pm_stay_awake(chg->wt_ws);
			chg->wakeup_flag = 1;
			pr_err("wt workup\n");
		}
		if (!chg->typec_mode && chg->pd_type_cnt > 50)
			chg->pd_type_cnt = 0;

		if(chg->usb_float){
			pr_err("chg->usb_float\n");
			schedule_delayed_work(&chg->usb_type_work, msecs_to_jiffies(1000));
		}

		if(chg->vbus_voltage < WT_VBUS_UNPLUG_MV)
			chg->vbus_cnt++;
		else
			chg->vbus_cnt = 0;
		if (!chg->isln8000flg) {
			if (!chg->cp_master_adc) {
				chg->cp_master_adc = 1;
				batt_set_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_ADC_ENABLE, chg->cp_master_adc);
			}
			if (!chg->cp_slave_adc) {
				chg->cp_slave_adc = 1;
				batt_set_iio_channel(chg, CP_SLAVE, CHARGE_PUMP_SC_ADC_ENABLE, chg->cp_slave_adc);
			}
		}
		if (READ_ONCE(chg->bypass_mode) && soc < 10) {
			mutex_lock(&chg->bypass_lock);
			if (READ_ONCE(chg->bypass_mode)) {
				WRITE_ONCE(chg->bypass_mode, 0);
				if (!READ_ONCE(chg->limit_hold) && chg->real_type && !chg->high_temp_flag)
					batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, 1);
				pr_warn("wt_chg: battery low (%d%% < 10%%), bypass auto-cleared\n", soc);
			}
			mutex_unlock(&chg->bypass_lock);
		}
		if (!READ_ONCE(chg->shutdown_flag) && READ_ONCE(chg->charge_stop_pct) > 0 && !delayed_work_pending(&chg->charge_limit_work))
			schedule_delayed_work(&chg->charge_limit_work, 0);
		sw_battery_jeita(chg);
		sw_battery_set_cv(chg);
		if (chg->real_type == POWER_SUPPLY_TYPE_USB || chg->real_type == POWER_SUPPLY_TYPE_USB_CDP)
			sw_battery_recharge(chg);
		chg->power_supply_count = 0;
		power_supply_changed(chg->usb_psy);
		if (READ_ONCE(chg->shutdown_flag))
			return;
		if(chg->is_pps_on) {
			if (chg->start_cnt <= 60)
				time = 1000;
			else
				time = 3000;
			batt_set_iio_channel(chg, MAIN, MAIN_ENBALE_CHAGER_TERM, false);
			power_supply_changed(chg->batt_psy);
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(time));
		} else {
			if (chg->start_cnt <= 40)
				time = 1500;
			else if(chg->high_temp_flag && chg->pd_auth && chg->battery_temp < 500)
				time = 1000;
			else if(chg->ui_soc >= 99 && (chg->batt_current_now != 0)){
				time = 2000;
			} else  {
				time = 10000;
			}
			if (get_boot_mode())
				time = 30000;

			if(chg->battery_temp < 455 && chg->battery_temp >= 448)
				time = 2000;
			batt_set_iio_channel(chg, MAIN, MAIN_ENBALE_CHAGER_TERM, true);
			power_supply_changed(chg->batt_psy);
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(time));
		}
	} else {
		if(chg->wakeup_flag) {
			__pm_relax(chg->wt_ws);
			chg->wakeup_flag = 0;
			pr_err("wt workup relax\n");
		}
		if (!READ_ONCE(chg->shutdown_flag) &&
		    (READ_ONCE(chg->limit_hold) || READ_ONCE(chg->charge_limit_state) == 2)) {
			cancel_delayed_work(&chg->charge_limit_work);
			schedule_delayed_work(&chg->charge_limit_work, 0);
		}
		if (READ_ONCE(chg->bypass_mode)) {
			int bq_vbus = 0;
			int rc = batt_get_iio_channel(chg, MAIN, MAIN_BUS_VOLTAGE, &bq_vbus);
			if (rc >= 0 && bq_vbus < WT_VBUS_UNPLUG_MV) {
				mutex_lock(&chg->bypass_lock);
				if (READ_ONCE(chg->bypass_mode)) {
					WRITE_ONCE(chg->bypass_mode, 0);
					if (!READ_ONCE(chg->limit_hold) && chg->real_type && !chg->high_temp_flag)
						batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, 1);
					pr_info("wt_chg: cable unplugged (bq_vbus=%d mV), bypass auto-cleared\n",
						bq_vbus);
				}
				mutex_unlock(&chg->bypass_lock);
			}
		}
		if (READ_ONCE(chg->usb_icl_override_ma)) {
			int bq_vbus = 0;
			int rc = batt_get_iio_channel(chg, MAIN, MAIN_BUS_VOLTAGE, &bq_vbus);
			if (rc >= 0 && bq_vbus < WT_VBUS_UNPLUG_MV) {
				mutex_lock(&chg->icl_lock);
				if (READ_ONCE(chg->usb_icl_override_ma)) {
					WRITE_ONCE(chg->usb_icl_override_ma, 0);
					pr_info("wt_chg: cable unplugged (bq_vbus=%d mV), usb_icl_override_ma auto-cleared\n",
						bq_vbus);
				}
				mutex_unlock(&chg->icl_lock);
			}
		}
		if (soc != chg->old_capacity || temp > 550 || chg->power_supply_count >= 6 || soc <= 10) {
			chg->power_supply_count = 0;
			chg->old_capacity = soc;
			power_supply_changed(chg->batt_psy);
		} else {
			chg->power_supply_count += 1;
		}
		if (chg->fastcharge_mode) {
			batt_set_iio_channel(chg, BMS, BATT_QG_FASTCHARGE_MODE, false);
			chg->fastcharge_mode = 0;
		}

		chg->cv_flag = 0;

		if (!chg->isln8000flg && !chg->typec_mode && chg->is_battery_on) {
			if (chg->cp_master_adc) {
				chg->cp_master_adc = 0;
				batt_set_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_ADC_ENABLE, chg->cp_master_adc);
			}
			if (chg->cp_slave_adc) {
				chg->cp_slave_adc = 0;
				batt_set_iio_channel(chg, CP_SLAVE, CHARGE_PUMP_SC_ADC_ENABLE, chg->cp_slave_adc);
			}
		}

		chg->low_temp_flag = 0;
		chg->charge_voltage_max = 4450000;
		if (READ_ONCE(chg->shutdown_flag))
			return;
		if(soc <= 10 || temp > 550 || temp < 50)
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(10000));
		else
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(30000));
	}
#ifdef WT_COMPILE_FACTORY_VERSION
	if(chg->ui_soc >= 79) {
		val = 1;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
	//	val = 0;
	//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, val);
		chg->is_chg_control = true;
		pr_err("ATO version capacity %d,node_flag %d\n", chg->ui_soc, chg->is_chg_control);
	}
	if(chg->ui_soc <= 60 && chg->is_chg_control) {
		val = 0;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
	//	val = 1;
	//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, val);
		chg->is_chg_control = false;
		pr_err("ATO version capacity %d,node_flag %d\n", chg->ui_soc, chg->is_chg_control);
	}
#endif

}

static void usb_therm(struct work_struct *work)
{
	int val;
	int temp;
	struct batt_chg *chg = container_of(work,
				struct batt_chg, usb_therm_work.work);

	if (!chg || READ_ONCE(chg->shutdown_flag))
		return;

	get_charger_connector_temp(chg, &temp);
	if(chg->connector_temp != 0) {
		val = chg->connector_temp;
	} else {
		val = temp;
	}
#ifdef WT_COMPILE_JP_CHARGE
	chg->usb_temp = val;
	if(val >= 680){
		chg->usb_temp_flag = 1;
		batt_set_iio_channel(chg, BMS, BATT_QG_FCC_MAX, 1000);
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, true);
		pr_err("chg->connector_temp %d stop charging, temp %d", val, temp);
	} else {
		if(chg->usb_temp_flag) {
			batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, -1);
			chg->usb_temp_flag = 0;
		}
		pr_err("chg->connector_temp %d", val);
	}

	schedule_delayed_work(&chg->usb_therm_work, msecs_to_jiffies(3000));
#endif
}

static void batt_usb_type(struct work_struct *work)
{
	struct batt_chg *chg = container_of(work,
				struct batt_chg, usb_type_work.work);

	if (!chg || READ_ONCE(chg->shutdown_flag))
		return;

	chg->usb_float = 0;
	chg->old_charge_type = POWER_SUPPLY_TYPE_UNKNOWN;
	power_supply_changed(chg->batt_psy);
	power_supply_changed(chg->usb_psy);
	pr_err("batt_usb_type chg->usb_float %d", chg->usb_float);
}

static const char *get_usb_type_name(u32 usb_type)
{
	u32 i;

	if (usb_type >= QTI_POWER_SUPPLY_USB_TYPE_HVDCP &&
	    usb_type <= QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3_CLASSB) {
		for (i = 0; i < ARRAY_SIZE(qc_power_supply_usb_type_text);
		     i++) {
			if (i == (usb_type - QTI_POWER_SUPPLY_USB_TYPE_HVDCP))
				return qc_power_supply_usb_type_text[i];
		}
		return "Unknown";
	}

	for (i = 0; i < ARRAY_SIZE(power_supply_usb_type_text); i++) {
		if (i == usb_type)
			return power_supply_usb_type_text[i];
	}

	return "Unknown";
}

/*static int read_property_id(struct batt_chg *chg,
			struct psy_state *pst, u32 prop_id)
{
	return 0;
}*/

static ssize_t usb_real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	chg->real_type = get_usb_real_type(chg);
	val = chg->real_type - 3;

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
}
static CLASS_ATTR_RO(usb_real_type);

static ssize_t real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	int val;

	chg->real_type = get_usb_real_type(chg);
	val = chg->real_type - 3;

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
}
static CLASS_ATTR_RO(real_type);

static ssize_t pd_auth_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = chg->pd_auth;
	power_supply_changed(chg->batt_psy);
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t pd_auth_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->pd_auth = val;
	pr_err("pd_auth = %d\n", chg->pd_auth);

	return count;
}
static CLASS_ATTR_RW(pd_auth);

static ssize_t cp_ibus_slave_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, CP_SLAVE, CHARGE_PUMP_SC_BUS_CURRENT, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_ibus_slave);

static ssize_t cp_ibus_master_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BUS_CURRENT, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_ibus_master);

static ssize_t input_current_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;
	int temp;

	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BUS_CURRENT, &temp);
	val = temp;
	batt_get_iio_channel(chg, CP_SLAVE, CHARGE_PUMP_SC_BUS_CURRENT, &temp);
	val += temp;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(input_current);

static ssize_t cp_temp_slave_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = get_charger_pump_slave_temp(chg, &chg->cp_slave_temp);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->cp_slave_temp);
}
static CLASS_ATTR_RO(cp_temp_slave);

static ssize_t cp_temp_master_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = get_charger_pump_master_temp(chg, &chg->cp_master_temp);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->cp_master_temp);
}
static CLASS_ATTR_RO(cp_temp_master);

static ssize_t cp_present_slave_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, CP_SLAVE, CHARGE_PUMP_SC_PRESENT, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_present_slave);

static ssize_t cp_present_master_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_PRESENT, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_present_master);

static ssize_t cp_vbus_voltage_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BUS_VOLTAGE, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_vbus_voltage);

static ssize_t cell_voltage_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;
	batt_get_iio_channel(chg, BMS, BATT_QG_VOLTAGE_NOW, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cell_voltage);

static ssize_t vbus_voltage_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, MAIN, MAIN_BUS_VOLTAGE, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(vbus_voltage);

static ssize_t typec_cc_orientation_show(struct class *c,
				struct class_attribute *attr, char *buf)
{

	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n",chg->polarity_state);

}
static CLASS_ATTR_RO(typec_cc_orientation);

static ssize_t stopcharging_test_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = 1;
	batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
//	val = 0;
//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, val);
	chg->node_flag = true;
	power_supply_changed(chg->usb_psy);
	power_supply_changed(chg->batt_psy);
	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->node_flag);
}
static CLASS_ATTR_RO(stopcharging_test);

static ssize_t startcharging_test_show(struct class *c,
				struct class_attribute *attr, char *buf)
{

	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = 0;
	chg->node_flag = false;
	batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
//	val = 1;
//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, val);
	power_supply_changed(chg->usb_psy);
	power_supply_changed(chg->batt_psy);
	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->node_flag);

}
static CLASS_ATTR_RO(startcharging_test);

static ssize_t charging_call_state_show(struct class *c,
				struct class_attribute *attr, char *buf)
{

	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->charging_call_state);

}
static ssize_t charging_call_state_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->charging_call_state = val;
	pr_err("chg->charging_call_state enable= %d\n", chg->charging_call_state);

	return count;
}
static CLASS_ATTR_RW(charging_call_state);

static ssize_t soc_decimal_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, BMS, BATT_QG_SOC_DECIMAL, &val);
	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
static CLASS_ATTR_RO(soc_decimal);

static ssize_t soc_decimal_rate_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, BMS, BATT_QG_SOC_DECIMAL_RATE, &val);
	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
static CLASS_ATTR_RO(soc_decimal_rate);

static ssize_t shutdown_delay_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int rc = 0;
	int vaule = 0;

	pr_err(" %s start vaule = %d\n", __func__,vaule);

	rc = batt_get_iio_channel(chg, BMS, BATT_QG_SHUTDOWN_DELAY, &vaule);
	if (rc < 0) {
		pr_err("Couldn't get prop rc = %d\n", rc);
		return -ENODATA;
	}
	if(chg->ui_soc > 1)
		vaule = 0;
	if (!chg->is_battery_on && chg->ui_soc == 1) {
		pr_err("no battery,batt capacity is 1\n");
		vaule = 1;
	}
	pr_err(" %s vaule = %d\n", __func__,vaule);

	return scnprintf(buf, PAGE_SIZE, "%d", vaule);
}
static CLASS_ATTR_RO(shutdown_delay);

#ifdef CONFIG_WT_QGKI
extern int get_verify_digest(char *buf);
extern int set_verify_digest(u8 *rand_num);
static ssize_t verify_digest_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	get_verify_digest(buf);

	return scnprintf(buf, PAGE_SIZE, "%s", buf);
}

static ssize_t verify_digest_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	char kbuf[70] = {0};
	pr_err("verify_digest_store = %s\n", buf);
	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, buf, count - 1);

	set_verify_digest(kbuf);
	return count;
}

static CLASS_ATTR_RW(verify_digest);
#endif

int pps_change_to_stop(void)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);

	if (!chg)
		return 0;

	return chg->input_suspend ? 1 : 0;
}
EXPORT_SYMBOL(pps_change_to_stop);

static ssize_t input_suspend_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d", chg->input_suspend);
}

static ssize_t input_suspend_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->input_suspend = val;
	pr_err("chg->input_suspend = %d\n", chg->input_suspend);
	if(val == 1) {
		chg->input_suspend  = 1;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, chg->input_suspend);
	//	chg->input_suspend = 0;
	//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, chg->input_suspend);
	//	chg->input_suspend  = 1;
	}
	if(val == 0) {
		chg->input_suspend = 0;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
	//	chg->input_suspend = 1;
	//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, val);
	//	chg->input_suspend = 0;
	}
	power_supply_changed(chg->usb_psy);
	power_supply_changed(chg->batt_psy);
	return count;
}
static CLASS_ATTR_RW(input_suspend);

static ssize_t battery_name_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	if(chg->batt_id == 1)
		return scnprintf(buf, PAGE_SIZE, "S88006_SWD_4V45_5000mAh\n");
	else if(chg->batt_id == 2)
		return scnprintf(buf, PAGE_SIZE, "S88006_NVT_4V45_5000mAh\n");
	else if(chg->batt_id == 4)
		return scnprintf(buf, PAGE_SIZE, "S88006_NVT_4V45_5000mAh\n");
	else
		return scnprintf(buf, PAGE_SIZE, "NOT_DEFAULT_BATTERY\n");
}
static CLASS_ATTR_RO(battery_name);

static ssize_t chip_ok_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	int val;

	batt_get_iio_channel(chg, BMS, BATT_QG_CHIP_OK, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(chip_ok);

static ssize_t authentic_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = chg->batt_auth;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t authentic_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->batt_auth = val;
	batt_set_iio_channel(chg, BMS, BATT_QG_BATTERY_AUTH, val);
	pr_err("batt_auth = %d\n", chg->batt_auth);

	return count;
}
static CLASS_ATTR_RW(authentic);

static ssize_t quick_charge_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", get_quick_charge_type(chg));
}
static CLASS_ATTR_RO(quick_charge_type);

static ssize_t apdo_max_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", get_apdo_max(chg));
}
static CLASS_ATTR_RO(apdo_max);

static const char * const usb_typec_mode_text[] = {
	"Nothing attached", "Source attached", "Sink attached",
	"Audio Adapter", "Non compliant",
};

static ssize_t typec_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int type;

	type = get_usb_real_type(chg);
	if (chg->typec_mode == 0 && type != 0){
		type = 1;
	} else {
		type = chg->typec_mode;
	}
	pr_err(" %s vaule = %d\n", __func__, type);
	return scnprintf(buf, PAGE_SIZE, "%s", usb_typec_mode_text[type]);
}
static CLASS_ATTR_RO(typec_mode);

static ssize_t mtbf_current_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = chg->mtbf_current;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t mtbf_current_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->mtbf_current = val;

	batt_set_iio_channel(chg, MAIN, MAIN_INPUT_CURRENT_SETTLED, val);
	pr_err("mtbf_current = %d\n", chg->mtbf_current);

	return count;
}
static CLASS_ATTR_RW(mtbf_current);

static ssize_t fastcharge_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, BMS, BATT_QG_FASTCHARGE_MODE, &chg->fastcharge_mode);
	val = chg->fastcharge_mode;
	if (chg->battery_temp < chg->dt.jeita_temp_step4 || chg->battery_temp > chg->dt.jeita_temp_step5)
		val = 0;
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(fastcharge_mode);

static ssize_t soh_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, BMS, BATT_QG_RESISTANCE_ID, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(soh);

static ssize_t resistance_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val = 0;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(resistance);

static ssize_t connector_temp_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;
	int temp;

	get_charger_connector_temp(chg, &temp);
	if(chg->connector_temp != 0) {
		val = chg->connector_temp;
	} else {
		val = temp;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t connector_temp_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->connector_temp = val;

	pr_err("connector_temp = %d\n", chg->connector_temp);

	return count;
}
static CLASS_ATTR_RW(connector_temp);

static ssize_t bypass_charger_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(chg->bypass_mode));
}

static ssize_t bypass_charger_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (!chg || READ_ONCE(chg->shutdown_flag))
		return -ENODEV;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	val = !!val;

	mutex_lock(&chg->bypass_lock);
	if (READ_ONCE(chg->bypass_mode) == val) {
		mutex_unlock(&chg->bypass_lock);
		return count;
	}

	WRITE_ONCE(chg->bypass_mode, val);
	if (val)
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, 0);
	mutex_unlock(&chg->bypass_lock);

	power_supply_changed(chg->usb_psy);
	power_supply_changed(chg->batt_psy);
	cancel_delayed_work(&chg->batt_chg_work);
	schedule_delayed_work(&chg->batt_chg_work, 0);

	pr_info("wt_chg: bypass mode set to %d\n", val);
	return count;
}
static struct class_attribute class_attr_bypass_charger = {
	.attr = {
		.name = "bypass_charger",
		.mode = 0660,
	},
	.show = bypass_charger_show,
	.store = bypass_charger_store,
};

static ssize_t usb_icl_override_ma_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(chg->usb_icl_override_ma));
}

static ssize_t usb_icl_override_ma_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (!chg || READ_ONCE(chg->shutdown_flag))
		return -ENODEV;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	if (val > 0) {
		if (val < 100)
			val = 100;
		else if (val > 1500)
			val = 1500;
	}

	mutex_lock(&chg->icl_lock);
	if (READ_ONCE(chg->usb_icl_override_ma) == val) {
		mutex_unlock(&chg->icl_lock);
		return count;
	}

	WRITE_ONCE(chg->usb_icl_override_ma, val);
	mutex_unlock(&chg->icl_lock);

	cancel_delayed_work(&chg->batt_chg_work);
	schedule_delayed_work(&chg->batt_chg_work, 0);

	pr_info("wt_chg: usb_icl_override_ma set to %d\n", val);
	return count;
}
static struct class_attribute class_attr_usb_icl_override_ma = {
	.attr = {
		.name = "usb_icl_override_ma",
		.mode = 0660,
	},
	.show = usb_icl_override_ma_show,
	.store = usb_icl_override_ma_store,
};

static ssize_t ignore_vendor_charging_thermal_show(struct class *c,
						   struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(ignore_vendor_charging_thermal));
}

static ssize_t ignore_vendor_charging_thermal_store(struct class *c,
						    struct class_attribute *attr,
						    const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	set_ignore_vendor_charging_thermal_val(val);
	return count;
}
static struct class_attribute class_attr_ignore_vendor_charging_thermal = {
	.attr = {
		.name = "ignore_vendor_charging_thermal",
		.mode = 0660,
	},
	.show = ignore_vendor_charging_thermal_show,
	.store = ignore_vendor_charging_thermal_store,
};

static ssize_t vendor_thermal_max_level_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(vendor_thermal_max_level));
}

static ssize_t vendor_thermal_max_level_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	set_vendor_thermal_max_level_val(val);
	return count;
}
static struct class_attribute class_attr_vendor_thermal_max_level = {
	.attr = {
		.name = "vendor_thermal_max_level",
		.mode = 0660,
	},
	.show = vendor_thermal_max_level_show,
	.store = vendor_thermal_max_level_store,
};

static ssize_t ignore_vendor_cpu_thermal_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(ignore_vendor_cpu_thermal));
}

static ssize_t ignore_vendor_cpu_thermal_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	set_ignore_vendor_cpu_thermal_val(val);
	return count;
}
static struct class_attribute class_attr_ignore_vendor_cpu_thermal = {
	.attr = {
		.name = "ignore_vendor_cpu_thermal",
		.mode = 0660,
	},
	.show = ignore_vendor_cpu_thermal_show,
	.store = ignore_vendor_cpu_thermal_store,
};

static ssize_t cpu_little_override_max_state_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(cpu_little_override_max_state));
}

static ssize_t cpu_little_override_max_state_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	set_cpu_little_override_max_state_val(val);
	return count;
}
static struct class_attribute class_attr_cpu_little_override_max_state = {
	.attr = {
		.name = "cpu_little_override_max_state",
		.mode = 0660,
	},
	.show = cpu_little_override_max_state_show,
	.store = cpu_little_override_max_state_store,
};

static ssize_t cpu_big_override_max_state_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(cpu_big_override_max_state));
}

static ssize_t cpu_big_override_max_state_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	set_cpu_big_override_max_state_val(val);
	return count;
}
static struct class_attribute class_attr_cpu_big_override_max_state = {
	.attr = {
		.name = "cpu_big_override_max_state",
		.mode = 0660,
	},
	.show = cpu_big_override_max_state_show,
	.store = cpu_big_override_max_state_store,
};

static ssize_t skin_override_max_mc_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(skin_override_max_mc));
}

static ssize_t skin_override_max_mc_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	set_skin_override_max_mc_val(val);
	return count;
}
static struct class_attribute class_attr_skin_override_max_mc = {
	.attr = {
		.name = "skin_override_max_mc",
		.mode = 0660,
	},
	.show = skin_override_max_mc_show,
	.store = skin_override_max_mc_store,
};

static ssize_t ignore_vendor_gpu_thermal_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(ignore_vendor_gpu_thermal));
}

static ssize_t ignore_vendor_gpu_thermal_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	set_ignore_vendor_gpu_thermal_val(val);
	return count;
}
static struct class_attribute class_attr_ignore_vendor_gpu_thermal = {
	.attr = {
		.name = "ignore_vendor_gpu_thermal",
		.mode = 0660,
	},
	.show = ignore_vendor_gpu_thermal_show,
	.store = ignore_vendor_gpu_thermal_store,
};

static ssize_t gpu_override_max_state_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(gpu_override_max_state));
}

static ssize_t gpu_override_max_state_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	set_gpu_override_max_state_val(val);
	return count;
}
static struct class_attribute class_attr_gpu_override_max_state = {
	.attr = {
		.name = "gpu_override_max_state",
		.mode = 0660,
	},
	.show = gpu_override_max_state_show,
	.store = gpu_override_max_state_store,
};

static ssize_t ignore_vendor_backlight_thermal_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(ignore_vendor_backlight_thermal));
}

static ssize_t ignore_vendor_backlight_thermal_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	set_ignore_vendor_backlight_thermal_val(val);
	return count;
}
static struct class_attribute class_attr_ignore_vendor_backlight_thermal = {
	.attr = {
		.name = "ignore_vendor_backlight_thermal",
		.mode = 0660,
	},
	.show = ignore_vendor_backlight_thermal_show,
	.store = ignore_vendor_backlight_thermal_store,
};

static ssize_t battery_trip_decic_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(battery_trip_decic));
}

static ssize_t battery_trip_decic_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	set_battery_trip_decic_val(val);
	return count;
}
static struct class_attribute class_attr_battery_trip_decic = {
	.attr = {
		.name = "battery_trip_decic",
		.mode = 0660,
	},
	.show = battery_trip_decic_show,
	.store = battery_trip_decic_store,
};

static ssize_t battery_clear_decic_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(battery_clear_decic));
}

static ssize_t battery_clear_decic_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	set_battery_clear_decic_val(val);
	return count;
}
static struct class_attribute class_attr_battery_clear_decic = {
	.attr = {
		.name = "battery_clear_decic",
		.mode = 0660,
	},
	.show = battery_clear_decic_show,
	.store = battery_clear_decic_store,
};

static int parse_next_int(const char **str_ptr, int *val)
{
	const char *p = *str_ptr;
	char token[16];
	size_t len = 0;
	int parsed;

	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '\0' || *p == '\n' || *p == '\r')
		return -EINVAL;

	if (*p == '-')
		return -EINVAL;

	while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
		if (len >= sizeof(token) - 1)
			return -EINVAL;
		token[len++] = *p++;
	}
	token[len] = '\0';

	if (kstrtoint(token, 10, &parsed) < 0)
		return -EINVAL;

	if (parsed < 0 || parsed > 100)
		return -EINVAL;

	*str_ptr = p;
	*val = parsed;
	return 0;
}

static bool is_trailing_clean(const char *p)
{
	while (*p) {
		if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
			return false;
		p++;
	}
	return true;
}

static int wt_chg_apply_limits_locked(struct batt_chg *chg, int new_stop, int new_resume)
{
	if (READ_ONCE(chg->shutdown_flag))
		return -ENODEV;

	/* Disable path: writing 0 or 100 stores 0 (reads back 0/0) */
	if (new_stop == 0 || new_stop == 100) {
		WRITE_ONCE(chg->charge_stop_pct, 0);
		WRITE_ONCE(chg->charge_resume_pct, 0);
		wt_chg_set_limit_state_locked(chg, 0);
		chg->limit_debounce_cnt = 0;
		chg->limit_invalid_soc_cnt = 0;
		if (chg->limit_hold) {
			chg->limit_hold = false;
			if (!READ_ONCE(chg->bypass_mode) && chg->real_type && !chg->high_temp_flag)
				sw_battery_jeita(chg);
			power_supply_changed(chg->batt_psy);
			pr_info("wt_chg: charge limit disabled, hold released (ui_soc=%d%%, cap=%d%%, cc_soc=%d.%02d%%)\n",
				chg->ui_soc, chg->limit_raw_cap,
				chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc / 100 : 0,
				chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc % 100 : 0);
		}
		cancel_delayed_work(&chg->charge_limit_work);
		return 0;
	}

	/* Range & relationship validation */
	if (new_stop < 50 || new_stop > 99)
		return -EINVAL;

	if (new_resume < 20 || new_resume >= new_stop || (new_stop - new_resume) < 2)
		return -EINVAL;

	WRITE_ONCE(chg->charge_stop_pct, new_stop);
	WRITE_ONCE(chg->charge_resume_pct, new_resume);

	/*
	 * Case 3: Thresholds changed while holding or charging.
	 * If already holding and current SoC is still >= new_stop, maintain hold
	 * seamlessly without dropping PPS or pulsing bypass.
	 * Otherwise, release hold and transition to charging.
	 */
	if (chg->limit_hold) {
		int cur_soc_pct = -1, cur_soc_bp = -1;
		bool valid = wt_chg_get_limit_soc(chg, &cur_soc_pct, &cur_soc_bp);

		if (valid && cur_soc_pct >= new_stop) {
			wt_chg_set_limit_state_locked(chg, 2);
			chg->limit_debounce_cnt = 0;
			pr_info("wt_chg: charge limit thresholds updated (%d/%d), maintaining hold (ui_soc=%d%%, cap=%d%%, cc_soc=%d.%02d%%)\n",
				new_stop, new_resume, chg->ui_soc, chg->limit_raw_cap,
				chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc / 100 : 0,
				chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc % 100 : 0);
		} else {
			chg->limit_hold = false;
			wt_chg_set_limit_state_locked(chg, 1);
			chg->limit_debounce_cnt = 0;
			if (!READ_ONCE(chg->bypass_mode) && chg->real_type && !chg->high_temp_flag)
				sw_battery_jeita(chg);
			power_supply_changed(chg->batt_psy);
			pr_info("wt_chg: charge limit thresholds updated (%d/%d), resuming charging (ui_soc=%d%%, cap=%d%%, cc_soc=%d.%02d%%)\n",
				new_stop, new_resume, chg->ui_soc, chg->limit_raw_cap,
				chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc / 100 : 0,
				chg->limit_raw_cc_soc > 0 ? chg->limit_raw_cc_soc % 100 : 0);
		}
	} else {
		wt_chg_set_limit_state_locked(chg, 1);
		chg->limit_debounce_cnt = 0;
	}

	chg->limit_invalid_soc_cnt = 0;

	/* Perform sample 1 immediately */
	wt_chg_evaluate_charge_limit_locked(chg);

	/* Reschedule non-deferrable work queue for sample 2 or regular polling */
	cancel_delayed_work(&chg->charge_limit_work);
	if (!READ_ONCE(chg->shutdown_flag) && wt_chg_is_cable_plugged(chg)) {
		int interval_ms = (chg->limit_debounce_cnt > 0) ? 500 : 2000;
		schedule_delayed_work(&chg->charge_limit_work, msecs_to_jiffies(interval_ms));
	}
	return 0;
}

static ssize_t charge_stop_pct_show(struct class *c,
				    struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);

	if (!chg)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(chg->charge_stop_pct));
}

static ssize_t charge_stop_pct_store(struct class *c,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);
	const char *ptr = buf;
	int val1 = -1, val2 = -1;
	int rc, stop, resume;
	bool has_second = false;

	if (!chg || READ_ONCE(chg->shutdown_flag))
		return -ENODEV;

	rc = parse_next_int(&ptr, &val1);
	if (rc < 0)
		return -EINVAL;

	while (*ptr == ' ' || *ptr == '\t')
		ptr++;

	if (*ptr != '\0' && *ptr != '\n' && *ptr != '\r') {
		rc = parse_next_int(&ptr, &val2);
		if (rc < 0)
			return -EINVAL;
		has_second = true;
	}

	if (!is_trailing_clean(ptr))
		return -EINVAL;

	mutex_lock(&chg->limit_lock);
	if (READ_ONCE(chg->shutdown_flag)) {
		mutex_unlock(&chg->limit_lock);
		return -ENODEV;
	}

	stop = val1;
	if (has_second) {
		resume = val2;
	} else if (stop == 0 || stop == 100) {
		resume = 0;
	} else {
		int cur_resume = READ_ONCE(chg->charge_resume_pct);
		resume = (cur_resume == 0) ? max(20, stop - 5) : cur_resume;
	}

	rc = wt_chg_apply_limits_locked(chg, stop, resume);
	mutex_unlock(&chg->limit_lock);

	return rc < 0 ? rc : count;
}
static struct class_attribute class_attr_charge_stop_pct = {
	.attr = {
		.name = "charge_stop_pct",
		.mode = 0644,
	},
	.show = charge_stop_pct_show,
	.store = charge_stop_pct_store,
};

static ssize_t charge_resume_pct_show(struct class *c,
				      struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);

	if (!chg)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(chg->charge_resume_pct));
}

static ssize_t charge_resume_pct_store(struct class *c,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);
	const char *ptr = buf;
	int val1 = -1, val2 = -1;
	int rc, stop, resume;
	bool has_second = false;

	if (!chg || READ_ONCE(chg->shutdown_flag))
		return -ENODEV;

	rc = parse_next_int(&ptr, &val1);
	if (rc < 0)
		return -EINVAL;

	while (*ptr == ' ' || *ptr == '\t')
		ptr++;

	if (*ptr != '\0' && *ptr != '\n' && *ptr != '\r') {
		rc = parse_next_int(&ptr, &val2);
		if (rc < 0)
			return -EINVAL;
		has_second = true;
	}

	if (!is_trailing_clean(ptr))
		return -EINVAL;

	mutex_lock(&chg->limit_lock);
	if (READ_ONCE(chg->shutdown_flag)) {
		mutex_unlock(&chg->limit_lock);
		return -ENODEV;
	}

	if (has_second) {
		stop = val1;
		resume = val2;
	} else {
		stop = READ_ONCE(chg->charge_stop_pct);
		/* Single write to resume node cannot enable feature if disabled */
		if (stop == 0 || stop == 100) {
			mutex_unlock(&chg->limit_lock);
			return -EINVAL;
		}
		resume = val1;
	}

	rc = wt_chg_apply_limits_locked(chg, stop, resume);
	mutex_unlock(&chg->limit_lock);

	return rc < 0 ? rc : count;
}
static struct class_attribute class_attr_charge_resume_pct = {
	.attr = {
		.name = "charge_resume_pct",
		.mode = 0644,
	},
	.show = charge_resume_pct_show,
	.store = charge_resume_pct_store,
};

static ssize_t charge_limit_state_show(struct class *c,
				       struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);

	if (!chg)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(chg->charge_limit_state));
}
static struct class_attribute class_attr_charge_limit_state = {
	.attr = {
		.name = "charge_limit_state",
		.mode = 0444,
	},
	.show = charge_limit_state_show,
};

#define VECTOR_ATTR_RO(_name) \
static ssize_t vector_##_name##_show(struct kobject *kobj, \
				     struct kobj_attribute *attr, char *buf) \
{ \
	struct batt_chg *chg = READ_ONCE(g_batt_chg); \
	if (!chg) \
		return -ENODEV; \
	return _name##_show(&chg->battery_class, &class_attr_##_name, buf); \
} \
static struct kobj_attribute vector_attr_##_name = { \
	.attr = { .name = #_name, .mode = 0444 }, \
	.show = vector_##_name##_show, \
};

#define VECTOR_ATTR_RW(_name) \
static ssize_t vector_##_name##_show(struct kobject *kobj, \
				     struct kobj_attribute *attr, char *buf) \
{ \
	struct batt_chg *chg = READ_ONCE(g_batt_chg); \
	if (!chg) \
		return -ENODEV; \
	return _name##_show(&chg->battery_class, &class_attr_##_name, buf); \
} \
static ssize_t vector_##_name##_store(struct kobject *kobj, \
				      struct kobj_attribute *attr, \
				      const char *buf, size_t count) \
{ \
	struct batt_chg *chg = READ_ONCE(g_batt_chg); \
	if (!chg) \
		return -ENODEV; \
	return _name##_store(&chg->battery_class, &class_attr_##_name, buf, count); \
} \
static struct kobj_attribute vector_attr_##_name = { \
	.attr = { .name = #_name, .mode = 0660 }, \
	.show = vector_##_name##_show, \
	.store = vector_##_name##_store, \
};

VECTOR_ATTR_RW(charge_stop_pct);
VECTOR_ATTR_RW(charge_resume_pct);
VECTOR_ATTR_RO(charge_limit_state);

VECTOR_ATTR_RW(bypass_charger);
VECTOR_ATTR_RW(usb_icl_override_ma);
VECTOR_ATTR_RW(ignore_vendor_charging_thermal);
VECTOR_ATTR_RW(vendor_thermal_max_level);
VECTOR_ATTR_RW(ignore_vendor_cpu_thermal);
VECTOR_ATTR_RW(cpu_little_override_max_state);
VECTOR_ATTR_RW(cpu_big_override_max_state);
VECTOR_ATTR_RW(skin_override_max_mc);
VECTOR_ATTR_RW(ignore_vendor_gpu_thermal);
VECTOR_ATTR_RW(gpu_override_max_state);
VECTOR_ATTR_RW(ignore_vendor_backlight_thermal);
VECTOR_ATTR_RW(battery_trip_decic);
VECTOR_ATTR_RW(battery_clear_decic);

static struct attribute *vector_attrs[] = {
	&vector_attr_charge_stop_pct.attr,
	&vector_attr_charge_resume_pct.attr,
	&vector_attr_charge_limit_state.attr,
	&vector_attr_bypass_charger.attr,
	&vector_attr_usb_icl_override_ma.attr,
	&vector_attr_ignore_vendor_charging_thermal.attr,
	&vector_attr_vendor_thermal_max_level.attr,
	&vector_attr_ignore_vendor_cpu_thermal.attr,
	&vector_attr_cpu_little_override_max_state.attr,
	&vector_attr_cpu_big_override_max_state.attr,
	&vector_attr_skin_override_max_mc.attr,
	&vector_attr_ignore_vendor_gpu_thermal.attr,
	&vector_attr_gpu_override_max_state.attr,
	&vector_attr_ignore_vendor_backlight_thermal.attr,
	&vector_attr_battery_trip_decic.attr,
	&vector_attr_battery_clear_decic.attr,
	NULL,
};

static const struct attribute_group vector_attr_group = {
	.attrs = vector_attrs,
};

static struct attribute *battery_class_attrs[] = {
	&class_attr_soc_decimal.attr,
	&class_attr_shutdown_delay.attr,
	&class_attr_soc_decimal_rate.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_real_type.attr,
	&class_attr_pd_auth.attr,
	&class_attr_cell_voltage.attr,
	&class_attr_vbus_voltage.attr,
	&class_attr_cp_vbus_voltage.attr,
	&class_attr_input_current.attr,
	&class_attr_cp_ibus_slave.attr,
	&class_attr_cp_ibus_master.attr,
	&class_attr_cp_present_slave.attr,
	&class_attr_cp_present_master.attr,
	&class_attr_cp_temp_slave.attr,
	&class_attr_cp_temp_master.attr,
	&class_attr_charging_call_state.attr,
#ifdef CONFIG_WT_QGKI
	&class_attr_verify_digest.attr,
#endif
	&class_attr_authentic.attr,
	&class_attr_battery_name.attr,
	&class_attr_chip_ok.attr,
	&class_attr_typec_cc_orientation.attr,
	&class_attr_stopcharging_test.attr,
	&class_attr_startcharging_test.attr,
	&class_attr_input_suspend.attr,
	&class_attr_quick_charge_type.attr,
	&class_attr_apdo_max.attr,
	&class_attr_typec_mode.attr,
	&class_attr_mtbf_current.attr,
	&class_attr_fastcharge_mode.attr,
	&class_attr_soh.attr,
	&class_attr_resistance.attr,
	&class_attr_connector_temp.attr,
	&class_attr_bypass_charger.attr,
	&class_attr_usb_icl_override_ma.attr,
	&class_attr_ignore_vendor_charging_thermal.attr,
	&class_attr_vendor_thermal_max_level.attr,
	&class_attr_ignore_vendor_cpu_thermal.attr,
	&class_attr_cpu_little_override_max_state.attr,
	&class_attr_cpu_big_override_max_state.attr,
	&class_attr_skin_override_max_mc.attr,
	&class_attr_ignore_vendor_gpu_thermal.attr,
	&class_attr_gpu_override_max_state.attr,
	&class_attr_ignore_vendor_backlight_thermal.attr,
	&class_attr_battery_trip_decic.attr,
	&class_attr_battery_clear_decic.attr,
	&class_attr_charge_stop_pct.attr,
	&class_attr_charge_resume_pct.attr,
	&class_attr_charge_limit_state.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_class);

#ifdef CONFIG_WT_QGKI
static ssize_t dev_verify_digest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	get_verify_digest(buf);

	return scnprintf(buf, PAGE_SIZE, "%s", buf);
}

static ssize_t dev_verify_digest_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char kbuf[70] = {0};
	pr_err("verify_digest_store = %s\n", buf);
	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, buf, count - 1);

	set_verify_digest(kbuf);
	return count;
}

static DEVICE_ATTR(verify_digest, 0660, dev_verify_digest_show, dev_verify_digest_store);
#endif
static ssize_t dev_real_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);
	int val;
	int type, real_type;

	if (!chg)
		return -ENODEV;

	batt_get_iio_channel(chg, MAIN, MAIN_CHARGER_TYPE, &type);
	real_type = get_usb_real_type(chg);

	val = real_type - 3;

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
}
static DEVICE_ATTR(real_type, 0664, dev_real_type_show, NULL);

static ssize_t dev_pd_auth_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);

	if (!chg)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->pd_auth);
}

static ssize_t dev_pd_auth_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);
	int val;

	if (!chg)
		return -ENODEV;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->pd_auth = val;
	pr_err("g_batt_chg->pd_auth enable= %d\n", chg->pd_auth);

	return count;
}
static DEVICE_ATTR(pd_auth, 0664, dev_pd_auth_show, dev_pd_auth_store);

static ssize_t dev_authentic_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);

	if (!chg)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->batt_auth);
}

static ssize_t dev_authentic_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);
	int val;

	if (!chg)
		return -ENODEV;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->batt_auth = val;
	pr_err("g_batt_chg->batt_auth enable= %d\n", chg->batt_auth);

	return count;
}
static DEVICE_ATTR(authentic, 0664, dev_authentic_show, dev_authentic_store);

static ssize_t dev_input_suspend_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);

	if (!chg)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%d", chg->input_suspend);
}

static ssize_t dev_input_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct batt_chg *chg = READ_ONCE(g_batt_chg);
	int val;

	if (!chg)
		return -ENODEV;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->input_suspend = val;
	pr_err("dev chg->input_suspend = %d\n", chg->input_suspend);
	if(val == 1) {
		chg->input_suspend  = 1;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, chg->input_suspend);
	//	chg->input_suspend = 0;
	//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, chg->input_suspend);
	//	chg->input_suspend  = 1;
		chg->mishow_flag = true;
	}
	if(val == 0) {
		chg->input_suspend = 0;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
	//	chg->input_suspend = 1;
	//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, val);
	//	chg->input_suspend = 0;
		chg->mishow_flag = false;
	}
	power_supply_changed(chg->usb_psy);
	power_supply_changed(chg->batt_psy);
	return count;
}
static DEVICE_ATTR(input_suspend, 0664, dev_input_suspend_show, dev_input_suspend_store);

static struct attribute *battery_attributes[] = {
#ifdef CONFIG_WT_QGKI
	&dev_attr_verify_digest.attr,
#endif
	&dev_attr_real_type.attr,
	&dev_attr_pd_auth.attr,
	&dev_attr_authentic.attr,
	&dev_attr_input_suspend.attr,
	NULL,
};

static const struct attribute_group battery_attr_group = {
	.attrs = battery_attributes,
};

static const struct attribute_group *battery_attr_groups[] = {
	&battery_attr_group,
	NULL,
};

static void wt_cleanup_vector_sysfs(void)
{
	if (vector_kobj) {
		sysfs_remove_group(vector_kobj, &vector_attr_group);
		kobject_put(vector_kobj);
		vector_kobj = NULL;
	}
}

static int wt_init_dev_class(struct batt_chg *chg)
{
	int rc = -EINVAL;
	if(!chg)
		return rc;

	chg->battery_class.name = "qcom-battery";
	chg->battery_class.class_groups = battery_class_groups;
	rc = class_register(&chg->battery_class);
	if (rc < 0) {
		pr_err("Failed to create battery_class rc=%d\n", rc);
		return rc;
	}

	vector_kobj = kobject_create_and_add("vector", kernel_kobj);
	if (!vector_kobj) {
		pr_err("wt_chg: failed to create vector kobject\n");
	} else {
		int v_rc = sysfs_create_group(vector_kobj, &vector_attr_group);
		if (v_rc) {
			pr_err("wt_chg: failed to create vector attribute group: %d\n", v_rc);
			kobject_put(vector_kobj);
			vector_kobj = NULL;
		}
	}

	chg->batt_device.class = &chg->battery_class;
	dev_set_name(&chg->batt_device, "odm_battery");
	chg->batt_device.parent = chg->dev;
	chg->batt_device.groups = battery_attr_groups;
	rc = device_register(&chg->batt_device);
	if (rc < 0) {
		pr_err("Failed to create odm_battery rc=%d\n", rc);
		wt_cleanup_vector_sysfs();
		class_unregister(&chg->battery_class);
		return rc;
	}

	return 0;
}

static int batt_node_map(struct batt_chg *chg)
{
	int i;
	if(!chg)
		return -ENOMEM;

	chg->psy_list[PSY_TYPE_BATTERY].map = battery_prop_map;
	chg->psy_list[PSY_TYPE_BATTERY].prop_count = BATT_PROP_MAX;
	chg->psy_list[PSY_TYPE_BATTERY].opcode_get = BC_BATTERY_STATUS_GET;
	chg->psy_list[PSY_TYPE_BATTERY].opcode_set = BC_BATTERY_STATUS_SET;
	chg->psy_list[PSY_TYPE_USB].map = usb_prop_map;
	chg->psy_list[PSY_TYPE_USB].prop_count = USB_PROP_MAX;
	chg->psy_list[PSY_TYPE_USB].opcode_get = BC_USB_STATUS_GET;
	chg->psy_list[PSY_TYPE_USB].opcode_set = BC_USB_STATUS_SET;
	chg->psy_list[PSY_TYPE_XM].prop_count = XM_PROP_MAX;
	chg->psy_list[PSY_TYPE_XM].opcode_get = BC_XM_STATUS_GET;
	chg->psy_list[PSY_TYPE_XM].opcode_set = BC_XM_STATUS_SET;

	for (i = 0; i < PSY_TYPE_MAX; i++) {
		chg->psy_list[i].prop =
			devm_kcalloc(chg->dev, chg->psy_list[i].prop_count,
					sizeof(u32), GFP_KERNEL);
		if (!chg->psy_list[i].prop)
			return -ENOMEM;
	}

	chg->psy_list[PSY_TYPE_BATTERY].model =
		devm_kzalloc(chg->dev, MAX_STR_LEN, GFP_KERNEL);
	if (!chg->psy_list[PSY_TYPE_BATTERY].model)
		return -ENOMEM;

	pr_err("wt_node_map start\n");
	return 0;
}

#define MAX_UEVENT_LENGTH 50
static void generate_xm_charge_uvent(struct work_struct *work)
{
	int count;
	struct batt_chg *chg = container_of(work, struct batt_chg, xm_prop_change_work.work);

	if (!chg || READ_ONCE(chg->shutdown_flag))
		return;

	static char uevent_string[][MAX_UEVENT_LENGTH+1] = {
		"POWER_SUPPLY_SOC_DECIMAL=\n",	//length=31+8
		"POWER_SUPPLY_SOC_DECIMAL_RATE=\n",	//length=31+8
		"POWER_SUPPLY_SHUTDOWN_DELAY=\n",//28+8
	};
	static char *envp[] = {
		uevent_string[0],
		uevent_string[1],
		uevent_string[2],

		NULL,

	};
	char *prop_buf = NULL;

	count = chg->update_cont;
	if(chg->update_cont < 0)
		return;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return;

	soc_decimal_show( &(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[0]+25, prop_buf,MAX_UEVENT_LENGTH-25);

	soc_decimal_rate_show( &(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[1]+30, prop_buf,MAX_UEVENT_LENGTH-30);

	shutdown_delay_show( &(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[2]+28, prop_buf,MAX_UEVENT_LENGTH-28);

	pr_err("uevent test : %s\n %s\n %s count %d\n",
			envp[0], envp[1], envp[2], count);

	/*add our prop end*/

	kobject_uevent_env(&chg->dev->kobj, KOBJ_CHANGE, envp);

	free_page((unsigned long)prop_buf);
	chg->update_cont = count - 1;
	if (!READ_ONCE(chg->shutdown_flag)) {
		if(chg->ui_soc > 1)
			schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(500));
		else
			schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(2000));
	}
	return;
}

#define CHARGING_PERIOD_S		30
#define DISCHARGE_PERIOD_S		300
static void xm_charger_debug_info_print_work(struct work_struct *work)
{
	struct batt_chg *chg = container_of(work, struct batt_chg, charger_debug_info_print_work.work);
	int interval = DISCHARGE_PERIOD_S;
	static int update_cnt_old;

	if (!chg || READ_ONCE(chg->shutdown_flag))
		return;

	if(chg->start_cnt != update_cnt_old) {
		update_cnt_old = chg->start_cnt;
	} else {
		cancel_delayed_work(&chg->batt_chg_work);
		schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(100));
	}
	pr_err("update_cnt_old %d\n", update_cnt_old);
	if (!READ_ONCE(chg->shutdown_flag))
		schedule_delayed_work(&chg->charger_debug_info_print_work, interval * HZ);
}

static int batt_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct batt_chg *chg = iio_priv(indio_dev);
	int rc = 0;

	if (!chg || READ_ONCE(chg->shutdown_flag))
		return -ENODEV;

	switch (chan->channel) {
	case PSY_IIO_PD_ACTIVE:
		chg->pd_active = val1;
		chg->update_cont = 15;
		batt_set_iio_channel(chg, MAIN, MAIN_INPUT_CURRENT_SETTLED, 512);
		cancel_delayed_work(&chg->xm_prop_change_work);
		schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(100));
		cancel_delayed_work(&chg->batt_chg_work);
		schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(1000));
		power_supply_changed(chg->usb_psy);
		power_supply_changed(chg->batt_psy);
		break;
	case PSY_IIO_PD_USB_SUSPEND_SUPPORTED:
		chg->pd_usb_suspend = val1;
		break;
	case PSY_IIO_PD_IN_HARD_RESET:
		chg->pd_in_hard_reset = val1;
		pr_err("chg->pd_in_hard_reset  %d\n", chg->pd_in_hard_reset);
		if(chg->pd_in_hard_reset)
			chg->pd_type_cnt = 0;
		break;
	case PSY_IIO_PD_CURRENT_MAX:
		chg->pd_cur_max = val1;
		break;
	case PSY_IIO_PD_VOLTAGE_MIN:
		chg->pd_min_vol = val1;
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		chg->pd_max_vol = val1;
		break;
	case PSY_IIO_OTG_ENABLE:
		chg->otg_enable = val1;
		batt_set_iio_channel(chg, MAIN, MAIN_OTG_ENABLE, val1);
		pr_err("enable otg %d\n", chg->otg_enable);
		break;
	case PSY_IIO_TYPEC_CC_ORIENTATION:
		chg->polarity_state = val1;
		break;
	case PSY_IIO_APDO_MAX_VOLT:
		chg->apdo_max_volt = val1;
		pr_err("set Achg->apdo_max_volt(%d)\n", chg->apdo_max_volt);
		break;
	case PSY_IIO_APDO_MAX_CURR:
		chg->apdo_max_curr = val1;
		pr_err("set Achg->apdo_max_curr(%d)\n", chg->apdo_max_curr);
		power_supply_changed(chg->batt_psy);
		break;
	case PSY_IIO_TYPEC_MODE:
		chg->typec_mode = val1;
		pr_err("chg->typec_mode(%d)\n", chg->typec_mode);
		break;
	default:
		pr_debug("Unsupported QG IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}
	if (rc < 0)
		pr_err_ratelimited("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int batt_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct batt_chg *chip = iio_priv(indio_dev);
	int rc = 0;

	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_PD_ACTIVE:
		*val1 = chip->pd_active;;
		break;
	case PSY_IIO_PD_USB_SUSPEND_SUPPORTED:
		*val1 = chip->pd_usb_suspend;
		break;
	case PSY_IIO_PD_IN_HARD_RESET:
		*val1 = chip->pd_in_hard_reset;
		break;
	case PSY_IIO_PD_CURRENT_MAX:
		*val1 = chip->pd_cur_max;
		break;
	case PSY_IIO_PD_VOLTAGE_MIN:
		*val1 = chip->pd_min_vol;
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		*val1 = chip->pd_max_vol;;
		break;
	case PSY_IIO_USB_REAL_TYPE:
		*val1 = get_real_type(chip);
		if(*val1 == 0) {
			chip->pd_type_cnt++;
		}
		break;
	case PSY_IIO_OTG_ENABLE:
		*val1 = chip->otg_enable;
		break;
	case PSY_IIO_TYPEC_CC_ORIENTATION:
		*val1 = chip->polarity_state;
		break;
	case PSY_IIO_APDO_MAX_VOLT:
		*val1 = chip->apdo_max_volt;
		break;
	case PSY_IIO_APDO_MAX_CURR:
		*val1 = chip->apdo_max_curr;
		break;

	default:
		pr_debug("Unsupported battery IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err_ratelimited("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int batt_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct batt_chg *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(battery_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info battery_iio_info = {
	.read_raw	= batt_iio_read_raw,
	.write_raw	= batt_iio_write_raw,
	.of_xlate	= batt_iio_of_xlate,
};

static int batt_init_iio_psy(struct batt_chg *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(battery_iio_psy_channels);
	int rc, i;

	pr_err("battery_init_iio_psy start\n");
	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);
	if (!chip->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &battery_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "batt_chg";
	for (i = 0; i < num_iio_channels; i++) {
		chip->int_iio_chans[i].indio_dev = indio_dev;
		chan = &chip->iio_chan[i];
		chip->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = battery_iio_psy_channels[i].channel_num;
		chan->type = battery_iio_psy_channels[i].type;
		chan->datasheet_name =
			battery_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			battery_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			battery_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register battery IIO device, rc=%d\n", rc);

	pr_err("battery IIO device, rc=%d\n", rc);
	return rc;
}

static int batt_ext_init_iio_psy(struct batt_chg *chip)
{
	if (!chip)
		return -ENOMEM;

	chip->gq_ext_iio_chans = devm_kcalloc(chip->dev,
				ARRAY_SIZE(qg_ext_iio_chan_name),
				sizeof(*chip->gq_ext_iio_chans),
				GFP_KERNEL);
	if (!chip->gq_ext_iio_chans)
		return -ENOMEM;

	chip->cp_ext_iio_chans = devm_kcalloc(chip->dev,
		ARRAY_SIZE(cp_iio_chan_name), sizeof(*chip->cp_ext_iio_chans), GFP_KERNEL);
	if (!chip->cp_ext_iio_chans)
		return -ENOMEM;

	chip->cp_psy_ext_iio_chans = devm_kcalloc(chip->dev,
		ARRAY_SIZE(cp_sec_iio_chan_name), sizeof(*chip->cp_psy_ext_iio_chans), GFP_KERNEL);
	if (!chip->cp_psy_ext_iio_chans)
		return -ENOMEM;

	chip->main_chg_ext_iio_chans = devm_kcalloc(chip->dev,
		ARRAY_SIZE(main_iio_chan_name), sizeof(*chip->main_chg_ext_iio_chans), GFP_KERNEL);
	if (!chip->main_chg_ext_iio_chans)
		return -ENOMEM;

	return 0;
}

static int batt_chg_probe(struct platform_device *pdev)
{
	struct batt_chg *batt_chg = NULL;
	struct iio_dev *indio_dev;
	int rc;

	if (pdev->dev.of_node) {
		indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(struct batt_chg));
		if (!indio_dev) {
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	} else {
		return -ENODEV;
	}

	batt_chg = iio_priv(indio_dev);
	batt_chg->indio_dev = indio_dev;
	batt_chg->dev = &pdev->dev;
	batt_chg->pdev = pdev;
	platform_set_drvdata(pdev, batt_chg);

	mutex_init(&batt_chg->bypass_lock);
	batt_chg->bypass_mode = 0;
	mutex_init(&batt_chg->icl_lock);
	batt_chg->usb_icl_override_ma = 0;
	mutex_init(&batt_chg->limit_lock);
	batt_chg->charge_stop_pct = 0;
	batt_chg->charge_resume_pct = 0;
	batt_chg->limit_hold = false;
	batt_chg->charge_limit_state = 0;
	batt_chg->limit_debounce_cnt = 0;
	batt_chg->limit_invalid_soc_cnt = 0;
	batt_chg->limit_raw_cc_soc = -1;
	batt_chg->limit_raw_cap = -1;
	batt_chg->last_debounce_sample_time = 0;
	INIT_DELAYED_WORK(&batt_chg->charge_limit_work, wt_chg_charge_limit_work_fn);
	INIT_DEFERRABLE_WORK(&batt_chg->batt_chg_work, batt_chg_main);
	INIT_DEFERRABLE_WORK(&batt_chg->usb_therm_work, usb_therm);
	INIT_DEFERRABLE_WORK(&batt_chg->usb_type_work, batt_usb_type);
	INIT_DEFERRABLE_WORK(&batt_chg->xm_prop_change_work, generate_xm_charge_uvent);
	INIT_DEFERRABLE_WORK(&batt_chg->charger_debug_info_print_work, xm_charger_debug_info_print_work);
	g_batt_chg = batt_chg;

	INIT_DELAYED_WORK(&thermal_override_poll_work, thermal_override_poll_work_fn);
	INIT_WORK(&thermal_override_reapply_work, thermal_override_reapply_work_fn);
	smp_store_release(&thermal_override_initialized, true);
	if (thermal_override_poll_needed())
		schedule_delayed_work(&thermal_override_poll_work, 0);

	rc = batt_init_iio_psy(batt_chg);
	if (rc < 0) {
		pr_err("Failed to initialize QG IIO PSY, rc=%d\n", rc);
	}

	rc = batt_ext_init_iio_psy(batt_chg);
	if (rc < 0) {
		pr_err("Failed to initialize QG IIO PSY, rc=%d\n", rc);
	}

	rc = batt_parse_dt(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
#ifdef CONFIG_WT_QGKI
		goto cleanup;
#endif
	}

	rc = batt_init_config(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		goto cleanup;
	}

	rc = batt_node_map(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		goto cleanup;
	}

	platform_set_drvdata(pdev, batt_chg);

	rc = wt_init_batt_psy(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = wt_init_usb_psy(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = wt_init_dev_class(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}

	batt_chg->wt_ws = wakeup_source_register(batt_chg->dev, "charge_wakeup");
	if (!batt_chg->wt_ws) {
		pr_err("wt chg workup fail!\n");
		wakeup_source_unregister(batt_chg->wt_ws);
	}

	schedule_delayed_work(&batt_chg->batt_chg_work, 0);
	schedule_delayed_work(&batt_chg->charger_debug_info_print_work, 30 * HZ);
	schedule_delayed_work(&batt_chg->xm_prop_change_work, msecs_to_jiffies(30000));
	schedule_delayed_work(&batt_chg->usb_therm_work, msecs_to_jiffies(10000));

	pr_err("batt_chg probe success\n");
	return 0;

cleanup:
	smp_store_release(&thermal_override_initialized, false);
	cancel_delayed_work_sync(&thermal_override_poll_work);
	cancel_work_sync(&thermal_override_reapply_work);
	if (batt_chg) {
		bool was_held;

		mutex_lock(&batt_chg->bypass_lock);
		was_held = batt_chg->bypass_mode != 0;
		batt_chg->bypass_mode = 0;
		mutex_unlock(&batt_chg->bypass_lock);

		mutex_lock(&batt_chg->limit_lock);
		was_held = was_held || batt_chg->limit_hold;
		batt_chg->limit_hold = false;
		batt_chg->charge_stop_pct = 0;
		batt_chg->charge_resume_pct = 0;
		batt_chg->charge_limit_state = 0;
		if (was_held)
			sw_battery_jeita(batt_chg);
		WRITE_ONCE(batt_chg->shutdown_flag, true);
		mutex_unlock(&batt_chg->limit_lock);

		cancel_delayed_work_sync(&batt_chg->batt_chg_work);
		cancel_delayed_work_sync(&batt_chg->charge_limit_work);
		cancel_delayed_work_sync(&batt_chg->charger_debug_info_print_work);
		cancel_delayed_work_sync(&batt_chg->xm_prop_change_work);
		cancel_delayed_work_sync(&batt_chg->usb_therm_work);
		cancel_delayed_work_sync(&batt_chg->usb_type_work);
	}
	g_batt_chg = NULL;
	wt_cleanup_vector_sysfs();
	pr_err("batt_chg probe fail\n");
	return rc;
}

static int batt_chg_remove(struct platform_device *pdev)
{
	struct batt_chg *chg = platform_get_drvdata(pdev);

	smp_store_release(&thermal_override_initialized, false);
	cancel_delayed_work_sync(&thermal_override_poll_work);
	cancel_work_sync(&thermal_override_reapply_work);
	if (chg) {
		mutex_lock(&chg->bypass_lock);
		chg->bypass_mode = 0;
		mutex_unlock(&chg->bypass_lock);

		mutex_lock(&chg->limit_lock);
		chg->limit_hold = false;
		chg->charge_stop_pct = 0;
		chg->charge_resume_pct = 0;
		chg->charge_limit_state = 0;
		sw_battery_jeita(chg);
		WRITE_ONCE(chg->shutdown_flag, true);
		mutex_unlock(&chg->limit_lock);

		cancel_delayed_work_sync(&chg->batt_chg_work);
		cancel_delayed_work_sync(&chg->charge_limit_work);
		cancel_delayed_work_sync(&chg->charger_debug_info_print_work);
		cancel_delayed_work_sync(&chg->xm_prop_change_work);
		cancel_delayed_work_sync(&chg->usb_therm_work);
		cancel_delayed_work_sync(&chg->usb_type_work);
	}
	g_batt_chg = NULL;
	wt_cleanup_vector_sysfs();
	return 0;
}

static void batt_chg_shutdown(struct platform_device *pdev)
{
	struct batt_chg *chg = platform_get_drvdata(pdev);

	pr_err("%s batt_chg_shutdown\n", __func__);
	smp_store_release(&thermal_override_initialized, false);
	cancel_delayed_work_sync(&thermal_override_poll_work);
	cancel_work_sync(&thermal_override_reapply_work);
	if (!chg)
		return;

	mutex_lock(&chg->bypass_lock);
	chg->bypass_mode = 0;
	mutex_unlock(&chg->bypass_lock);

	mutex_lock(&chg->limit_lock);
	chg->limit_hold = false;
	chg->charge_stop_pct = 0;
	chg->charge_resume_pct = 0;
	chg->charge_limit_state = 0;
	sw_battery_jeita(chg);
	WRITE_ONCE(chg->shutdown_flag, true);
	mutex_unlock(&chg->limit_lock);

	cancel_delayed_work_sync(&chg->batt_chg_work);
	cancel_delayed_work_sync(&chg->charge_limit_work);
	cancel_delayed_work_sync(&chg->charger_debug_info_print_work);
	cancel_delayed_work_sync(&chg->xm_prop_change_work);
	cancel_delayed_work_sync(&chg->usb_therm_work);
	cancel_delayed_work_sync(&chg->usb_type_work);

	g_batt_chg = NULL;
	return;
}

static const struct of_device_id batt_chg_dt_match[] = {
	{.compatible = "qcom,wt_chg"},
	{},
};

static struct platform_driver batt_chg_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "wt_chg",
		.of_match_table = batt_chg_dt_match,
	},
	.probe = batt_chg_probe,
	.remove = batt_chg_remove,
	.shutdown = batt_chg_shutdown,
};

static int __init batt_chg_init(void)
{
    platform_driver_register(&batt_chg_driver);
	pr_err("batt_chg init end\n");
    return 0;
}

static void __exit batt_chg_exit(void)
{
	pr_err("batt_chg exit\n");
	platform_driver_unregister(&batt_chg_driver);
}

module_init(batt_chg_init);
module_exit(batt_chg_exit);

MODULE_AUTHOR("WingTech Inc.");
MODULE_DESCRIPTION("battery driver");
MODULE_LICENSE("GPL");
