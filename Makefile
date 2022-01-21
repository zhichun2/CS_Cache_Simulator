#
# Student makefile for Cache Lab
#
SHELL = /bin/bash
CC = gcc
CFLAGS = -g -Wall -Werror -std=c99 $(COPT)
COPT = -Og
LDLIBS = -lm
LLVM_PATH = /usr/local/depot/llvm-7.0/bin/

ifneq (,$(wildcard /usr/lib/llvm-7/bin/))
  LLVM_PATH = /usr/lib/llvm-7/bin/
endif

HANDIN_TAR = cachelab-handin.tar
FILES = test-csim csim test-trans test-trans-simple tracegen-ct $(HANDIN_TAR)

.PHONY: all
all: $(FILES)

objs:
	mkdir $@

# Compile object files
objs/%.o: %.c cachelab.h | objs
	$(CC) $(CFLAGS) -o $@ -c $<

# Ignore some unused warnings in trans.c
objs/trans.o: COPT = -O0
objs/trans.o objs/trans_asan.o objs/trans_ct.bc objs/trans_check.bc: \
  CFLAGS += \
    -Wno-unused-const-variable \
    -Wno-unused-function \
    -Wno-unused-parameter

# Compile separately for ASAN
objs/trans_asan.o: trans.c
	$(CC) $(CFLAGS) -o $@ -c $<

# Compile tracegen-ct.o using clang
objs/tracegen-ct.o: COPT = -O3
objs/tracegen-ct.o: CC = $(LLVM_PATH)clang

# Compile bitcode files using LLVM pass
objs/trans_ct.bc: trans.c cachelab.h | objs objs/trans_check.bc
	$(LLVM_PATH)clang $(CFLAGS) -emit-llvm -S -o objs/trans.ll $<
	$(LLVM_PATH)opt -load=ct/CLabInst.so -CLabInst -o $@ objs/trans.ll

objs/trans_check.bc: COPT = -O0
objs/trans_check.bc: trans.c cachelab.h | objs
	$(LLVM_PATH)clang $(CFLAGS) -emit-llvm -S -o objs/trans_check.ll $<
	$(LLVM_PATH)opt -load=ct/Check.so -Check -o $@ objs/trans_check.ll

# Compile instrumented trans_fin.o by linking bitcode files
objs/trans_fin.o: COPT = -O3 -fno-unroll-loops
objs/trans_fin.o: CFLAGS += -DNDEBUG
objs/trans_fin.o: objs/trans_ct.bc ct/ct.bc
	$(LLVM_PATH)llvm-link -o objs/trans_fin.bc $^
	$(LLVM_PATH)clang $(CFLAGS) -o $@ -c objs/trans_fin.bc

# Compile binaries
csim: objs/csim.o objs/cachelab.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test-csim: objs/test-csim.o objs/cachelab.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test-trans: objs/test-trans.o objs/trans.o objs/cachelab.o | tracegen-ct
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test-trans-simple: CC = $(LLVM_PATH)clang
test-trans-simple: COPT = -O0
test-trans-simple: objs/test-trans-simple.o objs/trans_asan.o objs/cachelab.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

tracegen-ct: objs/trans_fin.o objs/cachelab.o objs/tracegen-ct.o
	$(LLVM_PATH)clang -o $@ $^ -pthread -lrt


.PHONY: clean
clean:
	rm -rf objs/
	rm -f $(FILES)
	rm -f trace.all trace.f*
	rm -f .csim_results .marker

# Include rules for submit, format, etc
FORMAT_FILES = csim.c trans.c
HANDIN_FILES = csim.c trans.c \
    .clang-format \
    traces/traces/tr1.trace \
    traces/traces/tr2.trace \
    traces/traces/tr3.trace
include helper.mk


# Compile certain targets with sanitizers
SAN_TARGETS = test-trans-simple
SAN_FLAGS = -fsanitize=integer,alignment,bounds,address -fno-sanitize-recover=bounds

# Add compiler flags
$(SAN_TARGETS): CFLAGS += $(SAN_FLAGS)

# Hacks for loading archive properly
ifneq (,$(wildcard $(SAN_LIBRARY_PATH)))
  $(SAN_TARGETS): LDFLAGS += \
    -Wl,--whole-archive,$(SAN_LIBRARY_PATH)clang/9.0.1/lib/linux/libclang_rt.asan-x86_64.a \
    -Wl,--no-whole-archive \
    -Wl,--export-dynamic \
    -Wl,--no-as-needed
  $(SAN_TARGETS): LDLIBS += -lpthread -lrt -lm -ldl
else
  $(SAN_TARGETS): LDLIBS += $(SAN_FLAGS)
endif


# Add check-format dependencies
submit: | check-format
$(FILES): | check-format
