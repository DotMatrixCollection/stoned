require "rbconfig"

puts RbConfig::CONFIG["ruby_version"]
puts RbConfig.ruby.end_with?("/stoned")

ENV["STONED_TEST_BOOT"] = "ok"
puts ENV["STONED_TEST_BOOT"]
puts ENV.to_hash.class == Hash

puts Process.pid > 0

Thread::Mutex.new.synchronize do
  puts "sync"
end

module Demo
  autoload :Value, "tests/fixtures/host_autoload_value"
end

puts Demo::Value
