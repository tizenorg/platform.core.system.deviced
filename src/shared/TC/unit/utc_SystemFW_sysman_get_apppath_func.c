#include <tet_api.h>
#include <sysman.h>

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SystemFW_sysman_get_apppath_func_01(void);
static void utc_SystemFW_sysman_get_apppath_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SystemFW_sysman_get_apppath_func_01, POSITIVE_TC_IDX },
	{ utc_SystemFW_sysman_get_apppath_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of sysman_get_apppath()
 */
static void utc_SystemFW_sysman_get_apppath_func_01(void)
{
	int ret_val = 0;
	char* app_path[255] = {'\0',};

	ret_val = sysman_get_apppath(1, app_path, 100);
	if(ret_val <0) {
		tet_infoline("sysman_get_apppath() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init sysman_get_apppath()
 */
static void utc_SystemFW_sysman_get_apppath_func_02(void)
{
	int ret_val = 0;
	char* app_path[255] = {'\0',};

	int pid = -1;
	
	ret_val = sysman_get_apppath(pid, app_path, 100);
	if(ret_val >= 0) {
		tet_infoline("sysman_get_apppath() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}
