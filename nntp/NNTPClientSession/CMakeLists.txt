add_library(NNTPClientSession
	NNTPClientSession.h
	NNTPClientSession.cpp
)
target_link_libraries(NNTPClientSession PUBLIC Poco::Net)
target_include_directories(NNTPClientSession PUBLIC .)
