add_executable(nntp-dump
	main.cpp 
)
target_link_libraries(nntp-dump PRIVATE NNTPClientSession)
