# Required keyword argument
def greet(name:)
  "Hello, #{name}!"
end
puts greet(name: "Alice")

# Optional keyword argument with default
def connect(host:, port: 80)
  "#{host}:#{port}"
end
puts connect(host: "example.com")
puts connect(host: "example.com", port: 443)

# Mixed positional and keyword arguments
def log(level, message:, timestamp: "now")
  "[#{timestamp}] #{level}: #{message}"
end
puts log("INFO", message: "started")
puts log("WARN", message: "slow", timestamp: "12:00")

# Double-splat collects remaining kwargs
def config(**opts)
  opts.map { |k, v| "#{k}=#{v}" }.sort.join(", ")
end
puts config(debug: true, verbose: false)

# Mix: positional, optional kwarg, double-splat
def full(x, y: 10, **rest)
  "x=#{x} y=#{y} rest=#{rest}"
end
puts full(1)
puts full(1, y: 20)
puts full(1, y: 20, z: 30)

# **hash at call site merges into kwargs
opts = {port: 9000}
puts connect(**opts, host: "local")
puts connect(host: "local", **opts)

# Merge order follows call order: later keyword sources win
host_opts = {host: "remote"}
puts connect(**host_opts, host: "local")
puts connect(host: "local", **host_opts)

# nil is a valid keyword value (not treated as missing)
begin
  greet(name: nil)
  puts "nil ok"
rescue ArgumentError => e
  puts e.message
end

# Missing required keyword raises ArgumentError
begin
  greet()
rescue ArgumentError => e
  puts e.message
end

# Unknown keywords are rejected unless **opts is present
begin
  connect(host: "example.com", scheme: "https")
rescue ArgumentError => e
  puts e.message
end
