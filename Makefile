#
#
#

UNAME := $(shell uname -s)
ARCH :=$(shell uname -m)

include ver.mk
include make.cfg

CROSS_COMPILER_BIN =gcc-arm-none-eabi-9-2019-q4-major/bin

ifeq ($(OS),Windows_NT)
TOOLS_PATH := /opt/$(CROSS_COMPILER_BIN)
PACKER  := ./bin/packfile.exe
else
ifeq ($(UNAME), Linux)
TOOLS_PATH := /opt/toolchains/$(CROSS_COMPILER_BIN)
PACKER  := ./bin/packfile
endif
ifeq ($(UNAME),Darwin)
TOOLS_PATH := /Users/weiwen/tools/$(CROSS_COMPILER_BIN)
PACKER  := ./bin/macpackfile
endif
endif

PREFIX := $(TOOLS_PATH)/arm-none-eabi-
CC := $(PREFIX)gcc
AS := $(PREFIX)as
AR := $(PREFIX)ar
LD := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy

SED := sed
MV := mv
RM := rm 
DD := dd
CAT := cat
CP  := cp -f

.SUFFIXES: .o .c .s .S

TOP_DIR =.

APP_DIR = $(TOP_DIR)/app
LIB_DIR = $(TOP_DIR)/lib
SCRIPTS_DIR =$(TOP_DIR)/scripts
BSP_DIR = $(TOP_DIR)/bsp
DRIVER_DIR =$(LIB_DIR)/Driver
DRIVER_SRC_DIR =$(LIB_DIR)/Driver/src

CMSIS_DIR = $(LIB_DIR)/CMSIS
CMSIS_CHIP_DIR = $(CMSIS_DIR)/Device/ST/STM32F4xx
CMSIS_SYS_DIR=$(CMSIS_CHIP_DIR)/Source/Templates
DEVICE_START_DIR = $(CMSIS_SYS_DIR)/gcc_ride7

INC_CMSIS  = -I$(CMSIS_DIR)/Include -I$(CMSIS_CHIP_DIR)/Include
INC_DRIVER = -I$(DRIVER_DIR)/inc -I$(BSP_DIR)
INC_APP    = -I$(APP_DIR)

INC_ALL = $(INC_CMSIS) $(INC_DRIVER) $(INC_APP)

BUILD_DIR = $(TOP_DIR)/build
output_dir = $(TOP_DIR)/out

################################################################################
APP_FLAGS = -DIR_SENSOR

ifeq ($(CODETYPE),IAP)
APP_FLAGS +=-DIAP_APP
endif

STARTUP_DEFS = -D__STARTUP_CLEAR_BSS -D__START=main
DFLAGS =  -DSTM32F411xE -DUSE_STDPERIPH_DRIVER  -DDEBUG

DFLAGS += -DVERSION_TAG=\"$(BUILD_TAG)\" -DVERSION_BUILD=\"$(BUILD_INFO)\"


################################################################################
ver = $(APP_DIR)/ver.h

startup_src =$(DEVICE_START_DIR)/startup_stm32f40_41xxx.s

app_src =  \
          $(APP_DIR)/main.c                             \
          $(APP_DIR)/crc8.c                             \
          $(APP_DIR)/comslave.c                         \
          $(APP_DIR)/htpa32_drv.c                       \
          $(APP_DIR)/htpa32_main.c                      \
          $(APP_DIR)/htpa32_table.c                      \
          $(APP_DIR)/setting.c                          \


bsp_src = $(BSP_DIR)/retarget.c                         \
          $(BSP_DIR)/system_stm32f4xx.c                 \
          $(BSP_DIR)/stm32f4xx_it.c                     \
          $(BSP_DIR)/hal.c                              \
          $(BSP_DIR)/usart.c                            \
          $(BSP_DIR)/i2c_drv.c                          \
          $(BSP_DIR)/flash_if.c                         \


