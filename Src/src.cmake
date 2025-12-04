# Рекурсивно находим все .cpp и .c файлы в папке Src и её подкаталогах
file(GLOB_RECURSE SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/*.cpp
        ${CMAKE_CURRENT_LIST_DIR}/*.c
        ${CMAKE_CURRENT_LIST_DIR}/*.asm
)

# Исключаем указанные файлы
set(EXCLUDE_FILES
        # ${CMAKE_CURRENT_LIST_DIR}/Legacy/chlib_stub.c
)

foreach (file ${EXCLUDE_FILES})
    list(REMOVE_ITEM SOURCES ${file})
endforeach ()

# Добавляем найденные исходники в target_sources
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${SOURCES})

# Рекурсивно ищем все директории в Src
file(GLOB_RECURSE ALL_ITEMS LIST_DIRECTORIES true ${CMAKE_SOURCE_DIR}/Src/*)

set(INCLUDE_DIRS "")
foreach (item ${ALL_ITEMS})
    if (IS_DIRECTORY ${item})
        list(APPEND INCLUDE_DIRS ${item})
    endif ()
endforeach ()

# Добавляем в target_include_directories
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${INCLUDE_DIRS})