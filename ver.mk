
empty :=
SPACE :=$(empty) $(empty)

#GIT_VER := $(subst -,.,$(subst g,,$(shell git describe --long --dirty --tag 2> /dev/null)))
#git_version := $(subst -,$(SPACE),$(shell git describe --long --dirty --tag 2> /dev/null))
#COMMIT := $(subst g,,$(strip $(word 3, $(git_version))))
#COMMITS_PAST := $(strip $(word 2, $(git_version)))
#DIRTY := $(strip $(word 4, $(git_version)))
#COMMIT4 := $(shell git rev-parse HEAD 2> /dev/null | cut -c 1-4)
#git_branch := $(shell git symbolic-ref HEAD 2>/dev/null | cut -d"/" -f 3)

COMP_VER :=0x$(COMMIT4)

ifneq ($(COMMITS_PAST), 0)
    BUILD_INFO_COMMITS := "."$(COMMITS_PAST)
endif

ifneq ($(DIRTY),)
    BUILD_INFO_DIRTY :="+"
endif

BUILD_TAG:= $(strip $(word 1, $(git_version)))
BUILD_INFO:= $(COMMITS_PAST).$(COMMIT)$(BUILD_INFO_DIRTY)
TAG_SP := $(subst .,$(SPACE),$(BUILD_TAG))
MAIN_VER := $(strip $(word 1, $(TAG_SP)))
MINOR_VER := $(strip $(word 2, $(TAG_SP)))
