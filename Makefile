# builds ./car_world on macos (homebrew glew + glfw) or linux (libglew-dev libglfw3-dev)

CXX      ?= c++
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Ithird_party/imgui -MMD -MP

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
  BREW     := $(shell brew --prefix 2>/dev/null || echo /opt/homebrew)
  CXXFLAGS += -I$(BREW)/include -DGL_SILENCE_DEPRECATION
  LDLIBS   += -L$(BREW)/lib -lGLEW -lglfw -lz -framework OpenGL
else
  LDLIBS   += -lGLEW -lglfw -lGL -lz
endif

IMGUI := imgui imgui_draw imgui_tables imgui_widgets imgui_demo imgui_impl_glfw imgui_impl_opengl3
OBJS  := src/main.o $(IMGUI:%=third_party/imgui/%.o)

car_world: $(OBJS)
	$(CXX) $(OBJS) $(LDLIBS) -o $@

clean:
	rm -f car_world $(OBJS) $(OBJS:.o=.d)

.PHONY: clean
-include $(OBJS:.o=.d)
