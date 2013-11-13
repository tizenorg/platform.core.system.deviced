#include <tet_api.h>
#include <dd-deviced.h>

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SystemFW_deviced_conf_set_permanent_func_01(void);
//static void utc_SystemFW_deviced_conf_set_permanent_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SystemFW_deviced_conf_set_permanent_func_01, POSITIVE_TC_IDX },
//	{ utc_SystemFW_deviced_conf_set_permanent_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of deviced_conf_set_permanent()
 */
static void utc_SystemFW_deviced_conf_set_permanent_func_01(void)
{

	int r = 0;

	r = deviced_conf_set_permanent();

	if (r<0) {
		tet_infoline("deviced_conf_set_permanent() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init deviced_conf_set_permanent()

static void utc_SystemFW_deviced_conf_set_permanent_func_02(void)
{
	int r = 0;

	r = deviced_conf_set_permanent();

	if (r>=0) {
		tet_infoline("deviced_conf_set_permanent() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}*/
