set(PUBLIC_HEADER_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/wlid_link_server_log

    ${CMAKE_CURRENT_SOURCE_DIR}/src/task/sle_server
)

set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app_entry.c
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/sle_server/sle_server.c

    ${CMAKE_CURRENT_SOURCE_DIR}/src/task/sle_server/sle_server_task.c
)

if ((DEFINED CONFIG_MAX30102_DRIVER_ENABLED) AND ("${CONFIG_MAX30102_DRIVER_ENABLED}" STREQUAL "y"))
    list(APPEND PUBLIC_HEADER_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/max30102
    )
    list(APPEND SOURCES_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/max30102/max30102.c
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/max30102/max30102_algorithm.c
    )
    if ((DEFINED CONFIG_MAX30102_TASK_ENABLED) AND ("${CONFIG_MAX30102_TASK_ENABLED}" STREQUAL "y"))
        list(APPEND PUBLIC_HEADER_LIST
            ${CMAKE_CURRENT_SOURCE_DIR}/src/task/max30102
        )
        list(APPEND SOURCES_LIST
            ${CMAKE_CURRENT_SOURCE_DIR}/src/task/max30102/max30102_task.c
        )
    endif()
endif()

if ((DEFINED CONFIG_MAX30205_DRIVER_ENABLED) AND ("${CONFIG_MAX30205_DRIVER_ENABLED}" STREQUAL "y"))
    list(APPEND PUBLIC_HEADER_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/max30205
    )
    list(APPEND SOURCES_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/max30205/max30205.c
    )
    if ((DEFINED CONFIG_MAX30205_TASK_ENABLED) AND ("${CONFIG_MAX30205_TASK_ENABLED}" STREQUAL "y"))
        list(APPEND PUBLIC_HEADER_LIST
            ${CMAKE_CURRENT_SOURCE_DIR}/src/task/max30205
        )
        list(APPEND SOURCES_LIST
            ${CMAKE_CURRENT_SOURCE_DIR}/src/task/max30205/max30205_task.c
        )
    endif()
endif()

list(APPEND SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/soft_i2c/soft_i2c.c
)

set(PUBLIC_HEADER "${PUBLIC_HEADER}" ${PUBLIC_HEADER_LIST} PARENT_SCOPE)
set(SOURCES "${SOURCES}" "${SOURCES_LIST}" PARENT_SCOPE)
