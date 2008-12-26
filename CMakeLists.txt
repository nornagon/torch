cmake_minimum_required(VERSION 2.6)
PROJECT (TORCH CXX C)

SET(CMAKE_BUILD_TYPE "Debug" CACHE STRING "The type of build to perform. Valid values are: RelWithDebInfo (default), Debug, Release, MinSizeRel")
IF (CMAKE_BUILD_TYPE STREQUAL "")
	SET(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "The type of build to perform. Valid values are: RelWithDebInfo (default), Debug, Release, MinSizeRel" FORCE)
ENDIF (CMAKE_BUILD_TYPE STREQUAL "")

SET (SRC "${TORCH_SOURCE_DIR}")
SET (GEN "${SRC}/arm9/adrift/entities")

FILE (GLOB engine_src ${SRC}/arm9/engine/*.cpp)
FILE (GLOB world_src ${SRC}/arm9/adrift/*.cpp)
FILE (GLOB native_src ${SRC}/native/src/*.cpp ${SRC}/native/src/*.c)
FILE (GLOB generic_src ${SRC}/arm9/generic/*.cpp ${SRC}/arm9/generic/*.c)
FILE (GLOB main_src ${SRC}/arm9/source/*.cpp ${SRC}/arm9/source/*.c)
SET (generated_files
	${GEN}/object.h
	${GEN}/object.cpp
	${GEN}/creature.h
	${GEN}/creature.cpp
	${GEN}/terrain.h
	${GEN}/terrain.cpp
	)
FILE (GLOB entities ${SRC}/arm9/adrift/*.rb)


INCLUDE_DIRECTORIES(${SRC}/arm9/include ${SRC}/arm9/engine ${SRC}/arm9/generic
	${SRC}/native/include ${SRC}/arm9/adrift)
INCLUDE_DIRECTORIES(${SDL_INCLUDE_DIR})

ADD_DEFINITIONS( "-Wall -Wunused" )
ADD_DEFINITIONS( -DNATIVE=1 )

FIND_PACKAGE(SDL REQUIRED)

ADD_CUSTOM_COMMAND(
	OUTPUT ${generated_files}
	COMMAND ruby -I${SRC} ${SRC}/genentities.rb ${GEN}
	DEPEND ${entities}
	)

SET(torch_sources
	${world_src}
	${engine_src}
	${native_src}
	${generic_src}
	${main_src}
	${generated_files}
	)

ADD_EXECUTABLE(torch ${torch_sources})

TARGET_LINK_LIBRARIES(torch SDL)
