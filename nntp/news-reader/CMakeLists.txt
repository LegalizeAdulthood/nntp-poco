add_executable(news-reader
	main.cpp 
)
target_link_libraries(news-reader PUBLIC NNTPClientSession)
