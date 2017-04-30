all:
	gcc main.c -L/usr/lib/x86_64-linux-gnu/ -lnetfilter_conntrack -o netnf

clean:
	rm -f *.o netnf