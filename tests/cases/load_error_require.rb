begin
  require "tests/fixtures/require/missing"
rescue LoadError => e
  puts e.class
  puts e.message
end
