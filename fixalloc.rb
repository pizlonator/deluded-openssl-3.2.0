#!/usr/bin/env ruby

Dir.glob("**/*.[ch]") {
    | filename |
    contents = IO::read(filename)
    contents.gsub!(/OPENSSL_zalloc\(sizeof\(\*([a-zA-Z0-9_]+)\)\)/) {
        | match |
        "zalloc_zero(typeof(*#{$1}), 1)"
    }
    IO::write(filename, contents)
}
