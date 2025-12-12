add_executable(vtsh_main main.c)
target_link_libraries(vtsh_main PRIVATE libvtsh)
target_include_directories(vtsh_main PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_executable(cpu_mat_mul cpu_mat_mul.c)
target_include_directories(cpu_mat_mul PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_options(cpu_mat_mul PRIVATE -O0) 

add_executable(gen_int_file gen_int_file.c)
target_include_directories(gen_int_file PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_executable(ema_sort_int ema_sort_int.c)
target_link_libraries(ema_sort_int PRIVATE libvtsh)
target_include_directories(ema_sort_int PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_executable(shell_seq shell_seq.c)
set_target_properties(shell_seq PROPERTIES OUTPUT_NAME "shell-seq")
target_include_directories(shell_seq PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_executable(proc_vfork proc_vfork.c)
set_target_properties(proc_vfork PROPERTIES OUTPUT_NAME "proc-vfork")
target_include_directories(proc_vfork PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_executable(cpu_short_path cpu_short_path.c)
set_target_properties(cpu_short_path PROPERTIES OUTPUT_NAME "cpu-short-path")
target_include_directories(cpu_short_path PRIVATE ${CMAKE_CURRENT_LIST_DIR})
