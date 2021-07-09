

objects = check.o script.o update_uboot.o


edit:$(objects)
	gcc -o update_uboot $(objects) -static
	rm -rf $(objects)
	mv update_uboot ../..

clean:
	rm -rf update_uboot $(objects)
