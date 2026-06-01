require "zlib"

p Zlib::BEST_COMPRESSION
p Zlib::BEST_SPEED
p Zlib::DEFAULT_COMPRESSION
p Zlib.deflate("abc")
p Zlib.inflate("abc")
p Zlib.adler32("abc", 7)
p Zlib.crc32("abc", 9)
