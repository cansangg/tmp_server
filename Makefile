# 编译器和库
CXX = g++
LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
TARGET = a.out

# 默认只编译
$(TARGET): a.cpp
	$(CXX) a.cpp -o $(TARGET) $(LIBS) -std=c++17

# 【核心：一键编译 + 软件渲染运行】
# 这里不需要 &&，因为 make 会按顺序检查依赖
run: $(TARGET)
	LIBGL_ALWAYS_SOFTWARE=1 ./$(TARGET)

# 清理
clean:
	rm -f $(TARGET)