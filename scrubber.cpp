/// Anthony Speicher
/// 1/13/2026
/// An EXIF metadata scrubber

#include <cstdio>
#include <cstdlib>
#include <cstdint>

/// CURRENTLY ONLY SUPPORTS JPEG IMAGE TYPES

int main (int argc, const char *argv[]) {
  /// instead of writing in place, EXIF block is completely removed

  /// validate usage ----------------------------------------------------------------------------------
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Incorrect argument format. Usage: ./a.out filename.jpg cleaned.jpg (default: cleaned_image.jpg)\n");
    exit(1);
  }

  /// open and validate input and output files --------------------------------------------------------
  FILE* input = std::fopen(argv[1], "rb");
  if (!input) {
    perror("Error opening input file");
    printf("Usage: ./a.out filename.jpg cleaned.jpg (default: cleaned_image.jpg)\n");
    exit(1);
  }
  FILE* output = std::fopen(argc == 3 ? argv[2] : "cleaned_image.jpg", "wb");
  if (!output) {
    perror("Error opening output file");
    printf("Usage: ./a.out filename.jpg cleaned.jpg (default: cleaned_image.jpg)\n");
    exit(1);
  }

  uint8_t buffer[2];
  uint16_t len;

  /// find file length --------------------------------------------------------------------------------
  if (fseek(input, 0, SEEK_END) != 0) {
    perror("fseek failed");
    exit(1);
  }

  long file_length = ftell(input);
  if (file_length < 0) {
    perror("ftell failed");
  }

  rewind(input);

  /// checks SOI segment to validate data -------------------------------------------------------------
  fread(buffer, 1, 2, input);
  if (!(buffer[0] == 0xFF && buffer[1] == 0xD8)) {
    printf("%d %d\n", buffer[0], buffer[1]);
    fprintf(stderr, "File corrupted or not of type JPEG.\n");
    exit(1);
  } else fwrite(buffer, 1, 2, output);

  /// loop until start of SOS segment (picture data) to remove all APP metadata -----------------------
  while (fread(buffer, 1, 2, input) == 2 && buffer[1] != 0xDA) {
    if (buffer[1] >= 0xE0 && buffer[1] <= 0xEF) {
      /// remove metadata - read length bytes
      fread(buffer, 1, 2, input);
      len = (buffer[0] << 8) | buffer[1];

      /// skip segment
      fseek(input, len - 2, SEEK_CUR);
    } else {
      /// keep necessary segments
      fwrite(buffer, 1, 2, output);

      /// read length bytes
      fread(buffer, 1, 2, input);
      len = (buffer[0] << 8 | buffer[1]);
      long segment_length = len - 2;
      uint8_t buf[64];

      while (segment_length) {
        len = segment_length > 64 ? 64 : segment_length;
        fread(buf, 1, len, input);
        fwrite(buf, 1, len, output);
        segment_length -= len;
      }
    }

  }
  fwrite(buffer, 1, 2, output);

  /// copy rest of file verbatim ----------------------------------------------------------------------
  uint8_t buf[256];
  file_length -= ftell(input);

  while (file_length) {
    len = file_length > 256 ? 256 : file_length;
    fread(buf, 1, len, input);
    fwrite(buf, 1, len, output);
    file_length -= len;
  }

  /// exit cleanup ------------------------------------------------------------------------------------
  exit(0);
}
