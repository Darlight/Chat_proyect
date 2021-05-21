chat: server client
server: Server.cpp message.pb.cc
		g++ -o  Server Server.cpp message.pb.cc -lpthread -lprotobuf
client: client;c-- message.pb.cc
		g++ -o client client.cpp message.pb.cc -lpthread -lprotobuf
message.pb.cc: message.proto
		protoc -I. --cpp_out=. message.proto
	
