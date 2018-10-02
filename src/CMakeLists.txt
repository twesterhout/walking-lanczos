
add_executable(main main.cpp spin_chain.cpp diffusion.cpp hamiltonian.cpp quantum_state.cpp)
target_link_libraries(main PRIVATE Lanczos)
