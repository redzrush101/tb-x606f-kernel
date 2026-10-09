#
# Makefile for the input misc drivers.
#

# Each configuration option enables a list of files.
ifeq ($(TARGET_FACTORY_VERSION),true)
subdir-ccflags-y += -DMAKE_FACTORY
endif
obj-$(CONFIG_MTK_SX9310)		+= sx932x.o
obj-$(CONFIG_MTK_SX9311)		+= sx9311.o
