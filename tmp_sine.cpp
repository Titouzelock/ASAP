#include <cmath>
#include <cstdio>

int main() {
  for (int i = 0; i < 256; ++i) {
    double angle = 2.0 * 3.14159265358979323846 * i / 256.0;
    int v = static_cast<int>(std::round(32767.0 * std::sin(angle)));
    std::printf("%6d%s", v, (i==255?"":","));
    if ((i+1)%8==0) std::printf("\n"); else std::printf(" ");
  }
  return 0;
}