chip_driver_src = \
            $(DRIVER_SRC_DIR)/misc.c                         \
            $(DRIVER_SRC_DIR)/stm32f4xx_gpio.c               \
            $(DRIVER_SRC_DIR)/stm32f4xx_flash.c              \
            $(DRIVER_SRC_DIR)/stm32f4xx_i2c.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_iwdg.c               \
            $(DRIVER_SRC_DIR)/stm32f4xx_rcc.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_syscfg.c             \
            $(DRIVER_SRC_DIR)/stm32f4xx_tim.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_usart.c              \
            $(DRIVER_SRC_DIR)/stm32f4xx_wwdg.c               \
            $(DRIVER_SRC_DIR)/stm32f4xx_rng.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_can.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_dma.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_dsi.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_exti.c               \
            $(DRIVER_SRC_DIR)/stm32f4xx_adc.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_rtc.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_crc.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_cryp_aes.c           \
            $(DRIVER_SRC_DIR)/stm32f4xx_cryp.c               \
            $(DRIVER_SRC_DIR)/stm32f4xx_cryp_des.c           \
            $(DRIVER_SRC_DIR)/stm32f4xx_cryp_tdes.c          \
            $(DRIVER_SRC_DIR)/stm32f4xx_dac.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_dbgmcu.c             \
            $(DRIVER_SRC_DIR)/stm32f4xx_dcmi.c               \
            $(DRIVER_SRC_DIR)/stm32f4xx_dfsdm.c              \
            $(DRIVER_SRC_DIR)/stm32f4xx_dma2d.c              \
            $(DRIVER_SRC_DIR)/stm32f4xx_flash_ramfunc.c      \
            $(DRIVER_SRC_DIR)/stm32f4xx_spi.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_cec.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_hash.c               \
            $(DRIVER_SRC_DIR)/stm32f4xx_hash_md5.c           \
            $(DRIVER_SRC_DIR)/stm32f4xx_hash_sha1.c          \
            $(DRIVER_SRC_DIR)/stm32f4xx_lptim.c              \
            $(DRIVER_SRC_DIR)/stm32f4xx_pwr.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_qspi.c               \
            $(DRIVER_SRC_DIR)/stm32f4xx_sdio.c               \
            
#           $(DRIVER_SRC_DIR)/stm32f4xx_spdifrx.c            \
            $(DRIVER_SRC_DIR)/stm32f4xx_sai.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_fmc.c                \
            $(DRIVER_SRC_DIR)/stm32f4xx_fsmc.c               \
            $(DRIVER_SRC_DIR)/stm32f4xx_fmpi2c.c             \
            $(DRIVER_SRC_DIR)/stm32f4xx_ltdc.c               \





driver_src = $(chip_driver_src) $(bsp_src)

all_src_c =   $(app_src) $(driver_src) 
all_src_asm = $(startup_src)


bootloader_src = $(boot_app_src) $(loader_drv_src)

###############################################################################
ifeq ($(CODETYPE),IAP)
ldscript := $(SCRIPTS_DIR)/stm32f411re_iap.ld
else	
ldscript := $(SCRIPTS_DIR)/stm32f411re.ld
endif

PACK_NAME =thermometer

packed_file :=$(strip $(BUILD_DIR)/$(PACK_NAME)_v$(GIT_VER).$(git_branch).$(shell date +%Y%m%d_%H%M%S).tar.gz)

fmware := $(BUILD_DIR)/thermom_fw.bin

app_bin := $(BUILD_DIR)/thermom.bin
app_elf := $(BUILD_DIR)/thermometer.out
up_bin  := $(BUILD_DIR)/thermom_app.bin

memmap = $(BUILD_DIR)/thermometer.map

bootloader := $(LIB_DIR)/bloader16.bin

cc_info :=$(BUILD_DIR)/ccinfo.txt

FLAGS_LTO = -flto

CPPFLAGS = $(INC_ALL) $(DFLAGS) -ffunction-sections -fdata-sections --specs=nano.specs \
            -Wl,--gc-sections -fno-builtin  -Wall -Wextra  -Wno-format  -Os  $(FLAGS_LTO) 

CFLAGS = -mcpu=cortex-m4 -mthumb -march=armv7e-m
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16

LDFLAGS = $(CFLAGS) -fno-builtin --specs=nosys.specs --specs=nano.specs  $(FLAGS_LTO) 
LDFLAGS += -nostartfiles $(STARTUP_DEFS) -lm 


M_LDFLAGS = -T $(ldscript) -Wl,-Map=$(memmap) $(LDFLAGS)


##############################################################################

main_objects =  $(addprefix $(BUILD_DIR)/, $(startup_src:.s=.o))
main_objects +=  $(addprefix $(BUILD_DIR)/, $(rtt_port_asm:.S=.o))
main_objects +=  $(addprefix $(BUILD_DIR)/, $(all_src_c:.c=.o))

main_assb = $(addprefix $(BUILD_DIR)/, $(all_src_c:.c=.s))

bootloader_obj := $(addprefix $(BUILD_DIR)/, $(startup_src:.s=.o))
bootloader_obj += $(addprefix $(BUILD_DIR)/, $(bootloader_src:.c=.bo))

dependencies := $(addprefix $(BUILD_DIR)/, $(all_src_c:.c=.d))
dependencies += $(addprefix $(BUILD_DIR)/, $(loader_drv_src:.c=.d))

define make-depend
  $(CC) $(CPPFLAGS) $(APP_FLAGS) -MM $1 2> /dev/null| \
  $(SED) -e 's,\($(notdir $2)\) *:,$(dir $2)\1: ,' > $3.tmp
  $(SED) -e 's/#.*//' \
      -e 's/^[^:]*: *//' \
      -e 's/ *\\$$//' \
      -e '/^$$/ d' \
      -e 's/$$/ :/' $3.tmp >> $3.tmp
  $(MV) $3.tmp $3
