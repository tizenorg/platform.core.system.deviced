#include <tet_api.h>
#include <devman.h>

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SystemFW_device_release_gamma_ctrl_func_01(void);
static void utc_SystemFW_device_release_gamma_ctrl_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SystemFW_device_release_gamma_ctrl_func_01, POSITIVE_TC_IDX },
	{ utc_SystemFW_device_release_gamma_ctrl_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of device_release_gamma_ctrl()
 */
static void utc_SystemFW_device_release_gamma_ctrl_func_01(void)
{
	int ret_val = 0;
	display_num_t disp = DEV_DISPLAY_0;
	display_gamma_t org_val = 1;

	ret_val = device_release_gamma_ctrl(disp,org_val);
	if(ret_val < 0) {
		tet_infoline("device_release_gamma_ctrl() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init device_release_gamma_ctrl()
 */
static void utc_SystemFW_device_release_gamma_ctrl_func_02(void)
{
	int ret_val = 0;
	int org_brt = -1;

	display_num_t disp = -1;

	ret_val = device_release_gamma_ctrl(disp,org_brt);
	if (ret_val >= 0) {
		tet_infoline("device_release_gamma_ctrl() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}
