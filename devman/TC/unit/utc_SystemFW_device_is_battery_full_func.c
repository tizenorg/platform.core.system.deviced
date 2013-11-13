#include <tet_api.h>
#include <devman.h>

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SystemFW_device_is_battery_full_func_01(void);
//static void utc_SystemFW_device_is_battery_full_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SystemFW_device_is_battery_full_func_01, POSITIVE_TC_IDX },
//	{ utc_SystemFW_device_is_battery_full_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of device_is_battery_full()
 */
static void utc_SystemFW_device_is_battery_full_func_01(void)
{
		
	int r = 0;


   	r = device_is_battery_full();

	if (r<0) {
		tet_infoline("device_is_battery_full() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init device_is_battery_full()
 */
//static void utc_SystemFW_device_is_battery_full_func_02(void)
//{
//	int r = 0;
//
  // 	r = device_is_battery_full();
//
//	if (r>=0) {
//		tet_infoline("device_is_battery_full() failed in negative test case");
//		tet_result(TET_FAIL);
//		return;
//	}
//	tet_result(TET_PASS);
//}
