function(mathhunter_add_test name)
    add_executable(${name} ${name}.cpp)
    # See top-level CMakeLists.txt: mathhunter_core's conjecture modules
    # self-register via [[gnu::constructor]], so every consumer -- test
    # binaries included -- must link it with --whole-archive or module
    # registration silently never runs.
    target_link_libraries(${name} PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,mathhunter_core>)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

mathhunter_add_test(test_modular)
mathhunter_add_test(test_certificate_roundtrip)
mathhunter_add_test(test_certificate_corruption)
mathhunter_add_test(test_kurepa_verifier)
mathhunter_add_test(test_toy_verifier)
mathhunter_add_test(test_pigeonhole)
mathhunter_add_test(test_encoded_blob)
mathhunter_add_test(test_work_unit)
mathhunter_add_test(test_work_unit_determinism)
mathhunter_add_test(test_tracker)
mathhunter_add_test(test_worker_pipeline)

add_test(NAME check_layering
         COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/check_layering.sh ${CMAKE_SOURCE_DIR})
