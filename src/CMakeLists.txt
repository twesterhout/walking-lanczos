
add_executable(main main.cpp spin_chain.cpp diffusion.cpp hamiltonian.cpp quantum_state.cpp)
target_link_libraries(main PUBLIC Lanczos)
target_link_libraries(main PUBLIC "-static-libstdc++ -static-libgcc")
if (COMPILER_LTO_SUPPORTED)
    set_property(TARGET main PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "LTO disabled: ${COMPILER_LTO_ERROR}")
endif()
