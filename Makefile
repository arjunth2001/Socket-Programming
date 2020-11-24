all :
	gcc -g ./client/client.c -o ./bin/client
	gcc -g ./client/client.c -o ./client/client 
	gcc -g ./server/server.c -o ./bin/server
	gcc -g ./server/server.c -o ./server/server
clean:
	-rm -f ./bin/client ./bin/server ./server/server ./client/client
