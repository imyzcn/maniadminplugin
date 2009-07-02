#
# Sample server plugin makefile for SRC engine
# October 2004, alfred@valvesoftware.com
#
# Updated by JTP10181, jtp@jtpage.net
# Version 1.1 - 12/13/04
#

#############################################################################
# Developer configurable items
#############################################################################

# the name of the plugin binary after its compiled (_i486.so is appended to the end)


NAME=$(PROJ_NAME)

# source files that should be compiled and linked into the binary
SRC_FILES =     cvars.cpp \
                mani_adverts.cpp \
                mani_afk.cpp \
                mani_anti_rejoin.cpp \
                mani_autokickban.cpp \
                mani_automap.cpp \
                mani_callback_sourcemm.cpp \
                mani_callback_valve.cpp \
                mani_chattrigger.cpp \
                mani_client.cpp \
                mani_client_interface.cpp \
                mani_client_sql.cpp \
                mani_client_util.cpp \
                mani_commands.cpp \
                mani_convar.cpp \
                mani_crontab.cpp \
                mani_css_betting.cpp \
                mani_css_bounty.cpp \
                mani_css_objectives.cpp \
                mani_customeffects.cpp \
                mani_database.cpp \
                mani_downloads.cpp \
                mani_effects.cpp \
                mani_file.cpp \
                mani_gametype.cpp \
                mani_globals.cpp \
                mani_ghost.cpp \
                mani_help.cpp \
                mani_hlxinterface.cpp \
                mani_keyvalues.cpp \
                mani_language.cpp \
		mani_log_css_stats.cpp \
		mani_log_dods_stats.cpp \
                mani_main.cpp \
                mani_mapadverts.cpp \
                mani_maps.cpp \
                mani_menu.cpp \
                mani_memory.cpp \
                mani_messagemode.cpp \
		mani_mostdestructive.cpp \
		mani_mp_restartgame.cpp \
		mani_mutex.cpp \
                mani_mysql.cpp \
		mani_netidvalid.cpp \
		mani_output.cpp \
		mani_panel.cpp \
		mani_parser.cpp \
		mani_ping.cpp \
		mani_player.cpp \
		mani_quake.cpp \
		mani_replace.cpp \
		mani_reservedslot.cpp \
		mani_save_scores.cpp \
		mani_sigscan.cpp \
		mani_skins.cpp \
		mani_sounds.cpp \
		mani_sourcehook.cpp \
		mani_spawnpoints.cpp \
		mani_sprayremove.cpp \
		mani_stats.cpp \
		mani_team.cpp \
		mani_team_join.cpp \
		mani_teamkill.cpp \
		mani_timers.cpp \
		mani_trackuser.cpp \
		mani_util.cpp \
		mani_vars.cpp \
		mani_vfuncs.cpp \
		mani_victimstats.cpp \
		mani_voice.cpp \
		mani_vote.cpp \
		mani_warmuptimer.cpp \
		mani_weapon.cpp \
		mrecipientfilter.cpp \
		serverplugin_convar.cpp

# the location of the SDK source code
HL2SDK_SRC_DIR=../sdk/
SOURCEMM_SRC_DIR=../sourcemm/

# the directory the base binaries (tier0_i486.so, etc) are located
HL2BIN_DIR=../srcds_1/bin

# compiler options (gcc 3.4.1 or above is required)
# to find the location for the CPP_LIB files use "updatedb" then "locate libstdc++.a"
CPLUS=/usr/bin/g++-3.4
CLINK=/usr/bin/gcc-3.4
CPP_LIB=/usr/lib/gcc/i486-linux-gnu/3.4.6/libstdc++.a \
	/usr/lib/gcc/i486-linux-gnu/3.4.6/libgcc_eh.a \
	./mysql5.1/linux_32/lib/libmysqlclient.a \
	./mysql5.1/linux_32/lib/libz.a \
	../sdk/linux_sdk/tier1_i486.a \
	../sdk/linux_sdk/mathlib_i486.a

#CPP_LIB=/usr/lib/gcc/i386-redhat-linux/3.4.3/libstdc++.a \
#	/usr/lib/gcc/i386-redhat-linux/3.4.3/libgcc_eh.a \
#	./mysql5.1/linux_32/lib/libmysqlclient.a \
#	/usr/lib/libz.a \

# put any compiler flags you want passed here
USER_CFLAGS=

# link flags for your mod, make sure to include any special libraries here
#LDFLAGS=-lm -ldl -lpthread tier0_i486.so vstdlib_i486.so
LDFLAGS=-lm -ldl $(STRIP_SYM) tier0_i486.so vstdlib_i486.so

#############################################################################
# Things below here shouldn't need to be altered
#############################################################################

# dirs for source code
PLUGIN_SRC_DIR=.
PUBLIC_SRC_DIR=$(HL2SDK_SRC_DIR)/public
TIER0_PUBLIC_SRC_DIR=$(HL2SDK_SRC_DIR)/public/tier0
TIER1_SRC_DIR=$(HL2SDK_SRC_DIR)/tier1
SOURCEHOOK_SRC_DIR=$(SOURCEMM_SRC_DIR)/sourcehook

# the dir we want to put binaries we build into
BUILD_DIR=.

# the place to put object files
BUILD_OBJ_DIR=$(BUILD_DIR)/obj
PLUGIN_OBJ_DIR=$(BUILD_OBJ_DIR)
PUBLIC_OBJ_DIR=$(BUILD_OBJ_DIR)/public
TIER0_OBJ_DIR=$(BUILD_OBJ_DIR)/tier0
TIER1_OBJ_DIR=$(BUILD_OBJ_DIR)/tier1
SOURCEHOOK_OBJ_DIR=$(BUILD_OBJ_DIR)/sourcehook

