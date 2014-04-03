#include <tet_api.h>
#include <dd-deviced.h>

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SystemFW_deviced_get_cmdline_name_func_01(void);
static void utc_SystemFW_deviced_get_cmdline_name_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SystemFW_deviced_get_cmdline_name_func_01, POSITIVE_TC_IDX },
	{ utc_SystemFW_deviced_get_cmdline_name_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of deviced_get_cmdline_name()
 */
static void utc_SystemFW_deviced_get_cmdline_name_func_01(void)
{
	char name[50]={'\0',};
	int ret_val = 0;

	ret_val = deviced_get_cmdline_name(1,name,50);
	if(ret_val < 0) {
		tet_infoline("deviced_get_cmdline_name() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init deviced_get_cmdline_name()
 */
static void utc_SystemFW_deviced_get_cmdline_name_func_02(void)
{
	char name[50]={'\0',};
	int ret_val = 0;

	ret_val = deviced_get_cmdline_name(-1,name,50);
	if(ret_val >= 0) {
		tet_infoline("deviced_get_cmdline_name() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}
