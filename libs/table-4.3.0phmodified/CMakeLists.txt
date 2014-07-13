cmake_minimum_required(VERSION 2.6)

project(Table)

set(Table_VERSION_MAJOR 4)
set(Table_VERSION_MINOR 3)

configure_file(
  "${PROJECT_SOURCE_DIR}/TableConfig.h.in"
  "${PROJECT_BINARY_DIR}/TableConfig.h"
)

include_directories("${PROJECT_BINARY_DIR}")

add_library(table STATIC table.c table.h table_loc.h)

add_executable(table_t table_t.c table.h)
target_link_libraries(table_t table)

add_executable(table_util table_util.c table.h)
target_link_libraries(table_util table)


