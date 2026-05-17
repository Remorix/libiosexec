CC       ?= cc
AR       ?= ar
LN       ?= ln
RANLIB   ?= ranlib
INSTALL  ?= install

MEMO_PREFIX     ?=
MEMO_SUB_PREFIX ?= /usr
LIBDIR          ?= $(MEMO_PREFIX)$(MEMO_SUB_PREFIX)/lib
INCLUDEDIR      ?= $(MEMO_PREFIX)$(MEMO_SUB_PREFIX)/include

DEFAULT_INTERPRETER ?= /bin/sh
ROOTLESS_PREFIX ?= /var/jb

SOVER    := 1

SRC      := execl.c execv.c get_new_argv.c posix_spawn.c system.c utils.c
PWD_SRC  := getgrent.c getpwent.c pwcache.c getusershell.c
WRAP_SRC := fake_getgrent.c fake_getpwent.c fake_getusershell.c

ifeq ($(shell uname -s), Linux)
CFLAGS          += -fPIE -fPIC
endif

ifeq ($(DEBUG),1)
CFLAGS += -g3
endif

LIBIOSEXEC_PREFIXED_ROOT ?= 0
ifeq ($(LIBIOSEXEC_PREFIXED_ROOT),1)
SHEBANG_REDIRECT_PATH    ?= $(ROOTLESS_PREFIX)
else
SHEBANG_REDIRECT_PATH    ?= /
endif

ifeq ($(LIBIOSEXEC_PREFIXED_ROOT),1)
SRC += $(PWD_SRC)
else
SRC += $(WRAP_SRC)
endif

CFLAGS += -I. -D_PW_NAME_LEN=MAXLOGNAME -DLIBIOSEXEC_INTERNAL -DLIBIOSEXEC_PREFIXED_ROOT=$(LIBIOSEXEC_PREFIXED_ROOT) -DDEFAULT_INTERPRETER='"$(DEFAULT_INTERPRETER)"'

DEFAULT_PATH ?= /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/bin/X11:/usr/games
STD_PATH ?= /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

ifeq ($(LIBIOSEXEC_PREFIXED_ROOT),1)
ROOTLESS_BSHELL ?= $(SHEBANG_REDIRECT_PATH)/bin/sh
ROOTLESS_CSHELL ?= $(SHEBANG_REDIRECT_PATH)/bin/csh
ROOTLESS_KSHELL ?= $(SHEBANG_REDIRECT_PATH)/bin/ksh
ROOTLESS_SHELLS ?= $(SHEBANG_REDIRECT_PATH)/etc/shells
ROOTLESS_PASSWD ?= $(SHEBANG_REDIRECT_PATH)/etc/passwd
ROOTLESS_MASTERPASSWD ?= $(SHEBANG_REDIRECT_PATH)/etc/master.passwd
ROOTLESS_GROUP ?= $(SHEBANG_REDIRECT_PATH)/etc/group
ROOTLESS_MP_DB ?= $(SHEBANG_REDIRECT_PATH)/etc/pwd.db
ROOTLESS_SMP_DB ?= $(SHEBANG_REDIRECT_PATH)/etc/spwd.db
REDIRECTED_DEFAULT_PATH := $(shell printf "%s\n" "$(DEFAULT_PATH)" | tr ':' '\n' | sed 'p; s|^|$(SHEBANG_REDIRECT_PATH)|' | tr '\n' ':' | sed 's|:$$||')
REDIRECTED_STD_PATH := $(shell printf "%s\n" "$(STD_PATH)" | tr ':' '\n' | sed 'p; s|^|$(SHEBANG_REDIRECT_PATH)|' | tr '\n' ':' | sed 's|:$$||')
else
ROOTLESS_BSHELL ?= /bin/sh
ROOTLESS_CSHELL ?= /bin/csh
ROOTLESS_KSHELL ?= /bin/ksh
ROOTLESS_SHELLS ?= /etc/shells
ROOTLESS_PASSWD ?= /etc/passwd
ROOTLESS_MASTERPASSWD ?= /etc/master.passwd
ROOTLESS_GROUP ?= /etc/group
ROOTLESS_MP_DB ?= /etc/pwd.db
ROOTLESS_SMP_DB ?= /etc/spwd.db
REDIRECTED_DEFAULT_PATH := $(DEFAULT_PATH)
REDIRECTED_STD_PATH := $(STD_PATH)
endif

all: libiosexec.$(SOVER).dylib libiosexec.a

%.o: %.c libiosexec_private.h paths.h
	$(CC) -c $(CFLAGS) -fvisibility=hidden $<

libiosexec_private.h: libiosexec_private.h.in
	sed -e 's|@DEFAULT_PATH@|$(REDIRECTED_DEFAULT_PATH)|' -e "s|@SHEBANG_REDIRECT_PATH@|$(SHEBANG_REDIRECT_PATH)|" $^ > $@

