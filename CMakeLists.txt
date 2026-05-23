cmake_minimum_required(VERSION 3.16)
project(ColumnarEngine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Общие исходники (библиотека) ───────────────────────────────────────────
set(COMMON_SOURCES
        src/types/type_register.cpp
        src/types/types_data/type_date.cpp
        src/types/types_data/type_int16.cpp
        src/types/types_data/type_int32.cpp
        src/types/types_data/type_int64.cpp
        src/types/types_data/type_string.cpp
        src/types/types_data/type_timestamp.cpp


        src/utils/date_utils.cpp

        src/utils/aggregate_utils.h  # добавлен, если используется

        src/structs/column.cpp
        src/structs/schema.cpp
        src/structs/columnar_format.cpp

        src/csv_io/csv_reader.cpp
        src/csv_io/csv_writer.cpp

        src/operators/iop.h
        src/operators/table_scan.cpp
        src/operators/filter.cpp
        src/operators/global_aggregate.cpp
        src/operators/projection.cpp
        src/operators/group_aggregate.cpp
        src/operators/topk.cpp
        src/operators/sort.cpp
        src/operators/limit.cpp
        src/operators/offset.cpp
        src/operators/materialized_scan.h
)

# ─── Бинарник 1: csv_to_columnar ────────────────────────────────────────────
# Путь вывода: build/Release/exe/csv_to_columnar
add_executable(csv_to_columnar
        src/tools/csv_to_columnar_main.cpp
        ${COMMON_SOURCES}
)
target_include_directories(csv_to_columnar PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(csv_to_columnar PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/exe"
)

# ─── Бинарник 2: ngn-clickbench-run ─────────────────────────────────────────
# Путь вывода: build/Release/clickbench/ngn-clickbench-run
add_executable(ngn-clickbench-run
        src/tools/run_queries_main.cpp
        ${COMMON_SOURCES}
)
target_include_directories(ngn-clickbench-run PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(ngn-clickbench-run PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/clickbench"
)
