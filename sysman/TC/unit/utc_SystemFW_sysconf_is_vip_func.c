#include <tet_api.h>
#include <sysman.h>

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SystemFW_sysconf_is_vip_func_01(void);
static void utc_SystemFW_sysconf_is_vip_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SystemFW_sysconf_is_vip_func_01, POSITIVE_TC_IDX },
	{ utc_SystemFW_sysconf_is_vip_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of sysconf_is_vip()
 */
static void utc_SystemFW_sysconf_is_vip_func_01(void)
{
	int ret_val = 0;
	int pid = getpid();

	ret_val = sysconf_is_vip(pid);
	if(ret_val < 0) {
		tet_infoline("sysconf_is_vip() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init sysconf_is_vip()
 */
static void utc_SystemFW_sysconf_is_vip_func_02(void)
{
	int r = 0;

   	r = sysconf_is_vip(-1);

	if (r>=0) {
		tet_infoline("sysconf_is_vip() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}
