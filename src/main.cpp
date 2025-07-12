#include <iostream>
#include <string>

namespace {

void print_usage(std::ostream& output) {
    output << "usage: ./ray-scene-tracer <scene.rt> <output.ppm> [--checksum]\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_usage(std::cout);
        return 0;
    }
    print_usage(std::cerr);
    return 2;
}
