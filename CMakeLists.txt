cmake_minimum_required(VERSION 3.31)
project("HevNet" VERSION 0.0.1 )
set(CMAKE_DEBUG_POSTFIX d)

add_library(${PROJECT_NAME} STATIC)


set(sources 
	${PROJECT_SOURCE_DIR}/src/rudp.cpp
	${PROJECT_SOURCE_DIR}/src/packet.cpp
	${PROJECT_SOURCE_DIR}/src/timer.cpp
)

set(headers 
	${PROJECT_SOURCE_DIR}/include/rudp.h
	${PROJECT_SOURCE_DIR}/include/packet.h
	${PROJECT_SOURCE_DIR}/include/timer.h
)

target_sources(${PROJECT_NAME}
	PRIVATE
	${sources}
	${headers}
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE ${PROJECT_SOURCE_DIR}/src 
    PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>

	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		${PROJECT_SOURCE_DIR}/include
)

