
# this is a very simple makefile for modules
TOP_DIR=../..

# define the module name - make sure name is max 8 characters
MODULE_NAME=crop_rec
MODULE_OBJS=crop_rec.o
MODULE_CFLAGS += -Wno-switch
# This build is EOS M only: always compile the EOS M code paths into the module
# (previously this only happened when MODEL was passed down, which the
# platform build does not do).
MODULE_CFLAGS += -DCONFIG_EOSM=1

# include modules environment
include $(TOP_DIR)/modules/Makefile.modules
