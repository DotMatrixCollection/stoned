begin
  require "tests/fixtures/require/bad_parse"
rescue LoadError => e
  puts e.class
  puts e.message
end
