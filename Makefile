# 注意：第二行开头必须是一个真实的 Tab 键，不能是空格！
a.out: a.cpp
	g++ a.cpp -o a.out -lraylib -lGL -lm -lpthread -ldl -lrt -lX11