# the CPU target for the build, must be i486 for now
ARCH=i486
ARCH_CFLAGS=-mtune=i686 -march=pentium3 -mmmx -O3

# -fpermissive is so gcc 3.4.x doesn't complain about some template stuff
BASE_CFLAGS=-fpermissive -D_LINUX -DNDEBUG -Dstricmp=strcasecmp -D_stricmp=strcasecmp \
	-D_strnicmp=strncasecmp -Dstrnicmp=strncasecmp -D_snprintf=snprintf \
	-D_vsnprintf=vsnprintf -D_alloca=alloca -Dstrcmpi=strcasecmp $(SOURCE_MM) -DMAPBETA
SHLIBEXT=so
SHLIBLDFLAGS=-shared

CFLAGS=$(USER_CFLAGS) $(BASE_CFLAGS) $(ARCH_CFLAGS)
#DEBUG=-g -ggdb
DEBUG=$(DEBUG_FLAGS)
CFLAGS+= $(DEBUG)

INCLUDEDIRS=-I$(PUBLIC_SRC_DIR) \
	-I$(PUBLIC_SRC_DIR)/tier1 \
	-I$(PUBLIC_SRC_DIR)/engine \
	-I$(PUBLIC_SRC_DIR)/dlls  \
	-I$(HL2SDK_SRC_DIR)/game_shared \
	-I$(HL2SDK_SRC_DIR)/common \
	-I$(HL2SDK_SRC_DIR)/dlls \
	-Dstrcmpi=strcasecmp -D_alloca=alloca \
	-I$(HL2SDK_SRC_DIR)/../mani_admin_plugin/mysql5.1/linux_32/include/ \
	-I../sourcemm/ -I../sourcemm/sourcemm/

DO_CC=$(CPLUS) $(INCLUDEDIRS) -w $(CFLAGS) -DARCH=$(ARCH) -o $@ -c $<

#####################################################################

PLUGIN_OBJS := $(SRC_FILES:%.cpp=$(PLUGIN_OBJ_DIR)/%.o)

#PUBLIC_OBJS = \
#	$(PUBLIC_OBJ_DIR)/mathlib.o

TIER1_OBJS = \
	$(TIER1_OBJ_DIR)/convar.o \
	$(TIER1_OBJ_DIR)/interface.o \
#	$(TIER1_OBJ_DIR)/KeyValues.o \
	$(TIER1_OBJ_DIR)/utlbuffer.o \
	$(TIER1_OBJ_DIR)/bitbuf.o \

TIER0_OBJS = \
	$(TIER0_OBJ_DIR)/memoverride-vc7.o \

SOURCEHOOK_OBJS = \
	$(SOURCEHOOK_OBJ_DIR)/sourcehook.o

all: dirs $(NAME)_$(ARCH).$(SHLIBEXT)

dirs:
	@if [ ! -f "tier0_i486.so" ]; then ln -s $(HL2BIN_DIR)/tier0_i486.so .; fi
	@if [ ! -f "vstdlib_i486.so" ]; then ln -s $(HL2BIN_DIR)/vstdlib_i486.so .; fi
	@if [ ! -d $(BUILD_DIR) ]; then mkdir $(BUILD_DIR); fi
	@if [ ! -d $(BUILD_OBJ_DIR) ]; then mkdir $(BUILD_OBJ_DIR); fi
	@if [ ! -d $(PLUGIN_OBJ_DIR) ]; then mkdir $(PLUGIN_OBJ_DIR); fi
	@if [ ! -d $(PUBLIC_OBJ_DIR) ]; then mkdir $(PUBLIC_OBJ_DIR); fi
	@if [ ! -d $(TIER0_OBJ_DIR) ]; then mkdir $(TIER0_OBJ_DIR); fi
	@if [ ! -d $(TIER1_OBJ_DIR) ]; then mkdir $(TIER1_OBJ_DIR); fi
	@if [ ! -d $(SOURCEHOOK_OBJ_DIR) ]; then mkdir $(SOURCEHOOK_OBJ_DIR); fi


$(NAME)_$(ARCH).$(SHLIBEXT): $(PLUGIN_OBJS) $(PUBLIC_OBJS) $(TIER0_OBJS) $(TIER1_OBJS) $(SOURCEHOOK_OBJS)
	$(CLINK) $(DEBUG) -o $(BUILD_DIR)/$@ $(SHLIBLDFLAGS) $(PLUGIN_OBJS) $(PUBLIC_OBJS) $(TIER0_OBJS) $(TIER1_OBJS) $(SOURCEHOOK_OBJS) $(CPP_LIB) $(LDFLAGS)

$(PLUGIN_OBJ_DIR)/%.o: $(PLUGIN_SRC_DIR)/%.cpp
	$(DO_CC)

$(PUBLIC_OBJ_DIR)/%.o: $(PUBLIC_SRC_DIR)/%.cpp
	$(DO_CC)

$(TIER0_OBJ_DIR)/%.o: $(TIER0_PUBLIC_SRC_DIR)/%.cpp
	$(DO_CC)

$(TIER1_OBJ_DIR)/%.o: $(TIER1_SRC_DIR)/%.cpp
	$(DO_CC)

$(SOURCEHOOK_OBJ_DIR)/%.o: $(SOURCEHOOK_SRC_DIR)/%.cpp
	$(DO_CC)

clean:
	-rm -Rf $(PLUGIN_OBJ_DIR)
	-rm -f $(NAME)_$(ARCH).$(SHLIBEXT) tier0_i486.so vstdlib_i486.so
#	-cp /home/cssource/srcds_1/bin/tier0_i486.so ./
#	-cp /home/cssource/srcds_1/bin/vstdlib_i486.so ./