endef


define make-depend_b
  $(CC) $(CPPFLAGS) -DBOOTLOADER -MM $1 2> /dev/null| \
  $(SED) -e 's,\($(notdir $2)\) *:,$(dir $2)\1: ,' > $3.tmp
  $(SED) -e 's/#.*//' \
      -e 's/^[^:]*: *//' \
      -e 's/ *\\$$//' \
      -e '/^$$/ d' \
      -e 's/$$/ :/' $3.tmp >> $3.tmp
  $(SED) -e 's/.o:/.bo:/' $3.tmp > $3.tmp0
  $(MV) $3.tmp0 $3
  $(RM) $3.tmp
endef


.PHONY: all clean assb bootcode

#all: $(app_bin)
all: $(packed_file)
ifneq ($(DIRTY),)
	@echo Dirty Version Build
else
	@echo Clean Version Build
endif
	@mkdir -p $(output_dir)
	@@$(CP) $(packed_file) $(output_dir)/
	@echo Finish building firmware for $(model)


ifeq ($(CODETYPE),IAP)
$(packed_file): $(boot_bin) $(app_bin) 
else
$(packed_file): $(app_bin) 
endif	
	@echo  Building model: $(model) > $(cc_info)
ifeq ($(CODETYPE),IAP)
	@echo  Release Files: $(notdir $(app_bin)) $(notdir $(fmware)) >> $(cc_info)
else
	@echo  Release Files: $(notdir $(app_bin))  >> $(cc_info)
endif
	@echo  Build Time: $(shell date '+%Y-%m-%d %H:%M:%S') >> $(cc_info)
	@echo  Release Version: $(BUILD_TAG)   >> $(cc_info)
	@echo  Build Git Version: $(BUILD_INFO)   >> $(cc_info)
	@echo  Git Branch: $(git_branch)   >> $(cc_info)
	@echo  Enabled Flags: $(APP_FLAGS) >> $(cc_info)
	@echo "Compiler: $(shell $(CC) -v 2>&1 | tail -n 1) " >>$(cc_info)
	@echo  Compile By: $(shell whoami) >>$(cc_info)
	@echo  Compile At: $(shell hostname) >>$(cc_info)
ifeq ($(CODETYPE),IAP)
	@$(CAT) $(bootloader) $(app_bin) > $(fmware)
	@$(PACKER) $(app_bin) $(up_bin)
	@tar zcvf $(packed_file) -C $(BUILD_DIR) $(notdir $(up_bin)) $(notdir $(fmware)) $(notdir $(cc_info))
else
	@tar zcvf $(packed_file) -C $(BUILD_DIR) $(notdir $(app_bin)) $(notdir $(cc_info))
endif
	@echo Generating $(notdir $@)


$(app_elf): $(ver) $(main_objects) $(ldscript)
	@echo Linking $(notdir $@)
	@$(LD)  -o $@ $(main_objects) $(M_LDFLAGS) 

$(app_bin): $(app_elf)
	@echo Creating Binary file $(notdir $@)  
	@$(OBJCOPY) -O binary $(app_elf) $(app_bin)
	@echo Done

$(ver): 
	@echo Generating version file $(ver) 
	@echo "#define COMPILE_BY \"$(shell whoami)\" " >>$(ver) 
	@echo "#define COMPILE_AT \"$(shell hostname)\" " >>$(ver) 
	@echo "#define COMPILER \"$(shell $(CC) -v 2>&1 | tail -n 1)\" " >>$(ver) 
ifneq ($(DIRTY),)
	@echo "#define VERSION_CHECK \"dirty\" " >>$(ver) 
else
	@echo "#define VERSION_CHECK \"clean\" " >>$(ver) 
endif
	@echo "#define GIT_VER \"$(BUILD_TAG)\" " >>$(ver)
	@echo "#define GIT_INFO \"$(BUILD_INFO)\" " >>$(ver)

clean:
	-@$(RM) -rf $(BUILD_DIR) 2> /dev/null
	-@$(RM) -rf $(output_dir) 2> /dev/null
	-@$(RM) -rf $(ver) 



$(BUILD_DIR)/%.o : %.c
	@echo $<
	@mkdir -p $(dir $@)
	@$(call make-depend,$<,$@,$(subst .o,.d,$@))
	@$(CC) $(CPPFLAGS) $(APP_FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o : %.s
	@echo $<
	@mkdir -p $(dir $@)
	@$(AS) $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o : %.S
	@echo $<
	@mkdir -p $(dir $@)
	@$(AS) $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.s : %.c
	@echo $<
	@mkdir -p $(dir $@)
	@$(call make-depend,$<,$@,$(subst .o,.d,$@))
	@$(CC) $(CPPFLAGS) $($(APP_FLAGS) $(CFLAGS) -fno-lto -S $< -o $@

ifneq "$(MAKECMDGOALS)" "clean"
-include $(dependencies)
endif

