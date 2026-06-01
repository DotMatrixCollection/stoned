require "timeout"

p Timeout.timeout(1) { "done" }
p Timeout::Error < RuntimeError
