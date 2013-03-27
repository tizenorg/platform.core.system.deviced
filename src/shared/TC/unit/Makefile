CC ?= gcc

TARGETS = utc_SystemFW_system_conf_is_vip_func	\
	  utc_SystemFW_system_conf_set_mempolicy_bypid_func	\
	  utc_SystemFW_system_conf_set_mempolicy_func	\
	  utc_SystemFW_system_conf_set_permanent_bypid_func	\
	  utc_SystemFW_system_conf_set_permanent_func	\
	  utc_SystemFW_system_conf_set_vip_func		\
	  utc_SystemFW_system_get_apppath_func		\
	  utc_SystemFW_system_get_cmdline_name_func	\
	  utc_SystemFW_system_get_pid_func		\
	  utc_SystemFW_system_set_datetime_func		\

PKGS = deviced

LDFLAGS = `pkg-config --libs $(PKGS)`
LDFLAGS += $(TET_ROOT)/lib/tet3/tcm_s.o
LDFLAGS += -L$(TET_ROOT)/lib/tet3 -ltcm_s
LDFLAGS += -L$(TET_ROOT)/lib/tet3 -lapi_s

CFLAGS = -I. `pkg-config --cflags $(PKGS)`
CFLAGS += -I$(TET_ROOT)/inc/tet3
CFLAGS += -Wall

all: $(TARGETS)

$(TARGETS): %: %.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGETS)
