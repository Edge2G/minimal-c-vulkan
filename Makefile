CFLAGS = -std=c++17 -O2
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

Compile: main.c
	gcc -o a.out main.c $(LDFLAGS)

test: a.out
	./a.out

clean:
	rm -f a.out