paths.h: paths.h.in
	sed \
		-e 's|@LIBIOSEXEC_BSHELL@|$(ROOTLESS_BSHELL)|' \
		-e 's|@LIBIOSEXEC_CSHELL@|$(ROOTLESS_CSHELL)|' \
		-e 's|@LIBIOSEXEC_KSHELL@|$(ROOTLESS_KSHELL)|' \
		-e 's|@LIBIOSEXEC_SHELLS@|$(ROOTLESS_SHELLS)|' \
		-e 's|@LIBIOSEXEC_DEFPATH@|$(REDIRECTED_DEFAULT_PATH)|' \
		-e 's|@LIBIOSEXEC_STDPATH@|$(REDIRECTED_STD_PATH)|' \
		-e 's|@LIBIOSEXEC_PASSWD@|$(ROOTLESS_PASSWD)|' \
		-e 's|@LIBIOSEXEC_MASTERPASSWD@|$(ROOTLESS_MASTERPASSWD)|' \
		-e 's|@LIBIOSEXEC_GROUP@|$(ROOTLESS_GROUP)|' \
		-e 's|@LIBIOSEXEC_MP_DB@|$(ROOTLESS_MP_DB)|' \
		-e 's|@LIBIOSEXEC_SMP_DB@|$(ROOTLESS_SMP_DB)|' \
		$^ > $@

libiosexec.$(SOVER).dylib: $(SRC:%.c=%.o)
ifeq ($(shell uname -s), Linux)
	$(CC) $(CFLAGS) $(LDFLAGS) -fvisibility=hidden -DLIBIOSEXEC_INTERNAL -lbsd -shared -o $@ $^
else ifeq ($(shell uname -s), Darwin)
	$(CC) $(CFLAGS) $(LDFLAGS) -fvisibility=hidden -DLIBIOSEXEC_INTERNAL -install_name $(LIBDIR)/$@ -shared -o $@ $^
endif

libiosexec.a: $(SRC:%.c=%.o)
	$(AR) cru $@ $^
	$(RANLIB) $@


TEST_progs := tests/t_ie_execve \
	tests/t_ie_execv \
	tests/t_ie_execle \
	tests/t_ie_execl \
	tests/t_ie_execvpe \
	tests/t_ie_execvp \
	tests/t_ie_execlp \
	tests/t_ie_posix_spawn \
	tests/t_ie_posix_spawnp \
	tests/t_ie_system

REGRESS_BUILD_VARS := LIBIOSEXEC_PREFIXED_ROOT=1 SHEBANG_REDIRECT_PATH="$(PWD)/tests/rootless"

REGRESS_PROGS := tests/t_ie_idlookup \
	tests/t_ie_getusershell \
	tests/t_ie_shadowlookup \
	tests/t_ie_exec_errno \
	tests/t_ie_spawn_errno

REGRESS_ROOTLESS := $(PWD)/tests/rootless
REGRESS_PATH := $(PWD)/tests/scripts:$(PWD)/tests/rootless/usr/bin:$(PWD)/tests/rootless/bin:$(PATH)

TEST_scripts := tests/scripts/empty.sh \
	tests/scripts/normal.sh \
	tests/scripts/normalwitharg.sh \
	tests/scripts/normalwithmultipleargs.sh
                

%: %.c libiosexec.a
	$(CC) -I. $(CFLAGS) $(LDFLAGS) -o $@ $^

check: $(TEST_progs)
	success=0; \
	PATH="$(PATH):$(PWD)/tests/scripts"; \
	for test in $^; do \
		for script in $(TEST_scripts); do \
			printf '%s %s... ' $$(basename $$test) $$(basename $$script); \
			if $$test $$script; then \
				printf 'success\n'; \
			else \
				success=1; \
				printf 'FAILED!\n'; \
			fi; \
		done; \
	done; \
	exit $$success

regress:
	$(MAKE) $(REGRESS_BUILD_VARS) $(REGRESS_PROGS) tests/t_ie_execvp tests/t_ie_posix_spawnp
	cp tests/scripts/normal.sh tests/scripts/nonexec.sh
	chmod 0644 tests/scripts/nonexec.sh
	./tests/t_ie_idlookup
	./tests/t_ie_getusershell
	./tests/t_ie_shadowlookup
	./tests/t_ie_exec_errno ./tests/scripts/nonexec.sh | grep '^13 '
	./tests/t_ie_spawn_errno ./tests/scripts/nonexec.sh | grep '^13 '
	PATH="$(REGRESS_PATH)" ./tests/t_ie_execvp $(PWD)/tests/scripts/normal.sh
	PATH="$(REGRESS_PATH)" ./tests/t_ie_posix_spawnp $(PWD)/tests/scripts/normal.sh
	PATH="$(REGRESS_PATH)" ./tests/t_ie_execvp $(PWD)/tests/scripts/noshebang.sh
	PATH="$(REGRESS_PATH)" ./tests/t_ie_posix_spawnp $(PWD)/tests/scripts/noshebang.sh

install: all
	$(INSTALL) -Dm644 libiosexec.$(SOVER).dylib $(DESTDIR)$(LIBDIR)/libiosexec.$(SOVER).dylib
	$(LN) -sf libiosexec.$(SOVER).dylib $(DESTDIR)$(LIBDIR)/libiosexec.dylib
	$(INSTALL) -Dm644 libiosexec.a $(DESTDIR)$(LIBDIR)/libiosexec.a
	$(INSTALL) -Dm644 libiosexec.h $(DESTDIR)$(INCLUDEDIR)/libiosexec.h

clean:
	rm -rf libiosexec.$(SOVER).dylib libiosexec.a *.o tests/test tests/*.dSYM *.dSYM libiosexec_private.h paths.h $(TEST_progs) $(REGRESS_PROGS) tests/scripts/nonexec.sh

.PHONY: all clean install check regress
