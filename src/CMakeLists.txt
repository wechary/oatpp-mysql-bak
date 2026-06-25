set(OATPP_THIS_MODULE_SOURCES 
        oatpp-mysql/mapping/Deserializer.cpp
        oatpp-mysql/mapping/Deserializer.hpp
        oatpp-mysql/mapping/ResultMapper.cpp
        oatpp-mysql/mapping/ResultMapper.hpp
        oatpp-mysql/mapping/Serializer.cpp
        oatpp-mysql/mapping/Serializer.hpp
        oatpp-mysql/ql_template/Parser.cpp
        oatpp-mysql/ql_template/Parser.hpp
        oatpp-mysql/ql_template/TemplateValueProvider.cpp
        oatpp-mysql/ql_template/TemplateValueProvider.hpp
        oatpp-mysql/Connection.cpp
        oatpp-mysql/Connection.hpp
        oatpp-mysql/ConnectionProvider.cpp
        oatpp-mysql/ConnectionProvider.hpp
        oatpp-mysql/Executor.cpp
        oatpp-mysql/Executor.hpp
        oatpp-mysql/QueryResult.cpp
        oatpp-mysql/QueryResult.hpp
        oatpp-mysql/orm.hpp
)

add_library(${OATPP_THIS_MODULE_NAME} SHARED ${OATPP_THIS_MODULE_SOURCES})

set_target_properties(${OATPP_THIS_MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
)

if(OATPP_MODULES_LOCATION STREQUAL OATPP_MODULES_LOCATION_EXTERNAL)
    add_dependencies(${OATPP_THIS_MODULE_NAME} ${LIB_OATPP_EXTERNAL})
endif()

target_link_oatpp(${OATPP_THIS_MODULE_NAME})

target_include_directories(${OATPP_THIS_MODULE_NAME}
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)
find_path(MYSQL_INCLUDE_DIR mysql/mysql.h /www/server/mysql/include /usr/include /usr/local/include)
if(NOT MYSQL_INCLUDE_DIR OR MYSQL_INCLUDE_DIR STREQUAL "MYSQL_INCLUDE_DIR-NOTFOUND")
    unset(MYSQL_INCLUDE_DIR CACHE)
    find_program(MYSQL_CONFIG mysql_config)
    if(MYSQL_CONFIG)
        execute_process(COMMAND ${MYSQL_CONFIG} --cflags
                        OUTPUT_VARIABLE MYSQL_CFLAGS
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REGEX MATCHALL "-I[^ ]+" MYSQL_INCLUDE_DIRS "${MYSQL_CFLAGS}")
        list(GET MYSQL_INCLUDE_DIRS 0 MYSQL_INCLUDE_DIR_OPTION)
        string(REGEX REPLACE "^-I" "" MYSQL_INCLUDE_DIR "${MYSQL_INCLUDE_DIR_OPTION}")
    endif()
endif()

if(NOT MYSQL_INCLUDE_DIR OR MYSQL_INCLUDE_DIR STREQUAL "MYSQL_INCLUDE_DIR-NOTFOUND")
    message(FATAL_ERROR "MySQL header mysql/mysql.h not found. Install MySQL client development headers or set MYSQL_INCLUDE_DIR.")
endif()

target_include_directories(${OATPP_THIS_MODULE_NAME} PRIVATE ${MYSQL_INCLUDE_DIR})

target_link_libraries(${OATPP_THIS_MODULE_NAME} PRIVATE mysqlclient)

#######################################################################################################
## install targets

if(OATPP_INSTALL)
    include("../cmake/module-install.cmake")
endif()
