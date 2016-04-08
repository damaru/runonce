
all:runonce

runonce: runonce.c
	gcc $< -L /usr/X11R6/lib/ -lX11 -lcargs -o $@ 


clean:
	@-rm -vf runonce test *.*~

indent:
	for f in *.[ch];do ./scripts/indent $$f; done

install:runonce
	cp runonce /usr/local/bin/

commit:clean indent
	git commit -m "commit for timestamp `date +%s`" -a
	git push origin master


