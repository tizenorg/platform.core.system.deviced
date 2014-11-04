#include <tet_api.h>
#include <devman.h>
#include <devman_haptic.h>

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SystemFW_device_haptic_open_func_01(void);
static void utc_SystemFW_device_haptic_open_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SystemFW_device_haptic_open_func_01, POSITIVE_TC_IDX },
	{ utc_SystemFW_device_haptic_open_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of device_haptic_open()
 */
static void utc_SystemFW_device_haptic_open_func_01(void)
{
	int r = 0;
	unsigned int mode =0;
	haptic_dev_idx dev_idx = DEV_IDX_0;

	r = device_haptic_open(dev_idx,mode);
	
	if (r < 0) {
		tet_infoline("device_haptic_open() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
	device_haptic_close(r);
}

/**
 * @brief Negative test case of ug_init device_haptic_open()
 */
static void utc_SystemFW_device_haptic_open_func_02(void)
{
	int r = 0;
	unsigned int mode =0;
	haptic_dev_idx dev_idx = 1000;
	
	r = device_haptic_open(dev_idx,mode);
	
	if (r >= 0) {
		tet_infoline("device_haptic_open() failed in negative test case");
		tet_result(TET_FAIL);
		device_haptic_close(r);
		return;
	}
	tet_result(TET_PASS);
}
