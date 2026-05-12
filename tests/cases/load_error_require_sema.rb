begin
  require "tests/fixtures/require/bad_sema"
rescue LoadError => e
  puts e.class
  puts e.message
end
