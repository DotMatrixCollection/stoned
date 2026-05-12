begin
  require_relative "../fixtures/require_relative/missing"
rescue LoadError => e
  puts e.class
  puts e.message
end
