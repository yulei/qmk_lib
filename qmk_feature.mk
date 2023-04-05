
INCS += $(QMK_DIR)/quantum/process_keycode

SPACE_CADET_ENABLE ?= yes
GRAVE_ESC_ENABLE ?= yes
TAP_DANCE_ENABLE ?= yes
DYNAMIC_TAPPING_TERM_ENABLE ?= yes

GENERIC_FEATURES = \
    CAPS_WORD \
    COMBO \
    COMMAND \
    DEFERRED_EXEC \
    DIGITIZER \
    DIP_SWITCH \
    DYNAMIC_KEYMAP \
    DYNAMIC_MACRO \
    GRAVE_ESC \
    HAPTIC \
    KEY_LOCK \
    KEY_OVERRIDE \
    LEADER \
    PROGRAMMABLE_BUTTON \
    SECURE \
    SPACE_CADET \
    SWAP_HANDS \
    TAP_DANCE \
    VELOCIKEY \
    WPM \
    DYNAMIC_TAPPING_TERM \

define HANDLE_GENERIC_FEATURE
    $$(info "Processing: $1_ENABLE $2.c")
    SRCS += $$(wildcard $$(QMK_DIR)/quantum/process_keycode/process_$2.c)
    SRCS += $$(wildcard $$(QMK_DIR)/quantum/$2.c)
    APP_DEFS += -D$1_ENABLE
endef

$(foreach F,$(GENERIC_FEATURES),\
    $(if $(filter yes, $(strip $($(F)_ENABLE))),\
        $(eval $(call HANDLE_GENERIC_FEATURE,$(F),$(shell echo $(F) | tr '[:upper:]' '[:lower:]'))) \
    ) \
)

MAGIC_ENABLE ?= yes
ifeq ($(strip $(MAGIC_ENABLE)), yes)
    SRCS += $(QMK_DIR)/quantum/process_keycode/process_magic.c
    APP_DEFS += -DMAGIC_KEYCODE_ENABLE
endif