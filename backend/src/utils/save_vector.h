#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

void save_vector_to_file(const std::vector<uint32_t>& data, const std::string& filename) {
    std::ofstream out_file(filename, std::ios::binary);
    if (!out_file) {
        std::cerr << "Could not open the file for writing!" << std::endl;
        return;
    }

    // Write the size of the vector
    uint32_t size = data.size();
    out_file.write(reinterpret_cast<const char*>(&size), sizeof(size));

    // Write the vector data
    out_file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint32_t));

    out_file.close();
}