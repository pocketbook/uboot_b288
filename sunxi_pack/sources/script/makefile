

objects = script.o


edit:$(objects)
	gcc -o script $(objects) -static
	rm -rf $(objects)
	mv script ../..

clean:
	rm -rf script $(objects)

