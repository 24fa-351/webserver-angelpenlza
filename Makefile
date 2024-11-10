w: web run clean

web: web.c
	gcc web.c -o web

run: 
	./web -p 8080

clean:
	rm web

t:
	telnet 127.0.0.1 8080