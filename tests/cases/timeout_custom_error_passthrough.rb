require "timeout"

value = Timeout.timeout(0.01, Timeout::Error) { 123 }
p value
