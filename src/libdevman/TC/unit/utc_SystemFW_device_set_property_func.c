#include <tet_api.h>
#include <devman.h>

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SystemFW_device_set_property_func_01(void);
static void utc_SystemFW_device_set_property_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SystemFW_device_set_property_func_01, POSITIVE_TC_IDX },
	{ utc_SystemFW_device_set_property_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of device_set_property()
 */
static void utc_SystemFW_device_set_property_func_01(void)
{
	int value = 6;
	int ret_val = 0;
	int property = DISPLAY_PROP_BRIGHTNESS;
	devtype_t devtype = DEVTYPE_DISPLAY0;

	//set brightness to 6
	ret_val = device_set_property(devtype,property,value);
	if((ret_val < 0) || (value == -1)) {
		tet_infoline("device_set_property() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init device_set_property()
 */
static void utc_SystemFW_device_set_property_func_02(void)
{
	int ret_val = 0;
	int property = DISPLAY_PROP_BRIGHTNESS; 
	devtype_t devtype = -1;

	ret_val = device_set_property(devtype,property,-1);                                                                  
	if(ret_val >= 0) {
		tet_infoline("device_set_property() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}
