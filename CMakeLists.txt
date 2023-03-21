project(ttyapple)

set(CMAKE_CXX_STANDARD 20)

find_library(AVCODEC_LIBRARY avcodec)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVUTIL_LIBRARY avutil)
find_library(SWSCALE_LIBRARY swscale)

find_package(fmt REQUIRED)
find_package(PNG REQUIRED)

include_directories(${PNG_LIBRARY})

set(SOURCES
    main.cpp
    paths.cpp
    stream_context.cpp

    output.cpp
    c_output.cpp
    tty_output.cpp
    uefi_output.cpp
)

add_executable(ttyapple ${SOURCES})

install(DIRECTORY data DESTINATION ${INSTALL_PATH}/data)

target_link_libraries(ttyapple
    fmt::fmt
    ${PNG_LIBRARY}
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${SWSCALE_LIBRARY}
)
