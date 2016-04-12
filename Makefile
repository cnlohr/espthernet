TARGET_OUT:=image.elf
FW_FILES:=image.elf-0x00000.bin image.elf-0x40000.bin
all : $(TARGET_OUT) $(FW_FILES)

SRCS:=driver/uart.c \
	common/http.c \
	common/mystuff.c \
	common/flash_rewriter.c \
	common/commonservices.c \
	common/http_custom.c \
	common/mdns.c \
	common/mfs.c \
	user/custom_commands.c \
	user/i2sduplex.c \
	user/user_main.c \
	user/manchestrate.c \
	etherhelp/net_compat.c \
	etherhelp/crc32.c \
	etherhelp/tcp.c \
	etherhelp/iparpetc.c


ESP_OPEN_SDK:=~/esp-open-sdk
GCC_FOLDER:=$(ESP_OPEN_SDK)/xtensa-lx106-elf
ESPTOOL_PY:=$(ESP_OPEN_SDK)/esptool/esptool.py
SDK:=$(ESP_OPEN_SDK)/esp_iot_sdk_v1.5.2
PORT:=/dev/ttyUSB0

XTLIB:=$(SDK)/lib
XTGCCLIB:=$(GCC_FOLDER)/lib/gcc/xtensa-lx106-elf/4.8.2/libgcc.a
FOLDERPREFIX:=$(GCC_FOLDER)/bin
PREFIX:=$(FOLDERPREFIX)/xtensa-lx106-elf-
CC:=$(PREFIX)gcc


CFLAGS:=-mlongcalls -I $(SDK)/include -Imyclib -Iinclude -Iuser -Os -I $(SDK)/include/ -Icommon -DICACHE_FLASH -Ietherhelp
CFLAGS:= $(CFLAGS)	-Wl,--gc-sections -flto

#Enable full duplex?
#Not enabling it is okay, it will still be full duplex, this is just for the FLP.
#Warning, it's currently not working.
#You can override link settings using the following command in Linux: # mii-tool -F 10baseT-FD
#CFLAGS:=$(CFLAGS) -DUSE_US_TIMER -DFULL_DUPLEX_FLP



LDFLAGS_CORE:=\
	-nostdlib \
	-L$(XTLIB) \
	-L$(XTGCCLIB) \
	-g \
	$(SDK)/lib/libmain.a \
	$(SDK)/lib/libpp.a \
	$(SDK)/lib/libnet80211.a \
	$(SDK)/lib/libwpa.a \
	$(SDK)/lib/liblwip.a \
	$(SDK)/lib/libssl.a \
	$(SDK)/lib/libupgrade.a \
	$(SDK)/lib/libnet80211.a \
	$(SDK)/lib/liblwip.a \
	$(SDK)/lib/libphy.a \
	$(SDK)/lib/libcrypto.a \
	$(XTGCCLIB) \
	-T $(SDK)/ld/eagle.app.v6.ld

LINKFLAGS:= \
	$(LDFLAGS_CORE) \
	-B $(XTLIB)

#image.elf : $(OBJS)
#	$(PREFIX)ld $^ $(LDFLAGS) -o $@

$(TARGET_OUT) : $(SRCS)
	$(PREFIX)gcc $(CFLAGS) $^  $(LINKFLAGS) -o $@
	nm -S -n $(TARGET_OUT) > image.map
	$(PREFIX)objdump -S $@ > image.lst
	$(PREFIX)size -A $(TARGET_OUT) | grep -v debug


$(FW_FILES): $(TARGET_OUT)
	@echo "FW $@"
	PATH=$(FOLDERPREFIX):$$PATH;$(ESPTOOL_PY) elf2image $(TARGET_OUT)

burn : $(FW_FILES)
	($(ESPTOOL_PY) --port $(PORT) write_flash 0x00000 image.elf-0x00000.bin 0x40000 image.elf-0x40000.bin)||(true)

#If you have space, MFS should live at 0x100000, if you don't it can also live at
#0x10000.  But, then it is limited to 180kB.  You might need to do this if you have a 512kB 
#ESP variant.

burnweb : web/page.mpfs
	($(ESPTOOL_PY) --port $(PORT) write_flash 0x10000 web/page.mpfs)||(true)


IP?=10.1.10.5

netburn : image.elf $(FW_FILES)
	web/execute_reflash $(IP) image.elf-0x00000.bin image.elf-0x40000.bin

mii :
	sudo mii-tool -F 10baseT-FD eth0

clean :
	rm -rf user/*.o driver/*.o $(TARGET_OUT) $(FW_FILES) image.lst image.map


