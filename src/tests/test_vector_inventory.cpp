#include "tests/test_vector_inventory.hpp"

#include "tests/crypto/vector_file.hpp"
#include "tests/test_support.hpp"

#include <filesystem>
#include <string>

#ifndef OMNI_TEST_VECTOR_DIR
#error OMNI_TEST_VECTOR_DIR must be defined for crypto vector tests
#endif

namespace omni::tests {
namespace {

const std::filesystem::path vector_root{OMNI_TEST_VECTOR_DIR};

std::size_t hex_count(const char* file_name) {
    return crypto_vectors::parse_hex_pairs(
        crypto_vectors::read_text(vector_root / file_name)).size();
}

std::size_t armax_count(const char* file_name) {
    return crypto_vectors::parse_armax_lines(
        crypto_vectors::read_text(vector_root / file_name)).size();
}

void test_supplied_vector_inventory() {
    require(hex_count("ARMAX[DECRYPTED-Armax(RAW)]-.hack__G.U. Vol 1_ Rebirth - Copy.txt") == 274U,
            "ARMAX decrypted vector count changed");
    require(armax_count("ARMAX[ENCRYPTED]-.hack__G.U. Vol 1_ Rebirth.txt") == 274U,
            "ARMAX encrypted vector count changed");

    require(hex_count("CB1-6[Decryption]-4 x 4 Evolution - SLUS_200.91.txt") == 9U,
            "CB1-6 decrypted vector count changed");
    require(hex_count("CB1-6[Encryption]-4 x 4 Evolution - SLUS_200.91.txt") == 9U,
            "CB1-6 encrypted vector count changed");

    require(hex_count("CB7+[Decryption]-187 Ride Or Die - SLUS_211.16.txt") == 111U,
            "CB7+ decrypted vector count changed");
    require(hex_count("CB7+[Encryption]-187 Ride Or Die - SLUS_211.16.txt") == 111U,
            "CB7+ encrypted vector count changed");

    require(hex_count("Xploderv5+[Decrypted]-4x4 Evo 2.txt") == 2U,
            "GS3 4x4 decrypted vector count changed");
    require(hex_count("Xploderv5+[Encrypted]-4x4 Evo 2.txt") == 4U,
            "GS3 4x4 encrypted vector count changed");
    require(hex_count("Xploderv5+[Decrypted]-Catwoman - Copy.txt") == 8U,
            "GS3 Catwoman decrypted vector count changed");
    require(hex_count("Xploderv5+[Encrypted]-Catwoman.txt") == 14U,
            "GS3 Catwoman encrypted vector count changed");
}

} // namespace

void run_vector_inventory_tests() {
    test_supplied_vector_inventory();
}

} // namespace omni::tests
