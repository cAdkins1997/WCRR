#include "application.h"

int main() {
    vulkan::Device device("WCGL", 3840, 2160);
    Application app(device);
}