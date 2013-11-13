CC ?= gcc

TARGETS = utc_SystemFW_haptic_open_func		\
	  utc_SystemFW_haptic_close_func		\
	  utc_SystemFW_haptic_play_monotone_func		\
	  utc_SystemFW_haptic_play_file_func		\
	  utc_SystemFW_haptic_stop_play_func		\
	  utc_SystemFW_haptic_get_file_duration_func	\


PKGS = haptic

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
