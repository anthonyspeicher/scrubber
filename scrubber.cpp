/// Anthony Speicher
/// 1/13/2026
/// An EXIF metadata scrubber

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <unordered_set>
#include <array>
#include <functional>


/// add documentation :P
void parse_JPEG(FILE* input, FILE* output);


void parse_PNG(FILE* input, FILE* output);


/// custom hash algorithm for unordered_set compatability
/// thanks to fredoverflow on https://stackoverflow.com/questions/8026890/c-how-to-insert-array-into-hash-set

struct Array_Hash {
  size_t operator() (const std::array<uint8_t, 4>& arr) const {
    std::hash<uint8_t> hasher;
    size_t seed = 0;
    for (uint8_t i : arr) {
      /// hash using magic constant
      seed ^= hasher(i) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
  }
};

int main (int argc, const char *argv[]) {
  /// instead of writing in place, EXIF block is completely removed

  /// validate usage ----------------------------------------------------------------------------------
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Incorrect argument format. Usage: ./a.out filename.jpg cleaned.jpg (default: cleaned_image)\n");
    exit(1);
  }

  /// open and validate input and output files --------------------------------------------------------
  FILE* input = std::fopen(argv[1], "rb");
  if (!input) {
    perror("Error opening input file");
    printf("Usage: ./a.out filename.jpg cleaned.jpg (default: cleaned_image)\n");
    exit(1);
  }
  FILE* output = std::fopen(argc == 3 ? argv[2] : "cleaned_image", "wb");
  if (!output) {
    perror("Error opening output file");
    printf("Usage: ./a.out filename.jpg cleaned.jpg (default: cleaned_image)\n");
    exit(1);
  }

  /// determine filetype and parse --------------------------------------------------------------------
  uint8_t buffer[8];
  uint8_t png_marker[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  fread(buffer, 1, 8, input);

  if (buffer[0] == 0xFF && buffer[1] == 0xD8) {         /// check for JPEG SOI
    parse_JPEG(input, output);
  } else if (memcmp(buffer, png_marker, 8*sizeof(uint8_t)) == 0) {        /// for PNG
    fwrite(buffer, 1, 8, output);
    parse_PNG(input, output);
  } else {
    fprintf(stderr, "File corrupted or not of recognized type\n");
  }


  /// exit cleanup ------------------------------------------------------------------------------------
  exit(0);
}


void parse_JPEG(FILE* input, FILE* output) {
  uint8_t buffer[2];
  size_t len;

  /// find length until 0xFF 0xD9 (EOI marker, may be metadata after) ----------------------------------
  if (fseek(input, 0, SEEK_END) != 0) {
    perror("fseek failed");
    exit(1);
  }

  long file_length = ftell(input);
  if (file_length < 0) {
    perror("ftell failed");
  }

  /// using 1kb chunks to reduce fread calls, reverse search data for EOI marker
  uint8_t temp_buf[1024];
  bool found = 0;
  size_t chunk_size;

  while (!found && file_length > 0) {
    if (file_length >= 1024) {
      file_length -= 1024;
      chunk_size = 1024;
    } else {
      chunk_size = file_length;
      file_length = 0;
    }
    fseek(input, file_length, SEEK_SET);
    fread(temp_buf, 1, chunk_size, input);

    /// search temp_buf for EOI
    for (int i = chunk_size - 2; i >= 0; i--) {
      if (temp_buf[i] == 0xFF && temp_buf[i + 1] == 0xD9) {
        found = 1;
        file_length += i;
        break;
      }
    }
  }

  /// sanity check
  if (file_length < 0) {
    fprintf(stderr, "File corrupted or not of type JPEG.\n");
    exit(1);
  }

  rewind(input);
  fread(buffer, 1, 2, input);
  fwrite(buffer, 1, 2, output);

  /// loop until start of SOS segment (picture data) to remove all APP metadata -----------------------
  while (fread(buffer, 1, 2, input) == 2 && buffer[1] != 0xDA) {
    if (buffer[1] > 0xE0 && buffer[1] <= 0xEF) {
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
      fwrite(buffer, 1, 2, output);
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

  if (file_length < 0) {
    fprintf(stderr, "Verbatim copy failed\n");
    exit(1);
  }

  while (file_length > 0) {
    len = file_length > 256 ? 256 : file_length;
    fread(buf, 1, len, input);
    fwrite(buf, 1, len, output);
    file_length -= len;
  }
}

void parse_PNG(FILE* input, FILE* output) {
  /// metadata segment headers
  const std::unordered_set<std::array<uint8_t, 4>, Array_Hash> metadata_headers {
    {0x74, 0x45, 0x58, 0x74},   /// tEXt
    {0x7A, 0x54, 0x58, 0x74},   /// zTXt
    {0x69, 0x54, 0x58, 0x74},   /// iTXt
    {0x65, 0x58, 0x49, 0x66},   /// eXIf
    {0x74, 0x49, 0x4D, 0x45},   /// tIME
    {0x70, 0x48, 0x59, 0x73}    /// pHYs
  };
  const std::array<uint8_t, 4> IEND = {0x49, 0x45, 0x4E, 0x44};

  uint8_t buffer[4];
  std::array<uint8_t, 4> header;
  uint8_t data[256];
  uint32_t length;
  long current;

  while (fread(buffer, 1, 4, input) == 4) {
    /// format length bytes
    length = ((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);
    length += 4; /// CRC bytes

    /// check header
    fread(header.data(), 1, 4, input);
    if (metadata_headers.find(header) != metadata_headers.end()) {
      /// skip metadata
      fseek(input, length, SEEK_CUR);
    } else {
      /// write data
      fwrite(buffer, 1, 4, output);
      fwrite(header.data(), 1, 4, output);

      while (length > 0) {
        current = length > 256 ? 256 : length;
        fread(data, 1, current, input);
        fwrite(data, 1, current, output);
        length -= current;
      }

      /// IEND (EOF) handling
      if (memcmp(header.data(), IEND.data(), 4 * sizeof(uint8_t)) == 0) {
        break;
      }
    }
  }
}
