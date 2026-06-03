# ========================
# Application Source Files
# ========================

# Recursively find all source files
file(GLOB_RECURSE APP_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/*.c
        ${CMAKE_CURRENT_LIST_DIR}/*.cpp
)

# Exclude specific files (if needed)
set(EXCLUDE_FILES
        # Example:
        # ${CMAKE_CURRENT_LIST_DIR}/devices/ds18b20/ds18b20.cpp
)

foreach (file ${EXCLUDE_FILES})
    list(REMOVE_ITEM APP_SOURCES ${file})
endforeach ()

# Add sources to the main target
target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        ${APP_SOURCES}
)

# Recursively find all include directories
file(GLOB_RECURSE ALL_DIRS LIST_DIRECTORIES TRUE ${CMAKE_CURRENT_LIST_DIR}/*)

set(INCLUDE_DIRS "")
foreach (item ${ALL_DIRS})
    if (IS_DIRECTORY ${item})
        list(APPEND INCLUDE_DIRS ${item})
    endif ()
endforeach ()

# Add include directories
target_include_directories(${CMAKE_PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        ${INCLUDE_DIRS}
)

# Print summary
list(LENGTH APP_SOURCES SOURCE_COUNT)
message(STATUS "Application: Added ${SOURCE_COUNT} source files from src/")
