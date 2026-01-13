# Anthony Speicher
# 1/13/2026
# An EXIF metadata scrubber

#include <iostream>
#include <fstream>
#include <cstdio>

/// CURRENTLY ONLY SUPPORTS JPEG IMAGE TYPES

/// FF XX  LL LL  <data>

bool main (int argc, const char *argv[]) {
  /// instead of writing in place, EXIF block is completely removed
  /// open and validate input and output images

  FILE input* = std::fopen("image.jpg", "rb");
  if (!input) {
    std::perror("Error opening input file");
    return false;
  }
  FILE output* = std::fopen("cleaned_image.jpg", "wb");
  if (!output) {
    std::perror("Error opening output file");
    return false;
  }

  uint8_t buffer[2];
  uint16_t len;

  /// checks SOI segment to validate data
  std::fread(buffer, 1, 2, input);
  if (!(buffer[0] == 0xFF)) {
    std::perror("File corrupted or not of type JPEG");
    std::fclose(input);
    std::fclose(output);
    return false;
  } else std::fwrite(buffer, 1, 2, output);

  /// loop until start of APP1 segment (EXIF data)
  while (std::fread(buffer, 1, 2, input) == 2 && buffer[0] != 0xFF && buffer[1] != 0xE1) {
    /// read length bytes
    std::fread(buffer, 1, 2, input);
    len = (buffer[0] << 8) | buffer[1];
    std::fwrite(buffer, 1, 2, output);

    /// read and write segment data
  }

  /// process EXIF data

  /// copy rest of file verbatim

  /// exit cleanup
  std::fclose(input);
  std::fclose(output);
  return true;
}
