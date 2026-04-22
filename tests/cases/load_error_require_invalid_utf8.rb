begin
  require "tests/fixtures/require/invalid_utf8"
rescue LoadError => e
  puts e.class
  puts e.message
end
