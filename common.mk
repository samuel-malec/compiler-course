# Shared rules for all chapter makefiles. Each chapter makefile sets
# SRC_D, SRC_E, SRC_P, SRC_R (and SRC_T for assignment sets) and TOP
# (path to the top of the bundle), then includes this file.

CXX      = g++
VALGRIND = valgrind
CXXFLAGS = -std=c++23 -g -Wall -Wextra -Werror

-include $(TOP)/local.mk

BIN_D    = ${SRC_D:.cpp=}
BIN_E    = ${SRC_E:.cpp=}
BIN_P    = ${SRC_P:.cpp=}
BIN_R    = ${SRC_R:.cpp=}
BIN_T    = ${SRC_T:.cpp=}

BIN      = $(BIN_D) $(BIN_E) $(BIN_P) $(BIN_R) $(BIN_T)
HELPER   = $(TOP)/.helper.sh

_ENV     = CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" VALGRIND="$(VALGRIND)"

all: $(BIN)

demo: $(BIN_D)
elem: $(BIN_E)
prep: $(BIN_P)
reg:  $(BIN_R)

$(BIN): %: %.cpp
	@env $(_ENV) sh $(HELPER) announce $@
	@env $(_ENV) sh $(HELPER) cxx $< -o $@
	@env $(_ENV) sh $(HELPER) grind ./$@

clean:
	@rm -f $(BIN) a.out core *.core *~ .build.out .valgrind.out

.PHONY: all clean demo elem prep reg
