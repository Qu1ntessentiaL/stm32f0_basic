# Рекурсивно находим все .cpp и .c файлы в папке Src и её подкаталогах
file(GLOB_RECURSE SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/*.cpp
        ${CMAKE_CURRENT_LIST_DIR}/*.c
)

# Исключаем указанные файлы
set(EXCLUDE_FILES
        # ${CMAKE_CURRENT_LIST_DIR}/main.cpp
)


foreach (file ${EXCLUDE_FILES})
    list(REMOVE_ITEM SOURCES ${file})
endforeach ()

# Добавляем найденные исходники в target_sources
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${SOURCES})

# Рекурсивно находим все папки в Src для добавления в target_include_directories
file(GLOB_RECURSE INCLUDES ${CMAKE_SOURCE_DIR}/Src/*)

# Добавляем папки в target_include_directories
foreach (include_dir ${INCLUDES})
    if (IS_DIRECTORY ${include_dir})
        target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${include_dir})
    endif ()
endforeach ()