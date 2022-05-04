add_executable(nntp-client 
	main.cpp 
	NNTPClientSession.h
	NNTPClientSession.cpp
)
target_link_libraries(nntp-client PRIVATE Poco::Net)
