CXX = clang++
CXXFLAGS = -m64 -std=c++17 -Xlinker /NODEFAULTLIB
INCLUDES = -I include/ -I C:\VulkanSDK\1.3.204.1\Include
LIBS = C:\VulkanSDK\1.3.204.1\Lib\vulkan-1.lib -L lib\static-ucrt -lglfw3

all: triangle.exe

triangle.exe: triangle.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) triangle.cpp -o triangle.exe