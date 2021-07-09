

objects = check.o script.o update_boot0.o


edit:$(objects)
	gcc -o update_boot0 $(objects) -static
	rm -rf $(objects)
	mv update_boot0 ../..

clean:
	rm -rf update_boot0 $(objects)
