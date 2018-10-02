
#include "quantum_state.hpp"
#include "hamiltonian.hpp"
#include "spin_chain.hpp"
#include <gtest/gtest.h>


TEST(Construction, InitializerList)
{
    {
        SpinVector spin{1, 0, 0, 1, 1, 0, 1};
        ASSERT_EQ(spin.size(), 7);
        ASSERT_EQ(static_cast<Spin>(spin[0]), Spin::up);
        ASSERT_EQ(static_cast<Spin>(spin[5]), Spin::down);
        ASSERT_EQ(spin.data()[0], std::byte{0x9A});
    }
    {
        SpinVector spin{0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1};
        ASSERT_EQ(spin.size(), 14);
        ASSERT_EQ(static_cast<Spin>(spin[0]), Spin::down);
        ASSERT_EQ(static_cast<Spin>(spin[6]), Spin::up);
        ASSERT_EQ(spin.data()[0], std::byte{0x32});
        ASSERT_EQ(spin.data()[1], std::byte{0xB4});
    }
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
