CC ?= gcc

TARGETS = utc_SystemFW_device_get_battery_pct_func	\
	  utc_SystemFW_device_get_display_brt_func	\
	  utc_SystemFW_device_get_display_gamma_func	\
	  utc_SystemFW_device_get_max_brt_func		\
	  utc_SystemFW_device_get_property_func		\
	  utc_SystemFW_device_haptic_open_func		\
	  utc_SystemFW_device_haptic_close_func		\
	  utc_SystemFW_device_haptic_get_file_duration_func	\
	  utc_SystemFW_device_haptic_get_pattern_duration_func	\
	  utc_SystemFW_device_haptic_play_file_func		\
	  utc_SystemFW_device_haptic_play_monotone_func		\
	  utc_SystemFW_device_haptic_play_pattern_func		\
	  utc_SystemFW_device_haptic_stop_play_func		\
	  utc_SystemFW_device_is_battery_full_func		\
	  utc_SystemFW_device_release_brt_ctrl_func		\
	  utc_SystemFW_device_release_gamma_ctrl_func		\
	  utc_SystemFW_device_set_display_brt_func		\
	  utc_SystemFW_device_set_display_gamma_func		\
	  utc_SystemFW_device_set_property_func			\



PKGS = devman

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
