#include <tet_api.h>
#include <sysman.h>

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SystemFW_sysconf_set_mempolicy_bypid_func_01(void);
static void utc_SystemFW_sysconf_set_mempolicy_bypid_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SystemFW_sysconf_set_mempolicy_bypid_func_01, POSITIVE_TC_IDX },
	{ utc_SystemFW_sysconf_set_mempolicy_bypid_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of sysconf_set_mempolicy_bypid()
 */
static void utc_SystemFW_sysconf_set_mempolicy_bypid_func_01(void)
{
	int ret_val = 0;
	int pid = getpid();
	int mempol = OOM_NORMAL;

	ret_val = sysconf_set_mempolicy_bypid(pid, mempol);
	if(ret_val < 0) {
		tet_infoline("sysconf_set_mempolicy_bypid() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init sysconf_set_mempolicy_bypid()
 */
static void utc_SystemFW_sysconf_set_mempolicy_bypid_func_02(void)
{
	int r = 0;
	int mempol = OOM_NORMAL;

   	r = sysconf_set_mempolicy_bypid(-1, mempol);

	if (r>=0) {
		tet_infoline("sysconf_set_mempolicy_bypid() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}
