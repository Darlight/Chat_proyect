chat: server client

payload.pb.cc: payload.proto
		protoc -I. --cpp_out=. payload.proto
	
server: Server.cpp payload.pb.cc
		g++ -o  Server Server.cpp payload.pb.cc -lpthread -lprotobuf

client: client.cpp payload.pb.cc
		g++ -o client client.cpp payload.pb.cc -lpthread -lprotobuf


