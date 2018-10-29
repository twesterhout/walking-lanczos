

add_executable(spin_vector_test spin_vector_test.cpp)
target_link_libraries(spin_vector_test PRIVATE Lanczos gtest Threads::Threads)
gtest_add_tests(TARGET spin_vector_test)
