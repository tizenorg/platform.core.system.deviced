#include <tet_api.h>
#include <pmapi.h>

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SystemFW_pm_unlock_state_func_01(void);
static void utc_SystemFW_pm_unlock_state_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SystemFW_pm_unlock_state_func_01, POSITIVE_TC_IDX },
	{ utc_SystemFW_pm_unlock_state_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0 },
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of pm_unlock_state()
 */
static void utc_SystemFW_pm_unlock_state_func_01(void)
{
	int ret_val = 0;

	//Lock State as LCD_DIM for infinite time
	ret_val = pm_lock_state(LCD_DIM, GOTO_STATE_NOW, 0);
	if(ret_val < 0)
	{
		tet_infoline("\nSystem Fwk : call to pm_lock_state for unlock failed \n");
	}


	//Unlock previously locked state
	ret_val = pm_unlock_state(LCD_DIM,GOTO_STATE_NOW);
	if(ret_val < 0) {
		tet_infoline("pm_unlock_state() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init pm_unlock_state()
 */
static void utc_SystemFW_pm_unlock_state_func_02(void)
{
	int r = 0;

   	r = pm_unlock_state(-1 ,GOTO_STATE_NOW);

	if (r>=0) {
		tet_infoline("pm_unlock_state() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}
